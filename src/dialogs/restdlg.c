#include "restdlg.h"

#include <stdbool.h>
#include <stddef.h>

#include "../ui/cliplabelview.h"
#include "../ui/combobox.h"
#include "../ui/control.h"
#include "../base/cstr.h"
#include "datelistbuilder.h"
#include "../app/event.h"
#include "../base/memory.h"
#include "../ui/pictctrl.h"
#include "../ui/picview.h"
#include "../base/str.h"
#include "../data/strrsrc.h"
#include "../ui/view.h"

static struct option_entry g_start_town_options[7] = {
	{"Independence", 1840, 1860, 1, 1},
	{"St. Joseph", 1843, 1860, 267, 2},
	{"Nauvoo", 1846, 1860, 289, 3},
	{"Kanesville ", 1846, 1846, 312, 4},
	{"Kanesville", 1847, 1853, 313, 4},
	{"Council Bluffs", 1854, 1860, 315, 4},
	{NULL, 0, 0, 0, 0},
};

struct option_entry g_destination_options[8] = {
	{"the Willamette Valley", 1840, 1841, 0, 1},
	{"Oregon City", 1842, 1860, 1, 1},
	{"the Sacramento Valley", 1840, 1847, 2, 2},
	{"Sacramento", 1848, 1860, 3, 2},
	{"Great Salt Lake City", 1847, 1860, 4, 3},
	{"the Rogue River Valley", 1846, 1851, 5, 4},
	{"Jacksonville, Oregon", 1852, 1860, 6, 4},
	{NULL, 0, 0, 0, 0},
};

struct date_list_builder {
	int32_t unused;
};

struct date_list_builder *DateListBuilderCreate(void)
{
	return MemAlloc(1, sizeof(struct date_list_builder));
}

void DateListBuilderDestroy(struct date_list_builder **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct date_list_builder *ctx = *ctx_mod;
	*ctx_mod = NULL;

	MemFree(ctx);
}

struct strrsrc *DateListBuilderBuildStartTownList(struct date_list_builder *ctx, int32_t ctx_year,
	const struct cstr *current, int32_t fallback, int32_t *sel_out)
{
	return DateListBuilderBuildFilteredComboList(ctx, ctx_year, current, fallback, sel_out, g_start_town_options);
}

struct strrsrc *DateListBuilderBuildDestinationList(struct date_list_builder *ctx, int32_t ctx_year,
	const struct cstr *current, int32_t fallback, int32_t *sel_out)
{
	return DateListBuilderBuildFilteredComboList(ctx, ctx_year, current, fallback, sel_out, g_destination_options);
}

int32_t DateListBuilderLookupStartTown(struct date_list_builder *ctx, const struct cstr *name)
{
	return DateListBuilderTableLookupField8(ctx, name, g_start_town_options);
}

int32_t DateListBuilderLookupDestination(struct date_list_builder *ctx, const struct cstr *name)
{
	return DateListBuilderTableLookupField8(ctx, name, g_destination_options);
}

struct strrsrc *DateListBuilderBuildFilteredComboList(struct date_list_builder *ctx, int32_t ctx_year,
	const struct cstr *current, int32_t fallback, int32_t *sel_out, const struct option_entry *table)
{
	int32_t want_code;
	struct strrsrc *options = StrRsrcCreate();
	*sel_out = 0;

	if (current)
		want_code = DateListBuilderTableLookupField10(ctx, current, table);
	else
		want_code = fallback;

	for (int32_t index = 0; table[index].name; index++) {
		if (table[index].min_ctx <= ctx_year && table[index].max_ctx >= ctx_year) {
			StrRsrcAppend(options, table[index].name);

			if (table[index].code10 == want_code)
				*sel_out = StrRsrcCount(options) - 1;
		}
	}

	return options;
}

int32_t DateListBuilderTableLookupField8(struct date_list_builder *ctx, const struct cstr *name,
	const struct option_entry *table)
{
	(void) ctx;
	int32_t code = 0;

	for (int32_t index = 0; table[index].name; index++) {
		if (CStrEqualBuf(name, table[index].name)) {
			code = table[index].code8;
			break;
		}
	}

	return code;
}

int32_t DateListBuilderTableLookupField10(struct date_list_builder *ctx, const struct cstr *name,
	const struct option_entry *table)
{
	(void) ctx;
	int32_t code = 0;

	for (int32_t index = 0; table[index].name; index++) {
		if (CStrEqualBuf(name, table[index].name)) {
			code = table[index].code10;
			break;
		}
	}

	return code;
}


const struct rest_dlg_ops REST_DLG_OPS_TABLE = {
	{
		{
			{rest_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			rest_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		rest_dlg_begin,
		rest_dlg_accept,
	},
};

void RestDlgInit(struct rest_dlg *self)
{
	ModalDialogInit(&self->base, 1, false, true);
	self->base.base.base.ops = &REST_DLG_OPS_TABLE.base.base.base;

	struct rect rect;
	struct point origin;
	struct strrsrc *days;
	struct view *child;
	struct view *content;
	char buf[32];
	int32_t handlers[2];
	const struct event *ev = &self->base.base.eh_event;

	self->base.title = CStrCreateFrom("Resting");

	origin.h = 0;
	origin.v = 0;
	self->base.content = (struct view *) PicViewCreatePoint(0, &origin, 2600);
	content = self->base.content;

	rect.top = 30;
	rect.left = 110;
	rect.right = 160;
	rect.bottom = 50;
	child = (struct view *) ClipLabelViewCreate(0, &rect, CStrCreateFrom("Rest for"), 3, 0, 1, 0, 0, 255, 0, 1);
	VIEW_OPS(content)->AddChild(content, child);

	days = StrRsrcCreate();

	for (int32_t day_index = 1; day_index <= 14; day_index++) {
		StrPrintf(buf, "%d", day_index);
		StrRsrcAppend(days, buf);
	}

	rect.top = 30;
	rect.left = 170;
	rect.right = 230;
	rect.bottom = 50;
	child = (struct view *) ComboBoxCreate(9001, ev, &rect, 230, days, 0);
	((struct combo_box *) child)->font_id = 3;
	ViewNotifyChanged(child);
	VIEW_OPS(content)->AddChild(content, child);

	rect.top = 30;
	rect.left = 240;
	rect.right = 280;
	rect.bottom = 50;
	child = (struct view *) ClipLabelViewCreate(0, &rect, CStrCreateFrom("days"), 3, 0, 1, 0, 0, 255, 0, 1);
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

	handlers[0] = CMD_CONTROL_ACTION;
	handlers[1] = CMD_CLOSE_WINDOW;
	EvtHandlerBindHandler(&self->base.base, 2, handlers);
}

struct rest_dlg *RestDlgCreate(void)
{
	struct rest_dlg *self = MemAlloc(1, sizeof(struct rest_dlg));
	RestDlgInit(self);

	return self;
}

void RestDlgDestroy(struct rest_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void rest_dlg_destroy(struct object *obj)
{
	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

void rest_dlg_begin(struct modal_dialog *self, intptr_t out)
{
	(void) self;
	*(int32_t *) out = 0;
}

void rest_dlg_accept(struct modal_dialog *self, intptr_t out)
{
	int32_t *out_hours = (int32_t *) out;
	struct control *combo = (struct control *) ViewFindById(self->content, 9001);
	*out_hours = (combo->value + 1) * 24;
}

void rest_dlg_on_event(struct evt_handler *self, struct cmd_event *evt)
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

	} else {
		if (evt->command == CMD_CLOSE_WINDOW)
			dlg->result = DLG_CANCEL;
	}
}
