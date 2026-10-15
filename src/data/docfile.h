#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/object.h"

struct doc_file;
struct datafile;
struct path;
struct cstr;
struct date;
struct point;
struct rect;

struct stream_object;

struct stream_object_ops {
	struct object_ops base;
	void (*Slot1)(struct stream_object *self);
	void (*ReadFrom)(struct stream_object *self, struct doc_file *file);
	void (*WriteTo)(struct stream_object *self, struct doc_file *file);
};

struct stream_object {
	struct object base;
};

#define STREAM_OBJECT_OPS(p) ((const struct stream_object_ops *) ((const struct object *) (p))->ops)

struct doc_file_ops {
	struct object_ops base;
	int32_t (*OpenRead)(struct doc_file *self);
	int32_t (*OpenSave)(struct doc_file *self, const char *filter_desc, const char *filter_pattern);
	int32_t (*OpenDefault)(struct doc_file *self);
};

struct doc_file {
	struct object base;
	struct datafile *stream;
	struct path *path;
	int32_t status;
	bool dirty;
	int32_t version;
};

#define DOC_FILE_OPS(p) ((const struct doc_file_ops *) ((const struct object *) (p))->ops)

extern const struct doc_file_ops DOC_FILE_OPS_TABLE;

void DocFileInit(struct doc_file *self);
void DocFileInitPath(struct doc_file *self, struct path *source);
void doc_file_destroy(struct object *self);

int32_t doc_file_open_read(struct doc_file *self);
int32_t doc_file_open_save(struct doc_file *self, const char *filter_desc, const char *filter_pattern);
int32_t doc_file_open_default(struct doc_file *self);

void DocFileSetStatus(struct doc_file *self, int32_t s);
bool DocFileIsOpen(struct doc_file *self);
int32_t DocFileGetStatus(struct doc_file *self);
bool DocFileIsDirty(struct doc_file *self);
void DocFileSetDirtyV(struct doc_file *self, bool v);
int32_t DocFileReadSized(struct doc_file *self, void *buf, int32_t a3);
int32_t DocFileWriteSized(struct doc_file *self, const void *buf, int32_t size);
int32_t DocFileReadObject2(struct doc_file *self, void **mem, int32_t *size);
int32_t DocFileReadHeader(struct doc_file *self, int32_t type, uint32_t min_version, uint32_t max_version);
int32_t DocFileSetSource(struct doc_file *self, struct path *new_source);
int32_t DocFileWriteObject(struct doc_file *self, const void *mem, int32_t size);
int32_t DocFileWriteHeader(struct doc_file *self, uint32_t type, int32_t ver);
int32_t DocFileStreamRead(struct doc_file *self, const void *buf, int32_t size);
int32_t DocFileStreamWrite(struct doc_file *self, void *buf, int32_t size);
struct doc_file *DocFileReadInt16(struct doc_file *self, int32_t value);
struct doc_file *DocFileReadInt32(struct doc_file *self, int32_t value);
struct doc_file *DocFileReadDword(struct doc_file *self, uint32_t value);
struct doc_file *DocFileReadDouble(struct doc_file *self, double value);
struct doc_file *DocFileReadByte(struct doc_file *self, int32_t value);
struct doc_file *DocFileWriteInt16(struct doc_file *self, int32_t *out);
struct doc_file *DocFileWriteInt32(struct doc_file *self, int32_t *out);
struct doc_file *DocFileWriteDword(struct doc_file *self, uint32_t *out);
struct doc_file *DocFileWriteDouble(struct doc_file *self, double *out);
struct doc_file *DocFileWriteByte(struct doc_file *self, int32_t *out);
struct doc_file *DocFileStreamInObject(struct doc_file *self, struct stream_object *obj);
struct doc_file *DocFileStreamOutObject(struct doc_file *self, struct stream_object *obj);
struct doc_file *DocFileStreamPoint(struct doc_file *self, const struct point *pt);
struct doc_file *DocFileStreamRect(struct doc_file *self, const struct rect *r);
struct doc_file *DocFileStreamDate(struct doc_file *self, const struct date *date);
struct doc_file *DocFileStreamReadBlob(struct doc_file *self, const void *blob, int32_t len);
struct doc_file *DocFileStreamReadDate(struct doc_file *self, struct date *date);
struct doc_file *DocFileStreamReadPoint(struct doc_file *self, struct point *pt);
struct doc_file *DocFileStreamReadCStr(struct doc_file *self, struct cstr *cstr);
struct doc_file *DocFileStreamReadRect(struct doc_file *self, struct rect *r);
struct cstr *DocFileGetPath(struct doc_file *self);

struct path *MakeFile(int32_t mode, const struct cstr *cstr, const char *filter_desc, const char *filter_pattern,
	void *owner);
