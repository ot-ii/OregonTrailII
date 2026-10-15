#pragma once

#include <stdint.h>

#include "invitem.h"

struct cstr;

enum wagon_kind {
	WAGON_CONESTOGA = 1,
	WAGON_LARGE_FARM = 2,
	WAGON_SMALL_FARM = 3,
};

struct wagon_row {
	char name[17];
	int32_t capacity_lb;
	int32_t unused;
	int32_t kind;
};

struct wagon_table {
	int32_t count;
	struct wagon_row *rows;
};

struct wagon_item {
	struct inv_item base;
	int32_t table_row;
};

extern struct wagon_table *g_wagon_table;

extern const struct inv_item_ops WAGON_ITEM_OPS_TABLE;

void WagonItemInit(struct wagon_item *self, const struct cstr *nm, int32_t qty);
struct wagon_item *WagonItemCreate(const struct cstr *nm, int32_t qty);
void WagonItemDestroy(struct wagon_item **ctx_mod);
void wagon_item_destroy(struct object *self);

void WagonItemLoadWagonRsrc(struct wagon_item *self);
char *WagonItemGetName(struct wagon_item *self, char *out);
int32_t WagonItemGetCapacity(struct wagon_item *self);
int32_t WagonItemGetUnused(struct wagon_item *self);
int32_t WagonItemGetKind(struct wagon_item *self);

void ReleaseWagonRsrc(void);
