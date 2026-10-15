#include "../palette.h"

#include <math.h>
#include <string.h>

#include <windows.h>

#include "../../data/assets.h"
#include "../../base/memory.h"
#include "../../data/rsrc.h"

#define INV_TABLE_SIZE 4100

const struct palette_ops PALETTE_OPS_TABLE = {
	{palette_destroy},
};

void PaletteInit(struct palette *self)
{
	ObjectInit(&self->base);
	self->base.ops = &PALETTE_OPS_TABLE.base;

	self->rsrc_id = -1;
	self->inv_table = NULL;
	self->palette_built = false;
}

void PaletteInitHandle(struct palette *self, void *hpal)
{
	ObjectInit(&self->base);
	self->base.ops = &PALETTE_OPS_TABLE.base;

	self->rsrc_id = -1;
	self->inv_table = NULL;
	self->owns_palette = false;
	self->hpal = hpal;
	self->palette_built = false;
}

void PaletteInitLoad(struct palette *self, struct assets *lib, int32_t id, bool load_inv)
{
	ObjectInit(&self->base);
	self->base.ops = &PALETTE_OPS_TABLE.base;

	self->rsrc_id = id;
	self->inv_table = NULL;
	self->hpal = NULL;
	self->owns_palette = true;
	self->palette_built = false;

	if (lib) {
		struct rsrc *rsrc = AssetsGetElement(lib, "COLRLST", id, true);

		if (rsrc) {
			uint8_t *colors = RsrcData(rsrc);
			self->hpal = PaletteBuildGammaPalette(self, colors, 1);
			RsrcDestroy(&rsrc);
			self->palette_built = true;
		}

		if (load_inv) {
			struct rsrc *inv = AssetsGetElement(lib, "INVTB", id, false);

			if (inv) {
				int32_t size = RsrcSize(inv);
				self->inv_table_size = size > INV_TABLE_SIZE ? size : INV_TABLE_SIZE;
				self->inv_table = MemAlloc((size_t) self->inv_table_size, 1);

				if (size > 0)
					memcpy(self->inv_table, RsrcData(inv), (size_t) size);

				RsrcDestroy(&inv);
			}
		}
	}
}

struct palette *PaletteCreate(void)
{
	struct palette *self = MemAlloc(1, sizeof(struct palette));
	PaletteInit(self);

	return self;
}

struct palette *PaletteCreateHandle(void *hpal)
{
	struct palette *self = MemAlloc(1, sizeof(struct palette));
	PaletteInitHandle(self, hpal);

	return self;
}

struct palette *PaletteCreateLoad(struct assets *lib, int32_t id, bool load_inv)
{
	struct palette *self = MemAlloc(1, sizeof(struct palette));
	PaletteInitLoad(self, lib, id, load_inv);

	return self;
}

void PaletteDestroy(struct palette **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void palette_destroy(struct object *obj)
{
	struct palette *self = (struct palette *) obj;

	if (self->inv_table) {
		MemFree(self->inv_table);
		self->inv_table = NULL;
	}

	if (self->owns_palette && self->hpal)
		DeleteObject((HGDIOBJ) self->hpal);

	object_destroy(obj);
}

void PaletteIndexToRGB(struct palette *self, uint8_t index, uint8_t *r, uint8_t *g, uint8_t *b)
{
	PALETTEENTRY pe;

	GetPaletteEntries((HPALETTE) self->hpal, index, 1, &pe);
	*r = pe.peRed;
	*b = pe.peBlue;
	*g = pe.peGreen;
}

uint8_t PaletteRGBToIndex(struct palette *self, uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t index = 0;

	if (!self->inv_table)
		PaletteBuildInvTable(self);

	if (self->inv_table) {
		int32_t packed = ((r & 0xF0) << 4) | (g & 0xF0) | (b >> 4);
		index = self->inv_table[packed];
	}

	return index;
}

void PaletteBuildInvTable(struct palette *self)
{
	self->inv_table_size = INV_TABLE_SIZE;
	self->inv_table = MemAlloc(INV_TABLE_SIZE, 1);

	for (int32_t red = 0; red < 0x10; ++red) {
		for (int32_t green = 0; green < 0x10; ++green) {
			for (int32_t blue = 0; blue < 0x10; ++blue) {
				COLORREF color = RGB((BYTE) ((red & 0xF) | (red << 4)), (BYTE) ((green & 0xF) | (green << 4)),
					(BYTE) ((blue & 0xF) | (blue << 4)));
				int32_t table_index = (red << 8) | (green << 4) | blue;
				self->inv_table[table_index] = (uint8_t) GetNearestPaletteIndex((HPALETTE) self->hpal, color);
			}
		}
	}
}

void PaletteLockInvTable(struct palette *self)
{
	if (!self->inv_table)
		PaletteBuildInvTable(self);
}

void PaletteUnlockInvTable(struct palette *self)
{
	(void) self;
}

void *PaletteBuildGammaPalette(struct palette *self, uint8_t *colors, int32_t flag)
{
	double gamma = 1.4;
	int32_t entry_base = 0;
	HDC hdc = GetDC(NULL);
	LOGPALETTE *logpal = MemAlloc(0x408, 1);
	logpal->palVersion = 0x300;
	logpal->palNumEntries = 256;

	if (colors) {
		if (colors[0] < 0xEC)
			entry_base = 10;

		for (int32_t index = 0; colors[0] >= index; ++index) {
			double value = (double) colors[4 * index + 2];
			value = pow(value / 256.0, 1.0 / gamma);
			colors[4 * index + 2] = (uint8_t) (int32_t) (value * 256.0);
			value = (double) colors[4 * index + 3];
			value = pow(value / 256.0, 1.0 / gamma);
			colors[4 * index + 3] = (uint8_t) (int32_t) (value * 256.0);
			value = (double) colors[4 * index + 4];
			value = pow(value / 256.0, 1.0 / gamma);
			colors[4 * index + 4] = (uint8_t) (int32_t) (value * 256.0);
			logpal->palPalEntry[entry_base + index].peRed = colors[4 * index + 2];
			logpal->palPalEntry[entry_base + index].peGreen = colors[4 * index + 3];
			logpal->palPalEntry[entry_base + index].peBlue = colors[4 * index + 4];
			logpal->palPalEntry[entry_base + index].peFlags = 4;
		}
	}

	if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE) {
		GetSystemPaletteEntries(hdc, 0, 10, logpal->palPalEntry);
		GetSystemPaletteEntries(hdc, 0xF6, 10, &logpal->palPalEntry[246]);

	} else {
		for (int32_t index = 0; index < 10; ++index) {
			logpal->palPalEntry[index].peRed = 0;
			logpal->palPalEntry[index].peGreen = 0;
			logpal->palPalEntry[index].peBlue = 0;
			logpal->palPalEntry[index].peFlags = 0;
			logpal->palPalEntry[index + 246].peRed = 0xFF;
			logpal->palPalEntry[index + 246].peGreen = 0xFF;
			logpal->palPalEntry[index + 246].peBlue = 0xFF;
			logpal->palPalEntry[index + 246].peFlags = 0;
		}
	}

	HPALETTE palette = CreatePalette(logpal);
	MemFree(logpal);
	ReleaseDC(NULL, hdc);

	return palette;
}
