#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct rsrc;
struct mecclib;

struct asset_source_ops {
	struct rsrc *(*get_element)(void *source, const char *name, int32_t id);
	bool (*get_info)(void *source, const char *name, int32_t id, char *path, size_t path_size, int64_t *offset,
		int32_t *size);
	bool (*is_available)(void *source);
	void (*destroy)(void *source);
};

#define ASSETS_PRIMARY  0x1

struct assets;

struct assets *AssetsCreate(void);
void AssetsDestroy(struct assets **ctx_mod);

void AssetsAddSource(struct assets *ctx, const struct asset_source_ops *ops, void *source, uint32_t flags);
void AssetsAddOverride(struct assets *ctx, const struct asset_source_ops *ops, void *source);
void AssetsAddMeccLib(struct assets *ctx, struct mecclib *lib, uint32_t flags);

bool AssetsIsAvailable(const struct assets *ctx);

int32_t AssetsSourceCount(const struct assets *ctx);
struct mecclib *AssetsSourceMeccLib(const struct assets *ctx, int32_t index);

struct rsrc *AssetsGetElement(struct assets *ctx, const char *name, int32_t id, bool fatal);
bool AssetsGetElementInfo(struct assets *ctx, const char *name, int32_t id, char *path, size_t path_size,
	int64_t *offset, int32_t *size);
