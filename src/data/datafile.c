#include "datafile.h"

#include <string.h>

#include "../base/byteorder.h"
#include "../base/file.h"
#include "../base/memory.h"

struct datafile {
	struct file *file;
	int32_t status;
	int32_t version;
};

static struct datafile *open_file(const char *path, int32_t mode)
{
	struct datafile *ctx = MemAlloc(1, sizeof(struct datafile));
	ctx->file = path ? FileOpen(path, mode) : NULL;

	if (!ctx->file)
		ctx->status = DATAFILE_IO_ERROR;

	return ctx;
}

struct datafile *DataFileOpenRead(const char *path)
{
	return open_file(path, FILE_READ);
}

struct datafile *DataFileOpenWrite(const char *path)
{
	return open_file(path, FILE_WRITE);
}

void DataFileClose(struct datafile **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct datafile *ctx = *ctx_mod;
	*ctx_mod = NULL;

	FileClose(&ctx->file);
	MemFree(ctx);
}

int32_t DataFileStatus(const struct datafile *ctx)
{
	return ctx->status;
}

int32_t DataFileVersion(const struct datafile *ctx)
{
	return ctx->version;
}

int32_t DataFileReadHeader(struct datafile *ctx, uint32_t type, uint32_t min_version, uint32_t max_version)
{
	uint8_t header[10];

	if (!ctx->file || !FileSeek(ctx->file, 0) || FileRead(ctx->file, header, 10) != 10)
		return ctx->status = DATAFILE_IO_ERROR;

	if (ReadBeU32(header) != 0x4D454343u)
		return ctx->status = DATAFILE_NOT_MECC;

	if (ReadBeU32(header + 4) != type)
		return ctx->status = DATAFILE_WRONG_TYPE;

	uint32_t version = ReadBeU16(header + 8);

	if (version < min_version || version > max_version)
		return ctx->status = DATAFILE_BAD_VERSION;

	ctx->version = (int32_t) version;

	return ctx->status = DATAFILE_OK;
}

int32_t DataFileWriteHeader(struct datafile *ctx, uint32_t type, int32_t version)
{
	uint8_t header[10];
	WriteBe32(header, 0x4D454343u);
	WriteBe32(header + 4, type);
	WriteBe16(header + 8, version);

	if (!ctx->file || !FileSeek(ctx->file, 0) || FileWrite(ctx->file, header, 10) != 10)
		return ctx->status = DATAFILE_IO_ERROR;

	return ctx->status = DATAFILE_OK;
}

uint8_t *DataFileReadBlock(struct datafile *ctx, int32_t *size)
{
	uint8_t len_bytes[4];

	*size = 0;

	if (!ctx->file) {
		ctx->status = DATAFILE_IO_ERROR;
		*size = 4096;
		return MemAlloc(4096, 1);
	}

	bool at_end = FileRead(ctx->file, len_bytes, 4) != 4;
	int32_t len = at_end ? 0 : ReadLe32(len_bytes);

	if (at_end || len < 0) {
		if (len < 0)
			ctx->status = DATAFILE_IO_ERROR;

		*size = 4096;
		return MemAlloc(4096, 1);
	}

	uint8_t *buf = MemAlloc((size_t) len + 1, 1);

	if (FileRead(ctx->file, buf, len) != len && len > 0)
		ctx->status = DATAFILE_IO_ERROR;

	*size = len;

	return buf;
}

int32_t DataFileReadBytes(struct datafile *ctx, void *buf, int32_t size)
{
	if (ctx->status != DATAFILE_OK)
		return ctx->status;

	if (!ctx->file || (size > 0 && FileRead(ctx->file, buf, size) != size))
		ctx->status = DATAFILE_IO_ERROR;

	return ctx->status;
}

int32_t DataFileWriteBytes(struct datafile *ctx, const void *buf, int32_t size)
{
	if (ctx->status != DATAFILE_OK)
		return ctx->status;

	if (!ctx->file || (size > 0 && FileWrite(ctx->file, buf, size) != size))
		ctx->status = DATAFILE_IO_ERROR;

	return ctx->status;
}

bool DataFileSeek(struct datafile *ctx, int64_t pos)
{
	return ctx->file && FileSeek(ctx->file, pos);
}

int64_t DataFileTell(struct datafile *ctx)
{
	return ctx->file ? FileTell(ctx->file) : -1;
}

int32_t DataFileWriteBlock(struct datafile *ctx, const void *buf, int32_t size)
{
	uint8_t len_bytes[4];
	WriteLe32(len_bytes, (uint32_t) size);

	if (!ctx->file || FileWrite(ctx->file, len_bytes, 4) != 4 || (size > 0 && FileWrite(ctx->file, buf, size) != size))
		ctx->status = DATAFILE_IO_ERROR;

	return ctx->status;
}
