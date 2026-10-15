#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct rsrc;
struct mecclib;

struct mecclib *MeccLibCreate(const char *path);
void MeccLibDestroy(struct mecclib **ctx_mod);

bool MeccLibIsAvailable(const struct mecclib *ctx);
const char *MeccLibPath(const struct mecclib *ctx);

bool MeccLibFind(const struct mecclib *ctx, const char *name, int32_t id, int64_t *offset, int32_t *size,
	int32_t *storage);
struct rsrc *MeccLibGetElement(struct mecclib *ctx, const char *name, int32_t id);

typedef void (*MeccLibEnumFunc)(const char *name, int32_t id, int32_t storage, int32_t size, int64_t offset,
	void *opaque);
void MeccLibEnumerate(const struct mecclib *ctx, MeccLibEnumFunc func, void *opaque);
