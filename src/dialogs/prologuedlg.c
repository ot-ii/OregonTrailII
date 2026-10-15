#include "prologuedlg.h"

#include <stddef.h>

#include "../data/assets.h"
#include "../ui/button.h"
#include "../base/cstr.h"
#include "../base/date.h"
#include "../draw/drawable.h"
#include "../app/event.h"
#include "../game/game.h"
#include "../data/gfxdev.h"
#include "../game/invitem.h"
#include "../game/journal.h"
#include "../game/member.h"
#include "../base/memory.h"
#include "../draw/objbase.h"
#include "../game/occupmgr.h"
#include "../app/otiiapp.h"
#include "../game/party.h"
#include "../ui/picview.h"
#include "../base/str.h"
#include "../data/strrsrc.h"
#include "../ui/textview.h"
#include "../ui/view.h"

const struct prologue_dlg_ops PROLOGUE_DLG_OPS_TABLE = {
	{
		{
			{prologue_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			prologue_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		prologue_dlg_begin,
		prologue_dlg_accept,
	},
};

void PrologueDlgInit(struct prologue_dlg *self)
{
	ModalDialogInit(&self->base, 50, false, true);
	self->base.base.base.ops = &PROLOGUE_DLG_OPS_TABLE.base.base.base;

	struct point size;
	struct rect rect;
	struct view *child;
	int32_t handlers[2];

	self->base.title = CStrCreateFrom("Prologue");

	size.h = 0;
	size.v = 0;
	self->base.content = (struct view *) PicViewCreatePoint(0, &size, 193);
	struct view *content = self->base.content;
	struct drawable *image = ((struct pic_view *) content)->image;
	OBJ_BASE_OPS(image)->GetSize((struct obj_base *) image, &size);

	rect.top = 0;
	rect.left = 0;
	rect.right = size.h;
	rect.bottom = size.v;
	child = (struct view *) ClipButtonCreate(2, &self->base.base.eh_event, &rect, CMD_CONTROL_ACTION, 0, 0);
	VIEW_OPS(content)->AddChild(content, child);

	rect.top = 95;
	rect.left = 45;
	rect.right = 565;
	rect.bottom = 200;
	child = (struct view *) TextViewCreate(1, &rect, NULL, 3, 0, 0, 0, 0, 0, 0, 255, 0, 1);
	VIEW_OPS(content)->AddChild(content, child);

	handlers[0] = CMD_CONTROL_ACTION;
	handlers[1] = CMD_CLOSE_WINDOW;
	EvtHandlerBindHandler(&self->base.base, 2, handlers);
}

struct prologue_dlg *PrologueDlgCreate(void)
{
	struct prologue_dlg *self = MemAlloc(1, sizeof(struct prologue_dlg));
	PrologueDlgInit(self);

	return self;
}

void PrologueDlgDestroy(struct prologue_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void prologue_dlg_destroy(struct object *obj)
{
	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

void prologue_dlg_on_event(struct evt_handler *self, struct cmd_event *evt)
{
	((struct modal_dialog *) self)->result = DLG_OK;
	evt->handled = true;
}

void prologue_dlg_begin(struct modal_dialog *self, intptr_t param)
{
	char buf[64];
	struct party *party = (struct party *) param;
	struct journal *journal = &((struct otii_app *) g_app)->journal;

	struct strrsrc *rsrc = StrRsrcLoad(g_text_lib, 2200, true);
	struct cstr *occupation = StrRsrcGetCStr(rsrc, party->occupation);
	StrRsrcDestroy(&rsrc);

	rsrc = StrRsrcLoad(g_text_lib, 2212, true);
	struct cstr *article = StrRsrcGetCStr(rsrc, party->occupation);
	StrRsrcDestroy(&rsrc);

	rsrc = StrRsrcLoad(g_text_lib, 901, true);
	struct cstr *town = StrRsrcGetCStr(rsrc, GetGameObj()->cur_node_id);
	struct cstr *destination = StrRsrcGetCStr(rsrc, GetGameObj()->next_node_id);
	StrRsrcDestroy(&rsrc);

	StrPrintf(buf, "%d", GetGameObj()->date->year);
	struct cstr *year_str = CStrCreateFrom(buf);
	StrPrintf(buf, "%d", PartyMemberCount(party, true));
	struct cstr *members_str = CStrCreateFrom(buf);

	struct inv_item *wagon = InvItemFromNode(party->wagons.base.head);
	struct cstr *wagon_name = InvItemGetNameSingular(wagon);

	struct strrsrc *prologue_lines = StrRsrcLoad(g_text_lib, 2211, true);
	struct occup_mgr *occup_mgr = OccupMgrCreate();
	struct cstr *leader = CStrCreateCopy(PartyGetMember(GetGameObj()->party, 0)->name);
	struct strrsrc *skill_names = occup_mgr->skill_names;
	struct date date = *GetGameObj()->date;
	int32_t skill_count = 0;
	DateSetDay(&date, date.day - 1);
	JournalSetDate(journal, &date);

	struct cstr *str = CStrCreateFrom("Here begins the journal of [NAME], formerly [A/AN] [OCCUPATION]. Tomorrow we "
		"leave [TOWN] behind to begin our journey west to [DESTINATION].  We must still "
		"purchase the supplies that our [WAGONTYPE] will hold to sustain us during our "
		"long trek.");
	CStrReplaceStr(str, "[NAME]", leader);
	CStrReplaceStr(str, "[A/AN]", article);
	CStrReplaceStr(str, "[OCCUPATION]", occupation);
	CStrReplaceStr(str, "[TOWN]", town);
	CStrReplaceStr(str, "[DESTINATION]", destination);
	CStrReplaceStr(str, "[WAGONTYPE]", wagon_name);
	JournalAddLine(journal, str);

	for (int32_t index = 0; index <= 12; index++) {
		if (GetGameObj()->party->skills[index])
			++skill_count;
	}

	if (skill_count) {
		if (skill_count > 1)
			str = CStrCreateFrom("I hope that the following skills will prove of some use along the trail: ");
		else
			str = CStrCreateFrom("I hope that my skill in ");

		skill_count = 0;

		for (int32_t index = 0; index <= 12; index++) {
			if (GetGameObj()->party->skills[index]) {
				struct cstr *skill_name = StrRsrcGetCStr(skill_names, index);

				if (skill_count)
					CStrAppendBuf(str, ", ");

				CStrAppendStr(str, skill_name);
				CStrDestroy(&skill_name);
				++skill_count;
			}
		}

		if (skill_count == 1)
			CStrAppendBuf(str, " will prove of some value.");
		else
			CStrAppendBuf(str, ".");

		JournalAddLine(journal, str);
	}

	OccupMgrDestroy(&occup_mgr);

	struct cstr *out = CStrCreate();

	for (int32_t index = 0; StrRsrcCount(prologue_lines) > index; index++) {
		struct cstr *line = StrRsrcGetCStr(prologue_lines, index);
		CStrReplaceStr(line, "[A/AN]", article);
		CStrReplaceStr(line, "[OCCUPATION]", occupation);
		CStrReplaceStr(line, "[TOWN]", town);
		CStrReplaceStr(line, "[YEAR]", year_str);
		CStrReplaceStr(line, "[DESTINATION]", destination);
		CStrReplaceStr(line, "[WAGONTYPE]", wagon_name);
		CStrReplaceStr(line, "[X]", members_str);
		CStrAppendStr(out, line);
		CStrDestroy(&line);
	}

	StrRsrcDestroy(&prologue_lines);
	CStrDestroy(&members_str);
	CStrDestroy(&wagon_name);
	CStrDestroy(&destination);
	CStrDestroy(&year_str);
	CStrDestroy(&town);
	CStrDestroy(&occupation);
	CStrDestroy(&article);

	CStrMacRomanToAnsi(out, 0);
	struct text_view *text_view = (struct text_view *) ViewFindById(self->content, 1);
	TextViewSetSource(text_view, out);
	CStrDestroy(&leader);
}

void prologue_dlg_accept(struct modal_dialog *self, intptr_t param)
{
	(void) self;
	(void) param;
}
