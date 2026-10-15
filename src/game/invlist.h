#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/list.h"

struct cstr;
struct doc_file;
struct inv_item;
struct rsrc;
struct strrsrc;

struct item_record {
	int32_t price_cents;
	int32_t weight_x16;
	int32_t portions;
	int32_t shelf_life_days;
	char groups[8];
	int32_t category;
};

struct item_table {
	int32_t count;
	struct item_record *records;
};

struct inv_list {
	struct list base;
};

extern struct item_table *g_item_catalog;
extern struct strrsrc *g_item_category_names;
extern struct strrsrc *g_item_key_names;
extern struct strrsrc *g_item_singular_names;
extern struct strrsrc *g_item_plural_names;

void InvListInit(struct inv_list *self);
void InvListInitStore(struct inv_list *self, int32_t rsrc_id, int32_t column, int32_t budget, int32_t per_person);
struct inv_list *InvListCreate(void);
struct inv_list *InvListCreateStore(int32_t rsrc_id, int32_t column, int32_t budget, int32_t per_person);
void inv_list_destroy(struct inv_list *self);
void InvListDestroy(struct inv_list **ctx_mod);

void InvListAdd(struct inv_list *self, const struct cstr *name, int32_t n, double price);
void InvListAppend(struct inv_list *self, struct inv_item *item);
void InvListClear(struct inv_list *self);
struct inv_item **InvListCollectCategory(struct inv_list *self, int32_t subtype, int32_t *count);
struct inv_item **InvListCollectGroup(struct inv_list *self, int32_t group, int32_t *count, int32_t flag);
struct inv_item *InvListFindByType(struct inv_list *self, int32_t catalog_index);
struct inv_item *InvListFindByName(struct inv_list *self, const struct cstr *name);
bool InvListHasGroup(struct inv_list *self, int32_t group);
struct cstr *InvListFormatContents(struct inv_list *self, bool append_period);
struct cstr *InvListFormatContentsPage(struct inv_list *self, struct inv_item **cursor, bool append_period);
struct cstr *InvListFormatContentsAll(struct inv_list *self, bool append_period);
struct inv_list *InvListLoseRandom(struct inv_list *self, int32_t amount, bool flag);
void InvListPurgeEmpty(struct inv_list *self);
int32_t InvListTotalWeight(struct inv_list *self);
int32_t InvListTotalValue(struct inv_list *self);
void InvListLoadItemCatalog(struct inv_list *self);
void InvListLoadState(struct inv_list *self, struct doc_file *file);
void InvListSaveState(struct inv_list *self, struct doc_file *file);

int32_t LookupItemId(const struct cstr *name);
struct strrsrc *GetItemKeyNames(void);
struct strrsrc *GetItemSingularNames(void);
struct strrsrc *GetItemPluralNames(void);
const struct item_table *GetItemTable(void);
struct strrsrc *GetItemCategoryNames(void);
struct inv_list *GenRandomFoundItems(int32_t arg0);

int32_t ItemCatalogParseRecord(const char *line, struct item_record *rec);
struct item_table *ItemCatalogBuild(struct rsrc *raw);
void ReleaseItemCatalog(void);

const char *PoundsWord(int32_t pounds);
