#include "journal.h"

#include <stddef.h>
#include <string.h>

#include "../data/assets.h"
#include "../base/byteorder.h"
#include "../base/cstr.h"
#include "../data/docfile.h"
#include "../base/file.h"
#include "../data/gfxdev.h"
#include "../dialogs/journaldlg.h"
#include "journalentry.h"
#include "../base/list.h"
#include "../base/log.h"
#include "../base/memory.h"
#include "../dialogs/modaldialog.h"
#include "../base/path.h"
#include "../base/str.h"
#include "../data/strrsrc.h"

const struct object_ops JOURNAL_OPS_TABLE = {
	journal_destroy,
};

static void entries_rewind(struct journal *self)
{
	OblistFirst(self->entries);
}

void JournalInit(struct journal *self)
{
	ObjectInit(&self->base);
	self->base.ops = &JOURNAL_OPS_TABLE;
	DateInit(&self->date);
	self->entries = OblistCreate(ObjectDelete);

	self->current_entry = NULL;
}

struct journal *JournalCreate(void)
{
	struct journal *self = MemAlloc(1, sizeof(struct journal));
	JournalInit(self);

	return self;
}

void JournalDestroy(struct journal **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void journal_destroy(struct object *self)
{
	struct journal *journal = (struct journal *) self;

	OblistDestroy(&journal->entries);

	object_destroy(self);
}

void JournalClear(struct journal *self)
{
	entries_rewind(self);

	void *node = OblistCurrent(self->entries);

	while (node) {
		OblistRemove(self->entries, node);
		node = OblistCurrent(self->entries);
	}

	self->current_entry = NULL;
}

void JournalAddLine(struct journal *self, struct cstr *str)
{
	if (!self->current_entry) {
		LogError(LOG_ERROR, "CJournal::Date(date) not called!");
		return;
	}

	JournalEntryStoreLine(self->current_entry, str);
}

struct strrsrc *JournalGetEntryLines(struct journal *self)
{
	if (!self->current_entry) {
		LogError(LOG_ERROR, "CJournal::Date(date) not called!");

		static struct strrsrc *empty_lines = NULL;

		if (!empty_lines)
			empty_lines = StrRsrcCreate();

		return empty_lines;
	}

	return self->current_entry->text;
}

struct date *JournalGetDate(struct journal *self)
{
	return &self->date;
}

void JournalSetDate(struct journal *self, const struct date *date)
{
	self->date = *date;
	entries_rewind(self);

	for (self->current_entry = OblistCurrent(self->entries);
		self->current_entry && DateLess(&self->current_entry->date, &self->date);
		self->current_entry = OblistNext(self->entries, true))
		;

	if (!self->current_entry || !DateEqual(&self->current_entry->date, &self->date)) {
		void *old = self->current_entry;
		self->current_entry = JournalEntryCreate(&self->date);

		if (!old) {
			OblistPushBack(self->entries, self->current_entry);

		} else {
			old = OblistPrev(self->entries, true);

			if (!old) {
				OblistPushFront(self->entries, self->current_entry);

			} else {
				OblistInsertAtCursor(self->entries, self->current_entry);
			}
		}
	}
}

void JournalShowDialog(struct journal *self, struct window *parent)
{
	if (!self->current_entry) {
		LogError(LOG_ERROR, "CJournal::Date(date) not called!");
		return;
	}

	struct journal_dlg *dlg = JournalDlgCreate();
	ModalDialogRun(&dlg->base, (intptr_t) self, parent);
	ObjectDelete(dlg);
}

static void write_text(struct file *file, const char *text)
{
	if (file)
		FileWrite(file, text, StrLen(text));
}

void JournalExportWri(struct journal *self)
{
	struct path *path = PathCreate();
	struct strrsrc *month_names = StrRsrcLoad(g_text_lib, 50, true);
	const int32_t buf_size = 32767;
	char *buf = MemAlloc((size_t) buf_size, 1);

	if (!self->current_entry) {
		LogError(LOG_ERROR, "CJournal::Date(date) not called!");
		goto done;
	}

	struct journal_entry *saved_entry = self->current_entry;
	PathSaveDialog(path, "Write Files (*.wri)", "*.wri");

	if (path->status)
		goto done;

	struct file *file = FileOpen(CStrGet(path->path), FILE_WRITE);
	entries_rewind(self);

	for (self->current_entry = OblistCurrent(self->entries); self->current_entry;
		self->current_entry = OblistNext(self->entries, true)) {
		struct date entry_date = self->current_entry->date;
		const char *text = month_names ? StrRsrcGetAt(month_names, entry_date.month - 1) : NULL;

		StrPrintfN(buf, (size_t) buf_size, "%s %d, %d\r\n", text ? text : "", entry_date.day, entry_date.year);
		write_text(file, buf);

		struct strrsrc *lines = self->current_entry->text;
		int32_t line_count = StrRsrcCount(lines);

		for (int32_t line_index = 0; line_count > line_index; ++line_index) {
			text = StrRsrcGetAt(lines, line_index);
			StrPrintfN(buf, (size_t) buf_size, "%s\r\n", text ? text : "");
			write_text(file, buf);
		}

		struct cstr *entry_text = self->current_entry->str;

		if (CStrLength(entry_text)) {
			StrPrintfN(buf, (size_t) buf_size, "\r\n%s\r\n\r\n", CStrGet(entry_text));

		} else {
			StrPrintfN(buf, (size_t) buf_size, "\r\n");
		}

		write_text(file, buf);
	}

	FileClose(&file);
	self->current_entry = saved_entry;
	OblistSeekTo(self->entries, self->current_entry);

done:
	MemFree(buf);
	StrRsrcDestroy(&month_names);
	PathDestroy(&path);
}

struct cstr *JournalGetEntryText(struct journal *self)
{
	if (!self->current_entry) {
		LogError(LOG_ERROR, "CJournal::Date(date) not called!");

		static struct cstr *empty_text = NULL;

		if (!empty_text)
			empty_text = CStrCreate();

		return empty_text;
	}

	return self->current_entry->str;
}

void JournalSetEntryText(struct journal *self, const struct cstr *text)
{
	if (!self->current_entry) {
		LogError(LOG_ERROR, "CJournal::Date(date) not called!");
		return;
	}

	if (text)
		CStrAssign(self->current_entry->str, text);
}

void JournalNextEntry(struct journal *self)
{
	if (!OblistNext(self->entries, false))
		return;

	self->current_entry = OblistNext(self->entries, true);
	self->date = self->current_entry->date;
}

void JournalPrevEntry(struct journal *self)
{
	if (!OblistPrev(self->entries, false))
		return;

	self->current_entry = OblistPrev(self->entries, true);
	self->date = self->current_entry->date;
}

static void write_text_block(struct doc_file *file, const struct cstr *text)
{
	int32_t size = CStrLength(text) + 5;
	char *blk = MemAlloc((size_t) size, 1);

	StrCopyN(blk, (size_t) size, CStrGet(text));
	DocFileWriteObject(file, blk, size);
	MemFree(blk);
}

void JournalSaveState(struct journal *self, struct doc_file *file)
{
	uint8_t blk[8] = {0};

	WriteLe16(blk + 0, self->date.year);
	WriteLe16(blk + 2, self->date.month);
	WriteLe16(blk + 4, self->date.day);
	DocFileWriteObject(file, blk, 6);

	entries_rewind(self);
	struct journal_entry *entry = OblistCurrent(self->entries);

	while (entry) {
		struct strrsrc *lines = entry->text;

		memset(blk, 0, sizeof(blk));
		WriteLe16(blk + 0, entry->date.year);
		WriteLe16(blk + 2, entry->date.month);
		WriteLe16(blk + 4, entry->date.day);
		WriteLe16(blk + 6, StrRsrcCount(lines));
		DocFileWriteObject(file, blk, 8);

		for (int32_t line_index = 0; StrRsrcCount(lines) > line_index; ++line_index) {
			struct cstr *line = StrRsrcGetCStr(lines, line_index);

			if (!line)
				line = CStrCreate();

			write_text_block(file, line);
			CStrDestroy(&line);
		}

		write_text_block(file, entry->str);
		entry = OblistNext(self->entries, true);
	}

	memset(blk, 0, sizeof(blk));
	DocFileWriteObject(file, blk, 2);
}

static int32_t block_word(const uint8_t *blk, int32_t size, int32_t i)
{
	if (!blk || 2 * i + 2 > size)
		return 0;

	return ReadLe16(blk + 2 * i);
}

static struct cstr *block_text(const uint8_t *blk, int32_t size)
{
	int32_t len = 0;

	while (blk && len < size && blk[len])
		len++;

	return CStrCreateBytes(len, (const char *) blk);
}

void JournalLoadState(struct journal *self, struct doc_file *file)
{
	void *mem = NULL;
	int32_t size = 0;
	struct date date;
	int32_t year_or_end;

	DateInit(&date);
	JournalClear(self);
	DocFileReadObject2(file, &mem, &size);

	DateSetDay(&date, 1);
	DateSetYear(&date, block_word(mem, size, 0));
	DateSetMonth(&date, block_word(mem, size, 1));
	DateSetDay(&date, block_word(mem, size, 2));
	JournalSetDate(self, &date);

	do {
		DocFileReadObject2(file, &mem, &size);
		year_or_end = block_word(mem, size, 0);

		if (year_or_end) {
			DateSetDay(&date, 1);
			DateSetYear(&date, block_word(mem, size, 0));
			DateSetMonth(&date, block_word(mem, size, 1));
			DateSetDay(&date, block_word(mem, size, 2));
			JournalSetDate(self, &date);
			int32_t line_count = block_word(mem, size, 3);

			for (int32_t line_index = 0; line_count > line_index; ++line_index) {
				DocFileReadObject2(file, &mem, &size);
				JournalAddLine(self, block_text(mem, size));
			}

			DocFileReadObject2(file, &mem, &size);
			struct cstr *line = block_text(mem, size);
			JournalSetEntryText(self, line);
			CStrDestroy(&line);
		}
	} while (year_or_end);

	MemFree(mem);
}
