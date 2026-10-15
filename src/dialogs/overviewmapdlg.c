#include "overviewmapdlg.h"

#include <stddef.h>

#include "../base/cstr.h"
#include "../app/event.h"
#include "../trail/maparea.h"
#include "../base/memory.h"
#include "../ui/view.h"

bool g_overviewmap_open;

static const char g_overviewmap_title[] = "Overview Map";

const struct overview_map_dlg_ops OVERVIEW_MAP_DLG_OPS_TABLE = {
	{
		{
			{overview_map_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			overview_map_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		overview_map_dlg_begin,
		overview_map_dlg_accept,
	},
};

void OverviewMapDlgInit(struct overview_map_dlg *self, int32_t str_index)
{
	ModalDialogInit(&self->base, 7764, true, true);
	self->base.base.base.ops = &OVERVIEW_MAP_DLG_OPS_TABLE.base.base.base;

	struct point pt = {0, 0};
	int32_t handlers[2];

	handlers[0] = CMD_CONTROL_ACTION;
	handlers[1] = CMD_CLOSE_WINDOW;
	self->base.title = CStrCreateFrom(g_overviewmap_title);
	self->base.content = (struct view *) MapAreaCreate(1, &pt, 4075, &self->base.base.eh_event, 920, str_index);
	g_overviewmap_open = true;
	EvtHandlerBindHandler(&self->base.base, 2, handlers);
}

struct overview_map_dlg *OverviewMapDlgCreate(int32_t str_index)
{
	struct overview_map_dlg *self = MemAlloc(1, sizeof(struct overview_map_dlg));
	OverviewMapDlgInit(self, str_index);

	return self;
}

void OverviewMapDlgDestroy(struct overview_map_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void overview_map_dlg_destroy(struct object *obj)
{
	g_overviewmap_open = false;

	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

void overview_map_dlg_accept(struct modal_dialog *self, intptr_t param)
{
	(void) self;
	(void) param;
}

void overview_map_dlg_begin(struct modal_dialog *self, intptr_t param)
{
	(void) self;
	(void) param;
}

void overview_map_dlg_on_event(struct evt_handler *self, struct cmd_event *evt)
{
	switch (evt->command) {
		case CMD_CLOSE_WINDOW:
		case CMD_CONTROL_ACTION:
			((struct modal_dialog *) self)->result = DLG_OK;
			evt->handled = true;
			break;
	}
}
