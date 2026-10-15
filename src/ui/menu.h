#pragma once

#include <stdint.h>

#include "../base/object.h"

struct menu_item;
struct obarray;
struct assets;

struct menu {
	struct object base;
	int32_t id;
	struct menu_item *owner;
	struct obarray *items;
	int32_t unused_10;
};

extern const struct object_ops MENU_OPS_TABLE;

struct menu *MenuCreate(int32_t id, struct assets *lib, struct menu_item *owner);
struct menu *MenuCreateId(int32_t id);
void MenuDestroy(struct menu **ctx_mod);
void menu_destroy(struct object *self);

void MenuAddItem(struct menu *self, struct menu_item *obj);
struct menu_item *MenuFindItem(struct menu *self, int32_t id, struct menu **out_menu, int32_t *out_index);
void MenuRemoveItem(struct menu *self, int32_t index);
void *MenuBuildPopup(struct menu *self, int32_t index);

void AudioDeviceMenuAppend(void *popup);
