#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../app/evthandler.h"

struct menu;
struct menu_item;
struct cstr;

struct menu_bar {
	struct evt_handler base;
	struct menu *menu;
	int32_t unused_1C;
};

extern const struct evt_handler_ops MENU_BAR_OPS_TABLE;

void MenuBarInit(struct menu_bar *self, struct menu *body);
struct menu_bar *MenuBarCreate(struct menu *body);
void MenuBarDestroy(struct menu_bar **ctx_mod);
void menu_bar_destroy(struct object *self);

void menu_bar_on_event(struct evt_handler *self, struct cmd_event *event);

void MenuBarUnused4A9230(struct menu_bar *self, int32_t arg);
void MenuBarUnused4A95C7(struct menu_bar *self, int32_t arg);
void MenuBarBuild(struct menu_bar *self);
void MenuBarEnableItem(struct menu_bar *self, uint32_t id, bool enable);
void MenuBarCheckItem(struct menu_bar *self, uint32_t id, bool checked);
struct menu_item *MenuBarFindItem(struct menu_bar *self, int32_t id, struct menu **out_menu, int32_t *out_index);
void MenuBarDeleteItem(struct menu_bar *self, int32_t id);
void MenuBarSetText(struct menu_bar *self, int32_t id, struct cstr *text);
void *MenuBarGetHandle(void);

extern struct menu_bar *g_menubar;

#define MENU_RSRC_BAR 11000

enum menu_item_id {
	MENU_ITEM_FILE = 1010,
	MENU_ITEM_EDIT = 1020,
	MENU_ITEM_GAME = 1050,
	MENU_ITEM_HELP = 1070,
	MENU_ITEM_NEW_GAME = 2010,
	MENU_ITEM_LOAD_GAME = 2020,
	MENU_ITEM_END_GAME = 2030,
	MENU_ITEM_SAVE = 2050,
	MENU_ITEM_SAVE_GAME = 2060,
	MENU_ITEM_EXIT = 2070,
	MENU_ITEM_EXPORT_DIARY = 2100,
	MENU_ITEM_UNDO = 3010,
	MENU_ITEM_CUT = 3020,
	MENU_ITEM_COPY = 3030,
	MENU_ITEM_PASTE = 3040,
	MENU_ITEM_DELETE = 3050,
	MENU_ITEM_SIM_SPEED = 6010,
	MENU_ITEM_LEGENDS = 6020,
	MENU_ITEM_SOUND_EFFECTS = 6030,
	MENU_ITEM_MUSIC = 6040,
	MENU_ITEM_SPEECH = 6050,
	MENU_ITEM_GLOSSARY = 6060,
	MENU_ITEM_TIME_OUT = 6070,
	MENU_ITEM_VIEW_MAP = 6080,
	MENU_ITEM_HELP_CONTENTS = 8010,
	MENU_ITEM_ABOUT = 8020,
};

#define FORCE_SAVE_MENU_ID 2061
#define PLAY_INTRO_MENU_ID 2062
#define DISCORD_MENU_ID 6091
#define DISCORD_URL "https://discord.gg/JyHzQMCdQR"
#define AUTOSAVE_MENU_ID 6098
#define DEBUG_SCREEN_MENU_ID 6099

void ForceSaveAddMenuItem(void *menu_bar);
void ForceSaveRun(void);
void PlayIntroAddMenuItem(void *menu_bar);
void PlayIntroRun(void);
bool AutosaveEnabled(void);
void AutosaveSetEnabled(bool enabled);
void DebugScreenToggle(void);
extern void *g_hmnu;
