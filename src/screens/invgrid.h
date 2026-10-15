#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../app/event.h"
#include "../base/geom.h"
#include "../ui/picview.h"

struct cstr;
struct inv_item;
struct inv_list;

enum grid_ctl {
	GRID_AMOUNT_HEADER = 12,
	GRID_QTY_HEADER = 13,
	GRID_ITEM_HEADER = 14,
	GRID_UNIT_HEADER = 15,
	GRID_LINE_HEADER = 16,
	GRID_NEXT_PAGE = 17,
	GRID_PREV_PAGE = 18,
	GRID_TOTAL_LABEL = 19,
	GRID_TOTAL = 20,
	GRID_LIMIT_LABEL = 21,
	GRID_LIMIT = 22,
	GRID_HAVE_HEADER = 23,
	GRID_ROW_AMOUNT = 30,
	GRID_ROW_QTY = 50,
	GRID_ROW_NAME = 70,
	GRID_ROW_UNIT = 90,
	GRID_ROW_LINE = 110,
	GRID_ROW_HAVE = 130,
};

struct inv_grid;

struct inv_grid_ops {
	struct pic_view_ops base;
	struct cstr *(*GetUnitText)(struct inv_grid *self, struct inv_item *item);
	struct cstr *(*GetLineText)(struct inv_grid *self, struct inv_item *item);
	struct cstr *(*Pause)(struct inv_grid *self);
};

struct inv_grid {
	struct pic_view base;
	struct inv_list *list;
	int32_t ctl_id;
	double capacity;
	double total;
	struct inv_item **table;
	bool has_edits;
	bool owns_list;
	struct inv_list *ref_list;
	int32_t category_filter;
	int32_t page;
	int32_t count;
	int32_t last_page;
};

#define INV_GRID_OPS(p) ((const struct inv_grid_ops *) ((const struct object *) (p))->ops)

extern const struct inv_grid_ops INV_GRID_OPS_TABLE;

void InvGridInit(struct inv_grid *self, int32_t id, const struct point *pos, int32_t img_id, struct inv_list *list,
	int32_t subtype_filter, double capacity, bool has_edits, bool owns_list, int32_t ctl_id,
	bool build_party_list);
struct inv_grid *InvGridCreate(int32_t id, const struct point *pos, int32_t img_id, struct inv_list *list,
	int32_t subtype_filter, double capacity, bool has_edits, bool owns_list, int32_t ctl_id,
	bool build_party_list);
void InvGridDestroy(struct inv_grid **ctx_mod);
void inv_grid_destroy(struct object *self);

void inv_grid_on_event(struct evt_handler *self, struct cmd_event *event);

void InvGridSetLabel(struct inv_grid *self, int32_t id, struct cstr *text);

struct cstr *inv_grid_get_unit_text(struct inv_grid *self, struct inv_item *item);
struct cstr *inv_grid_get_line_text(struct inv_grid *self, struct inv_item *item);
struct cstr *inv_grid_pause(struct inv_grid *self);

void InvGridRebuildPages(struct inv_grid *self);
void InvGridRefresh(struct inv_grid *self);
void InvGridSetCell(struct inv_grid *self, struct inv_item *item, int32_t cell);
void InvGridReadTotal(struct inv_grid *self);
struct inv_item *InvGridGetItemAt(struct inv_grid *self, int32_t cell);
void InvGridSetList(struct inv_grid *self, struct inv_list *list);


struct store_grid_ops {
	struct inv_grid_ops base;
};

struct store_grid {
	struct inv_grid base;
};

#define STORE_GRID_OPS(p) ((const struct store_grid_ops *) ((const struct object *) (p))->ops)

extern const struct store_grid_ops STORE_GRID_OPS_TABLE;

void StoreGridInit(struct store_grid *self, int32_t id, const struct point *pos, int32_t img_id,
	struct inv_list *list, int32_t direct, double capacity);
struct store_grid *StoreGridCreate(int32_t id, const struct point *pos, int32_t img_id, struct inv_list *list,
	int32_t direct, double capacity);
void StoreGridDestroy(struct store_grid **ctx_mod);
void store_grid_destroy(struct object *self);

struct cstr *store_grid_get_unit_text(struct inv_grid *self, struct inv_item *item);
struct cstr *store_grid_get_line_text(struct inv_grid *self, struct inv_item *item);
struct cstr *store_grid_pause(struct inv_grid *self);


struct drop_grid_ops {
	struct inv_grid_ops base;
};

struct drop_grid {
	struct inv_grid base;
};

#define DROP_GRID_OPS(p) ((const struct drop_grid_ops *) ((const struct object *) (p))->ops)

extern const struct drop_grid_ops DROP_GRID_OPS_TABLE;

void DropGridInit(struct drop_grid *self, int32_t id, const struct point *pos, int32_t img_id, struct inv_list *list,
	int32_t direct);
struct drop_grid *DropGridCreate(int32_t id, const struct point *pos, int32_t img_id, struct inv_list *list,
	int32_t direct);
void DropGridDestroy(struct drop_grid **ctx_mod);
void drop_grid_destroy(struct object *self);

struct cstr *drop_grid_get_unit_text(struct inv_grid *self, struct inv_item *item);
struct cstr *drop_grid_get_line_text(struct inv_grid *self, struct inv_item *item);
struct cstr *drop_grid_pause(struct inv_grid *self);
