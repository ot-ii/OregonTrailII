#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "../draw/shape.h"

struct poly_ctrl;
struct screen_dc;

struct poly_point {
	int32_t x;
	int32_t y;
};

struct poly_ctrl_ops {
	struct shape_ops base;
	void (*Move)(struct poly_ctrl *self, const struct point *delta);
};

struct poly_ctrl {
	struct shape base;
	int32_t point_count;
	int32_t edge_count;
	bool closed;
	struct poly_point *points;
};

#define POLY_CTRL_OPS(p) ((const struct poly_ctrl_ops *) ((const struct object *) (p))->ops)

extern const struct poly_ctrl_ops POLY_CTRL_OPS_TABLE;

void PolyCtrlInit(struct poly_ctrl *self, int32_t point_count, const struct point *points, bool closed,
	int32_t pen_color, int32_t brush_color, int32_t alt_brush_color, int32_t pen_width);
struct poly_ctrl *PolyCtrlCreate(int32_t point_count, const struct point *points, bool closed, int32_t pen_color,
	int32_t brush_color, int32_t alt_brush_color, int32_t pen_width);
void PolyCtrlDestroy(struct poly_ctrl **ctx_mod);
void poly_ctrl_destroy(struct object *self);

void poly_ctrl_draw_brush20(struct shape *self, struct screen_dc *dc);
void poly_ctrl_draw_brush12(struct shape *self, struct screen_dc *dc);
void poly_ctrl_draw_outline(struct shape *self, struct screen_dc *dc);
void poly_ctrl_draw_invert(struct shape *self, struct screen_dc *dc);
void poly_ctrl_draw(struct shape *self, struct screen_dc *dc);
void poly_ctrl_set_rect(struct shape *self, struct rect *rect);
struct rect poly_ctrl_get_rect_by_val(struct shape *self);
void poly_ctrl_move(struct poly_ctrl *self, const struct point *delta);

bool PolyCtrlContainsPoint(struct poly_ctrl *self, const struct point *pt);
void PolyCtrlSetPoints(struct poly_ctrl *self, int32_t point_count, const struct point *points, bool closed);
void PolyCtrlBuildPoints(struct poly_ctrl *self, const struct point *origin);
bool PolyCtrlInsertPoint(struct poly_ctrl *self, const struct point *pt, int32_t idx);
bool PolyCtrlMovePoint(struct poly_ctrl *self, const struct point *delta, int32_t idx);
bool PolyCtrlGetEdge(struct poly_ctrl *self, int32_t idx, struct point *start, struct point *end);
int32_t PolyCtrlFindNearEdge(struct poly_ctrl *self, const struct point *pt, int32_t threshold);
bool PolyCtrlPointNearSegment(struct poly_ctrl *self, const struct point *p1, const struct point *p2,
	const struct point *pt, int32_t thresh);
bool PolyCtrlNearestPoints(struct poly_ctrl *self, struct poly_ctrl *other, int32_t *best_this, int32_t *best_other,
	int32_t *prev_this, int32_t *prev_other, int32_t *pdx, int32_t *pdy, int32_t thresh);
bool PolyCtrlVerticesNear(struct poly_ctrl *self, struct poly_ctrl *other, int32_t thresh);
bool PolyCtrlRegionsIntersect(struct poly_ctrl *self, struct poly_ctrl *other);
bool PolyCtrlEdgesNear(struct poly_ctrl *self, struct poly_ctrl *other, const struct poly_point *buf1,
	const struct poly_point *buf2, int32_t thresh);
bool PolyCtrlEdgesNear2(struct poly_ctrl *self, struct poly_ctrl *other, int32_t thresh);
bool PolyCtrlCombine(struct poly_ctrl *self, struct poly_ctrl *other, int32_t thresh);
bool PolyCtrlDeleteVertex(struct poly_ctrl *self, int32_t idx);
bool PolyCtrlGetPoint(struct poly_ctrl *self, int32_t idx, struct point *out);


struct line_ctrl_ops {
	struct poly_ctrl_ops base;
};

struct line_ctrl {
	struct poly_ctrl base;
};

#define LINE_CTRL_OPS(p) ((const struct line_ctrl_ops *) ((const struct object *) (p))->ops)

extern const struct line_ctrl_ops LINE_CTRL_OPS_TABLE;

void LineCtrlInit(struct line_ctrl *self, const struct point *p0, const struct point *p1, int32_t pen_color,
	int32_t brush_color, int32_t alt_brush_color, int32_t pen_width);
struct line_ctrl *LineCtrlCreate(const struct point *p0, const struct point *p1, int32_t pen_color,
	int32_t brush_color, int32_t alt_brush_color, int32_t pen_width);
void LineCtrlDestroy(struct line_ctrl **ctx_mod);
void line_ctrl_destroy(struct object *self);
