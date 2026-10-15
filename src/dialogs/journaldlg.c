#include "journaldlg.h"

#include <stddef.h>

#include "../data/assets.h"
#include "../ui/button.h"
#include "../ui/cliplabelview.h"
#include "../ui/control.h"
#include "../base/cstr.h"
#include "../ui/cursormgr.h"
#include "../base/date.h"
#include "../ui/dlgwin.h"
#include "../ui/edit.h"
#include "../app/event.h"
#include "../data/gfxdev.h"
#include "../game/journal.h"
#include "../base/list.h"
#include "../base/memory.h"
#include "../app/otiiapp.h"
#include "../ui/picview.h"
#include "../draw/screendc.h"
#include "../draw/statefuldispobj.h"
#include "../base/str.h"
#include "../data/strrsrc.h"
#include "../draw/text.h"
#include "../draw/textlabel.h"
#include "../ui/textview.h"
#include "../ui/view.h"
#include "../draw/windc.h"
#include "../ui/window.h"

const struct journal_dlg_ops JOURNAL_DLG_OPS_TABLE = {
	{
		{
			{journal_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			journal_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		journal_dlg_begin,
		journal_dlg_accept,
	},
};

static void move_rect(struct rect *r, int32_t left, int32_t top)
{
	int32_t width = r->right - r->left;
	int32_t height = r->bottom - r->top;
	r->top = top;
	r->left = left;
	r->right = left + width;
	r->bottom = top + height;
}

void JournalDlgInit(struct journal_dlg *self)
{
	ModalDialogInit(&self->base, 1225, true, true);
	self->base.base.base.ops = &JOURNAL_DLG_OPS_TABLE.base.base.base;

	int32_t handlers[2];

	handlers[0] = CMD_CONTROL_ACTION;
	handlers[1] = CMD_CLOSE_WINDOW;

	struct rect rect;
	struct point origin = {0, 0};
	struct view *child;
	const struct event *ev = &self->base.base.eh_event;

	self->text_page = 0;
	self->text_more = false;
	self->base.title = CStrCreateFrom("Diary");
	self->base.content = (struct view *) PicViewCreatePoint(0, &origin, 600);
	struct view *content = self->base.content;
	rect = VIEW_OPS(content)->GetBounds(content);
	int32_t top0 = 0;
	int32_t left0 = 0;
	int32_t page_right = rect.right;
	int32_t page_bottom = rect.bottom;
	int32_t mid_x = (page_right - left0) / 2 + left0;

	rect.top = top0 + 20;
	rect.left = mid_x + 25;
	rect.right = page_right - 15;
	rect.bottom = page_bottom - 50;
	child = (struct view *) EditCreate(DIARY_NOTES, ev, &rect, NULL, true, false, 0, 4, 0, false, false, 0, false);
	VIEW_OPS(content)->AddChild(content, child);

	rect.top = top0 + 20;
	rect.left = left0 + 20;
	rect.right = mid_x - 20;
	rect.bottom = top0 + 40;
	child = (struct view *) ClipLabelViewCreate(DIARY_DATE, &rect, NULL, 6, 1, 1, 0, 0, 255, 0, 1);
	VIEW_OPS(content)->AddChild(content, child);

	rect.top = top0 + 60;
	rect.left = left0 + 20;
	rect.right = mid_x - 20;
	rect.bottom = page_bottom - 50;
	child = (struct view *) TextViewCreate(DIARY_TEXT, &rect, NULL, 4, 0, 0, 0, 0, 0, 0, 255, 0, 1);
	VIEW_OPS(content)->AddChild(content, child);

	struct stateful_disp_obj *button_art = StatefulDispObjCreate(602, NULL, &origin, 0, 0, 0);
	rect = TEXT_LABEL_OPS(button_art)->GetBounds((struct text_label *) button_art);
	move_rect(&rect, page_right - (rect.right - rect.left), page_bottom - (rect.bottom - rect.top));
	child = (struct view *) ButtonCreateDispObj(DIARY_NEXT, ev, (struct disp_obj *) button_art, &rect, 0, 0);
	VIEW_OPS(child)->SetCursorId(child, CURSOR_RIGHT);
	VIEW_OPS(content)->AddChild(content, child);

	button_art = StatefulDispObjCreate(601, NULL, &origin, 0, 0, 0);
	rect = TEXT_LABEL_OPS(button_art)->GetBounds((struct text_label *) button_art);
	move_rect(&rect, 0, page_bottom - (rect.bottom - rect.top));
	child = (struct view *) ButtonCreateDispObj(DIARY_PREV, ev, (struct disp_obj *) button_art, &rect, 0, 0);
	VIEW_OPS(child)->SetCursorId(child, CURSOR_LEFT);
	VIEW_OPS(content)->AddChild(content, child);

	EvtHandlerBindHandler(&self->base.base, 2, handlers);
}

struct journal_dlg *JournalDlgCreate(void)
{
	struct journal_dlg *self = MemAlloc(1, sizeof(struct journal_dlg));
	JournalDlgInit(self);

	return self;
}

void JournalDlgDestroy(struct journal_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void journal_dlg_destroy(struct object *obj)
{
	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

void journal_dlg_accept(struct modal_dialog *self, intptr_t journal)
{
	if (!journal)
		return;

	struct edit *ctrl = (struct edit *) WindowFindCtrlById(&self->dlg_win->base, DIARY_NOTES);
	struct cstr *text = EditGetText(ctrl);
	JournalSetEntryText((struct journal *) journal, text);
	CStrDestroy(&text);
}

void journal_dlg_begin(struct modal_dialog *self, intptr_t param)
{
	struct journal_dlg *dlg = (struct journal_dlg *) self;

	dlg->page = (struct journal *) param;
	dlg->text_page = 0;
	JournalDlgRefresh(dlg);
}

void journal_dlg_on_event(struct evt_handler *self, struct cmd_event *evt)
{
	struct journal_dlg *dlg = (struct journal_dlg *) self;

	if (evt->command == CMD_CONTROL_ACTION) {
		int32_t ctrl_id = evt->spec_b.id;

		switch (ctrl_id) {
			case DIARY_PREV:
				OtiiAppPlaySfx((struct otii_app *) g_app, 27015, false);

				if (dlg->text_page > 0) {
					--dlg->text_page;

				} else {
					MODAL_DIALOG_OPS(dlg)->Accept(&dlg->base, (intptr_t) dlg->page);
					JournalPrevEntry(dlg->page);
					dlg->text_page = 0x7FFF;
				}

				JournalDlgRefresh(dlg);
				break;
			case DIARY_NEXT:
				OtiiAppPlaySfx((struct otii_app *) g_app, 27015, false);

				if (dlg->text_more) {
					++dlg->text_page;

				} else {
					MODAL_DIALOG_OPS(dlg)->Accept(&dlg->base, (intptr_t) dlg->page);
					JournalNextEntry(dlg->page);
					dlg->text_page = 0;
				}

				JournalDlgRefresh(dlg);
				break;
			case DIARY_DONE:
				dlg->base.result = DLG_OK;
				break;
		}

		evt->handled = true;

	} else {
		if (evt->command == CMD_CLOSE_WINDOW) {
			dlg->base.result = DLG_OK;
			evt->handled = true;
		}
	}
}

static bool child_matches(const void *item, const void *key)
{
	const struct evt_handler *h = item;

	return EventMatches(key, &h->eh_event);
}

static struct cstr *month_name(const struct date *date)
{
	struct strrsrc *months = StrRsrcLoad(g_text_lib, 50, true);

	if (!months)
		return CStrCreate();

	struct cstr *name = StrRsrcGetCStr(months, date->month - 1);
	StrRsrcDestroy(&months);

	return name;
}

void JournalDlgRefresh(struct journal_dlg *self)
{
	struct event spec = EventMake(EVCLS_ANY, -1, -1);
	char buf[52];

	if (!self->page)
		return;

	struct date date = *JournalGetDate(self->page);
	struct obarray *children = ViewGetChildList(self->base.content);

	EventSetId(&spec, DIARY_TEXT);
	struct view *child = ObarrayFind(children, child_matches, &spec);
	{
		struct text *text_obj = (struct text *) ((struct clip_view *) child)->shape;
		struct strrsrc *lines = StrRsrcCreateCopy(JournalGetEntryLines(self->page));
		text_obj->paged = true;
		text_obj->page = self->text_page;
		TextViewSetText((struct text_view *) child, lines);

		if (self->base.dlg_win && self->base.dlg_win->base.windc)
			TextWrapLines(text_obj, &self->base.dlg_win->base.windc->base);

		self->text_page = text_obj->page;
		self->text_more = text_obj->page < text_obj->page_count - 1;
	}
	ViewNotifyChanged(child);

	struct cstr *month = month_name(&date);
	EventSetId(&spec, DIARY_DATE);
	child = ObarrayFind(children, child_matches, &spec);
	StrPrintf(buf, "%s %d, %d%s", CStrGet(month), date.day, date.year, self->text_page > 0 ? " (cont.)" : "");
	CStrDestroy(&month);
	struct cstr *text = CStrCreateFrom(buf);
	ClipLabelViewSetLabelAttr((struct clip_label_view *) child, text);

	EventSetId(&spec, DIARY_NOTES);
	child = ObarrayFind(children, child_matches, &spec);
	text = CStrCreateCopy(JournalGetEntryText(self->page));
	EditSetText((struct edit *) child, text);
	ViewNotifyChanged(child);

	EventSetId(&spec, DIARY_NEXT);
	child = ObarrayFind(children, child_matches, &spec);

	if (!OblistNext(self->page->entries, false) && !self->text_more)
		VIEW_OPS(child)->Hide(child, false, false);
	else
		VIEW_OPS(child)->Show(child);

	EventSetId(&spec, DIARY_PREV);
	child = ObarrayFind(children, child_matches, &spec);

	if (!OblistPrev(self->page->entries, false) && self->text_page == 0)
		VIEW_OPS(child)->Hide(child, false, false);
	else
		VIEW_OPS(child)->Show(child);
}
