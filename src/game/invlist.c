#include "invlist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../data/assets.h"
#include "../base/byteorder.h"
#include "../base/cstr.h"
#include "../data/docfile.h"
#include "game.h"
#include "../data/gfxdev.h"
#include "invitem.h"
#include "../base/log.h"
#include "../base/memory.h"
#include "party.h"
#include "../base/rng.h"
#include "../data/rsrc.h"
#include "../base/str.h"
#include "../data/strrsrc.h"

#define ITEM_RECORD_SIZE 22

struct item_table *g_item_catalog = NULL;
struct strrsrc *g_item_category_names = NULL;
struct strrsrc *g_item_key_names = NULL;
struct strrsrc *g_item_singular_names = NULL;
struct strrsrc *g_item_plural_names = NULL;

void InvListInit(struct inv_list *self)
{
	ListInit(&self->base);

	if (g_item_catalog == NULL)
		InvListLoadItemCatalog(self);
}

void InvListInitStore(struct inv_list *self, int32_t rsrc_id, int32_t column, int32_t budget, int32_t per_person)
{
	ListInit(&self->base);

	if (g_item_catalog == NULL)
		InvListLoadItemCatalog(self);

	int32_t items_left;

	if (budget) {
		items_left = budget;

	} else {
		items_left = 1000;
	}

	struct rsrc *store_rsrc = AssetsGetElement(g_data_lib, "Store", rsrc_id, true);

	if (!store_rsrc)
		return;

	int32_t qty_lo = 0;
	int32_t qty_hi = 0;
	int32_t chance = 0;
	int32_t price_pct[8] = {0};
	char name_buf[80];
	int32_t taken = 0;

	column--;

	char *cursor = (char *) RsrcData(store_rsrc);
	char *end = cursor + RsrcSize(store_rsrc);

	do {
		cursor++;
	} while (cursor < end && *cursor != 13);

	do {
		cursor++;
	} while (cursor < end && *cursor != 13);

	cursor++;
	char *list_start = cursor;

	while (cursor < end) {
		if (*cursor != '-') {
			char *line_end = cursor;

			do {
				line_end++;
			} while (line_end < end && *line_end != 13);

			if (line_end >= end)
				break;

			*line_end = 0;

			{
				int32_t *const fields[11] = {&qty_lo, &qty_hi, &chance, &price_pct[0], &price_pct[1], &price_pct[2],
					&price_pct[3], &price_pct[4], &price_pct[5], &price_pct[6], &price_pct[7]};
				char *field = cursor;
				int32_t name_len;

				for (name_len = 0; field[name_len] && field[name_len] != '\t'; ++name_len)
					;

				int32_t copy_len = name_len < 79 ? name_len : 79;
				memcpy(name_buf, field, (size_t) copy_len);
				name_buf[copy_len] = 0;
				field += name_len;

				for (int32_t field_n = 0; field_n < 11; ++field_n) {
					if (*field == '\t')
						++field;

					if (*field == '\t' || *field == 0) {
						*fields[field_n] = field_n == 1 ? qty_lo : 0;

					} else {
						*fields[field_n] = atoi(field);
					}

					while (*field && *field != '\t')
						++field;
				}
			}

			if (RandRangeAvg(0, 99, 1) < chance) {
				int32_t qty;

				taken++;
				struct cstr *name_str = CStrCreateFrom(name_buf);
				double price_mult = ((double) price_pct[column] + 100.0) / 100.0;

				if (qty_lo > 800)
					qty_lo = 800;

				if (qty_hi > 900)
					qty_hi = 900;

				if (qty_lo < 0) {
					int32_t party_factor;

					if (per_person == -1) {
						party_factor = PartyMemberCount(GetGameObj()->party, true) * -1;

					} else {
						party_factor = per_person;
					}

					qty = party_factor * qty_lo;

				} else {
					qty = RandRangeAvg(qty_lo, qty_hi, 1);
				}

				struct inv_item *item = InvItemCreate(name_str, qty, price_mult);
				ListInsertTail(&self->base, &item->node);
				CStrDestroy(&name_str);
				cursor = ++line_end;

				if (--items_left == 0 && cursor < end)
					*cursor = '-';

			} else {
				cursor = ++line_end;
			}
		}

		if (cursor < end && *cursor == '-' && taken == 0)
			cursor = list_start;

		if (cursor < end && *cursor == '-')
			break;
	}

	RsrcDestroy(&store_rsrc);
}

struct inv_list *InvListCreate(void)
{
	struct inv_list *self = MemAlloc(1, sizeof(struct inv_list));
	InvListInit(self);

	return self;
}

struct inv_list *InvListCreateStore(int32_t rsrc_id, int32_t column, int32_t budget, int32_t per_person)
{
	struct inv_list *self = MemAlloc(1, sizeof(struct inv_list));
	InvListInitStore(self, rsrc_id, column, budget, per_person);

	return self;
}

void inv_list_destroy(struct inv_list *self)
{
	InvListClear(self);
}

void InvListDestroy(struct inv_list **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct inv_list *ctx = *ctx_mod;
	*ctx_mod = NULL;

	inv_list_destroy(ctx);

	MemFree(ctx);
}

void InvListAdd(struct inv_list *self, const struct cstr *name, int32_t n, double price)
{
	struct inv_item *found = InvListFindByName(self, name);

	if (found && !InvItemIsPerishable(found)) {
		InvItemAddUnits(found, n);

	} else {
		struct inv_item *built = InvItemCreate(name, n, price);
		ListInsertTail(&self->base, &built->node);
	}
}

void InvListAppend(struct inv_list *self, struct inv_item *item)
{
	ListInsertTail(&self->base, &item->node);
}

void InvListClear(struct inv_list *self)
{
	struct list_node *node;

	while ((node = self->base.head) != NULL) {
		ListRemove(&self->base, node);
		ObjectDelete(InvItemFromNode(node));
	}
}

struct inv_item **InvListCollectCategory(struct inv_list *self, int32_t subtype, int32_t *count)
{
	struct inv_item **items = MemAlloc((size_t) self->base.count + 1, sizeof(struct inv_item *));
	struct inv_item *item = InvItemFromNode(self->base.head);

	*count = 0;

	for (; item; item = InvItemFromNode(item->node.next)) {
		if (InvItemInCategory(item, subtype))
			items[(*count)++] = item;
	}

	items[*count] = NULL;

	return items;
}

struct inv_item **InvListCollectGroup(struct inv_list *self, int32_t group, int32_t *count, int32_t flag)
{
	struct inv_item **result = MemAlloc((size_t) self->base.count + 1, sizeof(struct inv_item *));
	struct inv_item *node = InvItemFromNode(self->base.head);

	*count = 0;

	for (; node; node = InvItemFromNode(node->node.next)) {
		if (InvItemInGroup(node, group)) {
			if (flag == 0 || (int16_t) node->days_to_spoil <= (int16_t) flag) {
				result[*count] = node;
				(*count)++;
			}
		}
	}

	result[*count] = NULL;

	return result;
}

struct inv_item *InvListFindByType(struct inv_list *self, int32_t catalog_index)
{
	struct inv_item *item = InvItemFromNode(self->base.head);

	while (item) {
		if ((int16_t) item->table_index == (int16_t) catalog_index)
			return item;

		item = InvItemFromNode(item->node.next);
	}

	return NULL;
}

struct inv_item *InvListFindByName(struct inv_list *self, const struct cstr *name)
{
	struct inv_item *item = InvItemFromNode(self->base.head);
	int32_t id = LookupItemId(name);

	while (item) {
		if ((int16_t) item->table_index == id)
			return item;

		item = InvItemFromNode(item->node.next);
	}

	return NULL;
}

bool InvListHasGroup(struct inv_list *self, int32_t group)
{
	for (struct inv_item *cur = InvItemFromNode(self->base.head); cur; cur = InvItemFromNode(cur->node.next)) {
		if (InvItemInGroup(cur, group))
			return true;
	}

	return false;
}

struct cstr *InvListFormatContents(struct inv_list *self, bool append_period)
{
	struct cstr *result = CStrCreate();
	struct inv_item *item = InvItemFromNode(self->base.head);
	char prefix[8] = " and";
	char separator = ';';
	char line_buf[256];

	while (item) {
		prefix[0] = 0;
		struct inv_item *next_item = InvItemFromNode(item->node.next);

		if (!next_item)
			separator = 0;

		if (!next_item) {
			if (self->base.count > 1)
				prefix[0] = ' ';
		}

		struct cstr *name = InvItemGetName(item);
		StrPrintf(line_buf, "%s %d %s%c", prefix, InvItemGetCount(item, false), name ? CStrGet(name) : "",
			separator);
		CStrDestroy(&name);
		CStrAppendBuf(result, line_buf);

		if (CStrLength(result) > 105) {
			CStrAppendBuf(result, " and various other items");
			item = NULL;

		} else {
			item = next_item;
		}
	}

	if (append_period)
		CStrAppendBuf(result, ".");

	return result;
}

static struct cstr *inv_format_items(struct inv_list *list, struct inv_item **cursor, bool append_period,
	int32_t page_len)
{
	struct cstr *result = CStrCreate();
	struct inv_item *item = *cursor;
	char line[256];

	while (item) {
		struct inv_item *next = InvItemFromNode(item->node.next);
		struct cstr *name = InvItemGetName(item);

		StrPrintf(line, "%s %d %s%s", (!next && list->base.count > 1) ? " and" : "", InvItemGetCount(item, false),
			name ? CStrGet(name) : "", next ? ";" : "");
		CStrDestroy(&name);
		CStrAppendBuf(result, line);
		item = next;

		if (item && page_len && CStrLength(result) > page_len) {
			CStrAppendBuf(result, " and various other items");
			*cursor = item;
			return result;
		}
	}

	*cursor = NULL;

	if (append_period)
		CStrAppendBuf(result, ".");

	return result;
}

struct cstr *InvListFormatContentsPage(struct inv_list *self, struct inv_item **cursor, bool append_period)
{
	return inv_format_items(self, cursor, append_period, 105);
}

struct cstr *InvListFormatContentsAll(struct inv_list *self, bool append_period)
{
	struct inv_item *cursor = InvItemFromNode(self->base.head);

	return inv_format_items(self, &cursor, append_period, 0);
}

struct inv_list *InvListLoseRandom(struct inv_list *self, int32_t amount, bool flag)
{
	struct inv_list *lost = InvListCreate();
	struct inv_item *item = InvItemFromNode(self->base.head);

	amount = (int16_t) amount;

	while (item) {
		int32_t lost_count = 0;
		int32_t loss_rate;

		if (flag && InvItemInGroup(item, IGRP_WATER_DAMAGEABLE)) {
			loss_rate = amount;

		} else {
			loss_rate = amount / 2;
		}

		if (loss_rate < 100) {
			for (int32_t unit_index = 0; InvItemGetCount(item, false) > unit_index; ++unit_index) {
				if (RandRangeAvg(1, 100, 1) <= loss_rate)
					++lost_count;
			}

		} else {
			lost_count = (int16_t) InvItemGetCount(item, false);
		}

		if (lost_count) {
			InvItemRemoveUnits(item, lost_count);
			InvListAdd(lost, InvItemGetNameCStr(item), lost_count, 0.0);
		}

		item = InvItemFromNode(item->node.next);
	}

	InvListPurgeEmpty(self);

	return lost;
}

void InvListPurgeEmpty(struct inv_list *self)
{
	struct inv_item *item = InvItemFromNode(self->base.head);

	while (item) {
		struct inv_item *next = InvItemFromNode(item->node.next);

		if (!InvItemGetCount(item, false)) {
			ListRemove(&self->base, &item->node);
			ObjectDelete(item);
		}

		item = next;
	}
}

int32_t InvListTotalWeight(struct inv_list *self)
{
	int32_t total = 0;

	for (struct inv_item *item = InvItemFromNode(self->base.head); item; item = InvItemFromNode(item->node.next))
		total += INV_ITEM_OPS(item)->TotalWeight(item);

	return total;
}

int32_t InvListTotalValue(struct inv_list *self)
{
	int32_t total = 0;

	for (struct inv_item *item = InvItemFromNode(self->base.head); item; item = InvItemFromNode(item->node.next))
		total += InvItemTotalValue(item);

	return total;
}

int32_t LookupItemId(const struct cstr *name)
{
	if (!g_item_key_names)
		return -1;

	return (int16_t) StrRsrcIndexOf(g_item_key_names, CStrGet(name));
}

struct strrsrc *GetItemKeyNames(void)
{
	return g_item_key_names;
}

struct strrsrc *GetItemSingularNames(void)
{
	return g_item_singular_names;
}

struct strrsrc *GetItemPluralNames(void)
{
	return g_item_plural_names;
}

const struct item_table *GetItemTable(void)
{
	return g_item_catalog;
}

struct strrsrc *GetItemCategoryNames(void)
{
	return g_item_category_names;
}

struct inv_list *GenRandomFoundItems(int32_t arg0)
{
	int32_t pick_count = arg0 / 25;
	struct strrsrc *wagon_loot = StrRsrcLoad(g_data_lib, 2200, true);
	struct inv_list *found = InvListCreate();
	int32_t last_row = wagon_loot ? StrRsrcCount(wagon_loot) : 0;

	if (last_row) {
		--last_row;

		for (int32_t pick_index = 0; pick_index < pick_count; ++pick_index) {
			const char *row = StrRsrcGetAt(wagon_loot, RandRangeAvg(0, last_row, 1));

			if (!row)
				row = "";

			int32_t qty_max = (int16_t) atoi(row);
			const char *tab = strchr(row, '\t');
			struct cstr *name = CStrCreateFrom(tab ? tab + 1 : "");
			int32_t qty = RandRangeAvg(1, qty_max, 1);
			InvListAdd(found, name, qty, 0.0);
			CStrDestroy(&name);
		}
	}

	StrRsrcDestroy(&wagon_loot);

	return found;
}

int32_t ItemCatalogParseRecord(const char *line, struct item_record *rec)
{
	float weight_lb = 0;
	char extra[64] = "";
	char key[64] = "";
	char singular[64] = "";
	char flags[64] = "";
	float price = 0;
	float portions = 0;
	char plural[64] = "";
	float shelf_life = 0;
	char category[64] = "";

	memset(rec, 0, sizeof(*rec));
	sscanf(line, "%63[^\t] %63[^\t] %63[^\t] %f %f $%f %f %63[^\t] %63[^\t] %63[^\t\r]",
		key, singular, plural, &portions, &weight_lb, &price, &shelf_life, category, extra, flags);
	StrRsrcAppend(g_item_key_names, key);
	StrRsrcAppend(g_item_singular_names, singular);
	StrRsrcAppend(g_item_plural_names, plural);
	rec->weight_x16 = (int32_t) (weight_lb * 16.0f);
	rec->price_cents = (int32_t) (price * 100.0f);
	rec->portions = (int16_t) portions;
	rec->shelf_life_days = (int16_t) shelf_life;

	if (!rec->shelf_life_days)
		rec->shelf_life_days = -1;

	rec->category = (signed char) (g_item_category_names ? StrRsrcIndexOf(g_item_category_names, category) : -1);

	int32_t flag_count = 0;

	for (const char *cursor = flags; flag_count < 8 && *cursor; ++cursor) {
		if (*cursor != ',' && *cursor != ' ') {
			rec->groups[flag_count] = *cursor;
			++flag_count;
		}
	}

	return 0;
}

struct item_table *ItemCatalogBuild(struct rsrc *raw)
{
	int32_t capacity = 200;
	char *line = (char *) RsrcData(raw);
	struct item_table *table = MemAlloc(1, sizeof(struct item_table));
	table->records = MemAlloc((size_t) capacity, sizeof(struct item_record));

	g_item_key_names = StrRsrcCreate();
	g_item_singular_names = StrRsrcCreate();
	g_item_plural_names = StrRsrcCreate();

	int32_t row = 0;
	bool done = false;
	bool header_skipped = false;

	while (!done) {
		if (!header_skipped) {
			while (*line != 13)
				++line;

			++line;
			header_skipped = true;
		}

		char *line_end;

		for (line_end = line; *line_end != 13 && *line_end; ++line_end)
			continue;

		if (*line_end == 0) {
			done = true;

		} else {
			*line_end = 0;
		}

		if (capacity == row) {
			++capacity;
			table->records = MemRealloc(table->records, (size_t) capacity, sizeof(struct item_record));
		}

		ItemCatalogParseRecord(line, &table->records[row++]);
		line = line_end + 1;
	}

	table->count = row;

	return table;
}

void ReleaseItemCatalog(void)
{
	StrRsrcDestroy(&g_item_key_names);
	StrRsrcDestroy(&g_item_singular_names);
	StrRsrcDestroy(&g_item_plural_names);

	if (g_item_catalog) {
		MemFree(g_item_catalog->records);
		MemFree(g_item_catalog);
		g_item_catalog = NULL;
	}

	StrRsrcDestroy(&g_item_category_names);
}

static struct item_table *parse_item_catalog(struct rsrc *raw)
{
	const uint8_t *data = RsrcData(raw);
	struct item_table *table = MemAlloc(1, sizeof(struct item_table));

	table->count = RsrcSize(raw) / ITEM_RECORD_SIZE;
	table->records = MemAlloc((size_t) table->count + 1, sizeof(struct item_record));

	for (int32_t i = 0; i < table->count; i++) {
		const uint8_t *p = data + i * ITEM_RECORD_SIZE;
		struct item_record *rec = &table->records[i];

		rec->price_cents = ReadLe32(p + 0);
		rec->weight_x16 = ReadLe32(p + 4);
		rec->portions = ReadLe16(p + 8);
		rec->shelf_life_days = ReadLe16(p + 10);
		memcpy(rec->groups, p + 12, 8);
		rec->category = (int8_t) p[20];
	}

	return table;
}

void InvListLoadItemCatalog(struct inv_list *self)
{
	(void) self;

	if (g_item_category_names == NULL)
		g_item_category_names = StrRsrcLoad(g_text_lib, 2102, true);

	if (g_item_catalog != NULL)
		return;

	struct rsrc *raw = AssetsGetElement(g_data_lib, "INVIT", 2100, true);

	if (!raw) {
		g_item_catalog = MemAlloc(1, sizeof(struct item_table));
		return;
	}

	if (RsrcStorage(raw) == RSRC_TEXT) {
		LogError(LOG_ERROR, "Attempt load a raw resource");
		g_item_catalog = MemAlloc(1, sizeof(struct item_table));

	} else {
		RsrcLoadAndUnpack(raw);
		g_item_catalog = parse_item_catalog(raw);
		g_item_key_names = StrRsrcLoad(g_data_lib, 2100, true);
		g_item_singular_names = StrRsrcLoad(g_text_lib, 2100, true);
		g_item_plural_names = StrRsrcLoad(g_text_lib, 2101, true);
	}

	RsrcDestroy(&raw);
}

void InvListLoadState(struct inv_list *self, struct doc_file *file)
{
	void *mem = NULL;
	int32_t size = 0;

	DocFileReadObject2(file, &mem, &size);
	int32_t item_count = size >= 2 ? ReadLe16(mem) : 0;
	MemFree(mem);

	for (int32_t item_index = 0; item_index < item_count; item_index++) {
		struct inv_item *item = InvItemCreateEmpty();
		InvItemLoadState(item, file);
		ListInsertTail(&self->base, &item->node);
	}
}

void InvListSaveState(struct inv_list *self, struct doc_file *file)
{
	uint8_t blk[2] = {0};

	WriteLe16(blk, self->base.count);
	DocFileWriteObject(file, blk, 2);

	for (struct inv_item *item = InvItemFromNode(self->base.head); item; item = InvItemFromNode(item->node.next))
		InvItemSaveState(item, file);
}

const char *PoundsWord(int32_t pounds)
{
	return pounds == 1 ? "pound" : "pounds";
}
