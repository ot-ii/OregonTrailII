#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "../base/object.h"

struct datafile;
struct screen_dc;

struct region_ops {
	struct object_ops base;
};

struct region {
	struct object base;
	void *hrgn;
	int32_t state;
	int32_t corner_w;
	int32_t corner_h;
};

#define REGION_OPS(p) ((const struct region_ops *) ((const struct object *) (p))->ops)

extern const struct region_ops REGION_OPS_TABLE;

void RegionInit(struct region *self);
void RegionInitCopy(struct region *self, struct region *src);
void RegionInitRect(struct region *self, struct rect *rect, int32_t state, int32_t cw, int32_t ch);
void RegionInitDc(struct region *self, int32_t unused_x, int32_t kind, struct screen_dc *dc);
struct region *RegionCreate(void);
struct region *RegionCreateCopy(struct region *src);
struct region *RegionCreateRect(struct rect *rect, int32_t state, int32_t cw, int32_t ch);
struct region *RegionCreateDc(int32_t unused_x, int32_t kind, struct screen_dc *dc);
void RegionDestroy(struct region **ctx_mod);
void region_destroy(struct object *self);

void RegionOnSave(struct region *self, struct datafile *file);
void RegionOnLoad(struct region *self, struct datafile *file);
void RegionSetState(struct region *self, int32_t code);
void RegionUnion(struct region *self, struct region *other);
void RegionSubtract(struct region *self, struct region *other);
void RegionIntersect(struct region *self, struct region *other);
int32_t RegionPaint(struct region *self, struct screen_dc *dc, int32_t brush);
void RegionResetState(struct region *self, struct region *other);
struct region *RegionAssign(struct region *self, struct region *other);
struct region *RegionAssignRect(struct region *self, struct rect *rect);
struct region *RegionUnionInplace(struct region *self, struct region *other);
struct region *RegionFromRect(struct region *self, struct rect *rect);
struct region *RegionSubtractInplace(struct region *self, struct region *other);
struct region *RegionSubtractRect(struct region *self, struct rect *rect);

void RegionUnionOf(struct region *out, struct region *src, struct region *other);
void RegionUnionRect(struct region *out, struct rect *rect, struct region *other);
void RegionUnionRect2(struct region *out, struct region *other, struct rect *rect);
void RegionDiffOf(struct region *out, struct region *src, struct region *other);
void RegionDiffRect(struct region *out, struct rect *rect, struct region *other);
void RegionDiffRect2(struct region *out, struct region *other, struct rect *rect);
bool RegionEquals(struct region *lhs, struct region *rhs);
bool RegionNotEquals(struct region *lhs, struct region *rhs);
