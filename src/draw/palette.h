#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/object.h"

struct assets;

struct palette_ops {
	struct object_ops base;
};

struct palette {
	struct object base;
	bool palette_built;
	int32_t rsrc_id;
	bool owns_palette;
	uint8_t *inv_table;
	int32_t inv_table_size;
	void *hpal;
};

#define PALETTE_OPS(p) ((const struct palette_ops *) ((const struct object *) (p))->ops)

extern const struct palette_ops PALETTE_OPS_TABLE;

void PaletteInit(struct palette *self);
void PaletteInitHandle(struct palette *self, void *hpal);
void PaletteInitLoad(struct palette *self, struct assets *lib, int32_t id, bool load_inv);
struct palette *PaletteCreate(void);
struct palette *PaletteCreateHandle(void *hpal);
struct palette *PaletteCreateLoad(struct assets *lib, int32_t id, bool load_inv);
void PaletteDestroy(struct palette **ctx_mod);
void palette_destroy(struct object *self);

void *PaletteBuildGammaPalette(struct palette *self, uint8_t *colors, int32_t flag);
void PaletteIndexToRGB(struct palette *self, uint8_t index, uint8_t *r, uint8_t *g, uint8_t *b);
uint8_t PaletteRGBToIndex(struct palette *self, uint8_t r, uint8_t g, uint8_t b);
void PaletteBuildInvTable(struct palette *self);
void PaletteLockInvTable(struct palette *self);
void PaletteUnlockInvTable(struct palette *self);

extern struct palette *g_palette;
