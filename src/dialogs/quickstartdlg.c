#include "quickstartdlg.h"

#include <stdbool.h>
#include <stddef.h>

#include "../data/assets.h"
#include "../ui/control.h"
#include "../base/cstr.h"
#include "datelistbuilder.h"
#include "../ui/edit.h"
#include "../app/event.h"
#include "../data/gfxdev.h"
#include "../game/invlist.h"
#include "../game/member.h"
#include "../base/memory.h"
#include "../app/otiiapp.h"
#include "../game/party.h"
#include "../ui/pictctrl.h"
#include "../ui/picview.h"
#include "../app/replay.h"
#include "../base/rng.h"
#include "../base/str.h"
#include "../data/strrsrc.h"
#include "../ui/textview.h"
#include "../ui/view.h"

const struct quick_start_dlg_ops QUICK_START_DLG_OPS_TABLE = {
	{
		{
			{quick_start_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			quick_start_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		quick_start_dlg_begin,
		quick_start_dlg_accept,
	},
};

static void set_rect(struct rect *r, int32_t top, int32_t left, int32_t bottom, int32_t right)
{
	r->top = top;
	r->left = left;
	r->bottom = bottom;
	r->right = right;
}

void QuickStartDlgInit(struct quick_start_dlg *self)
{
	ModalDialogInit(&self->base, 51, false, true);
	self->base.base.base.ops = &QUICK_START_DLG_OPS_TABLE.base.base.base;

	struct point size;
	struct rect rect;
	struct point pos;
	struct view *child;
	struct pict_ctrl *button;
	struct cstr *text;
	int32_t handlers[4];
	const struct event *ev = &self->base.base.eh_event;

	self->choices = NULL;
	self->base.title = CStrCreateFrom("Prologue");
	size.h = 0;
	size.v = 0;
	self->base.content = (struct view *) PicViewCreatePoint(0, &size, 193);
	struct view *content = self->base.content;

	set_rect(&rect, 130, 45, 240, 565);
	child = (struct view *) TextViewCreate(1, &rect, NULL, 3, 0, 0, 0, 0, 0, 0, 255, 0, 1);
	VIEW_OPS(content)->AddChild(content, child);

	set_rect(&rect, 96, 195, 113, 290);
	text = CStrCreateFrom("The Journey of");
	child = (struct view *) TextViewCreate(45, &rect, text, 3, 0, 0, 0, 0, 0, 0, 255, 0, 1);
	VIEW_OPS(content)->AddChild(content, child);

	set_rect(&rect, 91, 291, 113, 400);
	text = CStrCreate();
	child = (struct view *) EditCreate(391, ev, &rect, text, false, true, 0, 3, 20, false, false, 0, false);
	VIEW_OPS(content)->AddChild(content, child);

	pos.h = 75;
	pos.v = 250;
	button = PictCtrlCreate(392, ev, &pos, 613, 0, 0, NULL, 0, 0, 0);
	ControlSetCommand((struct control *) button, CMD_QUICK_START_CANCEL, 0, 0);
	VIEW_OPS(content)->AddChild(content, (struct view *) button);

	pos.h = 240;
	button = PictCtrlCreate(393, ev, &pos, 614, 0, 0, NULL, 0, 0, 0);
	ControlSetCommand((struct control *) button, CMD_QUICK_START_REROLL, 0, 0);
	VIEW_OPS(content)->AddChild(content, (struct view *) button);

	pos.h = 400;
	button = PictCtrlCreate(394, ev, &pos, 615, 0, 0, NULL, 0, 0, 0);
	ControlSetCommand((struct control *) button, CMD_QUICK_START_ACCEPT, 0, 0);
	VIEW_OPS(content)->AddChild(content, (struct view *) button);

	handlers[0] = CMD_CLOSE_WINDOW;
	handlers[1] = CMD_QUICK_START_CANCEL;
	handlers[2] = CMD_QUICK_START_REROLL;
	handlers[3] = CMD_QUICK_START_ACCEPT;
	EvtHandlerBindHandler(&self->base.base, 4, handlers);
}

struct quick_start_dlg *QuickStartDlgCreate(void)
{
	struct quick_start_dlg *self = MemAlloc(1, sizeof(struct quick_start_dlg));
	QuickStartDlgInit(self);

	return self;
}

void QuickStartDlgDestroy(struct quick_start_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void quick_start_dlg_destroy(struct object *obj)
{
	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

void quick_start_dlg_on_event(struct evt_handler *self, struct cmd_event *event)
{
	struct quick_start_dlg *dlg = (struct quick_start_dlg *) self;

	switch (event->command) {
		case CMD_QUICK_START_ACCEPT:
			dlg->base.result = DLG_OK;
			break;
		case CMD_QUICK_START_REROLL:
			QuickStartDlgRoll(dlg);
			break;
		case CMD_CLOSE_WINDOW:
		case CMD_QUICK_START_CANCEL:
			dlg->base.result = DLG_CANCEL;
			break;
	}

	event->handled = true;
}

void quick_start_dlg_begin(struct modal_dialog *self, intptr_t param)
{
	struct quick_start_dlg *dlg = (struct quick_start_dlg *) self;

	dlg->choices = (struct quick_start_choices *) param;
	dlg->choices->party = NULL;
	RngSetSeed(RpTime());
	QuickStartDlgRoll(dlg);
}

void quick_start_dlg_accept(struct modal_dialog *self, intptr_t param)
{
	(void) param;
	struct quick_start_dlg *dlg = (struct quick_start_dlg *) self;
	struct edit *name_edit = (struct edit *) ViewFindById(self->content, 391);
	struct cstr *name = name_edit ? EditGetText(name_edit) : CStrCreate();

	if (!CStrLength(name)) {
		CStrDestroy(&name);
		struct strrsrc *names = StrRsrcLoad(g_text_lib, 2208, true);
		int32_t pick = RandRangeAvg(0, StrRsrcCount(names) - 1, 1);
		name = StrRsrcGetCStr(names, pick);
		StrRsrcDestroy(&names);
	}

	struct member *leader = PartyGetMember(dlg->choices->party, 0);
	CStrAssign(leader->name, name);
	CStrDestroy(&name);
}

void QuickStartDlgRoll(struct quick_start_dlg *self)
{
	struct quick_start_choices *choices = self->choices;
	char buffer[68];
	int32_t status = 0;

	if (choices->party)
		PartyDestroy(&choices->party);

	choices->party = PartyCreateQuickStart(CMD_NEW_GAME_OPTIONS);
	struct edit *name_edit = (struct edit *) ViewFindById(self->base.content, 391);
	struct member *leader = PartyGetMember(choices->party, 0);
	struct cstr *default_name = CStrCreate();
	CStrAssign(default_name, leader->name);
	int32_t name_length = 0;

	if (name_edit) {
		struct cstr *text = EditGetText(name_edit);
		name_length = CStrLength(text);
		CStrDestroy(&text);
	}

	if (name_edit && !name_length) {
		EditSetText(name_edit, default_name);
		EditSetSel(name_edit, 0, 100);

	} else {
		CStrDestroy(&default_name);
	}

	struct date_list_builder *date_lists = DateListBuilderCreate();

	struct strrsrc *strings = StrRsrcLoad(g_text_lib, 2200, true);
	struct cstr *occupation_text = StrRsrcGetCStr(strings, choices->party->occupation);
	StrRsrcDestroy(&strings);
	strings = StrRsrcLoad(g_text_lib, 2212, true);
	struct cstr *article_text = StrRsrcGetCStr(strings, choices->party->occupation);
	StrRsrcDestroy(&strings);

	StrPrintf(buffer, "%d", PartyMemberCount(choices->party, true) - 1);
	struct cstr *family_text = CStrCreateFrom(buffer);

	strings = StrRsrcLoad(g_text_lib, 50, true);
	choices->month = RandRangeAvg(3, 5, 1);
	struct cstr *month_text = StrRsrcGetCStr(strings, choices->month - 1);
	StrRsrcDestroy(&strings);

	choices->year = RandRangeAvg(1840, 1860, 1);
	struct cstr *year_text = CStrCreateInt(choices->year, false);

	struct strrsrc *town_list = DateListBuilderBuildStartTownList(date_lists, choices->year, NULL, 0, &status);
	int32_t count = StrRsrcCount(town_list);
	int32_t pick = RandRangeAvg(0, --count, 1);
	struct cstr *town_text = StrRsrcGetCStr(town_list, pick);
	choices->start_node = DateListBuilderLookupStartTown(date_lists, town_text);
	StrRsrcDestroy(&town_list);

	struct strrsrc *dest_list = DateListBuilderBuildDestinationList(date_lists, choices->year, NULL, 0, &status);
	count = StrRsrcCount(dest_list);
	pick = RandRangeAvg(0, --count, 1);
	struct cstr *destination_text = StrRsrcGetCStr(dest_list, pick);
	choices->destination = DateListBuilderLookupDestination(date_lists, destination_text);
	StrRsrcDestroy(&dest_list);

	choices->oxen = RandRangeAvg(0, 1, 1);
	choices->oxen = 2 * choices->oxen + 6;
	struct cstr *oxen_text = CStrCreateInt(choices->oxen, true);

	int32_t per_person = PartyMemberCount(choices->party, true) * -1;
	struct inv_list *gear = InvListCreateStore(104, 1, 0, per_person);
	double cost = InvListTotalValue(gear) / 100.0;
	PartyDistributeLoot(choices->party, gear);
	choices->party->cash += -cost;
	InvListDestroy(&gear);

	{
		struct cstr *ox_name = CStrCreateFrom("ox");
		gear = InvListCreate();
		InvListAdd(gear, ox_name, choices->oxen, 0.0);
		cost = InvListTotalValue(gear) / 100.0;
		PartyDistributeLoot(choices->party, gear);
		choices->party->cash += -cost;
		InvListDestroy(&gear);
		CStrDestroy(&ox_name);
	}

	if (choices->year < 1844)
		choices->wagons = RandRangeAvg(5, 15, 2);
	else
		choices->wagons = RandRangeAvg(5, 50, 1);

	struct cstr *wagons_text = CStrCreateInt(choices->wagons, true);
	choices->people = 5 * choices->wagons;
	choices->people = choices->people + RandRangeAvg(0, 9, 1);
	struct cstr *people_text = CStrCreateInt(choices->people, true);

	StrPrintf(buffer, "%5.2f", choices->party->cash);
	struct cstr *cash_text = CStrCreateFrom(buffer);

	struct strrsrc *lines = StrRsrcLoad(g_text_lib, 2214, true);
	struct cstr *text = CStrCreate();

	for (int32_t index = 0; index < StrRsrcCount(lines); index++) {
		struct cstr *line = StrRsrcGetCStr(lines, index);
		CStrReplaceStr(line, "[A/AN]", article_text);
		CStrReplaceStr(line, "[OCCUPATION]", occupation_text);
		CStrReplaceStr(line, "[TOWN]", town_text);
		CStrReplaceStr(line, "[MONTH]", month_text);
		CStrReplaceStr(line, "[YEAR]", year_text);
		CStrReplaceStr(line, "[DESTINATION]", destination_text);
		CStrReplaceStr(line, "[NUMOXEN]", oxen_text);
		CStrReplaceStr(line, "[CASH]", cash_text);
		CStrReplaceStr(line, "[FAMILY]", family_text);
		CStrReplaceStr(line, "[NUMWAGONS]", wagons_text);
		CStrReplaceStr(line, "[NUMPEOPLE]", people_text);
		CStrAppendStr(text, line);
		CStrDestroy(&line);
	}

	StrRsrcDestroy(&lines);
	CStrDestroy(&family_text);
	CStrDestroy(&destination_text);
	CStrDestroy(&year_text);
	CStrDestroy(&month_text);
	CStrDestroy(&town_text);
	CStrDestroy(&oxen_text);
	CStrDestroy(&cash_text);
	CStrDestroy(&occupation_text);
	CStrDestroy(&article_text);
	CStrDestroy(&wagons_text);
	CStrDestroy(&people_text);
	DateListBuilderDestroy(&date_lists);
	CStrMacRomanToAnsi(text, 0);
	struct text_view *text_view = (struct text_view *) ViewFindById(self->base.content, 1);

	if (text_view)
		TextViewSetSource(text_view, text);
	else
		CStrDestroy(&text);
}
