#include "pickerdlg.h"

#include <stddef.h>

#include "../data/assets.h"
#include "../features/autopilot.h"
#include "../ui/cliplabelview.h"
#include "../base/cstr.h"
#include "../base/date.h"
#include "../app/event.h"
#include "../game/game.h"
#include "../data/gfxdev.h"
#include "../base/memory.h"
#include "../ui/pictctrl.h"
#include "../ui/picview.h"
#include "../base/rng.h"
#include "../base/str.h"
#include "../data/strrsrc.h"
#include "../ui/textview.h"
#include "../ui/view.h"

const struct picker_dlg_ops PICKER_DLG_OPS_TABLE = {
	{
		{
			{picker_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			picker_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		picker_dlg_begin,
		picker_dlg_accept,
	},
};

void PickerDlgInit(struct picker_dlg *self)
{
	ModalDialogInit(&self->base, 1, false, true);
	self->base.base.base.ops = &PICKER_DLG_OPS_TABLE.base.base.base;

	struct point origin;
	struct rect rect;
	char text[64];
	int32_t handled_cmds[2];
	struct view *child;
	struct view *content;
	const struct event *ev = &self->base.base.eh_event;

	int32_t year = GetGameObj()->date->year;

	self->base.title = CStrCreateFrom("Wagon Train");

	origin.h = 0;
	origin.v = 0;
	self->base.content = (struct view *) PicViewCreatePoint(0, &origin, 2601);
	content = self->base.content;

	rect.top = 20;
	rect.left = 20;
	rect.right = 340;
	rect.bottom = 125;
	struct cstr *prompt = CStrCreateFrom("Before setting out, you need to pick a wagon train to travel with.  "
		"The following wagon trains are about ready to set out for ");
	struct strrsrc *place_names = StrRsrcLoad(g_text_lib, 901, true);
	struct cstr *destination = StrRsrcGetCStr(place_names, GetGameObj()->next_node_id);
	StrRsrcDestroy(&place_names);
	CStrAppendStr(prompt, destination);
	CStrAppendBuf(prompt, ".  Which do you want to go with?");
	CStrDestroy(&destination);
	child = (struct view *) TextViewCreate(4, &rect, prompt, 3, 0, 0, 0, 0, 0, 0, 255, 0, 1);
	VIEW_OPS(content)->AddChild(content, child);

	for (int32_t train_index = 0; train_index < 3; train_index++) {
		if (year < 1844)
			self->wagons[train_index] = RandRangeAvg(5, 15, 2);
		else
			self->wagons[train_index] = RandRangeAvg(5, 50, 1);

		self->people[train_index] = 0;

		for (int32_t wagon_index = 0; self->wagons[train_index] > wagon_index; wagon_index++)
			self->people[train_index] += RandRangeAvg(2, 5, 1);

		rect.top = 140;
		rect.left = 116 * train_index + 18;
		rect.right = 116 * train_index + 98;
		rect.bottom = 160;
		StrPrintf(text, "%d wagons", self->wagons[train_index]);
		child = (struct view *) ClipLabelViewCreate(train_index + 20, &rect, CStrCreateFrom(text), 3, 0, 1, 0, 0,
			255, 0, 1);
		VIEW_OPS(content)->AddChild(content, child);

		rect.top = 160;
		rect.left = 116 * train_index + 18;
		rect.right = 116 * train_index + 98;
		rect.bottom = 180;
		StrPrintf(text, "%d people", self->people[train_index]);
		child = (struct view *) ClipLabelViewCreate(train_index + 30, &rect, CStrCreateFrom(text), 3, 0, 1, 0, 0,
			255, 0, 1);
		VIEW_OPS(content)->AddChild(content, child);
	}

#ifdef OTII_AUTO_MODE
	const int32_t train_v = 188;
#else
	const int32_t train_v = 200;
#endif

	origin.h = 18;
	origin.v = train_v;
	child = (struct view *) PictCtrlCreate(1, ev, &origin, 2102, 0, 0, CStrCreateFrom("Train 1"), 3, 1, 1);
	VIEW_OPS(content)->AddChild(content, child);

	origin.h = 134;
	origin.v = train_v;
	child = (struct view *) PictCtrlCreate(2, ev, &origin, 2102, 0, 0, CStrCreateFrom("Train 2"), 3, 1, 1);
	VIEW_OPS(content)->AddChild(content, child);

	origin.h = 250;
	origin.v = train_v;
	child = (struct view *) PictCtrlCreate(3, ev, &origin, 2102, 0, 0, CStrCreateFrom("Train 3"), 3, 1, 1);
	VIEW_OPS(content)->AddChild(content, child);

#ifdef OTII_AUTO_MODE
	for (int32_t train_index = 0; train_index < 3; train_index++) {
		origin.h = 116 * train_index + 18;
		origin.v = 217;
		child = (struct view *) PictCtrlCreate(train_index + 11, ev, &origin, 2102, 0, 0, CStrCreateFrom("Auto"), 3, 1,
			1);
		VIEW_OPS(content)->AddChild(content, child);
	}
#endif

	handled_cmds[0] = CMD_CONTROL_ACTION;
	handled_cmds[1] = CMD_CLOSE_WINDOW;
	EvtHandlerBindHandler(&self->base.base, 2, handled_cmds);
}

struct picker_dlg *PickerDlgCreate(void)
{
	struct picker_dlg *self = MemAlloc(1, sizeof(struct picker_dlg));
	PickerDlgInit(self);

	return self;
}

void PickerDlgDestroy(struct picker_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void picker_dlg_destroy(struct object *obj)
{
	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

void picker_dlg_begin(struct modal_dialog *self, intptr_t param)
{
	(void) self;
	(void) param;
}

void picker_dlg_accept(struct modal_dialog *self, intptr_t out)
{
	struct picker_dlg *dlg = (struct picker_dlg *) self;
	int32_t *out_counts = (int32_t *) out;

	out_counts[0] = dlg->wagons[dlg->sel];
	out_counts[1] = dlg->people[dlg->sel];
}

void picker_dlg_on_event(struct evt_handler *self, struct cmd_event *evt)
{
	struct picker_dlg *dlg = (struct picker_dlg *) self;

	if (evt->command == CMD_CONTROL_ACTION) {
		dlg->sel = evt->spec_b.id - 1;
#ifdef OTII_AUTO_MODE
		if (evt->spec_b.id >= 11 && evt->spec_b.id <= 13) {
			dlg->sel = evt->spec_b.id - 11;
			AutoPilotStart();
		}
#endif

	} else {
		dlg->sel = 0;
	}

	dlg->base.result = DLG_OK;
	evt->handled = true;
}
