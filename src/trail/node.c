#include "node.h"

#include <stddef.h>
#include <string.h>

#include "../data/assets.h"
#include "../base/byteorder.h"
#include "../draw/canvas.h"
#include "../dialogs/convdlg.h"
#include "../base/date.h"
#include "../draw/drawable.h"
#include "../game/game.h"
#include "../data/gfxdev.h"
#include "../game/invitem.h"
#include "../game/invlist.h"
#include "../base/log.h"
#include "../draw/mediabase.h"
#include "../base/memory.h"
#include "../game/msg.h"
#include "../game/party.h"
#include "../draw/pictserver.h"
#include "../base/rng.h"
#include "../data/rsrc.h"
#include "../screens/sceneassets.h"
#include "../draw/server.h"
#include "../game/weather.h"

int32_t g_conv_block_count_a = 15;
int32_t g_conv_block_a[15] = {5, 197, 386, 460, 471, 537, 783, 803, 805, 813, 26, 334, 764, 777, 787};
int32_t g_conv_block_count_b = 2;
int32_t g_conv_block_b[2] = {290, 1090};
int32_t g_conv_block_count_c = 45;
int32_t g_conv_block_c[45] = {15, 17, 19, 25, 69, 75, 76, 87, 88, 129, 130, 180, 192, 236, 237, 241, 242,
	269, 270, 287, 291, 294, 306, 317, 318, 328, 335, 354, 376, 377, 384, 393, 419, 499, 563, 566, 569,
	608, 802, 813, 814, 815, 1086, 1087, 1088};

static int32_t read_be16(const uint8_t **data)
{
	int32_t value = ReadBe16(*data);
	*data += 2;

	return value;
}

static int32_t read_be32(const uint8_t **data)
{
	int32_t value = ReadBe32(*data);
	*data += 4;

	return value;
}

void NodeBaseInit(struct node_base *self, int32_t node_id)
{
	self->has_scenery_switch = false;
	self->scenery_switch_on = false;
	self->npcs_selected = false;
	self->rand_seed = RandRangeAvg(0, CMD_MOUSE, 1);
	self->node_kind = NODE_KIND_NONE;
	self->node_id = node_id;
	self->map_id = 0;
	self->unread1 = 0;
	self->link_count = 0;

	for (int32_t slot_index = 0; slot_index < 8; slot_index++)
		self->links[slot_index] = 0;

	self->start_month = self->start_day = self->start_year = 0;
	self->end_month = self->end_day = self->end_year = 0;
	self->weather_region = 0;
	self->conv_count = 0;
	memset(self->conv_table, 0, sizeof(self->conv_table));
}

void NodeBaseDestroy(struct node_base **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct node_base *ctx = *ctx_mod;
	*ctx_mod = NULL;

	MemFree(ctx);
}

int32_t NodeBaseGetTravelMusic(struct node_base *self, struct game *game)
{
	return self->music_by_dest[game->destination];
}

void NodeBaseSelectConvNpcs(struct node_base *self)
{
	bool keep;
	int32_t portraits[70];
	int32_t conv_ids[70];
	int32_t pick[6];

	if (self->npcs_selected)
		return;

	struct game *game = GetGameObj();
	struct weather *climate = game->weather;
	self->npcs_selected = true;

	for (int32_t i = 0; i < 70; ++i) {
		portraits[i] = 0;
		conv_ids[i] = 0;
	}

	int32_t conv_count = self->conv_count;
	int32_t kept_count = 0;

	for (int32_t i = 0; conv_count > i; ++i) {
		conv_ids[kept_count] = self->conv_table[i];
		int32_t conv_id = conv_ids[kept_count];
		int32_t check_id = conv_id;

		if (check_id > 100000)
			check_id /= 100;

		keep = true;

		if (self->node_kind != NODE_KIND_TRAIL) {
			if (keep) {
				if (!self->swiftness) {
					for (int32_t j = 0; g_conv_block_count_c > j; ++j) {
						if (g_conv_block_c[j] == check_id) {
							keep = false;
							j = g_conv_block_count_c;
						}
					}
				}
			}
		}

		if (keep) {
			if (climate->snow_depth) {
				for (int32_t j = 0; j < g_conv_block_count_a; ++j) {
					if (g_conv_block_a[j] == check_id) {
						keep = false;
						j = g_conv_block_count_a;
					}
				}
			}
		}

		if (keep) {
			if (!climate->snow_depth) {
				for (int32_t j = 0; j < g_conv_block_count_b; ++j) {
					if (g_conv_block_b[j] == check_id) {
						keep = false;
						j = g_conv_block_count_b;
					}
				}
			}
		}

		if (keep)
			portraits[kept_count++] = ConvGetPortraitId(conv_id);
	}

	conv_count = kept_count;

	for (int32_t i = 0; conv_count > i; ++i) {
		for (int32_t j = 0; conv_count > j; ++j) {
			if (i != j && portraits[i] == portraits[j]) {
				--conv_count;

				if (RandRangeAvg(0, 1, 1)) {
					portraits[i] = portraits[conv_count];
					conv_ids[i--] = conv_ids[conv_count];
					j = conv_count;

				} else {
					portraits[j] = portraits[conv_count];
					conv_ids[j] = conv_ids[conv_count];
				}
			}
		}
	}

	kept_count = RandRangeAvg(2, 5, 1);

	if (kept_count > conv_count)
		kept_count = conv_count;

	for (int32_t i = 0; kept_count > i; ++i) {
		int32_t pick_index;

		do {
			keep = true;
			pick_index = RandRangeAvg(0, conv_count - 1, 1);

			for (int32_t j = 0; j < i; ++j) {
				if (pick[j] == pick_index) {
					keep = false;
					j = i;
				}
			}
		} while (!keep);

		pick[i] = pick_index;
	}

	self->conv_count = kept_count;

	for (int32_t i = 0; kept_count > i; ++i)
		self->conv_table[i] = conv_ids[pick[i]];
}

enum trkey_id {
	TRKEY_SUMMER = 100,
	TRKEY_AUTUMN_WINTER = 200,
	TRKEY_SNOW = 300,
};

struct trkey_range {
	int32_t first;
	int32_t count;
};

struct trkey_segment {
	struct trkey_range b;
	struct trkey_range a;
	struct trkey_range c2;
	struct trkey_range c1;
};

static void trkey_segment_read(const uint8_t *trkey, int32_t segment, struct trkey_segment *out)
{
	const uint8_t *rec = trkey + 6 + segment * 16;

	out->b.first = ReadLe16(rec);
	out->b.count = ReadLe16(rec + 2);
	out->a.first = ReadLe16(rec + 4);
	out->a.count = ReadLe16(rec + 6);
	out->c2.first = ReadLe16(rec + 8);
	out->c2.count = ReadLe16(rec + 10);
	out->c1.first = ReadLe16(rec + 12);
	out->c1.count = ReadLe16(rec + 14);
}

void NodeDrawTrailsideScenery(struct node *self, struct canvas *dc, int32_t *a, int32_t *b, int32_t *c)
{
	int32_t trkey_id = 0;
	int32_t scenery_a_id;
	int32_t scenery_b_id;
	int32_t scenery_c_id;
	int32_t range_max;
	int32_t use_c1;

	struct game *game = GetGameObj();

	if (!game)
		return;

	switch (GameGetSeason(game)) {
		case SEASON_SUMMER:
			trkey_id = TRKEY_SUMMER;
			break;

		case SEASON_AUTUMN:
		case SEASON_WINTER:
			trkey_id = TRKEY_AUTUMN_WINTER;
			break;

		case SEASON_SNOW:
			trkey_id = TRKEY_SNOW;
			break;
	}

	struct rsrc *rsrc = AssetsGetElement(g_data_lib, "TrKey", trkey_id, true);

	if (!rsrc)
		return;

	if (RsrcStorage(rsrc) == RSRC_TEXT)
		LogError(LOG_ERROR, "Attempt load a raw resource");

	RsrcLoadAndUnpack(rsrc);
	const uint8_t *trkey = RsrcData(rsrc);

	int32_t base_b = ReadLe16(trkey);
	int32_t base_a = ReadLe16(trkey + 2);
	int32_t base_c = ReadLe16(trkey + 4);

	int32_t segment = self->base.scenery_segment;

	if (segment < 1 || segment > 79)
		segment = 1;

	--segment;

	struct trkey_segment rec;
	trkey_segment_read(trkey, segment, &rec);

	if (rec.a.first >= 0) {
		do {
			range_max = rec.a.count - 1;
			scenery_a_id = rec.a.first + RandRangeAvg(0, range_max, 1) + base_a;
		} while (*a == scenery_a_id);

	} else {
		scenery_a_id = 0;
	}

	if (rec.b.first >= 0) {
		do {
			range_max = rec.b.count - 1;
			scenery_b_id = rec.b.first + RandRangeAvg(0, range_max, 1) + base_b;
		} while (*b == scenery_b_id);

	} else {
		scenery_b_id = 0;
	}

	do {
		range_max = rec.c1.first;

		if (range_max == -1)
			use_c1 = 0;
		else
			use_c1 = RandRangeAvg(0, 1, 1);

		if (use_c1) {
			if (rec.c1.first >= 0) {
				range_max = rec.c1.count - 1;
				scenery_c_id = rec.c1.first + RandRangeAvg(0, range_max, 1) + base_c;

			} else {
				scenery_c_id = 0;
			}

		} else if (rec.c2.first >= 0) {
			range_max = rec.c2.count - 1;
			scenery_c_id = rec.c2.first + RandRangeAvg(0, range_max, 1) + base_c;

		} else {
			scenery_c_id = 0;
		}
	} while (*c == scenery_c_id);

	RsrcDestroy(&rsrc);

	struct rect bounds = RectMake(0, 0, 0, 0);
	struct point pos;

	struct drawable *drawable = ServerCreateVirtual(&g_pict_server->base.base,
		RegionalAssetIdOffset(RandRangeAvg(0, 3, 1)), true);

	if (!drawable)
		return;

	struct canvas *canvas = DRAWABLE_OPS(drawable)->GetRectAt(drawable, 0, &bounds);
	CANVAS_OPS(dc)->StretchBlit(dc, canvas, &bounds, &bounds, 0, &bounds);
	ReleaseDrawable(&drawable->base.base, false);

	if (scenery_a_id) {
		drawable = ServerCreateVirtual(&g_pict_server->base.base, scenery_a_id, true);

		if (!drawable)
			return;

		canvas = DRAWABLE_OPS(drawable)->GetRectAt(drawable, 0, &bounds);
		pos = PointMake(bounds.left, bounds.top);
		CANVAS_OPS(dc)->BlitImage(dc, canvas, &bounds, &pos, &bounds, false);
		ReleaseDrawable(&drawable->base.base, false);
	}

	if (scenery_b_id) {
		drawable = ServerCreateVirtual(&g_pict_server->base.base, scenery_b_id, true);

		if (!drawable)
			return;

		canvas = DRAWABLE_OPS(drawable)->GetRectAt(drawable, 0, &bounds);
		pos = PointMake(bounds.left, bounds.top);
		CANVAS_OPS(dc)->BlitImage(dc, canvas, &bounds, &pos, &bounds, false);
		ReleaseDrawable(&drawable->base.base, false);
	}

	if (scenery_c_id) {
		drawable = ServerCreateVirtual(&g_pict_server->base.base, scenery_c_id, true);

		if (!drawable)
			return;

		canvas = DRAWABLE_OPS(drawable)->GetRectAt(drawable, 0, &bounds);
		pos = PointMake(bounds.left, bounds.top);
		CANVAS_OPS(dc)->BlitImage(dc, canvas, &bounds, &pos, &bounds, false);
		ReleaseDrawable(&drawable->base.base, false);
	}

	if (!scenery_a_id)
		scenery_a_id = -1;

	if (!scenery_c_id)
		scenery_c_id = -1;

	if (!scenery_b_id)
		scenery_b_id = -1;

	*a = scenery_a_id;
	*b = scenery_b_id;
	*c = scenery_c_id;
}

void NodeBaseReadRecord(struct node_base *self, const uint8_t **data)
{
	int32_t value;

	self->node_kind = read_be16(data);
	self->node_id = read_be16(data);
	self->cmd_set_id = read_be16(data);
	self->sfx_id = read_be16(data);
	self->sfx_id_snow = read_be16(data);
	self->event_count = read_be16(data);

	for (int32_t index = 0; self->event_count > index; ++index) {
		value = read_be16(data);

		if (index < 20)
			self->events[index] = value;

		if (value == NODEFEAT_SCENERY_SWITCH_ON) {
			self->has_scenery_switch = true;
			self->scenery_switch_on = true;
		}

		if (value == NODEFEAT_SCENERY_SWITCH_OFF) {
			self->has_scenery_switch = true;
			self->scenery_switch_on = false;
		}
	}

	if (self->event_count > 20)
		self->event_count = 20;

	for (int32_t index = 0; index < 4; ++index)
		self->music_by_dest[index] = read_be16(data);

	for (int32_t index = 0; index < 4; ++index)
		self->scene_pic_by_season[index] = read_be16(data);

	self->map_id = read_be16(data);
	self->price_zone = read_be16(data);
	self->start_month = read_be16(data);
	self->start_day = read_be16(data);
	self->start_year = read_be16(data);
	self->end_month = read_be16(data);
	self->end_day = read_be16(data);
	self->end_year = read_be16(data);
	self->unread1 = read_be16(data);
	self->weather_region = read_be16(data);
	self->unread2 = read_be16(data);
	self->game_species_count = read_be16(data);

	for (int32_t index = 0; self->game_species_count > index; ++index) {
		value = read_be16(data);

		if (index < 50)
			self->game_species_ids[index] = value;
	}

	if (self->game_species_count > 50)
		self->game_species_count = 50;

	self->scenery_segment = read_be16(data);

	for (int32_t index = 0; index < 6; ++index)
		self->season_backdrop_ids[index] = read_be16(data);

	self->link_count = read_be16(data);

	for (int32_t index = 0; self->link_count > index; ++index) {
		value = read_be16(data);

		if (index < 8)
			self->links[index] = value;
	}

	if (self->link_count > 8)
		self->link_count = 8;

	self->conv_count = read_be16(data);

	for (int32_t index = 0; self->conv_count > index; ++index) {
		int32_t conv_value = read_be32(data);

		if (index < 70)
			self->conv_table[index] = conv_value;
	}

	if (self->conv_count > 70)
		self->conv_count = 70;

	self->inset_count = read_be16(data);

	for (int32_t index = 0; self->inset_count > index; ++index) {
		value = read_be16(data);

		if (index < 40)
			self->inset_ids[index] = value;
	}

	if (self->inset_count > 40)
		self->inset_count = 40;

	self->summer_inset_count = read_be16(data);

	for (int32_t index = 0; self->summer_inset_count > index; ++index) {
		value = read_be16(data);

		if (index < 20)
			self->summer_inset_ids[index] = value;
	}

	if (self->summer_inset_count > 20)
		self->summer_inset_count = 20;

	self->snow_inset_count = read_be16(data);

	for (int32_t index = 0; self->snow_inset_count > index; ++index) {
		value = read_be16(data);

		if (index < 20)
			self->snow_inset_ids[index] = value;
	}

	if (self->snow_inset_count > 20)
		self->snow_inset_count = 20;
}

bool NodeBaseActiveOnDate(struct node_base *self, const struct date *date)
{
	if (self->end_year < date->year)
		return false;

	if (self->start_year > date->year)
		return false;

	if (self->start_year == date->year) {
		if (self->start_month > date->month)
			return false;

		if (self->start_month == date->month) {
			if (self->start_day > date->day)
				return false;
		}
	}

	if (self->end_year == date->year) {
		if (self->end_month < date->month)
			return false;

		if (self->end_month == date->month) {
			if (self->end_day < date->day)
				return false;
		}
	}

	return true;
}

int32_t NodeBaseGetId(struct node_base *self)
{
	return self->node_kind;
}

int32_t NodeBaseGetArrivalSound(struct node_base *self, struct game *game)
{
	if (game && GameGetSeason(game) == SEASON_SNOW)
		return self->sfx_id_snow;

	return self->sfx_id;
}

int32_t LNodeGetLandmarkConv(struct lnode *self, struct game *game)
{
	if (game && GameGetSeason(game) == SEASON_SNOW && self->landmark_conv < 10000)
		return self->landmark_conv + 3000;

	return self->landmark_conv;
}

int32_t NodeBaseGetSeasonBackdrop(struct node_base *self, struct game *game)
{
	if (!game)
		return self->season_backdrop_ids[0];
	else
		return self->season_backdrop_ids[GameGetSeason(game)];
}

int32_t NodeBaseListActiveLinks(struct node_base *self, const struct date *date, int32_t node_id, int32_t *out)
{
	struct node_base *node;
	int32_t count = 0;
	int32_t expired_id = 0;
	int32_t expired_end = -1;

	for (int32_t seg_index = 0; seg_index < self->link_count; ++seg_index) {
		if (self->links[seg_index] != node_id) {
			if (self->node_kind == NODE_KIND_TRAIL)
				node = &LNodeCreate(self->links[seg_index], false)->base;
			else
				node = &NodeCreate(self->links[seg_index], true)->base;

			if (node) {
				if (node->node_kind == NODE_KIND_NONE) {
				} else if (NodeBaseActiveOnDate(node, date)) {
					out[count++] = self->links[seg_index];

				} else {
					int32_t end = 372 * node->end_year + 31 * node->end_month + node->end_day;

					if (end > expired_end) {
						expired_end = end;
						expired_id = self->links[seg_index];
					}
				}

				NodeBaseDestroy(&node);
			}
		}
	}

	if (!count && expired_id)
		out[count++] = expired_id;

	return count;
}

int32_t NodeInsetId(struct node *self, struct game *game)
{
	int32_t image_id = 0;
	int32_t season;

	if (game->weather->snow_depth) {
		season = 0;

	} else {
		struct date *date = game->date;

		if (date->month > MONTH_MARCH && date->month <= MONTH_NOVEMBER)
			season = RandRangeAvg(1, 2, 1);
		else
			season = 2;
	}

	switch (season) {
		case 0:
			if (self->base.snow_inset_count)
				image_id = self->base.snow_inset_ids[RandRangeAvg(0, self->base.snow_inset_count - 1, 1)];
			break;

		case 1:
			if (self->base.summer_inset_count)
				image_id = self->base.summer_inset_ids[RandRangeAvg(0, self->base.summer_inset_count - 1, 1)];
			break;

		case 2:
			if (self->base.inset_count)
				image_id = self->base.inset_ids[RandRangeAvg(0, self->base.inset_count - 1, 1)];
			break;
	}

	if (image_id < 0 || image_id > 140)
		LogError(LOG_ERROR, "Unexpected image id");

	return image_id + 10000;
}

struct river_price {
	int32_t base;
	int32_t per_wagon;
	int32_t per_animal;
	int32_t per_person;
};

struct river_record {
	int32_t swiftness;
	int32_t width_high;
	int32_t width;
	int32_t width_low;
	int32_t depth_high;
	int32_t depth;
	int32_t depth_low;
	int32_t crossing_pics_a[CROSSING_PIC_COUNT];
	int32_t crossing_pics_b[CROSSING_PIC_COUNT];
	int32_t salt_lake_ferry_free;
	int32_t ferry_first_month;
	int32_t ferry_last_month;
	int32_t guide_first_month;
	int32_t guide_last_month;
	int32_t guide_conv;
	struct river_price ferry_price;
	struct river_price toll_bridge_price;
};

static int32_t calc_crossing_price(struct river_price p)
{
	struct game *game = GetGameObj();
	struct party *party = game->party;

	if (!party)
		return p.base;

	struct inv_list *animals = &party->animals;
	struct inv_item *animal = InvItemFromNode(animals->base.head);
	int32_t result = p.base;
	int32_t animal_count = 0;

	while (animal) {
		animal_count += InvItemGetCount(animal, false);
		animal = InvItemFromNode(animal->node.next);
	}

	result += party->wagons.base.count * p.per_wagon;
	result += p.per_animal * animal_count;
	result += party->spare_animals.base.count * p.per_animal;
	result += PartyMemberCount(party, true) * p.per_person;

	return (int16_t) result;
}

static void river_price_read(const uint8_t *data, struct river_price *out)
{
	out->base = ReadLe16(data);
	out->per_wagon = ReadLe16(data + 2);
	out->per_animal = ReadLe16(data + 4);
	out->per_person = ReadLe16(data + 6);
}

static void river_record_read(const uint8_t *data, struct river_record *out)
{
	out->swiftness = ReadLe16(data);
	out->width_high = ReadLe16(data + 2);
	out->width = ReadLe16(data + 4);
	out->width_low = ReadLe16(data + 6);
	out->depth_high = ReadLe16(data + 8);
	out->depth = ReadLe16(data + 10);
	out->depth_low = ReadLe16(data + 12);

	for (int32_t i = 0; i < CROSSING_PIC_COUNT; i++) {
		out->crossing_pics_a[i] = ReadLe16(data + 14 + 2 * i);
		out->crossing_pics_b[i] = ReadLe16(data + 32 + 2 * i);
	}

	out->salt_lake_ferry_free = ReadLe16(data + 50);
	out->ferry_first_month = ReadLe16(data + 52);
	out->ferry_last_month = ReadLe16(data + 54);
	out->guide_first_month = ReadLe16(data + 56);
	out->guide_last_month = ReadLe16(data + 58);
	out->guide_conv = ReadLe16(data + 60);
	river_price_read(data + 62, &out->ferry_price);
	river_price_read(data + 70, &out->toll_bridge_price);
}

void LNodeInit(struct lnode *self, int32_t node_id, bool apply)
{
	NodeBaseInit(&self->base, node_id);

	int32_t portraits[70];
	int32_t conv_ids[70];
	int32_t index_j;
	int32_t pick_index;
	int32_t depth_from = 0;
	int32_t width_from = 0;
	int32_t depth_to = 0;
	int32_t width_to = 0;

	self->ferry_wait_days = 0;
	self->arrival_wait_days = self->ferry_wait_days;
	self->ferry_available = false;
	self->ferry_price = 0;
	self->toll_bridge_price = 0;
	self->guide_conv = 0;
	memset(self->crossing_pics_a, 0, sizeof(self->crossing_pics_a));
	memset(self->crossing_pics_b, 0, sizeof(self->crossing_pics_b));

	struct assets *lib = g_data_lib;

	if (!lib)
		return;

	struct rsrc *rsrc = AssetsGetElement(lib, "LNode", node_id, false);

	if (!rsrc)
		return;

	const uint8_t *cursor = RsrcData(rsrc);

	for (int32_t i = 0; i < 5; i++)
		self->route_by_dest[i] = read_be16(&cursor);

	self->landmark_conv = read_be16(&cursor);
	NodeBaseReadRecord(&self->base, &cursor);
	RsrcDestroy(&rsrc);

	if (!apply)
		return;

	struct weather *climate = GetGameObj()->weather;

	for (int32_t i = 0; i < 8; i++) {
		self->encounter[i].tile = RandRangeAvg(8450, 8461, 1);
		self->encounter[i].pct = RandRangeAvg(0, 99, 1);

		for (index_j = 0; index_j < 2; index_j++)
			self->encounter[i].conv[index_j] = 0;
	}

	if (self->landmark_conv && self->landmark_conv < 9999) {
		bool is_fixed_landmark = false;

		if (self->landmark_conv == 460 || self->landmark_conv == 510 || self->landmark_conv == 590 ||
			self->landmark_conv == 800 || self->landmark_conv == 801)
			is_fixed_landmark = true;

		int32_t conv_count = self->base.conv_count;

		for (int32_t i = 0; i < 70; i++) {
			portraits[i] = 0;
			conv_ids[i] = 0;
		}

		int32_t kept_count = 0;

		for (int32_t i = 0; conv_count > i; i++) {
			conv_ids[kept_count] = self->base.conv_table[i];
			int32_t conv_id = conv_ids[kept_count];
			bool keep;

			if (conv_ids[kept_count] < 9999)
				keep = true;
			else
				keep = false;

			if (keep) {
				if (climate->snow_depth) {
					for (index_j = 0; index_j < g_conv_block_count_a; index_j++) {
						if (g_conv_block_a[index_j] == conv_id) {
							keep = false;
							index_j = g_conv_block_count_a;
						}
					}
				}
			}

			if (keep) {
				if (!climate->snow_depth) {
					for (index_j = 0; index_j < g_conv_block_count_b; index_j++) {
						if (g_conv_block_b[index_j] == conv_id) {
							keep = false;
							index_j = g_conv_block_count_b;
						}
					}
				}
			}

			if (keep)
				portraits[kept_count++] = ConvGetPortraitId(conv_id);
		}

		conv_count = kept_count;

		for (int32_t i = 0; conv_count > i; i++) {
			for (index_j = 0; conv_count > index_j; index_j++) {
				if (index_j != i && portraits[index_j] == portraits[i]) {
					--conv_count;

					if (RandRangeAvg(0, 1, 1)) {
						portraits[i] = portraits[conv_count];
						conv_ids[i] = conv_ids[conv_count];
						--i;
						index_j = conv_count;

					} else {
						portraits[index_j] = portraits[conv_count];
						conv_ids[index_j] = conv_ids[conv_count];
					}
				}
			}
		}

		if (conv_count < 1)
			return;

		for (int32_t i = 0; i < 8 && conv_count > 0; i++) {
			if (is_fixed_landmark) {
				if (RandRangeAvg(0, 99, 1) < 30)
					index_j = 70;
				else
					index_j = 0;

			} else {
				index_j = RandRangeAvg(0, 99, 1);
			}

			if (index_j < 60) {
				--conv_count;
				index_j = RandRangeAvg(0, 1, 1);
				pick_index = RandRangeAvg(0, conv_count, 1);
				self->encounter[i].conv[index_j] = conv_ids[pick_index];
				conv_ids[pick_index] = conv_ids[conv_count];
				portraits[pick_index] = portraits[conv_count];

			} else if (index_j < 90) {
				for (int32_t conv_slot = 0; conv_slot < 2; conv_slot++) {
					if (conv_count > 0) {
						--conv_count;
						pick_index = RandRangeAvg(0, conv_count, 1);
						self->encounter[i].conv[conv_slot] = conv_ids[pick_index];
						conv_ids[pick_index] = conv_ids[conv_count];
						portraits[pick_index] = portraits[conv_count];
					}
				}
			}
		}

		if (is_fixed_landmark) {
			for (int32_t i = 1; i < 8; i++) {
				for (int32_t conv_slot = 0; conv_slot < 2; conv_slot++)
					self->encounter[i].conv[conv_slot] = self->encounter[0].conv[conv_slot];
			}
		}
	}

	self->base.depth = 0;
	self->base.width = 0;
	self->base.swiftness = 1;

	rsrc = AssetsGetElement(lib, "River", node_id, false);

	if (!rsrc)
		return;

	if (RsrcStorage(rsrc) == RSRC_TEXT)
		LogError(LOG_ERROR, "Attempt load a raw resource");

	RsrcLoadAndUnpack(rsrc);
	struct date *date = GetGameObj()->date;
	struct river_record river;
	river_record_read(RsrcData(rsrc), &river);

	self->base.depth = 6 * river.depth / 5;
	self->base.width = river.width;
	self->base.swiftness = river.swiftness;

	int32_t rain_excess = WeatherSumPrecip(climate, 30) - WeatherPrecipTotal(climate);
	int32_t swiftness_delta = (int16_t) ((rain_excess + 99) / 100);
	self->base.swiftness += swiftness_delta;

	if (rain_excess > 0) {
		if (swiftness_delta < 3) {
			depth_from = (river.depth + river.depth_low) / 2;
			depth_to = river.depth_high;
			width_from = (river.width + river.width_low) / 2;
			width_to = river.width_high;

		} else {
			depth_from = (river.depth * 2 + river.depth_low) / 3;
			depth_to = river.depth_high;
			width_from = (river.width * 2 + river.width_low) / 3;
			width_to = river.width_high;
		}

	} else if (rain_excess < 0) {
		swiftness_delta *= -1;

		if (swiftness_delta < 3) {
			depth_to = (river.depth_high + river.depth) / 2;
			depth_from = river.depth_low;
			width_to = (river.width_high + river.width) / 2;
			width_from = river.width_low;

		} else {
			depth_to = (river.depth * 2 + river.depth_high) / 3;
			depth_from = river.depth_low;
			width_to = (river.width * 2 + river.width_high) / 3;
			width_from = river.width_low;
		}
	}

	if (rain_excess) {
		int32_t roll = RandRangeAvg(0, 100, 2);
		int32_t scaled = (depth_to - depth_from) * roll * 6;
		scaled /= 500;
		self->base.depth = depth_from + scaled;
		scaled = (width_to - width_from) * roll;
		scaled /= 100;
		self->base.width = width_from + scaled;
	}

	if (self->base.width > 900) {
		self->base.width /= 100;
		self->base.width *= 100;

	} else if (self->base.width > 200) {
		self->base.width /= 50;
		self->base.width *= 50;

	} else if (self->base.width > 50) {
		self->base.width /= 10;
		self->base.width *= 10;
	}

	if (self->base.width < 10)
		self->base.width = 10;

	if (self->base.depth < 1)
		self->base.depth = 1;

	if (self->base.swiftness < 1)
		self->base.swiftness = 1;

	if (self->base.swiftness > 5)
		self->base.swiftness = 5;

	if (climate->snow_depth && WeatherCountFreezingDays(climate) > self->base.width / 250 + 5)
		self->base.swiftness = 0;

	self->ferry_available = true;

	if (!self->base.swiftness || date->month < river.ferry_first_month || river.ferry_last_month < date->month)
		self->ferry_available = false;

	self->guide_conv = 1;

	if (!self->base.swiftness || river.guide_first_month > date->month || river.guide_last_month < date->month)
		self->guide_conv = 0;

	if (self->guide_conv)
		self->guide_conv = river.guide_conv;

	if (GetGameObj()->destination == DEST_SALT_LAKE && river.salt_lake_ferry_free)
		self->ferry_price = 0;
	else
		self->ferry_price = calc_crossing_price(river.ferry_price);

	self->toll_bridge_price = calc_crossing_price(river.toll_bridge_price);

	for (int32_t i = 0; i < CROSSING_PIC_COUNT; i++)
		self->crossing_pics_a[i] = river.crossing_pics_a[i];

	for (int32_t i = 0; i < CROSSING_PIC_COUNT; i++)
		self->crossing_pics_b[i] = river.crossing_pics_b[i];

	RsrcDestroy(&rsrc);

	for (int32_t i = 0; self->base.event_count > i; i++) {
		if (self->base.events[i] == MSG_FERRY_DELAY) {
			while (RandRangeAvg(1, 100, 1) < 50)
				++self->ferry_wait_days;

		} else if (self->base.events[i] == MSG_CROWDED_RIVER) {
			while (RandRangeAvg(1, 100, 1) < 50)
				++self->arrival_wait_days;
		}
	}
}

struct lnode *LNodeCreate(int32_t node_id, bool apply)
{
	struct lnode *self = MemAlloc(1, sizeof(struct lnode));
	LNodeInit(self, node_id, apply);

	return self;
}

void LNodeDestroy(struct lnode **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct lnode *ctx = *ctx_mod;
	*ctx_mod = NULL;

	MemFree(ctx);
}

int32_t LNodeSelectNextRoute(struct lnode *self, struct game *game)
{
	if (PartyHasPlentyOfFood(game->party))
		return self->route_by_dest[game->destination];

	return self->route_by_dest[4];
}

struct encounter_slot *LNodeGetEncounterSlot(struct lnode *self, int32_t i)
{
	i = i % 8;
	return &self->encounter[i];
}

int32_t LNodeSelectCrossingPicture(struct lnode *self, int32_t method, int32_t outcome, bool game_field_c3e)
{
	int32_t *tiles;

	if (game_field_c3e)
		tiles = self->crossing_pics_a;
	else
		tiles = self->crossing_pics_b;

	switch (method) {
		case NODE_CMD_CAULK_AND_FLOAT:
			switch (outcome) {
				case CROSS_OUTCOME_SUCCESS:
					return tiles[CROSSING_PIC_FLOAT];

				case CROSS_OUTCOME_TIPPED_DEEP:
				case CROSS_OUTCOME_SWAMPED_DEEP:
				case CROSS_OUTCOME_TIPPED:
				case CROSS_OUTCOME_SWAMPED:
					return tiles[CROSSING_PIC_FLOAT_TROUBLE];

				case CROSS_OUTCOME_STUCK:
					return tiles[CROSSING_PIC_FLOAT_STUCK];

				default:
					return tiles[CROSSING_PIC_FLOAT];
			}

		case NODE_CMD_FORD:
			switch (outcome) {
				case CROSS_OUTCOME_SUCCESS:
					return tiles[CROSSING_PIC_FORD];

				case CROSS_OUTCOME_SWAMPED_DEEP:
				case CROSS_OUTCOME_SWAMPED:
					return tiles[CROSSING_PIC_FORD_SWAMPED];

				case CROSS_OUTCOME_TIPPED_DEEP:
				case CROSS_OUTCOME_TIPPED:
					return tiles[CROSSING_PIC_FORD_TIPPED];

				case CROSS_OUTCOME_STUCK:
					return tiles[CROSSING_PIC_FORD_STUCK];

				default:
					return tiles[CROSSING_PIC_FORD];
			}

		case NODE_CMD_FERRY:
			switch (outcome) {
				case CROSS_OUTCOME_SUCCESS:
					return tiles[CROSSING_PIC_FERRY];

				case CROSS_OUTCOME_TIPPED_DEEP:
				case CROSS_OUTCOME_STUCK:
				case CROSS_OUTCOME_SWAMPED_DEEP:
				case CROSS_OUTCOME_TIPPED:
				case CROSS_OUTCOME_SWAMPED:
					return tiles[CROSSING_PIC_FERRY_TROUBLE];

				default:
					return tiles[CROSSING_PIC_FERRY];
			}

		case NODE_CMD_CROSS_ICE:
			switch (outcome) {
				case CROSS_OUTCOME_SUCCESS:
					return 4300;

				default:
					return 4301;
			}
	}

	return 4300;
}

void NodeInit(struct node *self, int32_t node_id, bool apply)
{
	NodeBaseInit(&self->base, node_id);

	self->base.cmd_set_id = 0;

	struct assets *lib = g_data_lib;

	if (!lib)
		return;

	struct rsrc *rsrc = AssetsGetElement(lib, "TNode", node_id, false);

	if (!rsrc)
		return;

	const uint8_t *cursor = RsrcData(rsrc);
	self->base.miles = read_be16(&cursor);

	if (!self->base.miles)
		self->base.miles = 1;

	self->base.terrain = read_be16(&cursor);
	NodeBaseReadRecord(&self->base, &cursor);
	RsrcDestroy(&rsrc);

	if (!apply)
		return;

	switch (self->base.terrain) {
		case TERRAIN_NORMAL:
			GetGameObj()->travel_mod = 100;
			break;

		case TERRAIN_SAND:
			GetGameObj()->travel_mod = 60;
			break;

		case TERRAIN_UPHILL:
			GetGameObj()->travel_mod = 70;
			break;

		case TERRAIN_DOWNHILL:
			GetGameObj()->travel_mod = 60;
			break;

		case TERRAIN_ROUGH:
			GetGameObj()->travel_mod = 75;
			break;

		case TERRAIN_MOUNTAIN:
			GetGameObj()->travel_mod = 60;
			break;

		case TERRAIN_RAFT:
			GetGameObj()->travel_mod = 100;
			break;

		case TERRAIN_DESERT:
			GetGameObj()->travel_mod = 50;
			break;
	}

	self->base.steepness = 0;
	self->base.uphill = self->base.steepness;

	rsrc = AssetsGetElement(lib, "Hill", node_id, false);

	if (!rsrc)
		return;

	if (RsrcStorage(rsrc) == RSRC_TEXT)
		LogError(LOG_ERROR, "Attempt load a raw resource");

	RsrcLoadAndUnpack(rsrc);
	const uint8_t *hill = RsrcData(rsrc);
	self->base.steepness = ReadLe16(hill + 2);
	self->base.uphill = ReadLe16(hill);
	RsrcDestroy(&rsrc);
}

struct node *NodeCreate(int32_t node_id, bool apply)
{
	struct node *self = MemAlloc(1, sizeof(struct node));
	NodeInit(self, node_id, apply);

	return self;
}

void NodeDestroy(struct node **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct node *ctx = *ctx_mod;
	*ctx_mod = NULL;

	MemFree(ctx);
}

int32_t NodeBaseGetSegCount(struct node_base *self)
{
	return self->miles;
}

int32_t NodeGetTerrain(struct node *self)
{
	return self->base.terrain;
}
