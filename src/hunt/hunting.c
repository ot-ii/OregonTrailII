#include "hunting.h"

#include <stddef.h>

#include "animal.h"
#include "animalbase.h"
#include "animals.h"
#include "../data/assets.h"
#include "../base/byteorder.h"
#include "../draw/canvas.h"
#include "../base/date.h"
#include "../draw/drawable.h"
#include "../app/event.h"
#include "../app/evthandler.h"
#include "../game/game.h"
#include "../game/gamecmd.h"
#include "../dialogs/huntweapondlg.h"
#include "../base/log.h"
#include "../draw/mediabase.h"
#include "../base/memory.h"
#include "../trail/node.h"
#include "../app/otiiapp.h"
#include "../draw/palette.h"
#include "../game/party.h"
#include "../draw/pictserver.h"
#include "../base/rng.h"
#include "../data/rsrc.h"
#include "../draw/server.h"
#include "../app/settings.h"
#include "../media/sndbase.h"
#include "../media/sndobj.h"
#include "../media/sndstream.h"
#include "../draw/sprobj.h"
#include "../base/str.h"
#include "../media/strmdsnd.h"
#include "../draw/viewsurface.h"

struct hunting *g_hunt_state;
int32_t g_last_spawn_mile = -20;
int32_t g_weapon_damage[4] = {25, 80, 10, 0};

static const int32_t hunt_events[3] = {CMD_MOUSE, GCMD_HUNT_RELOAD, GCMD_HUNT_NEW_SCENE};

const struct hunting_ops HUNTING_OPS_TABLE = {
	{
		{
			{
				{hunting_destroy},
				evt_handler_is_handled,
				evt_handler_set_handled_for,
				evt_handler_set_handled,
				evt_handler_clear_handled_for,
				evt_handler_clear_handled,
				hunting_on_event,
				blink_view_on_idle,
				blink_view_on_idle_spare,
			},
			view_add_child,
			view_get_bounds,
			view_get_clip_region,
			view_hide,
			view_remove_child,
			view_remove_from_parent,
			view_set_cursor_id,
			view_set_device,
			view_show,
			blink_view_draw_children,
			view_move_children,
		},
	},
};

static struct view *view_of(struct hunting *self)
{
	return &self->base.base;
}

void HuntingInit(struct hunting *self, struct assets *data_lib, int32_t view_id, struct rect *rect,
	struct node *cnode, bool is_stampede)
{
	BlinkViewInitSurface(&self->base, view_id, rect, g_palette, true, true, false);
	self->base.base.base.base.ops = &HUNTING_OPS_TABLE.base.base.base.base;

	self->data_lib = data_lib;

	self->animal_count = 0;
	self->aim[HUNT_MEAT] = 0;
	self->aim[HUNT_SHOTS_FIRED] = 0;
	self->aim[HUNT_SHOTS_LEFT] = 20;
	self->aim[HUNT_HITS] = 0;
	self->unused_C0 = false;
	self->ready_to_fire = true;
	self->aim[HUNT_WEAPON] = WEAPON_RIFLE;
	self->scenery_c_drawable = NULL;
	self->scenery_c_id = -1;
	self->scenery_b_id = self->scenery_c_id;
	self->scenery_a_id = self->scenery_b_id;
	self->node = cnode;
	self->shnt_rsrc = NULL;
	self->is_stampede = is_stampede;

	for (int32_t index_j = 0; index_j < 3; index_j++)
		self->miss_sounds[index_j] = NULL;

	self->shot_sound = NULL;
	self->dry_fire_sound = NULL;
	self->blit_rect = ViewSurfaceGetBoundsV(self->base.surface);

	for (int32_t index_j = 0; index_j < 15; index_j++)
		self->animals[index_j] = NULL;

	HuntingLoadScene(self);
	HuntingSpawnTick(self, (struct node_base *) self->node);

	self->miss_marker = NULL;
	self->crosshair = ServerCreateVirtual(&g_pict_server->base.base, 16005, true);

	if (self->crosshair) {
		self->miss_marker = AnimalCreate(self->base.surface, (struct sprite *) self->crosshair);
		SPR_OBJ_OPS(self->miss_marker)->Hide(&self->miss_marker->base);
	}

	EvtHandlerBindHandler(&self->base.base.base, 3, hunt_events);
}

struct hunting *HuntingCreate(struct assets *data_lib, int32_t view_id, struct rect *rect, struct node *cnode,
	bool is_stampede)
{
	struct hunting *self = MemAlloc(1, sizeof(struct hunting));
	HuntingInit(self, data_lib, view_id, rect, cnode, is_stampede);

	return self;
}

void HuntingDestroy(struct hunting **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void hunting_destroy(struct object *obj)
{
	struct hunting *self = (struct hunting *) obj;

	if (self->shnt_rsrc)
		RsrcDestroy(&self->shnt_rsrc);

	if (self->crosshair)
		ReleaseDrawable(&self->crosshair->base.base, true);

	if (GetGameObj())
		g_last_spawn_mile = GetGameObj()->total_miles;

	for (int32_t index_b = 0; index_b < 3; index_b++) {
		if (self->miss_sounds[index_b])
			ReleaseDrawable(&self->miss_sounds[index_b]->base.base, false);
	}

	if (self->shot_sound)
		ReleaseDrawable(&self->shot_sound->base.base, false);

	if (self->dry_fire_sound)
		ReleaseDrawable(&self->dry_fire_sound->base.base, false);

	obj->ops = &BLINK_VIEW_OPS_TABLE.base.base.base;
	blink_view_destroy(obj);
}

static bool in_rect(const struct rect *b, const struct point *pt)
{
	if (pt->h < b->left)
		return false;

	if (pt->h > b->right)
		return false;

	if (pt->v < b->top)
		return false;

	if (pt->v > b->bottom)
		return false;

	return true;
}

struct animal_base *HuntingFindHitAnimal(struct hunting *self, struct point pt, int32_t *killed)
{
	struct animal_base *hit_o = NULL;
	struct rect bounds_p = {0};
	bool in_shape_a = false;

	*killed = 0;

	struct canvas *canvas_g = self->base.surface->cover_mask;
	uint8_t pixel_i = CanvasGetPixel(canvas_g, pt.h, pt.v);

	if (!pixel_i || pixel_i == 255) {
		for (int32_t index_k = 0; self->animal_count > index_k; index_k++) {
			struct spr_obj *animal = self->animals[index_k];

			if (animal->visible) {
				if (((struct animal_base *) animal)->alive) {
					bounds_p = animal->region1.rect;

					if (in_rect(&bounds_p, &pt)) {
						in_shape_a = SPR_OBJ_OPS(animal)->ObjectHitTest(animal, &pt);

						if (self->aim[HUNT_WEAPON] == WEAPON_SHOTGUN) {
							hit_o = (struct animal_base *) animal;

							if (in_shape_a)
								*killed = 1;

						} else if (in_shape_a) {
							hit_o = (struct animal_base *) animal;

							if (GetGameObj()->party->skills[SKILL_SHARPSHOOTING]) {
								*killed = 1;

							} else if ((bounds_p.right - bounds_p.left) / 2 + bounds_p.left > pt.h) {
								*killed = 1;

							} else {
								*killed = 0;
							}
						}

						break;
					}
				}
			}
		}
	}

	return hit_o;
}

void hunting_on_event(struct evt_handler *handler, struct cmd_event *ev)
{
	struct hunting *self = (struct hunting *) handler;
	struct animal_base *hit_p = NULL;
	struct point pos_b = {0};
	int32_t killed_a = 0;
	int32_t index_l;

	switch (ev->command) {
		case CMD_MOUSE: {
			struct mouse_event *mouse_b = (struct mouse_event *) ev;

			if (mouse_b->kind == MOUSE_DOWN) {
				pos_b = mouse_b->mouse_pos;
				ViewToLocal(view_of(self), &pos_b);

				if (!self->ready_to_fire || !self->aim[HUNT_SHOTS_LEFT]) {
					HuntingPlayCue(self, 2);

				} else {
					HuntingPlayCue(self, 0);
					self->ready_to_fire = false;
					++self->aim[HUNT_SHOTS_FIRED];
					--self->aim[HUNT_SHOTS_LEFT];
					HuntingDispatchEvent2(self, GCMD_HUNT_SHOT, 0, 0);
					HuntingJitterSpawnPos(self, &pos_b);
					hit_p = HuntingFindHitAnimal(self, pos_b, &killed_a);

					if (hit_p) {
						ANIMAL_BASE_OPS(hit_p)->TakeDamage(hit_p,
							g_weapon_damage[self->aim[HUNT_WEAPON]] + 15 * killed_a);
						++self->aim[HUNT_HITS];

						if (!hit_p->alive) {
							self->aim[HUNT_MEAT] = (int16_t) (self->aim[HUNT_MEAT] + hit_p->meat_yield);
						}

					} else if (pos_b.v >= 170) {
						index_l = RandRangeAvg(0, 10, 1);

						if (index_l < 3)
							HuntingPlayCue(self, 1);

						if (self->miss_marker) {
							struct spr_obj *marker = &self->miss_marker->base;

							SPR_OBJ_OPS(marker)->SetAnchor(marker, pos_b.h, pos_b.v);
							self->miss_marker->delay = 60;
							SPR_OBJ_OPS(marker)->Show(marker);
						}
					}

					if (!self->is_stampede) {
						for (index_l = 0; self->animal_count > index_l; index_l++) {
							struct spr_obj *animal = self->animals[index_l];
							struct animal_base *a = (struct animal_base *) animal;

							if (animal->visible) {
								if (a->alive) {
									if (a->direction == 1) {
										ANIMAL_BASE_OPS(a)->SetState(a, ANIM_RUN_LEFT);

									} else {
										ANIMAL_BASE_OPS(a)->SetState(a, ANIM_RUN_RIGHT);
									}

									a->retired = true;
									animal->tag = 0;
								}
							}
						}
					}
				}
			}

			ev->handled = true;
			break;
		}
		case GCMD_HUNT_RELOAD:
			self->ready_to_fire = true;
			ev->handled = true;
			break;
		case GCMD_HUNT_NEW_SCENE:
			self->is_stampede = false;
			HuntingClearAnimals(self);
			HuntingLoadScene(self);
			HuntingSpawnTick(self, (struct node_base *) self->node);
			ev->handled = true;
			ViewNotifyChanged(view_of(self));
			break;
		default:
			break;
	}
}

void HuntingJitterSpawnPos(struct hunting *self, struct point *pos)
{
	int32_t jitter_p = 0;
	int32_t distance_e = (self->blit_rect.bottom - pos->v) / 2;

	if (distance_e >= 30 && distance_e <= 80) {
		jitter_p = RandRangeAvg(0, 8, 2);

	} else if (distance_e > 80) {
		jitter_p = RandRangeAvg(5, 12, 2);
	}

	pos->v = pos->v + jitter_p;
	pos->h = pos->h + RandRangeAvg(-4, 4, 2);
}

void HuntingDispatchEvent2(struct hunting *self, int32_t code, int32_t param1, int32_t param2)
{
	(void) self;

	struct cmd_event *ev = CmdEventCreate(code);
	ev->param1 = param1;
	ev->param2 = param2;
	DispatchGlobalEvent(ev);
}

void HuntingGetAim2(struct hunting *self, struct hunt_result *out)
{
	out->shots_fired = self->aim[HUNT_SHOTS_FIRED];
	out->meat = self->aim[HUNT_MEAT];
}

void HuntingGetState3(struct hunting *self, struct hunt_status *out)
{
	out->shots_left = self->aim[HUNT_SHOTS_LEFT];
	out->weapon = self->aim[HUNT_WEAPON];
	out->meat = self->aim[HUNT_MEAT];
}

void HuntingSetAimXY(struct hunting *self, int32_t x, int32_t y)
{
	self->aim[HUNT_SHOTS_LEFT] = x;
	self->aim[HUNT_WEAPON] = y;
}

void HuntingLoadScene(struct hunting *self)
{
	struct view_surface *surface = self->base.surface;
	char msg_not_packed_p[100];
	char msg_bad_format_e[100];

	NodeDrawTrailsideScenery(self->node, surface->background, &self->scenery_a_id, &self->scenery_b_id,
		&self->scenery_c_id);

	struct canvas *main_canvas_b = surface->main_canvas;
	CANVAS_OPS(main_canvas_b)->StretchBlit(main_canvas_b, surface->background, &self->blit_rect, &self->blit_rect, 0,
		&self->blit_rect);

	if (self->scenery_c_id > 0) {
		self->scenery_c_drawable = ServerCreateVirtual(&g_pict_server->base.base, self->scenery_c_id, true);
		VIEW_SURFACE_OPS(surface)->SetCoverMask(surface, self->scenery_c_drawable, 1);

		if (self->scenery_c_drawable)
			ReleaseDrawable(&self->scenery_c_drawable->base.base, true);

	} else {
		CanvasFill(surface->cover_mask, 0);
	}

	if (!self->shnt_rsrc) {
		self->shnt_rsrc = AssetsGetElement(self->data_lib, "shnt", 150, true);

		if (!self->shnt_rsrc || !RsrcSize(self->shnt_rsrc))
			return;

		switch (RsrcStorage(self->shnt_rsrc)) {
			case 1:
				StrPrintf(msg_not_packed_p, "Scene resource %d not packed", self->scene_rsrc_id);
				LogError(LOG_ERROR, msg_not_packed_p);
				break;
			case 2:
			case 3:
				RsrcLoadAndUnpack(self->shnt_rsrc);
				break;
			default:
				StrPrintf(msg_bad_format_e, "Hunting scene resource %d, invalid format", self->scene_rsrc_id);
				LogError(LOG_ERROR, msg_bad_format_e);
				return;
		}
	}
}

void HuntingSpawnTick(struct hunting *self, struct node_base *cnode)
{
	const int32_t *species_a = NULL;
	int32_t species_count_j = 0;
	int32_t threshold_g = 10;

	if (self->is_stampede) {
		HuntingSpawnBigGame(self, 0, species_a);

	} else {
		struct game *game_obj = GetGameObj();
		int32_t miles_since_c = game_obj->total_miles - g_last_spawn_mile;

		if (miles_since_c < 20)
			threshold_g = 7;

		if (miles_since_c < 5)
			threshold_g = 2;

		if (RandRangeAvg(1, 10, 1) <= threshold_g) {
			species_count_j = cnode->game_species_count;
			species_a = cnode->game_species_ids;

			if (species_count_j > 0) {
				HuntingSpawnSmallGame(self, species_count_j, species_a);
				HuntingSpawnBigGame(self, species_count_j, species_a);
			}
		}
	}
}

void HuntingClearAnimals(struct hunting *self)
{
	for (int32_t index_b = 0; self->animal_count > index_b; ++index_b) {
		if (self->animals[index_b]) {
			SprObjDetach(self->animals[index_b], true);
			self->animals[index_b] = NULL;
		}
	}

	self->animal_count = 0;
}

static int32_t scene_count(const uint8_t *scene)
{
	return ReadLe16(scene + 2);
}

static const uint8_t *scene_entry(const uint8_t *scene, int32_t i)
{
	return scene + 4 + 10 * i;
}

static int32_t entry_kind(const uint8_t *scene, int32_t i)
{
	return (int8_t) scene_entry(scene, i)[0];
}

static int32_t find_scene_entry(const uint8_t *scene, int32_t kind)
{
	int32_t count = scene_count(scene);
	int32_t base = (kind - 1) / 3 * 3 + 1;

	for (int32_t i = 0; i < count; i++) {
		if (entry_kind(scene, i) == kind)
			return i;
	}

	for (int32_t i = 1; i + 1 < count; i++) {
		if (entry_kind(scene, i - 1) == kind - 1 && entry_kind(scene, i + 1) == kind + 1)
			return i;
	}

	for (int32_t i = 0; i < count; i++) {
		if (entry_kind(scene, i) >= base && entry_kind(scene, i) < base + 3)
			return i;
	}

	return 0;
}

static struct rect entry_rect(const uint8_t *scene, int32_t i)
{
	const uint8_t *e = scene_entry(scene, i);

	return RectMake(ReadLe16(e + 4), ReadLe16(e + 6), ReadLe16(e + 2), ReadLe16(e + 8));
}

static void add_animal(struct hunting *self, void *animal)
{
	self->animals[self->animal_count] = animal;
	++self->animal_count;
}

int32_t HuntingSpawnSmallGame(struct hunting *self, int32_t count, const int32_t *species_list)
{
	int32_t water_species_d[30];
	int32_t spawn_count_a = 0;
	int32_t species_a = 0;
	int32_t index_n;
	int32_t water_count_j = 0;
	struct rect area_g;

	if (count) {
		for (index_n = 0; index_n < count; index_n++) {
			if (species_list[index_n] >= SPECIES_FIRST_SMALL_GAME)
				water_species_d[water_count_j++] = species_list[index_n];
		}

		if (water_count_j > 0) {
			index_n = RandRangeAvg(0, water_count_j - 1, 1);
			species_a = water_species_d[index_n];
		}
	}

	const uint8_t *scene_g = RsrcData(self->shnt_rsrc);
	int32_t band_kind_m = species_a / 10;

	if (species_a == SPECIES_SQUIRREL || species_a == SPECIES_PRAIRIE_DOG || species_a == SPECIES_RABBIT) {
		band_kind_m += RandRangeAvg(1, 2, 1);

	} else if (RandRangeAvg(0, 100, 1) > 20) {
		band_kind_m += RandRangeAvg(1, 2, 1);
	}

	if (band_kind_m) {
		index_n = find_scene_entry(scene_g, band_kind_m);
		area_g = entry_rect(scene_g, index_n);
		int32_t direction_f = RandRangeAvg(1, 2, 1);
		struct view_surface *surface = self->base.surface;

		switch (species_a) {
			case SPECIES_RABBIT:
				spawn_count_a = 1;
				add_animal(self, AnimalLonerCreate(self->data_lib, surface, band_kind_m, &area_g, 0));
				break;
			case SPECIES_MALLARD:
			case SPECIES_GOOSE:
				spawn_count_a = RandRangeAvg(0, 7, 2);

				for (index_n = 0; spawn_count_a > index_n; index_n++)
					add_animal(self, AnimalBirdCreate(self->data_lib, surface, band_kind_m, &area_g, direction_f));

				break;
			case SPECIES_PRAIRIE_DOG:
				spawn_count_a = RandRangeAvg(0, 5, 2);

				for (index_n = 0; spawn_count_a > index_n; index_n++) {
					add_animal(self, AnimalBurrowerCreate(self->data_lib, surface, band_kind_m, &area_g,
						direction_f));
				}

				break;
			case SPECIES_SQUIRREL:
				spawn_count_a = 1;
				add_animal(self, AnimalBurrowerCreate(self->data_lib, surface, band_kind_m, &area_g, 0));
				break;
		}
	}

	return spawn_count_a;
}

int32_t HuntingSpawnBigGame(struct hunting *self, int32_t count, const int32_t *species_list)
{
	int32_t land_species_i[30];
	int32_t herd_count_n = 0;
	int32_t wander_dir_o = 0;
	int32_t species_d = 0;
	int32_t index_a;
	int32_t land_count_g = 0;
	int32_t threshold_e = 20;
	struct rect area_j;

	if (self->is_stampede) {
		species_d = SPECIES_BUFFALO;

	} else if (count) {
		for (index_a = 0; index_a < count; index_a++) {
			if (species_list[index_a] < SPECIES_FIRST_SMALL_GAME)
				land_species_i[land_count_g++] = species_list[index_a];
		}

		if (land_count_g > 0) {
			index_a = RandRangeAvg(0, land_count_g - 1, 1);
			species_d = land_species_i[index_a];
		}
	}

	const uint8_t *scene_h = RsrcData(self->shnt_rsrc);
	int32_t band_kind_p = species_d / 10;

	if (GetGameObj()->party->skills[SKILL_TRACKING])
		threshold_e = 80;

	if (RandRangeAvg(0, 100, 2) > threshold_e)
		band_kind_p += RandRangeAvg(1, 2, 1);

	if (band_kind_p) {
		index_a = find_scene_entry(scene_h, band_kind_p);
		area_j = entry_rect(scene_h, index_a);
		int32_t direction_g = RandRangeAvg(1, 2, 1);
		struct view_surface *surface = self->base.surface;

		switch (species_d) {
			case SPECIES_GRIZZLY:
			case SPECIES_BLACK_BEAR: {
				struct date *date_d = GetGameObj()->date;

				if (date_d->month > MONTH_MARCH) {
					if (date_d->month <= MONTH_NOVEMBER) {
						herd_count_n = 1;
						add_animal(self, AnimalLonerCreate(self->data_lib, surface, band_kind_p, &area_j, 0));
					}
				}

				break;
			}
			case SPECIES_MOOSE:
				herd_count_n = 1;
				add_animal(self, AnimalLonerCreate(self->data_lib, surface, band_kind_p, &area_j, 0));
				break;
			case SPECIES_BUFFALO:
			case SPECIES_DEER:
			case SPECIES_ANTELOPE:
			case SPECIES_ELK:
				if (self->is_stampede) {
					herd_count_n = 15;
					wander_dir_o = RandRangeAvg(3, 7, 1);

				} else {
					int32_t max_herd_o = 15 - self->animal_count;

					if (max_herd_o > 5)
						max_herd_o = 5;

					herd_count_n = RandRangeAvg(0, max_herd_o, 2);
				}

				for (index_a = 0; index_a < herd_count_n; index_a++) {
					struct animal_herd *herd = AnimalHerdCreate(self->data_lib, surface, band_kind_p, &area_j,
						direction_g);
					self->animals[self->animal_count] = &herd->base.base.base;

					if (self->is_stampede) {
						struct animal_base *a = &herd->base;

						if (direction_g == 1) {
							ANIMAL_BASE_OPS(a)->SetState(a, ANIM_RUN_LEFT);

						} else {
							ANIMAL_BASE_OPS(a)->SetState(a, ANIM_RUN_RIGHT);
						}

						self->animals[self->animal_count]->tag = wander_dir_o;
					}

					++self->animal_count;
				}

				break;
			case SPECIES_MOUNTAIN_GOAT:
				herd_count_n = 1;
				add_animal(self, AnimalGoatCreate(self->data_lib, surface, band_kind_p, &area_j));
				break;
			case SPECIES_UNUSED_130:
				break;
		}
	}

	return herd_count_n;
}

void HuntingPlayCue(struct hunting *self, int32_t sound_id)
{
	struct settings *device_a = ((struct otii_app *) g_app)->settings;

	if (!SettingsSoundEffectsOn(device_a))
		return;

	struct server *server = (struct server *) g_sound_server;

	switch (sound_id) {
		case 0:
			if (!self->shot_sound) {
				int32_t sound_key_e = 5 * self->aim[HUNT_WEAPON] + 26700;
				self->shot_sound = (struct snd_base *) SERVER_OPS(server)->AcquireObject(server,
					(SND_KIND_IN_MEMORY << 16) + (sound_key_e & 0xFFFF), false);
			}

			if (self->shot_sound)
				SND_OBJ_OPS(self->shot_sound)->Begin(&self->shot_sound->base, false);

			break;
		case 1: {
			int32_t cue_index_i = RandRangeAvg(0, 2, 1);

			if (!self->miss_sounds[cue_index_i]) {
				self->miss_sounds[cue_index_i] = (struct snd_base *) SERVER_OPS(server)->AcquireObject(server,
					(SND_KIND_IN_MEMORY << 16) + ((cue_index_i + 26720) & 0xFFFF), false);
			}

			if (self->miss_sounds[cue_index_i]) {
				struct snd_base *snd = self->miss_sounds[cue_index_i];
				SND_OBJ_OPS(snd)->Begin(&snd->base, false);
			}

			break;
		}
		case 2:
			if (!self->dry_fire_sound) {
				self->dry_fire_sound = (struct snd_base *) SERVER_OPS(server)->AcquireObject(server,
					(SND_KIND_IN_MEMORY << 16) + 26715, false);
			}

			if (self->dry_fire_sound)
				SND_OBJ_OPS(self->dry_fire_sound)->Begin(&self->dry_fire_sound->base, false);

			break;
	}
}
