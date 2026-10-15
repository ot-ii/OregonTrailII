#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/object.h"

struct menu;
struct cstr;
struct assets;

struct menu_item {
	struct object base;
	int32_t command;
	int32_t submenu_id;
	int32_t sub_command;
	bool enabled;
	bool check_state;
	bool is_separator;
	struct menu *submenu;
	struct cstr *text;
	bool checked;
};

extern const struct object_ops MENU_ITEM_OPS_TABLE;

struct menu_item *MenuItemCreateSeparator(void);
struct menu_item *MenuItemCreateAdopt(const struct cstr *label, int32_t command, struct menu *adopted_submenu,
	int32_t sub_command, bool enabled_flag, bool checked_flag);
struct menu_item *MenuItemCreate(struct assets *lib, const struct cstr *label, int32_t command, int32_t sub_menu_id,
	int32_t sub_command, bool enabled_flag, bool checked_flag);
void MenuItemDestroy(struct menu_item **ctx_mod);
void menu_item_destroy(struct object *self);

void MenuItemFireCommand(struct menu_item *self);
void MenuItemUnused4785E3(struct menu_item *self, int32_t arg);
