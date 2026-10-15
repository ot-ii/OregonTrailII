#include "../menubar.h"

#include <stddef.h>

#include <windows.h>

#include "../../features/autopilot.h"
#include "../../features/crowdcontrol.h"
#include "../../base/cstr.h"
#include "../../base/list.h"
#include "../../base/log.h"
#include "../../base/memory.h"
#include "../menu.h"
#include "../menuitem.h"
#include "../../features/twitchcontrol.h"

void *g_hmnu;

const struct evt_handler_ops MENU_BAR_OPS_TABLE = {
	{menu_bar_destroy},
	evt_handler_is_handled,
	evt_handler_set_handled_for,
	evt_handler_set_handled,
	evt_handler_clear_handled_for,
	evt_handler_clear_handled,
	menu_bar_on_event,
	evt_handler_on_idle,
	evt_handler_on_idle_spare,
};

void MenuBarInit(struct menu_bar *self, struct menu *body)
{
	EvtHandlerInit(&self->base, 0x4E29, 0);
	self->base.base.ops = &MENU_BAR_OPS_TABLE.base;

	int32_t handler_ids[2] = {30004, 30005};
	self->menu = body;
	self->unused_1C = 0;
	MenuBarBuild(self);
	EvtHandlerBindHandler(&self->base, 2, handler_ids);
}

struct menu_bar *MenuBarCreate(struct menu *body)
{
	struct menu_bar *self = MemAlloc(1, sizeof(struct menu_bar));
	MenuBarInit(self, body);

	return self;
}

void MenuBarDestroy(struct menu_bar **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void menu_bar_destroy(struct object *obj)
{
	struct menu_bar *self = (struct menu_bar *) obj;

	MenuDestroy(&self->menu);

	obj->ops = &EVT_HANDLER_OPS_TABLE.base;
	evt_handler_destroy(obj);
}

void MenuBarUnused4A9230(struct menu_bar *self, int32_t arg)
{
	(void) self;
	(void) arg;
}

void MenuBarEnableItem(struct menu_bar *self, uint32_t id, bool enable)
{
	struct menu *out_menu = NULL;
	int32_t out_index;
	struct menu_item *item = MenuBarFindItem(self, (int32_t) id, &out_menu, &out_index);

	if (!item)
		return;

	item->enabled = enable;
	EnableMenuItem((HMENU) g_hmnu, id, enable ? MF_ENABLED : MF_GRAYED);
}

void MenuBarCheckItem(struct menu_bar *self, uint32_t id, bool checked)
{
	struct menu *out_menu = NULL;
	int32_t out_index;
	struct menu_item *item = MenuBarFindItem(self, (int32_t) id, &out_menu, &out_index);

	if (!item) {
		LogError(LOG_ERROR, "Attempt to check/uncheck non-existent menu item");
		return;
	}

	item->check_state = checked;
	CheckMenuItem((HMENU) g_hmnu, id, checked ? MF_CHECKED : MF_UNCHECKED);
}

struct menu_item *MenuBarFindItem(struct menu_bar *self, int32_t id, struct menu **out_menu, int32_t *out_index)
{
	return MenuFindItem(self->menu, id, out_menu, out_index);
}

void menu_bar_on_event(struct evt_handler *self, struct cmd_event *event)
{
	(void) self;
	(void) event;
}

void MenuBarDeleteItem(struct menu_bar *self, int32_t id)
{
	struct menu *out_menu = NULL;
	int32_t out_index;
	struct menu_item *found = MenuBarFindItem(self, id, &out_menu, &out_index);

	if (found) {
		MenuRemoveItem(out_menu, out_index);

	} else {
		LogError(LOG_ERROR, "Attempt to delete non-existent menu item");
	}
}

void MenuBarSetText(struct menu_bar *self, int32_t id, struct cstr *text)
{
	struct menu *out_menu = NULL;
	int32_t out_index;
	struct menu_item *found = MenuBarFindItem(self, id, &out_menu, &out_index);

	if (!found) {
		LogError(LOG_ERROR, "Attempt to set text of non-existent menu item");
		return;
	}

	ModifyMenuA((HMENU) g_hmnu, (UINT) id, MF_BYCOMMAND | MF_STRING, 0, CStrGet(text));
}

void *MenuBarGetHandle(void)
{
	return g_hmnu;
}

void MenuBarBuild(struct menu_bar *self)
{
	HMENU popup = NULL;
	g_hmnu = CreateMenu();

	int32_t count = ObarrayCount(self->menu->items);
	struct cstr *label = CStrCreate();

	for (int32_t index = 0; index < count; index++) {
		popup = MenuBuildPopup(self->menu, index);
		struct menu_item *item = ObarrayGetAt(self->menu->items, index);
		CStrAssign(label, item->text);
		AppendMenuA((HMENU) g_hmnu, MF_POPUP, (UINT_PTR) popup, CStrGet(label));
	}

	CStrDestroy(&label);

	ForceSaveAddMenuItem(g_hmnu);
	PlayIntroAddMenuItem(g_hmnu);

	if (count > 0 && popup) {
		AppendMenuA(popup, MF_SEPARATOR, 0, NULL);
		AppendMenuA(popup, MF_STRING | (AutosaveEnabled() ? MF_CHECKED : 0), AUTOSAVE_MENU_ID, "Autosave");
#ifdef OTII_AUTO_MODE
		AppendMenuA(popup, MF_STRING, AUTOPILOT_MENU_ID, "Autopilot");
		AutoPilotAppendMenu(popup);
#endif
#ifdef OTII_TWITCH
		AppendMenuA(popup, MF_STRING, TWITCH_OPTIONS_MENU_ID, "Twitch Chat Options...");
#endif
		AppendMenuA(popup, MF_STRING, DEBUG_SCREEN_MENU_ID, "Debug State...");
		AppendMenuA(popup, MF_SEPARATOR, 0, NULL);
		AppendMenuA(popup, MF_STRING, DISCORD_MENU_ID, "Join the Discord...");
	}
#ifdef CROWD_CONTROL
	AppendMenuA((HMENU) g_hmnu, MF_STRING, CROWD_CONTROL_MENU_ID, "Crowd &Control");
#endif
}

void MenuBarUnused4A95C7(struct menu_bar *self, int32_t arg)
{
	(void) self;
	(void) arg;
}
