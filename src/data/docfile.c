#include "docfile.h"

#include <string.h>

#include "../base/byteorder.h"
#include "../base/cstr.h"
#include "datafile.h"
#include "../base/date.h"
#include "../base/geom.h"
#include "gfxdev.h"
#include "../base/memory.h"
#include "../base/path.h"
#include "strrsrc.h"

static int32_t g_datafile_count;

const struct doc_file_ops DOC_FILE_OPS_TABLE = {
	{doc_file_destroy},
	doc_file_open_read,
	doc_file_open_save,
	doc_file_open_default,
};

static void doc_file_init_fields(struct doc_file *self)
{
	++g_datafile_count;
	self->path = NULL;
	self->stream = NULL;
	self->dirty = false;
	self->status = 0;
}

void DocFileInit(struct doc_file *self)
{
	ObjectInit(&self->base);
	self->base.ops = &DOC_FILE_OPS_TABLE.base;
	doc_file_init_fields(self);
}

void DocFileInitPath(struct doc_file *self, struct path *source)
{
	ObjectInit(&self->base);
	self->base.ops = &DOC_FILE_OPS_TABLE.base;
	doc_file_init_fields(self);
	self->path = source;
}

void doc_file_destroy(struct object *obj)
{
	struct doc_file *self = (struct doc_file *) obj;

	PathDestroy(&self->path);

	object_destroy(obj);
}

int32_t DocFileGetStatus(struct doc_file *self)
{
	return self->status;
}

void DocFileSetStatus(struct doc_file *self, int32_t s)
{
	self->status = s;
}

bool DocFileIsDirty(struct doc_file *self)
{
	return self->dirty;
}

void DocFileSetDirtyV(struct doc_file *self, bool v)
{
	self->dirty = v;
}

static int32_t write_error(int32_t err)
{
	return err == DATAFILE_DISK_FULL ? 2 : 3;
}

int32_t DocFileReadObject2(struct doc_file *self, void **mem, int32_t *size)
{
	if (self->stream) {
		int32_t len = 0;
		uint8_t *block = DataFileReadBlock(self->stream, &len);

		if (DataFileStatus(self->stream) != DATAFILE_OK)
			DocFileSetStatus(self, 3);

		MemFree(*mem);
		*mem = block;
		*size = len;

	} else {
		DocFileSetStatus(self, 3);
	}

	return self->status;
}

int32_t DocFileReadSized(struct doc_file *self, void *buf, int32_t a3)
{
	(void) a3;

	if (self->stream) {
		uint8_t len_bytes[4];

		if (DataFileReadBytes(self->stream, len_bytes, 4) == DATAFILE_OK &&
			DataFileReadBytes(self->stream, buf, ReadLe32(len_bytes)) != DATAFILE_OK)
			DocFileSetStatus(self, 3);

	} else {
		DocFileSetStatus(self, 3);
	}

	return self->status;
}

int32_t DocFileReadHeader(struct doc_file *self, int32_t type, uint32_t min_version, uint32_t max_version)
{
	if (!self->stream) {
		DocFileSetStatus(self, 3);
		return self->status;
	}

	int32_t err = DataFileReadHeader(self->stream, (uint32_t) type, min_version, max_version);

	if (err != DATAFILE_OK) {
		DocFileSetStatus(self, err);
		return self->status;
	}

	self->version = DataFileVersion(self->stream);
	DocFileSetStatus(self, 0);

	return self->status;
}

int32_t DocFileSetSource(struct doc_file *self, struct path *new_source)
{
	PathDestroy(&self->path);
	self->path = new_source;
	DOC_FILE_OPS(self)->OpenDefault(self);

	return self->status;
}

struct cstr *DocFileGetPath(struct doc_file *self)
{
	struct cstr *result;

	if (DocFileIsOpen(self)) {
		result = PathCopyPath(self->path);

	} else {
		struct strrsrc *strings = StrRsrcLoad(g_text_lib, 5, true);

		if (strings) {
			result = StrRsrcGetCStr(strings, 6);
			StrRsrcDestroy(&strings);

		} else {
			result = CStrCreate();
		}
	}

	return result;
}

bool DocFileIsOpen(struct doc_file *self)
{
	return self->path != NULL;
}

int32_t DocFileWriteObject(struct doc_file *self, const void *mem, int32_t size)
{
	if (self->stream) {
		int32_t err = DataFileWriteBlock(self->stream, mem, size);

		if (err)
			DocFileSetStatus(self, write_error(err));

	} else {
		DocFileSetStatus(self, 3);
	}

	return self->status;
}

int32_t DocFileWriteSized(struct doc_file *self, const void *buf, int32_t size)
{
	if (self->stream) {
		int32_t err = DataFileWriteBlock(self->stream, buf, size);

		if (err)
			DocFileSetStatus(self, write_error(err));

	} else {
		DocFileSetStatus(self, 3);
	}

	return self->status;
}

int32_t DocFileWriteHeader(struct doc_file *self, uint32_t type, int32_t ver)
{
	if (!self->stream) {
		DocFileSetStatus(self, 3);
		return self->status;
	}

	if (DataFileWriteHeader(self->stream, type, ver) != DATAFILE_OK) {
		DocFileSetStatus(self, 3);
		return self->status;
	}

	DocFileSetStatus(self, 0);

	return self->status;
}

int32_t DocFileStreamRead(struct doc_file *self, const void *buf, int32_t size)
{
	if (self->status == 0) {
		int32_t err = DataFileWriteBytes(self->stream, buf, size);

		if (err) {
			DocFileSetStatus(self, write_error(err));

		} else {
			DocFileSetStatus(self, 0);
		}
	}

	return self->status;
}

int32_t DocFileStreamWrite(struct doc_file *self, void *buf, int32_t size)
{
	if (self->status == 0) {
		int32_t err = DataFileReadBytes(self->stream, buf, size);

		if (err) {
			DocFileSetStatus(self, 3);

		} else {
			DocFileSetStatus(self, 0);
		}
	}

	return self->status;
}

struct doc_file *DocFileReadInt16(struct doc_file *self, int32_t value)
{
	uint8_t b[2];
	WriteLe16(b, value);
	DocFileStreamRead(self, b, 2);

	return self;
}

struct doc_file *DocFileWriteInt16(struct doc_file *self, int32_t *out)
{
	uint8_t b[2];

	if (DocFileStreamWrite(self, b, 2) == 0)
		*out = ReadLe16(b);

	return self;
}

struct doc_file *DocFileReadInt32(struct doc_file *self, int32_t value)
{
	uint8_t b[4];
	WriteLe32(b, (uint32_t) value);
	DocFileStreamRead(self, b, 4);

	return self;
}

struct doc_file *DocFileWriteInt32(struct doc_file *self, int32_t *out)
{
	uint8_t b[4];

	if (DocFileStreamWrite(self, b, 4) == 0)
		*out = ReadLe32(b);

	return self;
}

struct doc_file *DocFileReadDword(struct doc_file *self, uint32_t value)
{
	uint8_t b[4];
	WriteLe32(b, value);
	DocFileStreamRead(self, b, 4);

	return self;
}

struct doc_file *DocFileWriteDword(struct doc_file *self, uint32_t *out)
{
	uint8_t b[4];

	if (DocFileStreamWrite(self, b, 4) == 0)
		*out = ReadLeU32(b);

	return self;
}

struct doc_file *DocFileReadDouble(struct doc_file *self, double value)
{
	uint64_t bits;
	memcpy(&bits, &value, sizeof(bits));

	uint8_t b[8];
	WriteLeU64(b, bits);
	DocFileStreamRead(self, b, 8);

	return self;
}

struct doc_file *DocFileWriteDouble(struct doc_file *self, double *out)
{
	uint8_t b[8];

	if (DocFileStreamWrite(self, b, 8) == 0) {
		uint64_t bits = ReadLeU64(b);
		memcpy(out, &bits, sizeof(bits));
	}

	return self;
}

struct doc_file *DocFileReadByte(struct doc_file *self, int32_t value)
{
	uint8_t b = (uint8_t) value;
	DocFileStreamRead(self, &b, 1);

	return self;
}

struct doc_file *DocFileWriteByte(struct doc_file *self, int32_t *out)
{
	uint8_t b;

	if (DocFileStreamWrite(self, &b, 1) == 0)
		*out = (int8_t) b;

	return self;
}

struct doc_file *DocFileStreamInObject(struct doc_file *self, struct stream_object *obj)
{
	STREAM_OBJECT_OPS(obj)->ReadFrom(obj, self);

	return self;
}

struct doc_file *DocFileStreamOutObject(struct doc_file *self, struct stream_object *obj)
{
	STREAM_OBJECT_OPS(obj)->WriteTo(obj, self);

	return self;
}

struct doc_file *DocFileStreamPoint(struct doc_file *self, const struct point *pt)
{
	DocFileReadInt16(DocFileReadInt16(self, pt->h), pt->v);

	return self;
}

struct doc_file *DocFileStreamReadPoint(struct doc_file *self, struct point *pt)
{
	int32_t h_raw = 0;
	int32_t v_raw = 0;
	DocFileWriteInt16(DocFileWriteInt16(self, &h_raw), &v_raw);
	pt->h = h_raw;
	pt->v = v_raw;

	return self;
}

struct doc_file *DocFileStreamRect(struct doc_file *self, const struct rect *r)
{
	DocFileReadInt16(DocFileReadInt16(DocFileReadInt16(DocFileReadInt16(self, r->top), r->left), r->bottom),
		r->right);

	return self;
}

struct doc_file *DocFileStreamReadRect(struct doc_file *self, struct rect *r)
{
	int32_t top_raw = 0;
	int32_t left_raw = 0;
	int32_t bottom_raw = 0;
	int32_t right_raw = 0;
	DocFileWriteInt16(DocFileWriteInt16(DocFileWriteInt16(DocFileWriteInt16(self, &top_raw), &left_raw),
		&bottom_raw), &right_raw);
	r->top = top_raw;
	r->left = left_raw;
	r->right = right_raw;
	r->bottom = bottom_raw;

	return self;
}

struct doc_file *DocFileStreamDate(struct doc_file *self, const struct date *date)
{
	DocFileReadInt16(DocFileReadInt16(DocFileReadInt16(self, date->year), date->month), date->day);

	return self;
}

struct doc_file *DocFileStreamReadDate(struct doc_file *self, struct date *date)
{
	int32_t year = 0;
	int32_t month = 0;
	int32_t day = 0;
	DocFileWriteInt16(DocFileWriteInt16(DocFileWriteInt16(self, &year), &month), &day);
	DateSetYear(date, year);
	DateSetMonth(date, month);
	DateSetDay(date, day);

	return self;
}

struct doc_file *DocFileStreamReadBlob(struct doc_file *self, const void *blob, int32_t len)
{
	DocFileReadInt32(self, len);
	DocFileStreamRead(self, blob, len);

	return self;
}

struct doc_file *DocFileStreamReadCStr(struct doc_file *self, struct cstr *cstr)
{
	int32_t len = 0;
	DocFileWriteInt32(self, &len);

	if (len < 0)
		len = 0;

	char *buf = MemAlloc((size_t) len + 1, 1);
	DocFileStreamWrite(self, buf, len);
	buf[len] = 0;
	CStrAssignBuf(cstr, buf);
	MemFree(buf);

	return self;
}

int32_t doc_file_open_read(struct doc_file *self)
{
	self->stream = NULL;
	self->status = 0;

	if (self->path) {
		self->stream = DataFileOpenRead(CStrGet(self->path->path));
		DataFileSeek(self->stream, 0);
	}

	return self->status;
}

int32_t doc_file_open_default(struct doc_file *self)
{
	return DOC_FILE_OPS(self)->OpenSave(self, NULL, NULL);
}

int32_t doc_file_open_save(struct doc_file *self, const char *filter_desc, const char *filter_pattern)
{
	self->stream = NULL;
	self->status = 0;

	if (!DocFileIsOpen(self)) {
		struct cstr *name = DocFileGetPath(self);
		struct path *spec = MakeFile(0, name, filter_desc, filter_pattern, NULL);
		CStrDestroy(&name);

		if (spec && spec->status == 0) {
			PathDestroy(&self->path);
			self->path = spec;

		} else {
			PathDestroy(&spec);
			DocFileSetStatus(self, 3);
		}
	}

	if (self->status == 0) {
		self->stream = DataFileOpenWrite(CStrGet(self->path->path));
		DataFileSeek(self->stream, 0);
	}

	return self->status;
}

struct path *MakeFile(int32_t mode, const struct cstr *cstr, const char *filter_desc, const char *filter_pattern,
	void *owner)
{
	struct path *spec = PathCreate();

	if (cstr)
		PathSetFromCStr(spec, cstr);

	switch (mode) {
		case 0:
			PathSaveDialog(spec, filter_desc, filter_pattern);
			break;
		case 1:
			PathOpenDialog(spec, filter_desc, filter_pattern, owner);
			break;
	}

	return spec;
}
