#include "journalentry.h"

#include "../base/cstr.h"
#include "../base/memory.h"
#include "../data/strrsrc.h"

const struct object_ops JOURNAL_ENTRY_OPS_TABLE = {
	journal_entry_destroy,
};

void JournalEntryInit(struct journal_entry *self, const struct date *when)
{
	ObjectInit(&self->base);
	self->base.ops = &JOURNAL_ENTRY_OPS_TABLE;
	DateInit(&self->date);
	self->text = StrRsrcCreate();
	self->str = CStrCreate();

	self->date = *when;
}

struct journal_entry *JournalEntryCreate(const struct date *when)
{
	struct journal_entry *self = MemAlloc(1, sizeof(struct journal_entry));
	JournalEntryInit(self, when);

	return self;
}

void JournalEntryDestroy(struct journal_entry **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void journal_entry_destroy(struct object *self)
{
	struct journal_entry *entry = (struct journal_entry *) self;

	CStrDestroy(&entry->str);
	StrRsrcDestroy(&entry->text);

	object_destroy(self);
}

void JournalEntryStoreLine(struct journal_entry *self, struct cstr *str)
{
	char marker[4];

	marker[0] = 127;
	marker[1] = 0;

	if (!str)
		return;

	CStrReplaceAll(str, "\n", "");
	CStrReplaceAll(str, "\r", "");
	CStrReplaceAll(str, marker, "");

	StrRsrcAppend(self->text, CStrGet(str));
	CStrDestroy(&str);
}
