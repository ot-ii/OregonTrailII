#include "glossarydlg.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "alert.h"
#include "../ui/button.h"
#include "../ui/control.h"
#include "../base/cstr.h"
#include "../ui/edit.h"
#include "../app/event.h"
#include "../base/memory.h"
#include "../draw/region.h"
#include "../data/strrsrc.h"
#include "../ui/view.h"

static struct cstr *g_glossary_last_search;
static int32_t g_glossary_table = -1;

const struct glossary_dlg_ops GLOSSARY_DLG_OPS_TABLE = {
	{
		{
			{glossary_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			glossary_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		glossary_dlg_begin,
		glossary_dlg_accept,
	},
};

static struct cstr *last_search(void)
{
	if (!g_glossary_last_search)
		g_glossary_last_search = CStrCreate();

	return g_glossary_last_search;
}

static void seed_rect(struct rect *r, int32_t top, int32_t left, int32_t bottom, int32_t right)
{
	r->top = top;
	r->left = left;
	r->right = right;
	r->bottom = bottom;
}

void GlossaryDlgInit(struct glossary_dlg *self, struct assets *owner)
{
	ModalDialogInit(&self->base, -2, false, true);
	self->base.base.base.ops = &GLOSSARY_DLG_OPS_TABLE.base.base.base;

	struct rect rect;
	struct cstr *label_text;
	int32_t handlers[3];
	struct view *child;
	struct rect bounds;
	const struct event *ev = &self->base.base.eh_event;

	self->owner = owner;
	self->base.title = CStrCreateFrom("Find Glossary Entry");

	rect.top = 0;
	rect.left = 0;
	rect.right = 220;
	rect.bottom = 70;
	self->base.content = ViewCreateId(0, &rect);
	struct view *content = self->base.content;
	bounds = rect;
	seed_rect(&rect, bounds.top + 8, bounds.left + 3, bounds.top + 33, bounds.right - 3);
	child = (struct view *) EditCreate(1, ev, &rect, CStrCreateCopy(last_search()), false, true, 0, 3, 0, false,
		false, 0, false);
	VIEW_OPS(content)->AddChild(content, child);

	seed_rect(&rect, bounds.bottom - 25, bounds.right - 80, bounds.bottom - 5, bounds.right - 15);
	label_text = CStrCreateFrom("OK");
	child = (struct view *) DefButtonCreate(2, ev, &rect, label_text, 0, 0);

	struct control *ctrl = (struct control *) child;
	ctrl->is_default = true;
	RegionDestroy(&ctrl->base.region_bounds);
	struct rect ctrl_bounds = VIEW_OPS(ctrl)->GetBounds(&ctrl->base);
	ctrl->base.region_bounds = RegionCreateRect(&ctrl_bounds, 1, 0, 0);
	VIEW_OPS(content)->AddChild(content, child);

	seed_rect(&rect, bounds.bottom - 25, bounds.left + 15, bounds.bottom - 5, bounds.left + 80);
	label_text = CStrCreateFrom("Cancel");
	child = (struct view *) DefButtonCreate(3, ev, &rect, label_text, 0, 0);
	VIEW_OPS(content)->AddChild(content, child);

	handlers[0] = CMD_CONTROL_ACTION;
	handlers[1] = CMD_CLOSE_WINDOW;
	handlers[2] = CMD_EDIT_COMMITTED;
	EvtHandlerBindHandler(&self->base.base, 3, handlers);
}

struct glossary_dlg *GlossaryDlgCreate(struct assets *owner)
{
	struct glossary_dlg *self = MemAlloc(1, sizeof(struct glossary_dlg));
	GlossaryDlgInit(self, owner);

	return self;
}

void GlossaryDlgDestroy(struct glossary_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void glossary_dlg_destroy(struct object *obj)
{
	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

void glossary_dlg_accept(struct modal_dialog *self, intptr_t out)
{
	struct glossary_dlg *dlg = (struct glossary_dlg *) self;
	bool found = false;
	int32_t found_index = 0;
	int32_t *out_index = (int32_t *) out;
	struct strrsrc *glossary = StrRsrcLoad(dlg->owner, GLOSSARY_STRLST, true);
	struct edit *edit_view = (struct edit *) ViewFindById(self->content, 1);
	struct cstr *typed_text = EditGetText(edit_view);

	if (CStrLength(typed_text)) {
		if (!CStrEqual(typed_text, last_search()))
			g_glossary_table = -1;

		CStrAssign(g_glossary_last_search, typed_text);
		CStrToUpper(typed_text);
		const char *needle = CStrGet(typed_text);

		for (int32_t entry_index = g_glossary_table + 1; !found && StrRsrcCount(glossary) > entry_index;
			++entry_index) {
			struct cstr *entry_text = StrRsrcGetCStr(glossary, entry_index);
			CStrToUpper(entry_text);

			if (strstr(CStrGet(entry_text), needle)) {
				found_index = entry_index;
				found = true;
			}

			CStrDestroy(&entry_text);
		}

		if (!found) {
			g_glossary_table = -1;

			for (int32_t retry_index = g_glossary_table + 1; !found && StrRsrcCount(glossary) > retry_index;
				++retry_index) {
				struct cstr *entry_text = StrRsrcGetCStr(glossary, retry_index);
				CStrToUpper(entry_text);

				if (strstr(CStrGet(entry_text), needle)) {
					found_index = retry_index;
					found = true;
				}

				CStrDestroy(&entry_text);
			}
		}

		CStrDestroy(&typed_text);
		StrRsrcDestroy(&glossary);

		if (found) {
			*out_index = found_index + 3;
			g_glossary_table = found_index;

		} else {
			struct cstr *message = CStrCreateFrom("There is no entry in the Glossary for '");
			CStrAppend(message, g_glossary_last_search);
			CStrAppendBuf(message, ".'");
			AlertShow(g_alert_handler, 1, message, 0, 1);
			CStrDestroy(&message);
			self->result = DLG_CANCEL;
			g_glossary_table = -1;
		}

	} else {
		CStrDestroy(&typed_text);
		StrRsrcDestroy(&glossary);
		self->result = DLG_CANCEL;
		g_glossary_table = -1;
	}
}

void glossary_dlg_begin(struct modal_dialog *self, intptr_t param)
{
	(void) param;
	ModalDialogTrimSystemMenu(self);
}

void glossary_dlg_on_event(struct evt_handler *self, struct cmd_event *evt)
{
	struct modal_dialog *dlg = (struct modal_dialog *) self;
	int32_t cmd = evt->spec_b.id;

	if (cmd == 2) {
		dlg->result = DLG_OK;
		evt->handled = true;

	} else if (cmd == 3) {
		dlg->result = DLG_CANCEL;
		evt->handled = true;

	} else {
		if (evt->command == CMD_EDIT_COMMITTED) {
			evt->handled = true;

		} else {
			if (evt->command == CMD_CLOSE_WINDOW) {
				dlg->result = DLG_CANCEL;
				evt->handled = true;
			}
		}
	}
}
