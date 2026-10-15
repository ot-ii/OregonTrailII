#include "msg.h"

#include <stddef.h>

#include "../app/app.h"
#include "../data/assets.h"
#include "../base/cstr.h"
#include "game.h"
#include "../data/gfxdev.h"
#include "journal.h"
#include "member.h"
#include "../base/memory.h"
#include "../app/otiiapp.h"
#include "party.h"
#include "../base/rng.h"
#include "../data/rsrc.h"
#include "../data/strrsrc.h"

struct strrsrc *g_numberwords_rsrc = NULL;

const struct object_ops MSG_OPS_TABLE = {
	msg_destroy,
};

void StripTrailingPeriod(struct cstr *s)
{
	if (!s)
		return;

	int32_t idx = (int16_t) (CStrLength(s) - 1);
	char ch = CStrCharAt(s, idx);

	if (ch == 46)
		CStrSubstr(s, 0, idx);
}

void MsgInit(struct msg *self, int32_t number, struct cstr *text, intptr_t member, intptr_t subject)
{
	ObjectInit(&self->base);
	self->base.ops = &MSG_OPS_TABLE;
	self->node.prev = NULL;
	self->node.next = NULL;
	self->number_index = number;
	self->choice_state = 0;
	self->payload = text;
	self->member = member;
	self->subject = subject;
	self->rsrc_index = 0;

	self->title = NULL;
	self->scene_variant = RandRangeAvg(1, 100, 1);

	for (int32_t i = 0; i < MSG_FLAG_COUNT; i++)
		self->flags[i] = true;

	if (g_numberwords_rsrc == NULL)
		g_numberwords_rsrc = StrRsrcLoad(g_data_lib, 2600, true);

	self->journal_posted = false;
	self->member_name = NULL;
}

struct msg *MsgCreate(int32_t number, struct cstr *text, intptr_t member, intptr_t subject)
{
	struct msg *self = MemAlloc(1, sizeof(struct msg));
	MsgInit(self, number, text, member, subject);

	return self;
}

void MsgDestroy(struct msg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void msg_destroy(struct object *self)
{
	struct msg *msg = (struct msg *) self;

	CStrDestroy(&msg->payload);
	CStrDestroy(&msg->title);
	CStrDestroy(&msg->member_name);

	object_destroy(self);
}

struct msg *MsgFromNode(struct list_node *node)
{
	if (!node)
		return NULL;

	return (struct msg *) ((uint8_t *) node - offsetof(struct msg, node));
}

int32_t MsgGetNumberValue(struct msg *self)
{
	struct cstr *word = g_numberwords_rsrc ? StrRsrcGetCStr(g_numberwords_rsrc, (int16_t) self->number_index) : NULL;
	int32_t value = 0;

	if (!word || !CStrToInt(word, &value))
		value = 0;

	CStrDestroy(&word);

	return (int16_t) value;
}

void MsgPresentText(struct msg *self, struct cstr *obj, struct cstr *line1, struct cstr *line2)
{
	struct strrsrc *lines = NULL;
	struct cstr *line = NULL;
	int32_t count = 0;
	struct journal *journal = &((struct otii_app *) g_app)->journal;

	if (self->choice_state == 2 && self->journal_posted)
		return;

	JournalSetDate(journal, GetGameObj()->date);

	if (self->number_index < 1000) {
		struct rsrc *probe = AssetsGetElement(g_text_lib, "STRLST", (int16_t) (self->number_index + 11200), false);

		if (probe) {
			RsrcDestroy(&probe);
			lines = StrRsrcLoad(g_text_lib, self->number_index + 11200, true);
			count = lines ? StrRsrcCount(lines) : 0;

		} else if (obj) {
			CStrDestroy(&obj);
		}

		if (count) {
			line = StrRsrcGetCStr(lines, RandRangeAvg(0, count - 1, 1));

			if (!line)
				line = CStrCreate();

			CStrMacRomanToAnsi(line, 0);
			struct cstr *subject = MsgGetSubjectText(self);
			CStrReplaceStr(line, "[option]", subject);
			MsgReplaceGrammar(self, line);
			MsgReplacePlace(self, line);

			if (obj) {
				CStrReplaceStr(line, "[p1]", obj);
				CStrDestroy(&obj);
			}

			CStrReplaceAll(line, " your ", " our ");
			CStrDestroy(&subject);
		}

		StrRsrcDestroy(&lines);
	}

	if (line)
		JournalAddLine(journal, line);

	if (line1) {
		MsgReplaceGrammar(self, line1);
		JournalAddLine(journal, line1);
	}

	if (line2) {
		MsgReplaceGrammar(self, line2);
		JournalAddLine(journal, line2);
	}

	self->journal_posted = true;
}

struct cstr *MsgGetSubjectText(struct msg *self)
{
	int32_t char_index = 0;
	struct cstr *text;
	struct strrsrc *lines = StrRsrcLoad(g_text_lib, self->number_index + 11000, true);
	int32_t count = lines ? StrRsrcCount(lines) : 0;

	text = NULL;

	if (self->rsrc_index >= 0 && count - 2 > self->rsrc_index)
		text = StrRsrcGetCStr(lines, (int16_t) (self->rsrc_index + 2));

	if (text) {
		int32_t len = CStrLength(text);

		while (char_index <= len && CStrCharAt(text, char_index++) != 9)
			;

		while (char_index <= len && CStrCharAt(text, char_index++) != 9)
			;

		CStrSubstr(text, char_index + 1, 999);
		CStrToLower(text);

	} else {
		text = CStrCreateFrom("");
	}

	StrRsrcDestroy(&lines);

	return text;
}

void MsgReplaceGrammar(struct msg *self, struct cstr *text)
{
	if (!self->member_name)
		return;

	struct game *game = GetGameObj();
	struct member *leader = PartyGetMember(game->party, 0);
	const char *name_value;
	const char *have_value;
	const char *amis_value;

	if (leader && CStrEqual(self->member_name, leader->name)) {
		name_value = "I";
		have_value = "have";
		amis_value = "am";

	} else {
		name_value = CStrGet(self->member_name);
		have_value = "has";
		amis_value = "is";
	}

	CStrReplaceAll(text, "[name]", name_value);
	CStrReplaceAll(text, "[have/has]", have_value);
	CStrReplaceAll(text, "[am/is]", amis_value);
}

void MsgClearFlag(struct msg *self, int32_t i)
{
	if (i >= 0 && i < MSG_FLAG_COUNT) {
		self->flags[i] = false;
		GameRun(GetGameObj(), NULL);
	}
}

void MsgSetFlag(struct msg *self, int32_t i)
{
	if (i >= 0 && i < MSG_FLAG_COUNT) {
		self->flags[i] = true;
		GameRun(GetGameObj(), NULL);
	}
}
