#include "mainui.h"

#include <stddef.h>

#include "../app/app.h"
#include "../ui/cliplabelview.h"
#include "../base/cstr.h"
#include "../base/date.h"
#include "../draw/drawable.h"
#include "../game/game.h"
#include "../data/gfxdev.h"
#include "../hunt/hunting.h"
#include "../base/list.h"
#include "../base/memory.h"
#include "../game/msg.h"
#include "../trail/node.h"
#include "../app/otiiapp.h"
#include "../game/party.h"
#include "../draw/pictserver.h"
#include "progressbar.h"
#include "sceneassets.h"
#include "../draw/server.h"
#include "../base/str.h"
#include "../data/strrsrc.h"
#include "../ui/textview.h"
#include "../game/weather.h"

struct pic_rec g_pics_travel[3] = {
	{10, 6, 0, 1},
	{10, 65, 163, 300},
	{-1, 0, 0, 0},
};

struct panel_rec g_panel_travel[12] = {
	{"", 7, 105, 27, 230, 4, 0, 5},
	{"Hi", 25, 95, 44, 110, 4, 0, 200},
	{"", 25, 113, 44, 140, 4, 0, 6},
	{"Lo", 40, 95, 60, 110, 4, 0, 201},
	{"", 40, 113, 60, 140, 4, 0, 7},
	{"", 7, 235, 27, 260, 4, 2, 8},
	{"miles traveled", 7, 265, 27, 355, 4, 0, 202},
	{"", 25, 145, 44, 245, 4, 0, 10},
	{"", 25, 250, 44, 355, 4, 0, 11},
	{"", 40, 145, 60, 245, 4, 0, 12},
	{"", 40, 250, 60, 355, 4, 0, 13},
	{NULL, 0, 0, 0, 0, 4, 0, 0},
};

struct panel_rec g_panel_node_desc[2] = {
	{"", 10, 3, 50, 338, 2, 1, 15},
	{NULL, 0, 0, 0, 0, 4, 0, 0},
};

struct pic_rec g_pics_hunt[22] = {
	{10, 6, 160, 400},
	{10, 190, 161, 401},
	{10, 205, 161, 402},
	{10, 220, 161, 403},
	{10, 235, 161, 404},
	{10, 250, 161, 405},
	{10, 265, 161, 406},
	{10, 280, 161, 407},
	{10, 295, 161, 408},
	{10, 310, 161, 409},
	{10, 325, 161, 410},
	{30, 190, 161, 411},
	{30, 205, 161, 412},
	{30, 220, 161, 413},
	{30, 235, 161, 414},
	{30, 250, 161, 415},
	{30, 265, 161, 416},
	{30, 280, 161, 417},
	{30, 295, 161, 418},
	{30, 310, 161, 419},
	{30, 325, 161, 420},
	{-1, 0, 0, 0},
};

struct panel_rec g_panel_event[2] = {
	{"", 10, 3, 60, 338, 4, 1, 14},
	{NULL, 0, 0, 0, 0, 4, 0, 0},
};

struct panel_rec g_panel_river[6] = {
	{"", 10, 10, 25, 350, 4, 0, 103},
	{"River Conditions -", 25, 10, 40, 105, 4, 0, 100},
	{"", 25, 110, 40, 350, 4, 0, 101},
	{"", 40, 110, 55, 350, 4, 0, 102},
	{"", 40, 10, 55, 100, 4, 0, 104},
	{NULL, 0, 0, 0, 0, 4, 0, 0},
};

struct panel_rec g_panel_hill[2] = {
	{"", 25, 10, 40, 350, 4, 1, 110},
	{NULL, 0, 0, 0, 0, 4, 0, 0},
};

struct pic_rec *g_pics_node_desc = NULL;
struct panel_rec *g_panel_hunt = NULL;
struct pic_rec *g_pics_event = NULL;
struct pic_rec *g_pics_river = NULL;
struct pic_rec *g_pics_hill = NULL;

int32_t g_backdrop_panel_image[80] = {
	2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500,
	2500, 2500, 2500, 2500, 2500, 2501, 2501, 2501, 2501, 2501, 2501, 2501,
	2501, 2501, 2501, 2512, 2509, 2509, 2513, 2513, 2513, 2513, 2513, 2507,
	2507, 2507, 2507, 2507, 2507, 2507, 2507, 2507, 2507, 2507, 2507, 2507,
	2507, 2506, 2505, 2505, 2505, 2505, 2505, 2502, 2502, 2502, 2508, 2508,
	2508, 2508, 2508, 2508, 2508, 2508, 2511, 2510, 2510, 2510, 2510, 2510,
	2510, 2510, 2504, 2504, 2503, 2503, 2503, 2503,
};

uint8_t g_main_ui_cat_flags[28];

const struct main_ui_ops MAIN_UI_OPS_TABLE = {
	{
		{
			{
				{main_ui_destroy},
				evt_handler_is_handled,
				evt_handler_set_handled_for,
				evt_handler_set_handled,
				evt_handler_clear_handled_for,
				evt_handler_clear_handled,
				pic_view_on_event,
				view_on_idle,
				main_ui_on_idle_spare,
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
			pic_view_draw_children,
			view_move_children,
		},
	},
};

static struct view *as_view(struct main_ui *self)
{
	return &self->base.base;
}

static bool child_matches(const void *item, const void *key)
{
	const struct evt_handler *h = item;

	return EventMatches(key, &h->eh_event);
}

static void *find_child(struct main_ui *self, int32_t id)
{
	struct event spec = EventMake(EVCLS_ANY, -1, -1);
	EventSetId(&spec, id);

	struct view *view = as_view(self);

	if (!view->has_child_list)
		return NULL;

	return ObarrayFind(view->child_list, child_matches, &spec);
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

void MainUiInit(struct main_ui *self, int32_t id, const struct point *pos, int32_t img_id)
{
	PicViewInitPoint(&self->base, id, pos, img_id);
	self->base.base.base.base.ops = &MAIN_UI_OPS_TABLE.base.base.base.base;

	MainUiBuildPicViewPanel(self, g_pics_travel);
	MainUiBuildPanel(self, g_panel_travel, 0);
	MainUiBuildPicViewPanel(self, g_pics_node_desc);
	MainUiBuildPanel(self, g_panel_node_desc, 0);
	MainUiBuildPicViewPanel(self, g_pics_hunt);
	MainUiBuildPanel(self, g_panel_hunt, 0);
	MainUiBuildPicViewPanel(self, g_pics_event);
	MainUiBuildPanel(self, g_panel_event, 1);
	MainUiBuildPicViewPanel(self, g_pics_river);
	MainUiBuildPanel(self, g_panel_river, 0);
	MainUiBuildPicViewPanel(self, g_pics_hill);
	MainUiBuildPanel(self, g_panel_hill, 0);
	MainUiSetPanelMode(self, 0);

	for (int32_t i = 0; i < 28; i++)
		g_main_ui_cat_flags[i] = 0;

	for (int32_t i = 0; i < 4; i++)
		self->slots[i] = -1;

	self->strings = StrRsrcLoad(g_text_lib, 2402, true);
}

struct main_ui *MainUiCreate(int32_t id, const struct point *pos, int32_t img_id)
{
	struct main_ui *self = MemAlloc(1, sizeof(struct main_ui));
	MainUiInit(self, id, pos, img_id);

	return self;
}

void MainUiDestroy(struct main_ui **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void main_ui_destroy(struct object *obj)
{
	struct main_ui *self = (struct main_ui *) obj;

	StrRsrcDestroy(&self->strings);

	if (g_app && ((struct otii_app *) g_app)->ui == self)
		((struct otii_app *) g_app)->ui = NULL;

	obj->ops = &PIC_VIEW_OPS_TABLE.base.base.base;
	pic_view_destroy(obj);
}

void main_ui_on_idle_spare(struct evt_handler *handler, void *param)
{
	MainUiRefresh((struct main_ui *) handler);
}

void MainUiSetPanelMode(struct main_ui *self, int32_t mode)
{
	struct evt_handler *handler = &as_view(self)->base;

	switch (mode) {
		case 0:
			MainUiHidePanelGroup(self, g_panel_node_desc);
			MainUiHideViewGroup(self, g_pics_node_desc);
			MainUiHidePanelGroup(self, g_panel_hunt);
			MainUiHideViewGroup(self, g_pics_hunt);
			MainUiHidePanelGroup(self, g_panel_event);
			MainUiHideViewGroup(self, g_pics_event);
			MainUiHidePanelGroup(self, g_panel_river);
			MainUiHideViewGroup(self, g_pics_river);
			MainUiHidePanelGroup(self, g_panel_hill);
			MainUiHideViewGroup(self, g_pics_hill);
			MainUiShowPanelGroup(self, g_panel_travel);
			MainUiShowViewGroup(self, g_pics_travel);
			EvtHandlerAddIdleHandler(handler, false, 200);
			break;

		case 1:
			EvtHandlerRemoveIdleHandler(handler, false);
			MainUiHidePanelGroup(self, g_panel_travel);
			MainUiHideViewGroup(self, g_pics_travel);
			MainUiHidePanelGroup(self, g_panel_hunt);
			MainUiHideViewGroup(self, g_pics_hunt);
			MainUiHidePanelGroup(self, g_panel_event);
			MainUiHideViewGroup(self, g_pics_event);
			MainUiHidePanelGroup(self, g_panel_river);
			MainUiHideViewGroup(self, g_pics_river);
			MainUiHidePanelGroup(self, g_panel_hill);
			MainUiHideViewGroup(self, g_pics_hill);
			MainUiShowPanelGroup(self, g_panel_node_desc);
			MainUiShowViewGroup(self, g_pics_node_desc);
			break;

		case 2:
			EvtHandlerRemoveIdleHandler(handler, false);
			MainUiHidePanelGroup(self, g_panel_travel);
			MainUiHideViewGroup(self, g_pics_travel);
			MainUiHidePanelGroup(self, g_panel_node_desc);
			MainUiHideViewGroup(self, g_pics_node_desc);
			MainUiHidePanelGroup(self, g_panel_event);
			MainUiHideViewGroup(self, g_pics_event);
			MainUiHidePanelGroup(self, g_panel_river);
			MainUiHideViewGroup(self, g_pics_river);
			MainUiHidePanelGroup(self, g_panel_hill);
			MainUiHideViewGroup(self, g_pics_hill);
			MainUiShowPanelGroup(self, g_panel_hunt);
			MainUiShowViewGroup(self, g_pics_hunt);
			break;

		case 3:
			EvtHandlerRemoveIdleHandler(handler, false);
			MainUiHidePanelGroup(self, g_panel_travel);
			MainUiHideViewGroup(self, g_pics_travel);
			MainUiHidePanelGroup(self, g_panel_node_desc);
			MainUiHideViewGroup(self, g_pics_node_desc);
			MainUiHidePanelGroup(self, g_panel_hunt);
			MainUiHideViewGroup(self, g_pics_hunt);
			MainUiHidePanelGroup(self, g_panel_river);
			MainUiHideViewGroup(self, g_pics_river);
			MainUiHidePanelGroup(self, g_panel_hill);
			MainUiHideViewGroup(self, g_pics_hill);
			MainUiShowPanelGroup(self, g_panel_event);
			MainUiShowViewGroup(self, g_pics_event);
			break;

		case 4:
			EvtHandlerRemoveIdleHandler(handler, false);
			MainUiHidePanelGroup(self, g_panel_travel);
			MainUiHideViewGroup(self, g_pics_travel);
			MainUiHidePanelGroup(self, g_panel_node_desc);
			MainUiHideViewGroup(self, g_pics_node_desc);
			MainUiHidePanelGroup(self, g_panel_hunt);
			MainUiHideViewGroup(self, g_pics_hunt);
			MainUiHidePanelGroup(self, g_panel_event);
			MainUiHideViewGroup(self, g_pics_event);
			MainUiHidePanelGroup(self, g_panel_hill);
			MainUiHideViewGroup(self, g_pics_hill);
			MainUiShowPanelGroup(self, g_panel_river);
			MainUiShowViewGroup(self, g_pics_river);
			break;

		case 5:
			EvtHandlerRemoveIdleHandler(handler, false);
			MainUiHidePanelGroup(self, g_panel_travel);
			MainUiHideViewGroup(self, g_pics_travel);
			MainUiHidePanelGroup(self, g_panel_node_desc);
			MainUiHideViewGroup(self, g_pics_node_desc);
			MainUiHidePanelGroup(self, g_panel_hunt);
			MainUiHideViewGroup(self, g_pics_hunt);
			MainUiHidePanelGroup(self, g_panel_event);
			MainUiHideViewGroup(self, g_pics_event);
			MainUiHidePanelGroup(self, g_panel_river);
			MainUiHideViewGroup(self, g_pics_river);
			MainUiShowPanelGroup(self, g_panel_hill);
			MainUiShowViewGroup(self, g_pics_hill);
			break;

		default:
			return;
	}

	self->panel_mode = mode;
}

static int32_t river_toll(struct lnode *node)
{
	if (node->ferry_price)
		return node->ferry_price;

	return node->toll_bridge_price;
}

static void set_cell_frame(struct pic_view *cell, int32_t v)
{
	if (cell->image) {
		if (cell->image->item_count <= v) {
			cell->cur_frame = cell->image->item_count;

		} else if (v < 0) {
			cell->cur_frame = 0;

		} else {
			cell->cur_frame = v;
		}
	}
}

void MainUiRefresh(struct main_ui *self)
{
	int32_t value = 0;
	struct game *game = GetGameObj();

	if (!game)
		return;

	if (!game->cur_node)
		return;

	switch (self->panel_mode) {
		case 0: {
			char text_buf[64];
			struct cstr *str;

			int32_t backdrop = NodeBaseGetSeasonBackdrop(game->cur_node, NULL);
			int32_t region_id = RegionalAssetId3000();

			if (self->last_backdrop != backdrop || self->last_region_id != region_id) {
				if (backdrop > 79)
					backdrop = 0;

				self->last_backdrop = backdrop;
				self->last_region_id = region_id;
				int32_t image_id = g_backdrop_panel_image[backdrop];

				if (game->weather->snow_depth)
					image_id += 200;

				MainUiSetViewImage(self, 1, RegionalAssetId3000(), image_id);
			}

			value = WeatherGetHighTemp(game->weather, 0);

			if (self->last_high_temp != value) {
				self->last_high_temp = value;
				StrPrintf(text_buf, "%d%c", value, 161);
				str = CStrCreateFrom(text_buf);
				CStrMacRomanToAnsi(str, 0);
				MainUiSetLabel(self, 6, str);
			}

			value = WeatherGetLowTemp(game->weather, 0);

			if (self->last_low_temp != value) {
				self->last_low_temp = value;
				StrPrintf(text_buf, "%d%c", value, 161);
				str = CStrCreateFrom(text_buf);
				CStrMacRomanToAnsi(str, 0);
				MainUiSetLabel(self, 7, str);
			}

			value = WeatherAvgTemp(game->weather, 0);

			if (self->last_avg_temp != value) {
				self->last_avg_temp = value;
				struct progress_bar *temp_bar = (struct progress_bar *) ViewFindById(as_view(self), 300);

				if (temp_bar)
					ProgressBarSetMarker(temp_bar, WeatherAvgTemp(game->weather, 0));
			}

			value = (int16_t) game->total_miles;

			if (self->last_val_96 != value) {
				self->last_val_96 = value;
				StrPrintf(text_buf, "%d", value);
				MainUiSetLabel(self, 8, CStrCreateFrom(text_buf));
			}

			value = game->date->day;

			if (self->last_day != value) {
				self->last_day = value;
				str = month_name(game->date);
				StrPrintf(text_buf, "%s %d, %d", CStrGet(str), game->date->day, game->date->year);
				CStrDestroy(&str);
				MainUiSetLabel(self, 5, CStrCreateFrom(text_buf));
			}

			PartyRefreshCatFlags(GetGameObj()->party);
			GameUpdateCmdFlags(GetGameObj());
			WeatherUpdateFlags(GetGameObj()->weather);
			MainUiUpdateCatSlots(self);
			break;
		}

		case 1: {
			value = game->cur_node->node_id;
			struct cstr *desc_str = StrRsrcGetCStr(game->node_names, value);
			CStrReplaceAll(desc_str, "\r", " ");
			MainUiSetLabel(self, 15, desc_str);
			break;
		}

		case 2: {
			struct hunt_status hunt_state;

			if (!g_hunt_state)
				return;

			HuntingGetState3(g_hunt_state, &hunt_state);
			MainUiSetViewFrame(self, 400, hunt_state.weapon);

			for (int32_t cell_index = 0; cell_index < 20; ++cell_index) {
				struct view *cell_view = ViewFindById(as_view(self), cell_index + 401);
				set_cell_frame((struct pic_view *) cell_view, hunt_state.weapon);

				if (hunt_state.shots_left > cell_index)
					VIEW_OPS(cell_view)->Show(cell_view);
				else
					VIEW_OPS(cell_view)->Hide(cell_view, false, true);
			}

			ViewNotifyChanged(as_view(self));
			break;
		}

		case 3: {
			struct msg *event = game->event_node;

			if (event) {
				struct cstr *event_str = event->payload;

				if (event_str)
					MainUiSetLabel(self, 14, CStrCreateCopy(event_str));
			}

			break;
		}

		case 4: {
			char text_buf[32];
			struct strrsrc *rsrc;

			struct node_base *node = game->cur_node;
			int32_t river_width = node->width;
			int32_t depth_index = (node->depth + 5) / 6;
			int32_t swiftness = node->swiftness;
			double toll = river_toll((struct lnode *) node) / 100.0;

			StrPrintf(text_buf, "width:  %d feet", river_width);
			MainUiSetLabel(self, 101, CStrCreateFrom(text_buf));

			if (!swiftness) {
				MainUiSetLabel(self, 102, CStrCreate());

			} else {
				rsrc = StrRsrcLoad(g_text_lib, 2400, true);

				if (StrRsrcCount(rsrc) <= depth_index) {
					StrPrintf(text_buf, "depth:  %d feet", depth_index / 2);
					MainUiSetLabel(self, 102, CStrCreateFrom(text_buf));

				} else {
					MainUiSetLabel(self, 102, StrRsrcGetCStr(rsrc, depth_index));
				}

				StrRsrcDestroy(&rsrc);
			}

			rsrc = StrRsrcLoad(g_text_lib, 2401, true);

			if (StrRsrcCount(rsrc) > swiftness)
				MainUiSetLabel(self, 103, StrRsrcGetCStr(rsrc, swiftness));
			else
				MainUiSetLabel(self, 103, CStrCreate());

			StrRsrcDestroy(&rsrc);

			if (toll != 0.0 && swiftness) {
				StrPrintf(text_buf, "Toll: $%4.2lf", toll);
				MainUiSetLabel(self, 104, CStrCreateFrom(text_buf));

			} else {
				MainUiSetLabel(self, 104, CStrCreate());
			}

			break;
		}

		case 5: {
			struct node_base *node = game->cur_node;
			int32_t hill = node->steepness;

			if (!hill) {
				MainUiSetLabel(self, 110, CStrCreate());

			} else {
				bool cat_flag = MainUiGetCatFlag(CAT_MUDDY_TRAIL);
				int32_t snow = GetGameObj()->weather->snow_depth;
				int32_t string_index;
				--hill;

				if ((uint8_t) node->uphill)
					string_index = 0;
				else
					string_index = 15;

				if (snow)
					string_index += 10;
				else if (cat_flag)
					string_index += 5;

				string_index += hill;

				struct strrsrc *rsrc = StrRsrcLoad(g_text_lib, 2403, true);
				StrRsrcRemapHighChars(rsrc, 0);
				MainUiSetLabel(self, 110, StrRsrcGetCStr(rsrc, string_index));
				StrRsrcDestroy(&rsrc);
			}

			break;
		}

		default:
			break;
	}
}

void MainUiSetLabel(struct main_ui *self, int32_t label_id, struct cstr *text)
{
	struct clip_label_view *label_view = find_child(self, label_id);

	if (label_view)
		ClipLabelViewSetLabelAttr(label_view, text);
	else
		CStrDestroy(&text);
}

void MainUiSetViewFrame(struct main_ui *self, int32_t id, int32_t frame)
{
	struct pic_view *pic_view = find_child(self, id);

	if (pic_view) {
		if (pic_view->image) {
			if (pic_view->image->item_count <= frame) {
				pic_view->cur_frame = pic_view->image->item_count;

			} else {
				if (frame < 0)
					pic_view->cur_frame = 0;
				else
					pic_view->cur_frame = frame;
			}
		}
	}
}

void MainUiSetViewImage(struct main_ui *self, int32_t id, int32_t type, int32_t flag)
{
	struct drawable *image = ServerCreateVirtualPacked(&g_pict_server->base.base, type, flag, true);
	struct pic_view *pic_view = find_child(self, id);

	PicViewSetImage(pic_view, image);
}

void MainUiBuildPicViewPanel(struct main_ui *self, const struct pic_rec *table)
{
	struct point origin = ViewGetOrigin(as_view(self));

	if (!table)
		return;

	for (int32_t index = 0; table[index].off_v > 0; ++index) {
		struct point pos;
		pos.h = table[index].off_h + origin.h;
		pos.v = table[index].off_v + origin.v;

		struct view *view;

		if (table[index].id == 300) {
			view = (struct view *) ProgressBarCreate(table[index].id, &pos, table[index].img_id);

		} else if (table[index].img_id) {
			view = (struct view *) PicViewCreatePoint(table[index].id, &pos, table[index].img_id);

		} else {
			view = (struct view *) PicViewCreatePointEmpty(table[index].id, &pos);
		}

		VIEW_OPS(self)->AddChild(as_view(self), view);
	}
}

void MainUiBuildPanel(struct main_ui *self, const struct panel_rec *table, int32_t as_textview)
{
	struct point origin = ViewGetOrigin(as_view(self));

	if (!table)
		return;

	for (int32_t index = 0; table[index].text; ++index) {
		struct cstr *text = CStrCreateFrom(table[index].text);

		struct rect rect;
		rect.top = origin.v + table[index].off_top;
		rect.left = origin.h + table[index].off_left;
		rect.right = origin.h + table[index].off_right;
		rect.bottom = origin.v + table[index].off_bot;

		struct view *view;

		if ((uint8_t) as_textview) {
			view = (struct view *) TextViewCreate(table[index].id, &rect, text, table[index].font, table[index].param,
				1, 0, 0, 0, 0, 255, 0, 1);

		} else {
			view = (struct view *) ClipLabelViewCreate(table[index].id, &rect, text, table[index].font,
				table[index].param, 1, 0, 0, 255, 0, 1);
		}

		VIEW_OPS(self)->AddChild(as_view(self), view);
	}
}

void MainUiHidePanelGroup(struct main_ui *self, const struct panel_rec *table)
{
	if (!table)
		return;

	for (int32_t index = 0; table[index].text; index++) {
		struct view *view = find_child(self, table[index].id);
		VIEW_OPS(view)->Hide(view, false, true);
	}
}

void MainUiShowPanelGroup(struct main_ui *self, const struct panel_rec *table)
{
	if (!table)
		return;

	for (int32_t index = 0; table[index].text; index++) {
		struct view *view = find_child(self, table[index].id);
		VIEW_OPS(view)->Show(view);
	}
}

void MainUiHideViewGroup(struct main_ui *self, const struct pic_rec *table)
{
	if (!table)
		return;

	for (int32_t index = 0; table[index].off_v != -1; index++) {
		struct view *view = find_child(self, table[index].id);
		VIEW_OPS(view)->Hide(view, false, true);
	}
}

void MainUiShowViewGroup(struct main_ui *self, const struct pic_rec *table)
{
	if (!table)
		return;

	for (int32_t index = 0; table[index].off_v != -1; index++) {
		struct view *view = find_child(self, table[index].id);
		VIEW_OPS(view)->Show(view);
	}
}

void MainUiShowTravelPanel(struct main_ui *self)
{
	MainUiSetPanelMode(self, 0);
	MainUiRefresh(self);
}

void MainUiShowNodeDescPanel(struct main_ui *self)
{
	MainUiSetPanelMode(self, 1);
	MainUiRefresh(self);
}

void MainUiShowHuntPanel(struct main_ui *self)
{
	MainUiSetPanelMode(self, 2);
	MainUiRefresh(self);
}

void MainUiShowEventPanel(struct main_ui *self)
{
	MainUiSetPanelMode(self, 3);
	MainUiRefresh(self);
}

void MainUiShowRiverPanel(struct main_ui *self)
{
	MainUiSetPanelMode(self, 4);
	MainUiRefresh(self);
}

void MainUiShowHillPanel(struct main_ui *self)
{
	MainUiSetPanelMode(self, 5);
	MainUiRefresh(self);
}

void MainUiUpdateCatSlots(struct main_ui *self)
{
	int32_t slot = 0;

	for (int32_t cat = 0; cat < 28 && slot < 4; cat++) {
		if (g_main_ui_cat_flags[cat]) {
			if (self->slots[slot] != cat) {
				MainUiSetLabel(self, slot + 10, StrRsrcGetCStr(self->strings, cat));
				self->slots[slot] = cat;
			}

			++slot;
		}
	}

	while (slot < 4) {
		self->slots[slot] = -1;
		MainUiSetLabel(self, slot++ + 10, CStrCreate());
	}
}

bool MainUiGetCatFlag(int32_t cat)
{
	if (cat < 0 || cat >= CAT_COUNT)
		return false;

	return g_main_ui_cat_flags[cat];
}

void MainUiSetCatFlag(int32_t cat)
{
	if (cat < 0 || cat >= CAT_COUNT)
		return;

	g_main_ui_cat_flags[cat] = 1;
}

void MainUiClearCatFlag(int32_t cat)
{
	if (cat < 0 || cat >= CAT_COUNT)
		return;

	g_main_ui_cat_flags[cat] = 0;
}
