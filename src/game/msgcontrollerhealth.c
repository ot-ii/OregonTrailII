#include "msgcontroller.h"

#include <stddef.h>
#include <string.h>

#include "ailment.h"
#include "../app/app.h"
#include "../base/cstr.h"
#include "game.h"
#include "gamecmd.h"
#include "journal.h"
#include "member.h"
#include "msg.h"
#include "msgqueue.h"
#include "../app/otiiapp.h"
#include "party.h"
#include "../screens/sceneframe.h"
#include "../data/strrsrc.h"

static struct cstr *ailment_text(struct ailment *ailment_b)
{
	if (ailment_b->worsened)
		return AILMENT_OPS(ailment_b)->GetText(ailment_b, AILTEXT_WORSENED);

	return AILMENT_OPS(ailment_b)->GetText(ailment_b, AILTEXT_ONSET);
}

static struct cstr *ailment_headline(struct ailment *ailment_b)
{
	if (ailment_b->worsened)
		return CStrCreateFrom("a turn for the worse");

	return StrRsrcGetCStr(ailment_b->text, 0);
}

void msg_controller_on_event(struct evt_handler *self, struct cmd_event *event_o)
{
	struct msg_controller *ctl = (struct msg_controller *) self;
	struct ailment *ailment_a = NULL;
	struct member *member;
	int32_t msg_id_l;
	struct cstr *headline_c = NULL;
	struct msg *message_k = NULL;
	struct cstr *text_n = NULL;
	struct cstr *status_o;
	struct cstr *journal_line_l;

	member = (struct member *) event_o->param1;
	switch (event_o->command) {
	case GCMD_AILMENT_WORSENED:
		if (member->state == MEMBER_ALIVE) {
			ailment_a = (struct ailment *) event_o->param2;
			msg_id_l = SceneFrameEventCodeToMsgId(NULL, ailment_a->def->id);
			if (msg_id_l == 96) {
				if (ctl->thirst_warned)
					break;
				ctl->thirst_warned = true;
				text_n = CStrCreateFrom("Your party is suffering from thirst!");

			} else if (AILMENT_OPS(ailment_a)->IsKnown(ailment_a)) {
				text_n = ailment_text(ailment_a);

			} else {
				status_o = MemberGetStatusText(member);
				text_n = CStrCreateCopy(member->name);
				CStrAppendBuf(text_n, " is suffering from: ");
				if (!strncmp(CStrGet(status_o), "feeling", 7))
					CStrAppendBuf(text_n, "an unknown ailment.");
				else
					CStrAppendStr(text_n, status_o);
				CStrDestroy(&status_o);
			}
			headline_c = ailment_headline(ailment_a);
			message_k = MsgQueuePost(GetMessageQueue(), msg_id_l, text_n, (intptr_t) member, (intptr_t) ailment_a,
				headline_c, -1);
		}
		break;

	case GCMD_MEMBER_DIED:
		text_n = (struct cstr *) event_o->param2;
		headline_c = CStrCreateCopy(member->name);
		CStrAppendBuf(headline_c, " died.");
		if (PartyGetMember(GetGameObj()->party, 0) == member) {
			struct journal *journal = &((struct otii_app *) g_app)->journal;

			journal_line_l = CStrCreate();
			CStrAssign(journal_line_l, headline_c);
			JournalSetDate(journal, GetGameObj()->date);
			JournalAddLine(journal, journal_line_l);
			message_k = MsgQueuePost(GetMessageQueue(), MSG_LEADER_DIED, text_n, (intptr_t) member, 0, headline_c, -1);

		} else {
			message_k = MsgQueuePost(GetMessageQueue(), MSG_MEMBER_DIED, text_n, (intptr_t) member, 0, headline_c, -1);
		}
		break;
	}

	if (message_k)
		message_k->member_name = CStrCreateCopy(member->name);
	event_o->handled = true;
}
