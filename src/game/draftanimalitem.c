#include "draftanimalitem.h"

#include <string.h>

#include "../data/assets.h"
#include "../base/byteorder.h"
#include "../base/cstr.h"
#include "../data/gfxdev.h"
#include "../base/log.h"
#include "../base/memory.h"
#include "../data/rsrc.h"
#include "../base/str.h"

#define DRAFT_ANIMAL_ROW_SIZE 44

struct draft_animal_table *g_draftanimal_table = NULL;

const struct inv_item_ops DRAFT_ANIMAL_ITEM_OPS_TABLE = {
	{draft_animal_item_destroy},
	inv_item_total_weight,
	inv_item_get_unit_weight_x16,
};

void DraftAnimalItemInit(struct draft_animal_item *self, const struct cstr *nm, int32_t qty)
{
	InvItemInit(&self->base, nm, qty, 0.0);
	self->base.base.ops = &DRAFT_ANIMAL_ITEM_OPS_TABLE.base;

	if (g_draftanimal_table == NULL)
		DraftAnimalItemLoadDraftAnimalRsrc(self);

	const char *name_chars = CStrGet(nm);
	self->table_row = -1;

	for (int32_t row_index = 0; row_index < g_draftanimal_table->count; row_index++) {
		if (strcmp(name_chars, g_draftanimal_table->rows[row_index].name) == 0) {
			self->table_row = row_index;
			break;
		}
	}
}

struct draft_animal_item *DraftAnimalItemCreate(const struct cstr *nm, int32_t qty)
{
	struct draft_animal_item *self = MemAlloc(1, sizeof(struct draft_animal_item));
	DraftAnimalItemInit(self, nm, qty);

	return self;
}

void DraftAnimalItemDestroy(struct draft_animal_item **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void draft_animal_item_destroy(struct object *self)
{
	self->ops = &INV_ITEM_OPS_TABLE.base;
	inv_item_destroy(self);
}

static const struct draft_animal_row *row_of(struct draft_animal_item *self)
{
	if (self->table_row == -1 || g_draftanimal_table == NULL)
		return NULL;

	return &g_draftanimal_table->rows[self->table_row];
}

char *DraftAnimalItemGetName(struct draft_animal_item *self, char *out)
{
	const struct draft_animal_row *row = row_of(self);

	if (!row)
		return NULL;

	StrCopyN(out, sizeof(row->name), row->name);

	return out;
}

int32_t DraftAnimalItemGetSpeed(struct draft_animal_item *self)
{
	const struct draft_animal_row *row = row_of(self);

	return row ? row->speed : 0;
}

int32_t DraftAnimalItemGetStrainedLoad(struct draft_animal_item *self)
{
	const struct draft_animal_row *row = row_of(self);

	return row ? row->strained_lb : 0;
}

int32_t DraftAnimalItemGetEasyLoad(struct draft_animal_item *self)
{
	const struct draft_animal_row *row = row_of(self);

	return row ? row->easy_lb : 0;
}

int32_t DraftAnimalItemGetPerWagon(struct draft_animal_item *self)
{
	const struct draft_animal_row *row = row_of(self);

	return row ? row->per_wagon : 0;
}

int32_t DraftAnimalItemGetPaceRisk(struct draft_animal_item *self)
{
	const struct draft_animal_row *row = row_of(self);

	return row ? row->pace_risk : 0;
}

int32_t DraftAnimalItemGetUnread1(struct draft_animal_item *self)
{
	const struct draft_animal_row *row = row_of(self);

	return row ? row->unread1 : 0;
}

int32_t DraftAnimalItemGetWanderRisk(struct draft_animal_item *self)
{
	const struct draft_animal_row *row = row_of(self);

	return row ? row->wander_risk : 0;
}

int32_t DraftAnimalItemGetColdRisk(struct draft_animal_item *self)
{
	const struct draft_animal_row *row = row_of(self);

	return row ? row->cold_risk : 0;
}

int32_t DraftAnimalItemGetHeatRisk(struct draft_animal_item *self)
{
	const struct draft_animal_row *row = row_of(self);

	return row ? row->heat_risk : 0;
}

int32_t DraftAnimalItemGetHoleRisk(struct draft_animal_item *self)
{
	const struct draft_animal_row *row = row_of(self);

	return row ? row->hole_risk : 0;
}

int32_t DraftAnimalItemGetStormRisk(struct draft_animal_item *self)
{
	const struct draft_animal_row *row = row_of(self);

	return row ? row->storm_risk : 0;
}

int32_t DraftAnimalItemGetUnread2(struct draft_animal_item *self)
{
	const struct draft_animal_row *row = row_of(self);

	return row ? row->unread2 : 0;
}

int32_t DraftAnimalItemGetUnread3(struct draft_animal_item *self)
{
	const struct draft_animal_row *row = row_of(self);

	return row ? row->unread3 : 0;
}

int32_t DraftAnimalItemGetFeedNeed(struct draft_animal_item *self)
{
	const struct draft_animal_row *row = row_of(self);

	return row ? row->feed_need : 0;
}

int32_t DraftAnimalItemStrainedTeamLoad(struct draft_animal_item *self, int32_t team)
{
	if (self->table_row == -1 || g_draftanimal_table == NULL)
		return 0;

	return DraftAnimalItemGetStrainedLoad(self) * DraftAnimalItemWorkUnits(self, team);
}

int32_t DraftAnimalItemEasyTeamLoad(struct draft_animal_item *self, int32_t team)
{
	if (self->table_row == -1 || g_draftanimal_table == NULL)
		return 0;

	return DraftAnimalItemGetEasyLoad(self) * DraftAnimalItemWorkUnits(self, team);
}

int32_t DraftAnimalItemWorkUnits(struct draft_animal_item *self, int32_t team)
{
	if (self->table_row == -1 || g_draftanimal_table == NULL)
		return 0;

	int32_t have = InvItemGetCount(&self->base, false);
	int32_t usable = team * DraftAnimalItemGetPerWagon(self);

	return usable >= have ? have : usable;
}

void ReleaseDraftanimalRsrc(void)
{
	if (g_draftanimal_table) {
		MemFree(g_draftanimal_table->rows);
		MemFree(g_draftanimal_table);
	}

	g_draftanimal_table = NULL;
}

void DraftAnimalItemLoadDraftAnimalRsrc(struct draft_animal_item *self)
{
	(void) self;

	struct rsrc *raw = AssetsGetElement(g_data_lib, "DRFTA", 2200, true);
	g_draftanimal_table = MemAlloc(1, sizeof(struct draft_animal_table));

	if (!raw)
		return;

	if (RsrcStorage(raw) == RSRC_TEXT) {
		LogError(LOG_ERROR, "Attempt load a raw resource");

	} else {
		RsrcLoadAndUnpack(raw);

		const uint8_t *data = RsrcData(raw);
		g_draftanimal_table->count = RsrcSize(raw) / DRAFT_ANIMAL_ROW_SIZE;
		g_draftanimal_table->rows = MemAlloc((size_t) g_draftanimal_table->count + 1,
			sizeof(struct draft_animal_row));

		for (int32_t i = 0; i < g_draftanimal_table->count; i++) {
			const uint8_t *p = data + i * DRAFT_ANIMAL_ROW_SIZE;
			struct draft_animal_row *row = &g_draftanimal_table->rows[i];

			memcpy(row->name, p, 16);
			row->name[16] = 0;
			row->speed = ReadLe16(p + 16);
			row->strained_lb = ReadLe16(p + 18);
			row->easy_lb = ReadLe16(p + 20);
			row->per_wagon = ReadLe16(p + 22);
			row->pace_risk = ReadLe16(p + 24);
			row->unread1 = ReadLe16(p + 26);
			row->wander_risk = ReadLe16(p + 28);
			row->cold_risk = ReadLe16(p + 30);
			row->heat_risk = ReadLe16(p + 32);
			row->hole_risk = ReadLe16(p + 34);
			row->storm_risk = ReadLe16(p + 36);
			row->unread2 = ReadLe16(p + 38);
			row->unread3 = ReadLe16(p + 40);
			row->feed_need = ReadLe16(p + 42);
		}
	}

	RsrcDestroy(&raw);
}
