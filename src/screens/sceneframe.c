#include "sceneframe.h"

#include <stddef.h>
#include <stdio.h>

#include "../game/ailment.h"
#include "../app/app.h"
#include "../data/assets.h"
#include "../ui/button.h"
#include "../base/byteorder.h"
#include "../draw/canvas.h"
#include "../ui/clipview.h"
#include "../dialogs/convdlg.h"
#include "../base/cstr.h"
#include "../ui/cursormgr.h"
#include "../base/date.h"
#include "../draw/drawable.h"
#include "../app/event.h"
#include "../ui/framewin.h"
#include "../game/game.h"
#include "../game/gamecmd.h"
#include "../data/gfxdev.h"
#include "../game/invlist.h"
#include "../game/journal.h"
#include "../base/list.h"
#include "../base/log.h"
#include "mainui.h"
#include "../draw/mediabase.h"
#include "../base/memory.h"
#include "../dialogs/modaldialog.h"
#include "../game/msg.h"
#include "../trail/node.h"
#include "../draw/objbase.h"
#include "../app/otiiapp.h"
#include "../game/party.h"
#include "../draw/pictserver.h"
#include "../dialogs/purchasedlg.h"
#include "../draw/region.h"
#include "../base/rng.h"
#include "../data/rsrc.h"
#include "sceneassets.h"
#include "../draw/server.h"
#include "../draw/shape.h"
#include "../base/str.h"
#include "../data/strrsrc.h"
#include "../ui/textview.h"

int32_t g_scene_prev = 0;
int32_t g_scene_cur = 0;
int32_t g_scene_view = 0;
int32_t g_scene_music = 0;
int32_t g_last_headline_idx = -1;
bool g_msgq_ready = false;

const struct scene_frame_ops SCENE_FRAME_OPS_TABLE = {
	{
		{
			{scene_frame_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			scene_frame_on_event,
			view_on_idle,
			view_on_idle_spare,
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
		view_draw_children,
		view_move_children,
	},
};

static struct view *as_view(struct scene_frame *self)
{
	return &self->base;
}

static struct rect get_bounds(struct scene_frame *self)
{
	return VIEW_OPS(self)->GetBounds(as_view(self));
}

void SceneRecordRead(const uint8_t *data, struct scene_record *out)
{
	out->picture = ReadLe16(data);
	out->exit_left = ReadLe16(data + 2);
	out->exit_right = ReadLe16(data + 4);
	out->exit_forward = ReadLe16(data + 6);
	out->exit_back = ReadLe16(data + 8);
	out->cursor_left = ReadLe16(data + 10);
	out->cursor_right = ReadLe16(data + 12);
	out->cursor_forward = ReadLe16(data + 14);
	out->cursor_back = ReadLe16(data + 16);
	out->music = ReadLe16(data + 18);
	out->store_list = ReadLe16(data + 20);
	out->speaker_pos[0].v = ReadLe16(data + 22);
	out->speaker_pos[0].h = ReadLe16(data + 24);
	out->speaker_pos[1].v = ReadLe16(data + 26);
	out->speaker_pos[1].h = ReadLe16(data + 28);
	out->hotspot_count = ReadLe16(data + 30);
}

void SceneHotspotRead(const uint8_t *data, int32_t index, struct scene_hotspot *out)
{
	const uint8_t *rec = data + (SCENE_HEADER_WORDS + SCENE_HOTSPOT_WORDS * index) * 2;

	out->rect = RectMake(ReadLe16(rec), ReadLe16(rec + 2), ReadLe16(rec + 4), ReadLe16(rec + 6));
	out->kind = ReadLe16(rec + 8);
	out->command = ReadLe32(rec + 10);
	out->argument = ReadLe32(rec + 14);
	out->cursor = ReadLe16(rec + 18);
}

static void offset_by_top_left(struct scene_frame *self, struct rect *r)
{
	struct rect bounds = get_bounds(self);

	r->left += bounds.left;
	r->right += bounds.left;
	r->top += bounds.top;
	r->bottom += bounds.top;
}

static void move_to(struct rect *r, int32_t left, int32_t top)
{
	int32_t width = r->right - r->left;
	int32_t height = r->bottom - r->top;
	r->top = top;
	r->left = left;
	r->right = left + width;
	r->bottom = top + height;
}

static void add_to_child_view(struct scene_frame *self, struct view *child)
{
	VIEW_OPS(self->child_view)->AddChild(self->child_view, child);
}

static void set_cursor(struct clip_button *button, int32_t cursor_id)
{
	struct view *view = (struct view *) button;
	VIEW_OPS(view)->SetCursorId(view, cursor_id);
}

static struct cstr *month_name(const struct date *date)
{
	struct strrsrc *months = StrRsrcLoad(g_text_lib, 50, true);

	if (!months)
		return CStrCreate();

	struct cstr *name = StrRsrcGetCStr(months, date->month - 1);
	StrRsrcDestroy(&months);

	return name;
}

void SceneFrameStartWalk(int32_t scene_id)
{
	g_scene_prev = -1;
	g_scene_cur = (int16_t) scene_id;
	g_scene_view = 0;
}

void SceneFrameInit(struct scene_frame *self, int32_t id, const struct point *pos, int32_t node_id)
{
	ViewInitPoint(&self->base, EVCLS_PICVIEW, id, pos);
	self->base.base.base.ops = &SCENE_FRAME_OPS_TABLE.base.base.base;

	struct view *view = as_view(self);

	if ((int16_t) node_id > 0)
		SceneFrameStartWalk(node_id);
	else
		node_id = g_scene_cur;

	int32_t handler = 9;
	EvtHandlerBindHandler(&view->base, 1, &handler);
	handler = 16;
	EvtHandlerBindHandler(&view->base, 1, &handler);
	handler = 17;
	EvtHandlerBindHandler(&view->base, 1, &handler);

	self->data_lib = g_data_lib;
	self->store_items = NULL;
	self->store_picture_id = 0;
	self->scene_rsrc = SceneFrameBuildSceneView(self, node_id);

	ObjectDelete(view->region_bounds);

	struct rect bounds = view_get_bounds(view);
	view->region_bounds = RegionCreateRect(&bounds, 1, 0, 0);
}

struct scene_frame *SceneFrameCreate(int32_t id, const struct point *pos, int32_t node_id)
{
	struct scene_frame *self = MemAlloc(1, sizeof(struct scene_frame));
	SceneFrameInit(self, id, pos, node_id);

	return self;
}

void SceneFrameDestroy(struct scene_frame **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void scene_frame_destroy(struct object *obj)
{
	struct scene_frame *self = (struct scene_frame *) obj;

	if (self->store_items)
		InvListDestroy(&self->store_items);

	if (self->scene_rsrc)
		RsrcDestroy(&self->scene_rsrc);

	obj->ops = &VIEW_OPS_TABLE.base.base;
	view_destroy(obj);
}

static void build_hotspot(struct scene_frame *self, const uint8_t *scene_data, int32_t rec_index)
{
	struct scene_hotspot hot;
	SceneHotspotRead(scene_data, rec_index, &hot);

	struct rect hot_rect = hot.rect;
	offset_by_top_left(self, &hot_rect);

	struct clip_button *button;

	if (hot.kind == GCMD_PLAY_SFX)
		button = ClipButtonCreate(-2, &GetGameObj()->base.eh_event, &hot_rect, hot.kind, hot.command, hot.argument);
	else
		button = ClipButtonCreate(-2, &as_view(self)->base.eh_event, &hot_rect, hot.kind, hot.command, hot.argument);

	add_to_child_view(self, (struct view *) button);

	if (hot.cursor)
		set_cursor(button, hot.cursor + CURSOR_SCENE_BASE);
}

static void build_newspaper(struct scene_frame *self, const struct scene_record *scene)
{
	char date_buf[52];
	struct rect date_rect = RectMake(53, 185, 66, 406);
	struct rect headline_rect = RectMake(72, 185, 119, 406);
	struct cstr *text;
	int32_t line_day = 0;

	struct strrsrc *headlines = StrRsrcLoad(g_text_lib, 910, true);
	SceneFrameBuildBackdrop(self, scene, scene->picture);
	offset_by_top_left(self, &date_rect);
	offset_by_top_left(self, &headline_rect);

	struct date *date = GetGameObj()->date;
	int32_t day_number = 372 * date->year + 31 * date->month + date->day;
	struct cstr *month = month_name(date);
	StrPrintf(date_buf, "%s %d, %d", CStrGet(month), date->day, date->year);
	CStrDestroy(&month);

	text = CStrCreateFrom(date_buf);
	struct view *text_view = (struct view *) TextViewCreate(-2, &date_rect, text, 3, 1, 1, 0, 0, 0, 0, 255, 0, 1);
	add_to_child_view(self, text_view);

	if (headlines == NULL) {
		text = CStrCreateFrom("Dr. Wayne Studer Accused of Software Design!");

	} else {
		int32_t headline_index = 0;
		StrRsrcRemapHighChars(headlines, 0);
		int32_t line_count = StrRsrcCount(headlines);

		if (line_count) {
			sscanf(StrRsrcGetAt(headlines, 0), "%d", &line_day);
			int32_t start_index = (day_number - line_day) / 7 - 20;

			if (start_index < 1)
				start_index = 1;

			for (int32_t line_index = start_index; StrRsrcCount(headlines) > line_index; line_index++) {
				sscanf(StrRsrcGetAt(headlines, line_index), "%d", &line_day);

				if (day_number < line_day) {
					headline_index = line_index - 1;
					line_index = StrRsrcCount(headlines);
				}
			}

			if (headline_index) {
				text = StrRsrcGetCStr(headlines, headline_index);
				CStrSubstr(text, 8, 999);

			} else {
				text = CStrCreateFrom("Dr. Wayne Studer Accused of Software Design!");
			}

		} else {
			text = CStrCreateFrom("Dr. Wayne Studer Accused of Software Design!");
		}

		if (headline_index != g_last_headline_idx) {
			g_last_headline_idx = headline_index;
			struct cstr *journal_line = CStrCreateFrom("Saw a headline today: ");
			CStrAppendStr(journal_line, text);
			CStrReplaceAll(journal_line, "\r", " ");
			JournalSetDate(&((struct otii_app *) g_app)->journal, GetGameObj()->date);
			JournalAddLine(&((struct otii_app *) g_app)->journal, journal_line);
		}
	}

	text_view = (struct view *) TextViewCreate(-2, &headline_rect, text, 8, 1, 1, 0, 0, 0, 0, 255, 0, 1);
	add_to_child_view(self, text_view);

	struct rect bounds = get_bounds(self);
	struct clip_button *button = ClipButtonCreate(-2, &as_view(self)->base.eh_event, &bounds, GCMD_SCENE_WALK, 0,
		SCENE_EXIT_BACK);
	add_to_child_view(self, (struct view *) button);
	set_cursor(button, CURSOR_EXIT);
	StrRsrcDestroy(&headlines);
}

struct rsrc *SceneFrameBuildSceneView(struct scene_frame *self, int32_t node_id)
{
	struct view *view = as_view(self);
	struct clip_button *button;

	struct rsrc *elem = AssetsGetElement(self->data_lib, "SCENE", (int16_t) node_id, true);

	if (!elem)
		return NULL;

	struct rsrc *rsrc;

	if (RsrcStorage(elem) != RSRC_TEXT) {
		rsrc = elem;

	} else {
		LogError(LOG_ERROR, "Attempt load a raw resource");
		RsrcDestroy(&elem);
		return NULL;
	}

	RsrcLoadAndUnpack(rsrc);
	const uint8_t *scene_data = RsrcData(rsrc);
	struct scene_record scene;
	SceneRecordRead(scene_data, &scene);

	if (scene.store_list == SCENE_STORE_BLACKSMITH)
		GetGameObj()->event_days = 0;

	if (scene.store_list == SCENE_DOCTOR)
		GetGameObj()->party->daily_health_drain -= 100;

	if (view->has_child_list) {
		struct obarray *list = view->child_list;
		ObarrayDestroy(&list);
		view->child_list = NULL;
		view->has_child_list = false;
	}

	if (scene.hotspot_count >= 0)
		SceneFrameBuildBackdrop(self, &scene, scene.picture);

	struct rect left_zone = get_bounds(self);
	struct rect right_zone = get_bounds(self);
	struct rect bottom_zone = get_bounds(self);
	struct rect center_zone = get_bounds(self);
	struct rect bounds = get_bounds(self);
	int32_t eighth_w = (bounds.right - bounds.left) / 8;
	bounds = get_bounds(self);
	int32_t eighth_h = (bounds.bottom - bounds.top) / 8;

	left_zone.right = left_zone.left + eighth_w;
	right_zone.right = right_zone.left + eighth_w;
	bounds = get_bounds(self);
	move_to(&right_zone, bounds.right - eighth_w, right_zone.top);
	bottom_zone.bottom = bottom_zone.top + eighth_h;

	if (scene.exit_left) {
		bounds = get_bounds(self);
		move_to(&bottom_zone, bounds.left + eighth_w, bounds.bottom - eighth_h);

	} else {
		bounds = get_bounds(self);
		move_to(&bottom_zone, bounds.left, bounds.bottom - eighth_h);
	}

	if (scene.exit_right) {
		bounds = get_bounds(self);
		bottom_zone.right = bounds.right - eighth_w;
	}

	bounds = get_bounds(self);
	move_to(&center_zone, bounds.left + eighth_w, bounds.top + eighth_h);
	center_zone.right = bounds.right - eighth_w;
	center_zone.bottom = bounds.bottom - eighth_h;

	if (scene.hotspot_count > 0) {
		for (int32_t rec_index = 0; scene.hotspot_count > rec_index; rec_index++)
			build_hotspot(self, scene_data, rec_index);

	} else if (scene.hotspot_count < 0) {
		switch (scene.hotspot_count) {
			case SCENE_NEWSPAPER:
				build_newspaper(self, &scene);
				break;

			case SCENE_ENCOUNTER_BACKDROP: {
				struct node_base *node = GetGameObj()->cur_node;
				struct encounter_slot *slot = LNodeGetEncounterSlot((struct lnode *) node, g_scene_prev);
				SceneFrameBuildBackdrop(self, &scene, slot->tile);

				bounds = get_bounds(self);
				button = ClipButtonCreate(-2, &view->base.eh_event, &bounds, GCMD_SCENE_WALK, 0, SCENE_EXIT_BACK);
				add_to_child_view(self, (struct view *) button);
				set_cursor(button, CURSOR_EXIT);
				break;
			}
		}
	}

	if (scene.exit_left) {
		button = ClipButtonCreate(-2, &view->base.eh_event, &left_zone, GCMD_SCENE_WALK, SCENE_EDGE_LEFT, 0);
		add_to_child_view(self, (struct view *) button);

		if (scene.cursor_left)
			set_cursor(button, scene.cursor_left + CURSOR_SCENE_BASE);
	}

	if (scene.exit_back) {
		button = ClipButtonCreate(-2, &view->base.eh_event, &bottom_zone, GCMD_SCENE_WALK, SCENE_EDGE_BACK, 0);
		add_to_child_view(self, (struct view *) button);

		if (scene.cursor_back)
			set_cursor(button, scene.cursor_back + CURSOR_SCENE_BASE);
	}

	if (scene.exit_forward) {
		button = ClipButtonCreate(-2, &view->base.eh_event, &center_zone, GCMD_SCENE_WALK, SCENE_EDGE_FORWARD, 0);
		add_to_child_view(self, (struct view *) button);

		if (scene.cursor_forward)
			set_cursor(button, scene.cursor_forward + CURSOR_SCENE_BASE);
	}

	if (scene.exit_right) {
		button = ClipButtonCreate(-2, &view->base.eh_event, &right_zone, GCMD_SCENE_WALK, SCENE_EDGE_RIGHT, 0);
		add_to_child_view(self, (struct view *) button);

		if (scene.cursor_right)
			set_cursor(button, scene.cursor_right + CURSOR_SCENE_BASE);
	}

	if (scene.music > 0) {
		g_scene_music = scene.music;
		OtiiAppPlayMusic((struct otii_app *) g_app, scene.music, true);

	} else if (scene.music < 0) {
		g_scene_music = scene.music;
		OtiiAppPlayPoolMusic((struct otii_app *) g_app, -scene.music);
	}

	if (scene.store_list > 0 && (!self->store_items || self->store_picture_id != scene.picture)) {
		if (self->store_items)
			InvListDestroy(&self->store_items);

		self->store_items = InvListCreateStore(scene.store_list, GetGameObj()->cur_node->price_zone, 0, -1);
		self->store_picture_id = scene.picture;
	}

	return rsrc;
}

void SceneFrameBuildBackdrop(struct scene_frame *self, const struct scene_record *scene, int32_t flag)
{
	struct view *view = as_view(self);
	struct point size = PointMake(0, 0);
	struct rect rect = RectMake(0, 0, 0, 0);
	struct rect src_rect = RectMake(0, 0, 0, 0);
	struct drawable *drawable;

	if (flag > 0) {
		drawable = ServerCreateVirtualPacked(&g_pict_server->base.base, RegionalAssetIdOffset(g_scene_view), flag,
			true);

	} else {
		drawable = ServerCreateVirtual(&g_pict_server->base.base, -flag, true);
	}

	if (!drawable)
		return;

	OBJ_BASE_OPS(drawable)->GetSize(&drawable->base, &size);
	view->rect_b.bottom = size.v + view->rect_b.top;
	view->rect_b.right = size.h + view->rect_b.left;

	struct canvas *src_canvas = OBJ_BASE_OPS(drawable)->GetRect(&drawable->base, &src_rect);
	rect = RectMake(0, 0, size.v, size.h);

	struct canvas *canvas = CanvasCreateRect(&rect, 8, 255);
	CANVAS_OPS(canvas)->StretchBlit(canvas, src_canvas, &src_rect, &rect, 0, &rect);
	ReleaseDrawable(&drawable->base.base, false);
	offset_by_top_left(self, &rect);
	SHAPE_OPS(canvas)->SetRect(&canvas->base, &rect);
	self->child_view = (struct view *) ClipViewCreate(-2, &rect, &canvas->base, false);

	if (scene->speaker_pos[0].h && scene->speaker_pos[0].v) {
		struct game *game = GetGameObj();
		struct lnode *node = (struct lnode *) game->cur_node;
		int32_t *slots = LNodeGetEncounterSlot(node, scene->picture)->conv;

		for (int32_t slot_index = 0; slot_index < 2; ++slot_index) {
			if (slots[slot_index]) {
				drawable = ServerCreateVirtual(&g_pict_server->base.base, ConvGetSpeaker(slots[slot_index]) + 6200,
					true);

				if (!drawable)
					drawable = ServerCreateVirtual(&g_pict_server->base.base, 6201, true);

				src_canvas = DRAWABLE_OPS(drawable)->GetRectAt(drawable, 0, &rect);
				struct rect r3 = rect;
				int32_t offset_h = scene->speaker_pos[slot_index].h - (rect.right - rect.left) / 2;
				int32_t offset_v = scene->speaker_pos[slot_index].v - (rect.bottom - rect.top);
				rect.left += offset_h;
				rect.right += offset_h;
				rect.top += offset_v;
				rect.bottom += offset_v;

				struct point pos = PointMake(rect.left, rect.top);
				CANVAS_OPS(canvas)->BlitImage(canvas, src_canvas, &r3, &pos, &rect, false);
				ReleaseDrawable(&drawable->base.base, false);
				offset_by_top_left(self, &rect);

				struct clip_button *button = ClipButtonCreate(-2, &view->base.eh_event, &rect, GCMD_TALK,
					slots[slot_index], NodeBaseGetSeasonBackdrop(&node->base, game));
				add_to_child_view(self, (struct view *) button);
				set_cursor(button, CURSOR_TALK);
			}
		}
	}

	VIEW_OPS(view)->AddChild(view, self->child_view);
}

int32_t SceneFrameEventCodeToMsgId(struct scene_frame *self, int32_t event_code)
{
	switch (event_code) {
		case AILMENT_ACCIDENTAL_GUNSHOT:
			return MSG_ACCIDENTAL_GUNSHOT;

		case AILMENT_ALKALI_SICKNESS:
			return MSG_ALKALI_SICKNESS;

		case AILMENT_ANIMAL_BITE:
			return MSG_ANIMAL_BITE;

		case AILMENT_ANIMAL_MAULING:
			return MSG_ANIMAL_MAULING;

		case AILMENT_BAD_COLD:
			return MSG_BAD_COLD;

		case AILMENT_BERIBERI:
			return MSG_BERIBERI;

		case AILMENT_BROKEN_ARM:
		case AILMENT_BROKEN_LEG:
		case AILMENT_BROKEN_FOOT:
		case AILMENT_BROKEN_HIP:
		case AILMENT_BROKEN_WRIST:
			return MSG_BROKEN_BONE;

		case AILMENT_BURNS:
			return MSG_BURNS;

		case AILMENT_CHOLERA:
			return MSG_CHOLERA;

		case AILMENT_CONCUSSION:
			return MSG_CONCUSSION;

		case AILMENT_CONSUMPTION:
			return MSG_CONSUMPTION;

		case AILMENT_CUTS_AND_ABRASIONS:
			return MSG_CUTS_AND_ABRASIONS;

		case AILMENT_DIPHTHERIA:
			return MSG_DIPHTHERIA;

		case AILMENT_DROWNING:
			return MSG_DROWNING;

		case AILMENT_DYSENTERY:
			return MSG_DYSENTERY;

		case AILMENT_FOOD_POISONING:
			return MSG_FOOD_POISONING;

		case AILMENT_FREEZING:
			return MSG_FREEZING;

		case AILMENT_FROSTBITE:
			return MSG_FROSTBITE;

		case AILMENT_GANGRENE:
			return MSG_GANGRENE;

		case AILMENT_INFECTION:
			return MSG_INFECTION;

		case AILMENT_INTERNAL_INJURIES:
			return MSG_INTERNAL_INJURIES;

		case AILMENT_LOCKJAW:
			return MSG_LOCKJAW;

		case AILMENT_MALARIA:
			return MSG_MALARIA;

		case AILMENT_MEASLES:
			return MSG_MEASLES;

		case AILMENT_MOUNTAIN_FEVER:
			return MSG_MOUNTAIN_FEVER;

		case AILMENT_PNEUMONIA:
			return MSG_PNEUMONIA;

		case AILMENT_RABIES:
			return MSG_RABIES;

		case AILMENT_SCARLET_FEVER:
			return MSG_SCARLET_FEVER;

		case AILMENT_SCURVY:
			return MSG_SCURVY;

		case AILMENT_SMALLPOX:
			return MSG_SMALLPOX;

		case AILMENT_SNAKEBITE:
			return MSG_SNAKEBITE;

		case AILMENT_SPRAINED_WRIST:
		case AILMENT_SPRAINED_ELBOW:
		case AILMENT_SPRAINED_KNEE:
		case AILMENT_SPRAINED_ANKLE:
		case AILMENT_SPRAINED_SHOULDER:
			return MSG_SPRAIN;

		case AILMENT_STARVATION:
			return MSG_STARVATION;

		case AILMENT_THE_GRIPPE:
			return MSG_THE_GRIPPE;

		case AILMENT_THIRST:
			return MSG_THIRST;

		case AILMENT_TYPHOID_FEVER:
			return MSG_TYPHOID_FEVER;

		case AILMENT_TYPHUS:
			return MSG_TYPHUS;

		case AILMENT_WATER_POISONING:
			return MSG_WATER_POISONING;

		case AILMENT_WOUND:
			return MSG_WOUND;
	}

	return MSG_UNKNOWN_AILMENT;
}


void SceneFrameBuySupplyPackage(struct scene_frame *self)
{
	char buf[32];
	struct inv_list *packages[6];
	struct cstr *journal_line;

	int32_t lo_index = 4;
	int32_t hi_index = 6;
	double budget = GetGameObj()->party->cash - 60.0;

	struct strrsrc *labels = StrRsrcCreate();
	struct strrsrc *prices = StrRsrcCreate();
	StrRsrcAppend(labels, "");
	StrRsrcAppend(prices, "");

	for (int32_t index = 0; index < 6; ++index)
		packages[index] = NULL;

	for (int32_t index = 0; index < 6; ++index) {
		packages[index] = InvListCreateStore(index + 100, 1, 0, -1);

		if ((double) InvListTotalValue(packages[index]) / 100.0 > budget) {
			lo_index = index - 2;
			hi_index = index;
			break;
		}
	}

	if (lo_index < 0)
		lo_index = 0;

	for (int32_t index = lo_index; index < hi_index; ++index) {
		StrPrintf(buf, "%d month%c", index + 1, index == 0 ? 32 : 115);
		StrRsrcAppend(labels, buf);
		StrPrintf(buf, "$%5.2f", (double) InvListTotalValue(packages[index]) / 100.0);
		StrRsrcAppend(prices, buf);
	}

	for (int32_t index = hi_index - lo_index; lo_index + 10 > index; ++index) {
		StrRsrcAppend(labels, "-");
		StrRsrcAppend(prices, "-");
	}

	struct conv_dlg *conv = ConvDlgCreate(RandRangeAvg(0, 1, 1) + 898, 0, 190);
	ConvDlgBuildTradeList(conv, labels, prices);
	JournalSetDate(&((struct otii_app *) g_app)->journal, GetGameObj()->date);
	int32_t index = (int16_t) ModalDialogRun(&conv->base, 0, &g_parent_win->base);

	if (index != -2) {
		journal_line = CStrCreateFrom("Took advantage of the package deal offered to me. What a relief not to have to "
			"shop for all that individually!");
		int32_t choice = lo_index + index - 2;
		double cost = (double) InvListTotalValue(packages[choice]) / 100.0;
		PartyDistributeLoot(GetGameObj()->party, packages[choice]);

		struct party *party = GetGameObj()->party;
		party->cash = party->cash + -cost;

	} else {
		journal_line = CStrCreateFrom("Decided not to buy a package deal. I can do better on my own!");
	}

	JournalAddLine(&((struct otii_app *) g_app)->journal, journal_line);
	ObjectDelete(conv);

	for (int32_t i = 0; i < 6; ++i) {
		if (packages[i])
			InvListDestroy(&packages[i]);
	}

	StrRsrcDestroy(&prices);
	StrRsrcDestroy(&labels);
}


void scene_frame_on_event(struct evt_handler *handler, struct cmd_event *evt)
{
	struct scene_frame *self = (struct scene_frame *) handler;

	int32_t next_scene = 0;
	int32_t sfx_id = 0;

	if (evt->command == GCMD_TALK) {
		int32_t conv_id = (int32_t) evt->param1;
		int32_t dir_value = (int16_t) evt->param2;
		evt->handled = true;

		if (dir_value == -1) {
			struct game *game = GetGameObj();
			dir_value = (int16_t) NodeBaseGetSeasonBackdrop(game->cur_node, game);
		}

		struct conv_dlg *conv = ConvDlgCreate(conv_id, 0, dir_value + 5100);
		int32_t conv_result = ModalDialogRun(&conv->base, 0, &g_parent_win->base);
		int32_t rsrc_id = ConvDlgGetRsrcId(conv);
		ObjectDelete(conv);

		if (conv_result > 0) {
			struct cmd_event *conv_event = CmdEventCreate(GCMD_OPEN_TRADE_DLG);
			conv_event->param1 = rsrc_id | (conv_result << 16);
			conv_event->param2 = dir_value + 5100;
			DispatchGlobalEvent(conv_event);
		}
	}

	if (evt->command == GCMD_BUY_ITEM && self->scene_rsrc) {
		struct party *party = GetGameObj()->party;
		double cash = party->cash;
		int32_t category = (int16_t) evt->param1;
		struct purchase_dlg *dlg = PurchaseDlgCreate(self->store_items, category, cash);
		ModalDialogRun(&dlg->base, (intptr_t) party, &g_parent_win->base);
		ObjectDelete(dlg);
		evt->handled = true;
	}

	if (evt->command == GCMD_SCENE_WALK) {
		evt->handled = true;
		next_scene = (int16_t) evt->param2;

		if (next_scene != 0) {
			sfx_id = (int16_t) evt->param1;

			if (sfx_id != 0) {
				if (sfx_id != next_scene)
					OtiiAppPlaySfx((struct otii_app *) g_app, sfx_id, false);
			}

		} else if (self->scene_rsrc) {
			struct scene_record scene;
			SceneRecordRead(RsrcData(self->scene_rsrc), &scene);

			switch (evt->param1) {
				case SCENE_EDGE_LEFT:
					next_scene = scene.exit_left;

					if (next_scene > 0) {
						if (--g_scene_view < 0)
							g_scene_view = 3;
					}

					break;

				case SCENE_EDGE_RIGHT:
					next_scene = scene.exit_right;

					if (next_scene > 0) {
						if (++g_scene_view > 3)
							g_scene_view = 0;
					}

					break;

				case SCENE_EDGE_FORWARD:
					next_scene = scene.exit_forward;
					break;

				case SCENE_EDGE_BACK:
					next_scene = scene.exit_back;

					if (next_scene > 0) {
						switch (g_scene_view) {
							case 0:
								g_scene_view = 2;
								break;

							case 2:
								g_scene_view = 0;
								break;

							case 1:
								g_scene_view = 3;
								break;

							case 3:
								g_scene_view = 1;
								break;
						}
					}

					break;
			}
		}

		if (next_scene != 0) {
			if (!g_msgq_ready && GetGameObj()->train_wagons == -1) {
				g_msgq_ready = true;
				SceneFrameBuySupplyPackage(self);
			}

			if (next_scene == SCENE_EXIT_BACK)
				next_scene = g_scene_prev;

			if (next_scene > 0) {
				struct rsrc *old_view = self->scene_rsrc;
				g_scene_prev = g_scene_cur;
				g_scene_cur = next_scene;
				self->scene_rsrc = SceneFrameBuildSceneView(self, next_scene);

				if (self->scene_rsrc)
					RsrcDestroy(&old_view);
				else
					self->scene_rsrc = old_view;

				ViewNotifyChanged(as_view(self));

			} else {
				if (((struct otii_app *) g_app)->ui)
					MainUiShowTravelPanel(((struct otii_app *) g_app)->ui);

				DispatchGlobalEvent(CmdEventCreate(GCMD_NODE_CONTINUE));
			}
		}
	}
}
