#include "../file.h"

#include <string.h>

#include <windows.h>

#include "../memory.h"
#include "../str.h"

struct file {
	HANDLE handle;
};

struct file *FileOpen(const char *path, int32_t mode)
{
	DWORD access = GENERIC_READ;
	DWORD share = FILE_SHARE_READ;
	DWORD disposition = OPEN_EXISTING;

	if (mode == FILE_WRITE) {
		access = GENERIC_WRITE;
		share = 0;
		disposition = CREATE_ALWAYS;

	} else if (mode == FILE_UPDATE) {
		access = GENERIC_READ | GENERIC_WRITE;
		share = 0;
	}

	HANDLE handle = CreateFileA(path, access, share, NULL, disposition, FILE_ATTRIBUTE_NORMAL, NULL);

	if (handle == INVALID_HANDLE_VALUE)
		return NULL;

	struct file *ctx = MemAlloc(1, sizeof(struct file));
	ctx->handle = handle;

	return ctx;
}

void FileClose(struct file **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct file *ctx = *ctx_mod;
	*ctx_mod = NULL;

	CloseHandle(ctx->handle);

	MemFree(ctx);
}

int32_t FileRead(struct file *ctx, void *buf, int32_t size)
{
	DWORD done = 0;

	if (size <= 0 || !ReadFile(ctx->handle, buf, (DWORD) size, &done, NULL))
		return 0;

	return (int32_t) done;
}

int32_t FileWrite(struct file *ctx, const void *buf, int32_t size)
{
	DWORD done = 0;

	if (size <= 0 || !WriteFile(ctx->handle, buf, (DWORD) size, &done, NULL))
		return 0;

	return (int32_t) done;
}

bool FileSeek(struct file *ctx, int64_t pos)
{
	LARGE_INTEGER to;
	to.QuadPart = pos;

	return SetFilePointerEx(ctx->handle, to, NULL, FILE_BEGIN) != 0;
}

int64_t FileTell(struct file *ctx)
{
	LARGE_INTEGER zero, at;
	zero.QuadPart = 0;

	if (!SetFilePointerEx(ctx->handle, zero, &at, FILE_CURRENT))
		return -1;

	return at.QuadPart;
}

int64_t FileSize(struct file *ctx)
{
	LARGE_INTEGER size;

	if (!GetFileSizeEx(ctx->handle, &size))
		return -1;

	return size.QuadPart;
}

bool FileExists(const char *path)
{
	DWORD attr = GetFileAttributesA(path);

	return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool FileDelete(const char *path)
{
	return DeleteFileA(path) != 0;
}

bool FileRename(const char *from, const char *to)
{
	return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING) != 0;
}

void *FileReadAll(const char *path, size_t *size)
{
	*size = 0;

	struct file *f = FileOpen(path, FILE_READ);

	if (!f)
		return NULL;

	int64_t len = FileSize(f);

	if (len < 0 || len > 0x7FFFFFFF) {
		FileClose(&f);
		return NULL;
	}

	uint8_t *buf = MemAlloc((size_t) len + 1, 1);

	if (FileRead(f, buf, (int32_t) len) != (int32_t) len) {
		MemFree(buf);
		FileClose(&f);
		return NULL;
	}

	FileClose(&f);
	*size = (size_t) len;

	return buf;
}

bool FileWriteAll(const char *path, const void *buf, size_t size)
{
	struct file *f = FileOpen(path, FILE_WRITE);

	if (!f)
		return false;

	bool ok = size <= 0x7FFFFFFF && FileWrite(f, buf, (int32_t) size) == (int32_t) size;
	FileClose(&f);

	return ok;
}

void FileGetExeDir(char *dir, size_t size)
{
	char path[MAX_PATH];
	DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);

	if (n == 0 || n >= MAX_PATH) {
		StrCopyN(dir, size, ".\\");
		return;
	}

	char *slash = strrchr(path, '\\');

	if (slash)
		slash[1] = 0;

	StrCopyN(dir, size, path);
}

void FileGetExeName(char *name, size_t size)
{
	char path[MAX_PATH];
	DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);

	if (n == 0 || n >= MAX_PATH) {
		StrCopyN(name, size, "otii");
		return;
	}

	char *base = strrchr(path, '\\');
	base = base ? base + 1 : path;

	char *dot = strrchr(base, '.');

	if (dot)
		*dot = 0;

	StrCopyN(name, size, base);
}
