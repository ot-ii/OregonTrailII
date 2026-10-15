#pragma once

#include "../base/date.h"
#include "../base/object.h"

struct cstr;
struct doc_file;
struct journal_entry;
struct oblist;
struct strrsrc;
struct window;

struct journal {
	struct object base;
	struct date date;
	struct oblist *entries;
	struct journal_entry *current_entry;
};

extern const struct object_ops JOURNAL_OPS_TABLE;

void JournalInit(struct journal *self);
struct journal *JournalCreate(void);
void JournalDestroy(struct journal **ctx_mod);
void journal_destroy(struct object *self);

void JournalExportWri(struct journal *self);
void JournalSaveState(struct journal *self, struct doc_file *file);
void JournalLoadState(struct journal *self, struct doc_file *file);
void JournalClear(struct journal *self);
void JournalAddLine(struct journal *self, struct cstr *str);
struct date *JournalGetDate(struct journal *self);
void JournalSetDate(struct journal *self, const struct date *date);
void JournalShowDialog(struct journal *self, struct window *parent);
struct strrsrc *JournalGetEntryLines(struct journal *self);
struct cstr *JournalGetEntryText(struct journal *self);
void JournalSetEntryText(struct journal *self, const struct cstr *text);
void JournalNextEntry(struct journal *self);
void JournalPrevEntry(struct journal *self);
