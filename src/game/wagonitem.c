#include "wagonitem.h"

#include <string.h>

#include "../data/assets.h"
#include "../base/byteorder.h"
#include "../base/cstr.h"
#include "../data/gfxdev.h"
#include "../base/log.h"
#include "../base/memory.h"
#include "../data/rsrc.h"
#include "../base/str.h"

#define WAGON_ROW_SIZE 22

struct wagon_table *g_wagon_table = NULL;

const struct inv_item_ops WAGON_ITEM_OPS_TABLE = {
	{wagon_item_destroy},
	inv_item_total_weight,
	inv_item_get_unit_weight_x16,
};

void WagonItemInit(struct wagon_item *self, const struct cstr *nm, int32_t qty)
{
	InvItemInit(&self->base, nm, qty, 0.0);
	self->base.base.ops = &WAGON_ITEM_OPS_TABLE.base;

	if (g_wagon_table == NULL)
		WagonItemLoadWagonRsrc(self);

	const char *name_chars = CStrGet(nm);
	self->table_row = -1;

	for (int32_t row_index = 0; row_index < g_wagon_table->count; row_index++) {
		if (strcmp(name_chars, g_wagon_table->rows[row_index].name) == 0) {
			self->table_row = row_index;
			break;
		}
	}
}

struct wagon_item *WagonItemCreate(const struct cstr *nm, int32_t qty)
{
	struct wagon_item *self = MemAlloc(1, sizeof(struct wagon_item));
	WagonItemInit(self, nm, qty);

	return self;
}

void WagonItemDestroy(struct wagon_item **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void wagon_item_destroy(struct object *self)
{
	self->ops = &INV_ITEM_OPS_TABLE.base;
	inv_item_destroy(self);
}

char *WagonItemGetName(struct wagon_item *self, char *out)
{
	if (self->table_row == -1)
		return NULL;

	StrCopyN(out, sizeof(g_wagon_table->rows[0].name), g_wagon_table->rows[self->table_row].name);

	return out;
}

int32_t WagonItemGetCapacity(struct wagon_item *self)
{
	if (self->table_row == -1)
		return 0;

	return g_wagon_table->rows[self->table_row].capacity_lb;
}

int32_t WagonItemGetUnused(struct wagon_item *self)
{
	if (self->table_row == -1)
		return 0;

	return g_wagon_table->rows[self->table_row].unused;
}

int32_t WagonItemGetKind(struct wagon_item *self)
{
	if (self->table_row == -1)
		return 0;

	return g_wagon_table->rows[self->table_row].kind;
}

void ReleaseWagonRsrc(void)
{
	if (g_wagon_table) {
		MemFree(g_wagon_table->rows);
		MemFree(g_wagon_table);
	}

	g_wagon_table = NULL;
}

void WagonItemLoadWagonRsrc(struct wagon_item *self)
{
	(void) self;

	struct rsrc *raw = AssetsGetElement(g_data_lib, "WAGON", 2200, true);
	g_wagon_table = MemAlloc(1, sizeof(struct wagon_table));

	if (!raw)
		return;

	if (RsrcStorage(raw) == RSRC_TEXT) {
		LogError(LOG_ERROR, "Attempt load a raw resource");

	} else {
		RsrcLoadAndUnpack(raw);

		const uint8_t *data = RsrcData(raw);
		g_wagon_table->count = RsrcSize(raw) / WAGON_ROW_SIZE;
		g_wagon_table->rows = MemAlloc((size_t) g_wagon_table->count + 1, sizeof(struct wagon_row));

		for (int32_t i = 0; i < g_wagon_table->count; i++) {
			const uint8_t *p = data + i * WAGON_ROW_SIZE;
			struct wagon_row *row = &g_wagon_table->rows[i];

			memcpy(row->name, p, 16);
			row->name[16] = 0;
			row->capacity_lb = ReadLe16(p + 16);
			row->unused = ReadLe16(p + 18);
			row->kind = ReadLe16(p + 20);
		}
	}

	RsrcDestroy(&raw);
}
