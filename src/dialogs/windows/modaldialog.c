#include "../modaldialog.h"

#include <stddef.h>

#include <windows.h>

#include "../../app/app.h"
#include "../../base/cstr.h"
#include "../../ui/dlgwin.h"
#include "../../base/memory.h"
#include "../../ui/view.h"
#include "../../ui/window.h"

struct modal_dialog *g_running_dialog;

const struct modal_dialog_ops MODAL_DIALOG_OPS_TABLE = {
	{
		{modal_dialog_destroy},
		evt_handler_is_handled,
		evt_handler_set_handled_for,
		evt_handler_set_handled,
		evt_handler_clear_handled_for,
		evt_handler_clear_handled,
		evt_handler_on_event,
		evt_handler_on_idle,
		evt_handler_on_idle_spare,
	},
	modal_dialog_begin,
	modal_dialog_accept,
};

void ModalDialogInit(struct modal_dialog *self, int32_t id, bool flag22, bool redraw_parent)
{
	EvtHandlerInit(&self->base, 0x4E25, id);
	self->base.base.ops = &MODAL_DIALOG_OPS_TABLE.base.base;

	self->content = NULL;
	self->title = NULL;
	self->dlg_win = NULL;
	self->flag_22 = flag22;
	self->redraw_parent = redraw_parent;
}

void modal_dialog_destroy(struct object *obj)
{
	struct modal_dialog *self = (struct modal_dialog *) obj;

	if (self->content)
		ObjectDelete(self->content);

	if (self->dlg_win)
		ObjectDelete(self->dlg_win);

	CStrDestroy(&self->title);

	obj->ops = &EVT_HANDLER_OPS_TABLE.base;
	evt_handler_destroy(obj);
}

int32_t ModalDialogRun(struct modal_dialog *self, intptr_t param, struct window *parent)
{
	struct app *app = g_app;
	self->result = DLG_RUNNING;

	struct cstr *title_copy = self->title ? CStrCreateCopy(self->title) : CStrCreate();
	self->dlg_win = DlgWinCreate(title_copy, self->content, parent, self->flag_22);

	if (!self->dlg_win)
		return DLG_CANCEL;

	struct modal_dialog *outer_dialog = g_running_dialog;
	g_running_dialog = self;

	struct window *win = &self->dlg_win->base;
	MemFree(win->trigger_event);
	win->trigger_event = MemDup(&self->base.eh_event, sizeof(struct event));

	HMENU sys_menu = GetSystemMenu((HWND) win->hwnd, FALSE);
	DeleteMenu(sys_menu, SC_SIZE, MF_BYCOMMAND);
	DeleteMenu(sys_menu, SC_MINIMIZE, MF_BYCOMMAND);
	DeleteMenu(sys_menu, SC_MAXIMIZE, MF_BYCOMMAND);
	DeleteMenu(sys_menu, SC_RESTORE, MF_BYCOMMAND);

	MODAL_DIALOG_OPS(self)->Begin(self, param);
	WindowBringToFront(win);
	WindowCenterMethod(win);
	APP_OPS(app)->UpdateEditMenuState(app, 0);

	do {
		APP_OPS(g_app)->PumpTick(g_app);

		if (g_quit_requested)
			self->result = DLG_CANCEL;

	} while (self->result == DLG_RUNNING);

	if (self->result != DLG_CANCEL)
		MODAL_DIALOG_OPS(self)->Accept(self, param);

	g_running_dialog = outer_dialog;
	WindowSetRootView(win, NULL);
	DlgWinDestroy(&self->dlg_win);

	if (parent && self->redraw_parent) {
		WINDOW_OPS(parent)->Draw(parent, NULL);
		WindowValidate(parent, NULL);
	}

	APP_OPS(app)->UpdateEditMenuState(app, 0);

	return self->result;
}

void ModalDialogTrimSystemMenu(struct modal_dialog *self)
{
	HMENU sys_menu = GetSystemMenu((HWND) self->dlg_win->base.hwnd, FALSE);
	DeleteMenu(sys_menu, SC_SIZE, MF_BYCOMMAND);
	DeleteMenu(sys_menu, SC_MINIMIZE, MF_BYCOMMAND);
	DeleteMenu(sys_menu, SC_MAXIMIZE, MF_BYCOMMAND);
	DeleteMenu(sys_menu, SC_RESTORE, MF_BYCOMMAND);
}

void modal_dialog_accept(struct modal_dialog *self, intptr_t param)
{
	(void) self;
	(void) param;
}

void modal_dialog_begin(struct modal_dialog *self, intptr_t param)
{
	(void) self;
	(void) param;
}
