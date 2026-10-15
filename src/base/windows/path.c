#include "../path.h"

#include <stdio.h>
#include <string.h>

#include <windows.h>
#include <commdlg.h>

#include "../cstr.h"
#include "../memory.h"
#include "../../app/replay.h"
#include "../str.h"

#pragma comment(lib, "comdlg32.lib")

const struct object_ops PATH_OPS_TABLE = {
	path_destroy,
};

static bool file_not_found(const char *path)
{
	return GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
}

static void check_exists(struct path *self)
{
	if (file_not_found(CStrGet(self->path))) {
		self->exists = false;
		self->status = 2;

	} else {
		self->exists = true;
		self->status = 0;
	}
}

void PathInit(struct path *self)
{
	ObjectInit(&self->base);
	self->base.ops = &PATH_OPS_TABLE;
	self->status = 0;
	self->exists = false;
	self->path = CStrCreate();
}

void PathInitStr(struct path *self, const struct cstr *s)
{
	ObjectInit(&self->base);
	self->base.ops = &PATH_OPS_TABLE;
	self->status = 0;
	self->exists = false;
	self->path = CStrCreateCopy(s);

	if (file_not_found(CStrGet(self->path))) {
		self->status = 2;

	} else {
		self->exists = true;
		self->status = 0;
	}
}

void PathInitCopy(struct path *self, const struct path *src)
{
	ObjectInit(&self->base);
	self->base.ops = &PATH_OPS_TABLE;
	self->status = src->status;
	self->exists = src->exists;
	self->path = CStrCreateCopy(src->path);
}

struct path *PathCreate(void)
{
	struct path *self = MemAlloc(1, sizeof(struct path));
	PathInit(self);

	return self;
}

struct path *PathCreateStr(const struct cstr *s)
{
	struct path *self = MemAlloc(1, sizeof(struct path));
	PathInitStr(self, s);

	return self;
}

struct path *PathCreateCopy(const struct path *src)
{
	struct path *self = MemAlloc(1, sizeof(struct path));
	PathInitCopy(self, src);

	return self;
}

void PathDestroy(struct path **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void path_destroy(struct object *obj)
{
	struct path *self = (struct path *) obj;

	CStrDestroy(&self->path);

	object_destroy(obj);
}

struct path *PathAssign(struct path *self, const struct path *other)
{
	self->status = other->status;
	self->exists = other->exists;
	CStrDestroy(&self->path);
	self->path = CStrCreateCopy(other->path);

	return self;
}

void PathSetFromCStr(struct path *self, const struct cstr *src)
{
	CStrDestroy(&self->path);
	self->path = CStrCreateCopy(src);
	check_exists(self);
}

struct cstr *PathCopyPath(const struct path *self)
{
	return CStrCreateCopy(self->path);
}

int32_t PathPathLength(const struct path *self)
{
	if (!self->path)
		return 0;

	return CStrLength(self->path);
}

void PathSetPath(struct path *self, const char *s)
{
	if (!s)
		return;

	if (CStrLength(self->path) > 0) {
		if (CStrCharAt(self->path, CStrLength(self->path) - 1) != '\\')
			CStrAppendBuf(self->path, "\\");

		CStrAppendBuf(self->path, s);

	} else {
		CStrDestroy(&self->path);
		self->path = CStrCreateFrom(s);
	}

	check_exists(self);
}

void PathSetPathObj(struct path *self, const char *path_obj)
{
	if (!path_obj)
		return;

	PathSetPath(self, path_obj);
}

int32_t PathRenameTo(struct path *self, const struct path *other)
{
	struct cstr *copy = PathCopyPath(other);
	int32_t ok = PathRename(self, copy);
	CStrDestroy(&copy);

	return ok;
}

int32_t PathDoRename(struct path *self, const char *newpath)
{
	if (!newpath)
		return 1;

	int err = rename(CStrGet(self->path), newpath);

	if (!err) {
		CStrDestroy(&self->path);
		self->path = CStrCreateN(newpath, 64);
		return 0;
	}

	return 1;
}

int32_t PathRename(struct path *self, const struct cstr *newpath)
{
	if (!newpath)
		return 1;

	int err = rename(CStrGet(self->path), CStrGet(newpath));

	if (err == 0) {
		CStrAssign(self->path, newpath);
		return 0;
	}

	return 1;
}

void PathOpenDialogDefault(struct path *self)
{
	PathOpenDialog(self, NULL, NULL, NULL);
}

static void build_filter(char *filter, size_t size, char *ext, size_t ext_size, const char *filter_desc,
	const char *filter_pattern)
{
	filter[0] = 0;

	if (filter_desc) {
		StrCopyN(filter, size, filter_desc);
		StrCatN(filter, size, "&");
		StrCatN(filter, size, filter_pattern);
		StrCatN(filter, size, "&");
		const char *dot = strchr(filter_pattern, '.');

		if (dot) {
			StrCopyN(ext, ext_size, dot + 1);

		} else {
			StrCopyN(ext, ext_size, "txt");
		}
	}

	StrCatN(filter, size, "All files");
	StrCatN(filter, size, "&");
	StrCatN(filter, size, "*.*&&");

	char *cursor = filter;

	while ((cursor = strchr(cursor, '&')) != NULL) {
		*cursor = 0;
		++cursor;
	}
}

void PathOpenDialog(struct path *self, const char *filter_desc, const char *filter_pattern, void *owner)
{
	OPENFILENAMEA ofn;
	char file_buf[256];
	char filter[256];
	char ext[4] = {0};

	file_buf[0] = 0;
	memset(&ofn, 0, sizeof(ofn));
	build_filter(filter, sizeof(filter), ext, sizeof(ext), filter_desc, filter_pattern);

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = (HWND) owner;
	ofn.lpstrFilter = filter;
	ofn.lpstrCustomFilter = NULL;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = file_buf;
	ofn.nMaxFile = 256;
	ofn.lpstrFileTitle = NULL;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;
	ofn.lpstrDefExt = filter_desc ? ext : NULL;

#ifdef OTII_TWITCH
	self->status = 1;
#else
	if (!RpQueryBytes(RPQ_FILE_DIALOG, file_buf, 256, RpMode() == RP_REPLAY ? true : GetOpenFileNameA(&ofn) != 0)) {
		self->status = 1;
		return;
	}

	CStrDestroy(&self->path);
	self->path = CStrCreateFrom(file_buf);
	self->exists = true;
	self->status = 0;
#endif
}

void PathSaveDialogDefault(struct path *self)
{
	PathSaveDialog(self, NULL, NULL);
}

void PathSaveDialog(struct path *self, const char *filter_desc, const char *filter_pattern)
{
	OPENFILENAMEA ofn;
	char file_buf[256];
	char filter[256];
	char ext[4] = {0};

	file_buf[0] = 0;
	memset(&ofn, 0, sizeof(ofn));
	build_filter(filter, sizeof(filter), ext, sizeof(ext), filter_desc, filter_pattern);

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = NULL;
	ofn.lpstrFilter = filter;
	ofn.lpstrCustomFilter = NULL;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = file_buf;
	ofn.nMaxFile = 256;
	ofn.lpstrFileTitle = NULL;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
	ofn.lpstrDefExt = filter_desc ? ext : NULL;

#ifdef OTII_TWITCH
	self->status = 1;
#else
	if (!RpQueryBytes(RPQ_FILE_DIALOG, file_buf, 256, RpMode() == RP_REPLAY ? true : GetSaveFileNameA(&ofn) != 0)) {
		self->status = 1;
		return;
	}

	CStrDestroy(&self->path);
	self->path = CStrCreateFrom(file_buf);
	self->exists = true;
	self->status = 0;
#endif
}
