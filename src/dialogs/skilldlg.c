#include "skilldlg.h"

#include <stddef.h>
#include <stdlib.h>

#include "../data/assets.h"
#include "../ui/cliplabelview.h"
#include "../ui/control.h"
#include "../base/cstr.h"
#include "../app/event.h"
#include "../data/gfxdev.h"
#include "../base/list.h"
#include "../base/memory.h"
#include "../game/occupmgr.h"
#include "../ui/pictctrl.h"
#include "../ui/picview.h"
#include "../base/str.h"
#include "../data/strrsrc.h"
#include "../ui/textview.h"
#include "../ui/uicontainer.h"
#include "../ui/view.h"

const struct skill_dlg_ops SKILL_DLG_OPS_TABLE = {
	{
		{
			{skill_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			skill_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		skill_dlg_begin,
		skill_dlg_accept,
	},
};

void SkillDlgInit(struct skill_dlg *self, struct occup_mgr *body)
{
	ModalDialogInit(&self->base, 49, false, true);
	self->base.base.base.ops = &SKILL_DLG_OPS_TABLE.base.base.base;
	self->body = body;

	char label_buf[64];
	struct point origin;
	int32_t handlers[2];
	struct rect rect;
	struct view *child;
	struct strrsrc *text_rsrc;
	const struct event *ev = &self->base.base.eh_event;

	self->owned = StrRsrcLoad(g_text_lib, 2213, true);
	self->base.title = CStrCreateFrom("Skills");

	origin.h = 0;
	origin.v = 0;
	self->base.content = (struct view *) PicViewCreatePoint(0, &origin, 194);
	struct view *content = self->base.content;

	rect.top = 65;
	rect.left = 20;
	rect.right = 590;
	rect.bottom = 155;
	text_rsrc = StrRsrcLoad(g_text_lib, 2210, true);
	StrRsrcRemapHighChars(text_rsrc, 0);
	child = (struct view *) TextViewCreateStrs(0, &rect, text_rsrc, 3, 0, 0, 0, 0, 0, 0, 255, 0, 1);
	VIEW_OPS(content)->AddChild(content, child);

	text_rsrc = StrRsrcLoad(g_text_lib, 2205, true);
	struct strrsrc *names = self->body->skill_names;
	int32_t skill_count = StrRsrcCount(names);

	if (skill_count) {
		for (int32_t skill_index = 0; skill_index < skill_count; skill_index++) {
			struct rect r;
			r.top = 20 * (skill_index % 5) + 160;
			r.left = 185 * (skill_index / 5) + 20;
			r.bottom = 20 * (skill_index % 5) + 180;
			r.right = 185 * (skill_index / 5) + 180;
			const char *skill_name = StrRsrcGetAt(names, skill_index);
			const char *cost_text = StrRsrcGetAt(text_rsrc, skill_index);
			StrPrintf(label_buf, "%s (%d)", skill_name, atoi(cost_text));
			child = (struct view *) UiContainerCreate(skill_index + 50, ev, &r, 2202, 2, 0, CStrCreateFrom(label_buf),
				3, 5, 0);
			VIEW_OPS(content)->AddChild(content, child);
		}
	}

	StrRsrcDestroy(&text_rsrc);

	rect.top = 260;
	rect.left = 20;
	rect.right = 590;
	rect.bottom = 280;
	child = (struct view *) ClipLabelViewCreate(100, &rect, NULL, 3, 0, 1, 0, 0, 255, 0, 1);
	VIEW_OPS(content)->AddChild(content, child);

	rect.top = 290;
	rect.left = 250;
	rect.right = 315;
	rect.bottom = 310;
	child = (struct view *) ClipLabelViewCreate(0, &rect, CStrCreateFrom("Points left:"), 3, 0, 1, 0, 0, 255, 0, 1);
	VIEW_OPS(content)->AddChild(content, child);

	rect.top = 290;
	rect.left = 320;
	rect.right = 350;
	rect.bottom = 310;
	child = (struct view *) ClipLabelViewCreate(12, &rect, NULL, 3, 0, 1, 0, 0, 255, 0, 1);
	VIEW_OPS(content)->AddChild(content, child);

	rect.top = 290;
	rect.left = 130;
	rect.right = 210;
	rect.bottom = 310;
	origin.v = rect.top;
	origin.h = rect.left;
	child = (struct view *) PictCtrlCreate(1, ev, &origin, 2102, 0, 0, CStrCreateFrom("OK"), 3, 1, 1);
	VIEW_OPS(content)->AddChild(content, child);

	rect.top = 290;
	rect.left = 390;
	rect.right = 470;
	rect.bottom = 310;
	origin.v = rect.top;
	origin.h = rect.left;
	child = (struct view *) PictCtrlCreate(2, ev, &origin, 2102, 0, 0, CStrCreateFrom("Cancel"), 3, 1, 1);
	VIEW_OPS(content)->AddChild(content, child);

	handlers[0] = CMD_CONTROL_ACTION;
	handlers[1] = CMD_CLOSE_WINDOW;
	EvtHandlerBindHandler(&self->base.base, 2, handlers);
}

struct skill_dlg *SkillDlgCreate(struct occup_mgr *body)
{
	struct skill_dlg *self = MemAlloc(1, sizeof(struct skill_dlg));
	SkillDlgInit(self, body);

	return self;
}

void SkillDlgDestroy(struct skill_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void skill_dlg_destroy(struct object *obj)
{
	struct skill_dlg *self = (struct skill_dlg *) obj;

	StrRsrcDestroy(&self->owned);

	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

void skill_dlg_begin(struct modal_dialog *self, intptr_t checks)
{
	struct skill_dlg *dlg = (struct skill_dlg *) self;

	dlg->points_left = 120;
	dlg->flags = *(const struct skill_flags *) checks;

	for (int32_t skill_index = 0; StrRsrcCount(dlg->body->skill_names) > skill_index; skill_index++) {
		struct control *ctrl = (struct control *) ViewFindById(self->content, skill_index + 50);

		if (dlg->flags.checked[skill_index]) {
			CONTROL_OPS(ctrl)->SetValue(ctrl, 1, false);

			if (dlg->flags.enabled[skill_index])
				EVT_HANDLER_OPS(ctrl)->ClearHandled((struct evt_handler *) ctrl);
			else
				dlg->points_left -= OccupMgrGetSkillCost(dlg->body, skill_index);
		}
	}

	SkillDlgUpdateTotal(dlg);
}

void skill_dlg_accept(struct modal_dialog *self, intptr_t out)
{
	*(struct skill_flags *) out = ((struct skill_dlg *) self)->flags;
}

void skill_dlg_on_event(struct evt_handler *self, struct cmd_event *evt)
{
	struct skill_dlg *dlg = (struct skill_dlg *) self;
	int32_t sel = evt->spec_b.id;

	if (evt->command == CMD_CONTROL_ACTION) {
		switch (sel) {
			case 1:
				dlg->base.result = DLG_OK;
				break;
			case 2:
				dlg->base.result = DLG_CANCEL;
				break;
			default:
				SkillDlgToggleOption(dlg, sel);
		}

		evt->handled = true;

	} else {
		if (evt->command == CMD_CLOSE_WINDOW) {
			dlg->base.result = DLG_OK;
			evt->handled = true;
		}
	}
}

void SkillDlgToggleOption(struct skill_dlg *self, int32_t id)
{
	int32_t skill_index = id - 50;
	int32_t old_points = self->points_left;
	int32_t cost = OccupMgrGetSkillCost(self->body, skill_index);
	struct control *ctrl = (struct control *) ViewFindById(self->base.content, id);

	if (ctrl->value && self->points_left - cost >= 0) {
		self->flags.checked[skill_index] = true;
		self->points_left -= cost;

	} else {
		if (self->flags.checked[skill_index] && !self->flags.enabled[skill_index]) {
			self->points_left += cost;
			self->flags.checked[skill_index] = false;
		}

		if (ctrl->value)
			CONTROL_OPS(ctrl)->SetValue(ctrl, 0, true);
	}

	struct clip_label_view *label = (struct clip_label_view *) ViewFindById(self->base.content, 100);

	if (ctrl->value)
		ClipLabelViewSetLabelAttr(label, StrRsrcGetCStr(self->owned, skill_index));
	else
		ClipLabelViewSetLabelAttr(label, CStrCreate());

	if (self->points_left != old_points)
		SkillDlgUpdateTotal(self);
}

static bool child_matches(const void *item, const void *key)
{
	const struct evt_handler *h = item;

	return EventMatches(key, &h->eh_event);
}

void SkillDlgUpdateTotal(struct skill_dlg *self)
{
	char buf[32];
	struct event find_key = EventMake(EVCLS_ANY, -1, -1);

	StrPrintf(buf, "%d", self->points_left);
	EventSetId(&find_key, 12);
	struct clip_label_view *label = ObarrayFind(ViewGetChildList(self->base.content), child_matches, &find_key);
	ClipLabelViewSetLabelAttr(label, CStrCreateFrom(buf));
}
