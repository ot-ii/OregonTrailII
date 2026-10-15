#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FILE_READ   1
#define FILE_WRITE  2
#define FILE_UPDATE 3

struct file;

struct file *FileOpen(const char *path, int32_t mode);
void FileClose(struct file **ctx_mod);

int32_t FileRead(struct file *ctx, void *buf, int32_t size);
int32_t FileWrite(struct file *ctx, const void *buf, int32_t size);
bool FileSeek(struct file *ctx, int64_t pos);
int64_t FileTell(struct file *ctx);
int64_t FileSize(struct file *ctx);

bool FileExists(const char *path);
bool FileDelete(const char *path);
bool FileRename(const char *from, const char *to);
void *FileReadAll(const char *path, size_t *size);
bool FileWriteAll(const char *path, const void *buf, size_t size);

void FileGetExeDir(char *dir, size_t size);
void FileGetExeName(char *name, size_t size);
