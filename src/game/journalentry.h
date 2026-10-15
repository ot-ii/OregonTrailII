#pragma once

#include "../base/date.h"
#include "../base/object.h"

struct cstr;
struct strrsrc;

struct journal_entry {
	struct object base;
	struct date date;
	struct strrsrc *text;
	struct cstr *str;
};

extern const struct object_ops JOURNAL_ENTRY_OPS_TABLE;

void JournalEntryInit(struct journal_entry *self, const struct date *when);
struct journal_entry *JournalEntryCreate(const struct date *when);
void JournalEntryDestroy(struct journal_entry **ctx_mod);
void journal_entry_destroy(struct object *self);

void JournalEntryStoreLine(struct journal_entry *self, struct cstr *str);
