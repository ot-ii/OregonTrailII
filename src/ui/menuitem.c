#include "menuitem.h"

#include <stddef.h>

#include "../base/cstr.h"
#include "../app/event.h"
#include "../app/evthandler.h"
#include "../base/memory.h"
#include "menu.h"

const struct object_ops MENU_ITEM_OPS_TABLE = {
	menu_item_destroy,
};

static struct menu_item *menu_item_alloc(void)
{
	struct menu_item *self = MemAlloc(1, sizeof(struct menu_item));
	ObjectInit(&self->base);
	self->base.ops = &MENU_ITEM_OPS_TABLE;

	return self;
}

struct menu_item *MenuItemCreateSeparator(void)
{
	struct menu_item *self = menu_item_alloc();
	self->is_separator = true;
	self->text = CStrCreateFrom("-");
	self->command = 0;
	self->submenu_id = 0;
	self->enabled = false;
	self->submenu = NULL;
	self->sub_command = 0;
	self->checked = false;

	return self;
}

struct menu_item *MenuItemCreateAdopt(const struct cstr *label, int32_t command, struct menu *adopted_submenu,
	int32_t sub_command, bool enabled_flag, bool checked_flag)
{
	struct menu_item *self = menu_item_alloc();
	self->is_separator = false;
	self->command = command;
	self->text = CStrCreateCopy(label);
	self->submenu_id = 0;
	self->sub_command = sub_command;
	self->enabled = enabled_flag;
	self->checked = checked_flag;

	if (adopted_submenu) {
		self->submenu = adopted_submenu;
		self->submenu_id = adopted_submenu->id;

	} else {
		self->submenu = NULL;
	}

	return self;
}

struct menu_item *MenuItemCreate(struct assets *lib, const struct cstr *label, int32_t command, int32_t sub_menu_id,
	int32_t sub_command, bool enabled_flag, bool checked_flag)
{
	struct menu_item *self = menu_item_alloc();
	self->is_separator = false;
	self->command = command;
	self->text = CStrCreateCopy(label);
	self->submenu_id = sub_menu_id;
	self->sub_command = sub_command;
	self->enabled = enabled_flag;
	self->checked = checked_flag;

	if (self->submenu_id) {
		self->submenu = MenuCreate(self->submenu_id, lib, self);

	} else {
		self->submenu = NULL;
	}

	return self;
}

void MenuItemDestroy(struct menu_item **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void menu_item_destroy(struct object *obj)
{
	struct menu_item *self = (struct menu_item *) obj;

	CStrDestroy(&self->text);
	MenuDestroy(&self->submenu);

	object_destroy(obj);
}

void MenuItemFireCommand(struct menu_item *self)
{
	if (self->sub_command) {
		struct cmd_event *ev = CmdEventCreate(self->sub_command);
		ev->param2 = self->command;
		DispatchGlobalEvent(ev);
	}
}

void MenuItemUnused4785E3(struct menu_item *self, int32_t arg)
{
	(void) self;
	(void) arg;
}
