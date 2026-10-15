#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/list.h"
#include "../base/object.h"

struct cstr;
struct doc_file;
struct inv_item;

enum item_group {
	IGRP_FISHING_GEAR = '#',
	IGRP_ANIMAL_FEED = '%',
	IGRP_FOOD = '1',
	IGRP_SEASONING = '2',
	IGRP_CLOTHING = '4',
	IGRP_FIREARMS = '7',
	IGRP_DRAFT_ANIMAL = '9',
	IGRP_WAGON = '@',
	IGRP_FOOD_GRAIN = 'A',
	IGRP_FOOD_MEAT = 'B',
	IGRP_FOOD_PRODUCE = 'C',
	IGRP_FOOD_OTHER = 'D',
	IGRP_LIVESTOCK = 'L',
	IGRP_MILK_COW = 'M',
	IGRP_CHICKEN = 'N',
	IGRP_ANTISCURVY = 'Q',
	IGRP_ANTIBERIBERI = 'R',
	IGRP_SEWING = 'T',
	IGRP_COOKWARE = 'X',
	IGRP_WATER_DAMAGEABLE = ']',
	IGRP_WAGON_PART = 'a',
	IGRP_CASH = 'c',
};

struct inv_item_ops {
	struct object_ops base;
	int32_t (*TotalWeight)(struct inv_item *self);
	int32_t (*GetUnitWeightX16)(struct inv_item *self);
};

struct inv_item {
	struct object base;
	struct list_node node;
	struct cstr *name;
	int32_t table_index;
	int32_t quantity;
	int32_t partial_portions;
	int32_t days_to_spoil;
	int32_t selected_qty;
	double price_factor;
};

#define INV_ITEM_OPS(p) ((const struct inv_item_ops *) ((const struct object *) (p))->ops)

extern const struct inv_item_ops INV_ITEM_OPS_TABLE;

void InvItemInit(struct inv_item *self, const struct cstr *nm, int32_t qty, double price);
void InvItemInitEmpty(struct inv_item *self);
struct inv_item *InvItemCreate(const struct cstr *nm, int32_t qty, double price);
struct inv_item *InvItemCreateEmpty(void);
void InvItemDestroy(struct inv_item **ctx_mod);
void inv_item_destroy(struct object *self);

struct inv_item *InvItemFromNode(struct list_node *node);

int32_t inv_item_total_weight(struct inv_item *self);
int32_t inv_item_get_unit_weight_x16(struct inv_item *self);

int32_t InvItemTotalValue(struct inv_item *self);
int32_t InvItemGetCount(struct inv_item *self, bool raw);
int32_t InvItemGetPartialPortions(struct inv_item *self);
bool InvItemInGroup(struct inv_item *self, int32_t group);
void InvItemAddUnits(struct inv_item *self, int32_t n);
struct cstr *InvItemGetNameCStr(struct inv_item *self);
bool InvItemRemoveUnits(struct inv_item *self, int32_t n);
bool InvItemInCategory(struct inv_item *self, int32_t subtype);
int32_t InvItemGetPortionsPerUnit(struct inv_item *self);
int32_t InvItemConsumePortions(struct inv_item *self, int32_t weight);
int32_t InvItemTotalPortions(struct inv_item *self);
bool InvItemIsPerishable(struct inv_item *self);
bool InvItemTickSpoilage(struct inv_item *self);
int32_t InvItemGetUnitPrice(struct inv_item *self);
struct cstr *InvItemGetName(struct inv_item *self);
struct cstr *InvItemGetNameSingular(struct inv_item *self);
struct cstr *InvItemGetNamePlural(struct inv_item *self);

void InvItemLoadState(struct inv_item *self, struct doc_file *file);
void InvItemSaveState(struct inv_item *self, struct doc_file *file);
