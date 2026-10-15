#include "mecclib.h"

#include <string.h>

#include "../base/byteorder.h"
#include "../base/file.h"
#include "../base/memory.h"
#include "rsrc.h"
#include "../base/str.h"

struct mecc_rec {
	int32_t storage;
	int32_t id;
	int32_t size;
	int32_t offset;
};

struct mecc_type {
	char name[9];
	int32_t rec_count;
	struct mecc_rec *recs;
};

struct mecclib {
	char path[260];
	struct file *file;
	int32_t table_offset;
	int32_t data_offset;
	int32_t version;
	int32_t type_count;
	struct mecc_type *types;
	struct mecc_rec *recs;
};

static bool read_at(struct file *f, int64_t pos, void *buf, int32_t size)
{
	return FileSeek(f, pos) && FileRead(f, buf, size) == size;
}

static void read_directory(struct mecclib *ctx)
{
	uint8_t header[10];

	if (!read_at(ctx->file, 0, header, sizeof(header)))
		return;

	ctx->table_offset = ReadBe16(header);
	ctx->data_offset = ReadBe32(header + 2);
	ctx->type_count = ReadBe16(header + 6);
	ctx->version = ReadBe16(header + 8);

	if (ctx->type_count <= 0)
		return;

	int32_t table_size = 14 * ctx->type_count;
	uint8_t *table = MemAlloc((size_t) table_size, 1);

	if (!read_at(ctx->file, ctx->table_offset, table, table_size)) {
		MemFree(table);
		ctx->type_count = 0;
		return;
	}

	ctx->types = MemAlloc((size_t) ctx->type_count, sizeof(struct mecc_type));

	int32_t total = 0;
	int64_t *rec_offsets = MemAlloc((size_t) ctx->type_count, sizeof(int64_t));

	for (int32_t i = 0; i < ctx->type_count; i++) {
		const uint8_t *e = table + 14 * i;
		struct mecc_type *t = &ctx->types[i];

		memcpy(t->name, e, 8);
		t->name[8] = 0;
		t->rec_count = ReadBe16(e + 8);

		if (t->rec_count < 0)
			t->rec_count = 0;

		rec_offsets[i] = ReadBeU32(e + 10);
		total += t->rec_count;
	}

	MemFree(table);

	ctx->recs = MemAlloc((size_t) (total ? total : 1), sizeof(struct mecc_rec));
	struct mecc_rec *rec = ctx->recs;

	for (int32_t i = 0; i < ctx->type_count; i++) {
		struct mecc_type *t = &ctx->types[i];
		int32_t size = 12 * t->rec_count;
		uint8_t *buf = MemAlloc((size_t) (size ? size : 1), 1);

		if (size && !read_at(ctx->file, rec_offsets[i], buf, size))
			memset(buf, 0, (size_t) size);

		t->recs = rec;

		for (int32_t j = 0; j < t->rec_count; j++, rec++) {
			const uint8_t *r = buf + 12 * j;
			rec->storage = ReadBe16(r);
			rec->id = ReadBe16(r + 2);
			rec->size = ReadBe32(r + 4);
			rec->offset = ReadBe32(r + 8);
		}

		MemFree(buf);
	}

	MemFree(rec_offsets);
}

struct mecclib *MeccLibCreate(const char *path)
{
	struct mecclib *ctx = MemAlloc(1, sizeof(struct mecclib));
	StrCopy(ctx->path, path ? path : "");

	ctx->file = FileOpen(ctx->path, FILE_READ);

	if (ctx->file)
		read_directory(ctx);

	return ctx;
}

void MeccLibDestroy(struct mecclib **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct mecclib *ctx = *ctx_mod;
	*ctx_mod = NULL;

	FileClose(&ctx->file);
	MemFree(ctx->recs);
	MemFree(ctx->types);
	MemFree(ctx);
}

bool MeccLibIsAvailable(const struct mecclib *ctx)
{
	return ctx->file != NULL;
}

const char *MeccLibPath(const struct mecclib *ctx)
{
	return ctx->path;
}

static void upper_copy(char *dst, size_t size, const char *src)
{
	StrCopyN(dst, size, src);

	for (char *p = dst; *p; p++)
		if (*p >= 'a' && *p <= 'z')
			*p -= 32;
}

static const struct mecc_rec *find_rec(const struct mecclib *ctx, const char *name, int32_t id)
{
	char want[12], have[12];
	upper_copy(want, sizeof(want), name);

	for (int32_t i = 0; i < ctx->type_count; i++) {
		upper_copy(have, sizeof(have), ctx->types[i].name);

		if (strcmp(want, have) != 0)
			continue;

		const struct mecc_type *t = &ctx->types[i];

		for (int32_t j = 0; j < t->rec_count; j++)
			if (t->recs[j].id == id)
				return &t->recs[j];

		return NULL;
	}

	return NULL;
}

bool MeccLibFind(const struct mecclib *ctx, const char *name, int32_t id, int64_t *offset, int32_t *size,
	int32_t *storage)
{
	const struct mecc_rec *rec = ctx->file ? find_rec(ctx, name, id) : NULL;

	if (!rec)
		return false;

	if (offset)
		*offset = rec->offset;

	if (size)
		*size = rec->size;

	if (storage)
		*storage = rec->storage;

	return true;
}

struct rsrc *MeccLibGetElement(struct mecclib *ctx, const char *name, int32_t id)
{
	const struct mecc_rec *rec = ctx->file ? find_rec(ctx, name, id) : NULL;

	if (!rec)
		return NULL;

	struct rsrc *r = RsrcCreate(name, id, rec->size, rec->storage);

	if (rec->size > 0 && !read_at(ctx->file, rec->offset, RsrcData(r), rec->size)) {
	}

	if (rec->storage == RSRC_TEXT) {
		RsrcResize(r, rec->size + 1);
		RsrcData(r)[rec->size] = 0;
	}

	return r;
}

void MeccLibEnumerate(const struct mecclib *ctx, MeccLibEnumFunc func, void *opaque)
{
	for (int32_t i = 0; i < ctx->type_count; i++) {
		const struct mecc_type *t = &ctx->types[i];

		for (int32_t j = 0; j < t->rec_count; j++)
			func(t->name, t->recs[j].id, t->recs[j].storage, t->recs[j].size, t->recs[j].offset, opaque);
	}
}
