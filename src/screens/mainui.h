#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "../ui/picview.h"

struct cstr;
struct strrsrc;

struct pic_rec {
	int32_t off_v;
	int32_t off_h;
	int32_t img_id;
	int32_t id;
};

struct panel_rec {
	const char *text;
	int32_t off_top;
	int32_t off_left;
	int32_t off_bot;
	int32_t off_right;
	int32_t font;
	int32_t param;
	int32_t id;
};

enum cat_flag {
	CAT_NO_FOOD = 0,
	CAT_NO_WAGON = 1,
	CAT_NO_DRAFT_ANIMALS = 2,
	CAT_PERSON_NEAR_DEATH = 3,
	CAT_NO_PROGRESS = 4,
	CAT_LOW_FOOD = 5,
	CAT_NO_GRASS = 6,
	CAT_NO_WATER = 7,
	CAT_ALKALI_WATER = 8,
	CAT_BAD_WATER = 9,
	CAT_MUDDY_TRAIL = 10,
	CAT_DEEP_SAND = 11,
	CAT_SOMEONE_SICK = 12,
	CAT_NO_MEAT = 13,
	CAT_NO_FRUIT = 14,
	CAT_NO_VEGETABLES = 15,
	CAT_DESERT = 16,
	CAT_ROUGH_TRAIL = 17,
	CAT_SNOWING = 18,
	CAT_RAINY = 19,
	CAT_POOR_HEALTH = 20,
	CAT_MORALE_LOW = 21,
	CAT_RESTING = 23,
	CAT_MORALE_VERY_LOW = 24,
	CAT_MORALE_FAIR = 25,
	CAT_MORALE_HIGH = 26,
	CAT_SLOW_PROGRESS = 27,
	CAT_COUNT = 28,
};

struct main_ui_ops {
	struct pic_view_ops base;
};

struct main_ui {
	struct pic_view base;
	int32_t panel_mode;
	struct strrsrc *strings;
	int32_t last_region_id;
	int32_t last_backdrop;
	int32_t last_high_temp;
	int32_t last_low_temp;
	int32_t last_avg_temp;
	int32_t last_val_96;
	uint8_t flags[28];
	int32_t slots[4];
	int32_t last_day;
};

#define MAIN_UI_OPS(p) ((const struct main_ui_ops *) ((const struct object *) (p))->ops)

extern const struct main_ui_ops MAIN_UI_OPS_TABLE;

extern struct pic_rec g_pics_travel[3];
extern struct panel_rec g_panel_travel[12];
extern struct panel_rec g_panel_node_desc[2];
extern struct pic_rec g_pics_hunt[22];
extern struct panel_rec g_panel_event[2];
extern struct panel_rec g_panel_river[6];
extern struct panel_rec g_panel_hill[2];
extern struct pic_rec *g_pics_node_desc;
extern struct panel_rec *g_panel_hunt;
extern struct pic_rec *g_pics_event;
extern struct pic_rec *g_pics_river;
extern struct pic_rec *g_pics_hill;
extern int32_t g_backdrop_panel_image[80];
extern uint8_t g_main_ui_cat_flags[28];

void MainUiInit(struct main_ui *self, int32_t id, const struct point *pos, int32_t img_id);
struct main_ui *MainUiCreate(int32_t id, const struct point *pos, int32_t img_id);
void MainUiDestroy(struct main_ui **ctx_mod);
void main_ui_destroy(struct object *self);

void main_ui_on_idle_spare(struct evt_handler *self, void *param);
void MainUiSetPanelMode(struct main_ui *self, int32_t mode);
void MainUiRefresh(struct main_ui *self);
void MainUiSetLabel(struct main_ui *self, int32_t label_id, struct cstr *text);
void MainUiSetViewFrame(struct main_ui *self, int32_t id, int32_t frame);
void MainUiSetViewImage(struct main_ui *self, int32_t id, int32_t type, int32_t flag);
void MainUiBuildPicViewPanel(struct main_ui *self, const struct pic_rec *table);
void MainUiBuildPanel(struct main_ui *self, const struct panel_rec *table, int32_t as_textview);
void MainUiHidePanelGroup(struct main_ui *self, const struct panel_rec *table);
void MainUiShowPanelGroup(struct main_ui *self, const struct panel_rec *table);
void MainUiHideViewGroup(struct main_ui *self, const struct pic_rec *table);
void MainUiShowViewGroup(struct main_ui *self, const struct pic_rec *table);
void MainUiShowTravelPanel(struct main_ui *self);
void MainUiShowNodeDescPanel(struct main_ui *self);
void MainUiShowHuntPanel(struct main_ui *self);
void MainUiShowEventPanel(struct main_ui *self);
void MainUiShowRiverPanel(struct main_ui *self);
void MainUiShowHillPanel(struct main_ui *self);
void MainUiUpdateCatSlots(struct main_ui *self);
bool MainUiGetCatFlag(int32_t cat);
void MainUiSetCatFlag(int32_t cat);
void MainUiClearCatFlag(int32_t cat);
