#include "speeddlg.h"

#include <stddef.h>

#include "../ui/cliplabelview.h"
#include "../ui/combobox.h"
#include "../ui/control.h"
#include "../base/cstr.h"
#include "../app/event.h"
#include "../base/memory.h"
#include "../ui/pictctrl.h"
#include "../ui/picview.h"
#include "../app/settings.h"
#include "../data/strrsrc.h"
#include "../ui/view.h"

const struct speed_dlg_ops SPEED_DLG_OPS_TABLE = {
	{
		{
			{speed_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			speed_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		speed_dlg_begin,
		speed_dlg_accept,
	},
};

void SpeedDlgInit(struct speed_dlg *self)
{
	ModalDialogInit(&self->base, -2, false, true);
	self->base.base.base.ops = &SPEED_DLG_OPS_TABLE.base.base.base;

	struct rect rect;
	struct point origin;
	struct view *child;
	struct strrsrc *items;
	struct view *content;
	const struct event *ev = &self->base.base.eh_event;

	self->base.title = CStrCreateFrom("Simulation Speed");

	origin.h = 0;
	origin.v = 0;
	self->base.content = (struct view *) PicViewCreatePoint(0, &origin, 2600);
	content = self->base.content;

	rect.top = 30;
	rect.left = 110;
	rect.right = 160;
	rect.bottom = 50;
	child = (struct view *) ClipLabelViewCreate(0, &rect, CStrCreateFrom(" Speed"), 3, 0, 1, 0, 0, 255, 0, 1);
	VIEW_OPS(content)->AddChild(content, child);

	items = StrRsrcCreate();
	StrRsrcAppend(items, "Slow");
	StrRsrcAppend(items, "Medium");
	StrRsrcAppend(items, "Fast");
	rect.top = 30;
	rect.left = 170;
	rect.right = 270;
	rect.bottom = 50;
	child = (struct view *) ComboBoxCreate(9018, ev, &rect, 60, items, 0);
	((struct combo_box *) child)->font_id = 3;
	ViewNotifyChanged(child);
	VIEW_OPS(content)->AddChild(content, child);

	rect.top = 150;
	rect.left = 120;
	rect.right = 180;
	rect.bottom = 170;
	origin.v = rect.top;
	origin.h = rect.left;
	child = (struct view *) PictCtrlCreate(1, ev, &origin, 2102, 0, 0, CStrCreateFrom("OK"), 3, 1, 1);
	VIEW_OPS(content)->AddChild(content, child);

	rect.top = 150;
	rect.left = 220;
	rect.right = 280;
	rect.bottom = 170;
	origin.v = rect.top;
	origin.h = rect.left;
	child = (struct view *) PictCtrlCreate(2, ev, &origin, 2102, 0, 0, CStrCreateFrom("Cancel"), 3, 1, 1);
	VIEW_OPS(content)->AddChild(content, child);

	int32_t handlers[1];
	handlers[0] = CMD_CONTROL_ACTION;
	EvtHandlerBindHandler(&self->base.base, 1, handlers);
}

struct speed_dlg *SpeedDlgCreate(void)
{
	struct speed_dlg *self = MemAlloc(1, sizeof(struct speed_dlg));
	SpeedDlgInit(self);

	return self;
}

void SpeedDlgDestroy(struct speed_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void speed_dlg_destroy(struct object *obj)
{
	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

void speed_dlg_accept(struct modal_dialog *self, intptr_t s)
{
	struct settings *settings = (struct settings *) s;

	if (!settings)
		return;

	struct control *combo = (struct control *) ViewFindById(self->content, 9018);
	settings->speed = combo->value;
	SettingsSave(settings);
}

void speed_dlg_begin(struct modal_dialog *self, intptr_t s)
{
	struct settings *settings = (struct settings *) s;

	if (!settings)
		return;

	struct control *combo = (struct control *) ViewFindById(self->content, 9018);
	CONTROL_OPS(combo)->SetValue(combo, settings->speed, false);
}

void speed_dlg_on_event(struct evt_handler *self, struct cmd_event *evt)
{
	struct modal_dialog *dlg = (struct modal_dialog *) self;
	int32_t sel = evt->spec_b.id;

	if (evt->command == CMD_CONTROL_ACTION) {
		switch (sel) {
			case 1:
				dlg->result = DLG_OK;
				break;
			case 2:
				dlg->result = DLG_CANCEL;
				break;
		}

		evt->handled = true;
	}
}
