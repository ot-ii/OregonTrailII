#pragma once

#include <stdbool.h>
#include <stdint.h>

enum rsrc_storage {
	RSRC_NONE = 0,
	RSRC_TEXT = 1,
	RSRC_PACKED = 2,
	RSRC_NATIVE = 3,
	RSRC_NOT_FOUND = 4,
};

struct rsrc;

struct rsrc *RsrcCreate(const char *name, int32_t id, int32_t size, int32_t storage);
void RsrcDestroy(struct rsrc **ctx_mod);

uint8_t *RsrcData(struct rsrc *ctx);
int32_t RsrcSize(const struct rsrc *ctx);
void RsrcResize(struct rsrc *ctx, int32_t size);
const char *RsrcName(const struct rsrc *ctx);
int32_t RsrcId(const struct rsrc *ctx);
int32_t RsrcStorage(const struct rsrc *ctx);
int32_t RsrcLoadAndUnpack(struct rsrc *ctx);
int32_t RsrcUnpack(struct rsrc *ctx, const uint8_t *recipe);
void RsrcCacheClear(void);
int32_t RsrcUnpackTruncations(void);
