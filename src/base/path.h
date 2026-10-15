#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "object.h"

struct cstr;

struct path {
	struct object base;
	int32_t status;
	struct cstr *path;
	bool exists;
};

extern const struct object_ops PATH_OPS_TABLE;

void PathInit(struct path *self);
void PathInitStr(struct path *self, const struct cstr *s);
void PathInitCopy(struct path *self, const struct path *src);
struct path *PathCreate(void);
struct path *PathCreateStr(const struct cstr *s);
struct path *PathCreateCopy(const struct path *src);
void PathDestroy(struct path **ctx_mod);
void path_destroy(struct object *self);

struct path *PathAssign(struct path *self, const struct path *other);
void PathSetFromCStr(struct path *self, const struct cstr *src);
struct cstr *PathCopyPath(const struct path *self);
int32_t PathPathLength(const struct path *self);
void PathSetPath(struct path *self, const char *s);
void PathSetPathObj(struct path *self, const char *path_obj);
int32_t PathRenameTo(struct path *self, const struct path *other);
int32_t PathDoRename(struct path *self, const char *newpath);
int32_t PathRename(struct path *self, const struct cstr *newpath);
void PathOpenDialog(struct path *self, const char *filter_desc, const char *filter_pattern, void *owner);
void PathOpenDialogDefault(struct path *self);
void PathSaveDialog(struct path *self, const char *filter_desc, const char *filter_pattern);
void PathSaveDialogDefault(struct path *self);
