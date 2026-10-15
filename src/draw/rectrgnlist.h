#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "shapebase.h"

#define RECT_RGN_LIST_MAX 500

struct rect_entry {
	struct rect rect;
	struct rect_entry *next;
};

struct rect_rgn_list_ops {
	struct shape_base_ops base;
};

struct rect_rgn_list {
	struct shape_base base;
	struct rect bounds;
	bool flag;
	int32_t count;
	struct rect_entry entries[RECT_RGN_LIST_MAX];
	struct rect_entry head;
};

#define RECT_RGN_LIST_OPS(p) ((const struct rect_rgn_list_ops *) ((const struct object *) (p))->ops)

extern const struct rect_rgn_list_ops RECT_RGN_LIST_OPS_TABLE;

void RectRgnListInit(struct rect_rgn_list *self);
struct rect_rgn_list *RectRgnListCreate(void);
void RectRgnListDestroy(struct rect_rgn_list **ctx_mod);
void rect_rgn_list_destroy(struct object *self);

void rect_rgn_list_reset(struct shape_base *self);
int32_t rect_rgn_list_count(struct shape_base *self);
void rect_rgn_list_get_rect_at(struct shape_base *self, struct rect *out, int32_t index);
void rect_rgn_list_add_rect(struct shape_base *self, struct rect *rect);
bool rect_rgn_list_intersects_rect(struct shape_base *self, struct rect *rect);
