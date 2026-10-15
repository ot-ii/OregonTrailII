#include "../polyctrl.h"

#include <stddef.h>

#include <windows.h>

#include "../../base/memory.h"
#include "../../draw/screendc.h"

const struct poly_ctrl_ops POLY_CTRL_OPS_TABLE = {
	{
		{poly_ctrl_destroy},
		poly_ctrl_draw_brush20,
		poly_ctrl_draw_brush12,
		poly_ctrl_draw_outline,
		poly_ctrl_draw_invert,
		poly_ctrl_draw,
		poly_ctrl_get_rect_by_val,
		poly_ctrl_set_rect,
		shape_make_region,
	},
	poly_ctrl_move,
};

void PolyCtrlInit(struct poly_ctrl *self, int32_t point_count, const struct point *points, bool closed,
	int32_t pen_color, int32_t brush_color, int32_t alt_brush_color, int32_t pen_width)
{
	ShapeInit(&self->base, pen_color, brush_color, alt_brush_color, pen_width, 0);
	self->base.base.ops = &POLY_CTRL_OPS_TABLE.base.base;

	self->points = NULL;
	PolyCtrlSetPoints(self, point_count, points, closed);
}

struct poly_ctrl *PolyCtrlCreate(int32_t point_count, const struct point *points, bool closed, int32_t pen_color,
	int32_t brush_color, int32_t alt_brush_color, int32_t pen_width)
{
	struct poly_ctrl *self = MemAlloc(1, sizeof(struct poly_ctrl));
	PolyCtrlInit(self, point_count, points, closed, pen_color, brush_color, alt_brush_color, pen_width);

	return self;
}

void PolyCtrlDestroy(struct poly_ctrl **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void poly_ctrl_destroy(struct object *self)
{
	struct poly_ctrl *p = (struct poly_ctrl *) self;

	MemFree(p->points);
	p->points = NULL;

	self->ops = &SHAPE_OPS_TABLE.base;
	shape_destroy(self);
}

bool PolyCtrlInsertPoint(struct poly_ctrl *self, const struct point *pt, int32_t idx)
{
	bool inserted = false;

	if (self->point_count < 1 || self->point_count <= idx)
		return false;

	struct poly_point *old_points = self->points;
	int32_t new_h = pt->h;
	int32_t new_v = pt->v;
	++idx;
	int32_t dst_count = 0;
	struct poly_point *new_points = MemAlloc((size_t) self->point_count + 1, sizeof(struct poly_point));
	new_points[0] = old_points[0];
	++dst_count;

	for (int32_t src_index = 1; self->point_count > src_index; ++src_index) {
		if (src_index == idx
			&& (old_points[src_index - 1].x != new_h || old_points[src_index - 1].y != new_v)
			&& (old_points[src_index].x != new_h || old_points[src_index].y != new_v)) {
			new_points[dst_count].x = new_h;
			new_points[dst_count].y = new_v;
			++dst_count;
			inserted = true;
		}

		new_points[dst_count].x = old_points[src_index].x;
		new_points[dst_count].y = old_points[src_index].y;
		++dst_count;
	}

	if (self->point_count == idx) {
		new_points[dst_count].x = new_h;
		new_points[dst_count].y = new_v;
		++dst_count;
		inserted = true;
	}

	MemFree(self->points);
	self->points = new_points;
	self->edge_count = dst_count;
	self->point_count = self->edge_count;

	if (!self->closed)
		--self->edge_count;

	return inserted;
}

void poly_ctrl_set_rect(struct shape *self, struct rect *rect)
{
	struct poly_ctrl *p = (struct poly_ctrl *) self;

	if (p->point_count < 1)
		return;

	struct rect old_bounds = shape_get_rect_by_val(self);
	struct poly_point *pts = p->points;

	for (int32_t i = 0; p->point_count > i; ++i) {
		if (old_bounds.right - old_bounds.left)
			pts[i].x = rect->left + (pts[i].x - old_bounds.left) * (rect->right - rect->left) /
				(old_bounds.right - old_bounds.left);

		if (old_bounds.bottom - old_bounds.top)
			pts[i].y = rect->top + (pts[i].y - old_bounds.top) * (rect->bottom - rect->top) /
				(old_bounds.bottom - old_bounds.top);
	}

	shape_set_rect(self, rect);
}

struct rect poly_ctrl_get_rect_by_val(struct shape *self)
{
	return shape_get_rect_by_val(self);
}

void poly_ctrl_draw_brush20(struct shape *self, struct screen_dc *dc)
{
	struct poly_ctrl *p = (struct poly_ctrl *) self;

	if (!p->points)
		return;

	if (!p->closed) {
		SHAPE_OPS(self)->DrawOutline(self, dc);
		return;
	}

	HDC hdc = dc->hdc;
	ScreenDcSetBrush(dc, self->brushColor);
	ScreenDcSetPen(dc, self->penColor, self->penWidth, self->penStyle);
	Polygon(hdc, (const POINT *) p->points, p->point_count);
}

void poly_ctrl_draw_brush12(struct shape *self, struct screen_dc *dc)
{
	struct poly_ctrl *p = (struct poly_ctrl *) self;

	if (!p->points)
		return;

	if (!p->closed) {
		int32_t saved_color = self->brushColor;
		self->brushColor = self->altBrushColor;
		SHAPE_OPS(self)->DrawOutline(self, dc);
		self->brushColor = saved_color;
		return;
	}

	ScreenDcSetBrush(dc, self->altBrushColor);
	ScreenDcSetPen(dc, self->penColor, self->penWidth, self->penStyle);
	HRGN region = CreatePolygonRgn((const POINT *) p->points, p->point_count, ALTERNATE);

	DeleteObject(region);
}

void poly_ctrl_draw_outline(struct shape *self, struct screen_dc *dc)
{
	struct poly_ctrl *p = (struct poly_ctrl *) self;

	if (!p->points)
		return;

	HDC hdc = dc->hdc;
	ScreenDcSetPen(dc, self->penColor, self->penWidth, self->penStyle);
	HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

	if (p->closed) {
		Polygon(hdc, (const POINT *) p->points, p->point_count);

	} else {
		Polyline(hdc, (const POINT *) p->points, p->point_count);
	}

	SelectObject(hdc, old_brush);
}

bool PolyCtrlRegionsIntersect(struct poly_ctrl *self, struct poly_ctrl *other)
{
	if (!self->points || !other->points)
		return false;

	if (self->point_count < 3 || other->point_count < 3)
		return false;

	HRGN dst = CreateRectRgn(0, 0, 0, 0);
	HRGN src1 = CreatePolygonRgn((const POINT *) self->points, self->point_count, ALTERNATE);
	HRGN src2 = CreatePolygonRgn((const POINT *) other->points, other->point_count, ALTERNATE);
	int combined = CombineRgn(dst, src1, src2, RGN_AND);
	bool intersects = combined != NULLREGION;
	DeleteObject(dst);
	DeleteObject(src1);
	DeleteObject(src2);

	return intersects;
}

bool PolyCtrlPointNearSegment(struct poly_ctrl *self, const struct point *p1, const struct point *p2,
	const struct point *pt, int32_t thresh)
{
	double dist_sq, dh, dv;
	double thresh_sq = (double) (thresh * thresh);

	dh = (double) (pt->h - p1->h);
	dv = (double) (pt->v - p1->v);
	dist_sq = dv * dv + dh * dh;

	if (dist_sq <= thresh_sq)
		return true;

	dh = (double) (pt->h - p2->h);
	dv = (double) (pt->v - p2->v);
	dist_sq = dv * dv + dh * dh;

	if (dist_sq <= thresh_sq)
		return true;

	if (p1->v == p2->v) {
		dist_sq = (double) (pt->v - p1->v);

		if (dist_sq < 0.0)
			dist_sq = dist_sq * -1.0;

		if ((double) thresh >= dist_sq
			&& ((pt->h <= p1->h && pt->h >= p2->h) || (pt->h >= p1->h && pt->h <= p2->h)))
			return true;

		return false;
	}

	if (p1->h == p2->h) {
		dist_sq = (double) (pt->h - p1->h);

		if (dist_sq < 0.0)
			dist_sq = dist_sq * -1.0;

		if ((double) thresh >= dist_sq
			&& ((pt->v <= p1->v && pt->v >= p2->v) || (pt->v >= p1->v && pt->v <= p2->v)))
			return true;

		return false;
	}

	double slope = ((double) p1->v - p2->v) / ((double) p1->h - p2->h);
	double slope_sq = slope * slope;
	double proj_h = ((pt->v - p1->v) * slope + p1->h * slope_sq + pt->h) / (slope_sq + 1.0);
	double proj_v = (proj_h - p1->h) * slope + p1->v;

	if ((p1->h >= proj_h && p2->h <= proj_h) || (p1->h <= proj_h && p2->h >= proj_h)) {
		dh = pt->h - proj_h;
		dv = pt->v - proj_v;
		dist_sq = dv * dv + dh * dh;

		if (dist_sq <= thresh_sq)
			return true;
	}

	return false;
}

bool PolyCtrlEdgesNear(struct poly_ctrl *self, struct poly_ctrl *other, const struct poly_point *buf1,
	const struct poly_point *buf2, int32_t thresh)
{
	struct point p1, p2, vertex;
	int32_t other_count = other->point_count;
	bool other_closed = other->closed;

	if (self->point_count < 1 || other->point_count < 1)
		return false;

	for (int32_t edge = 0; self->edge_count > edge; ++edge) {
		p1.h = buf1[edge].x;
		p1.v = buf1[edge].y;

		if (self->edge_count == edge + 1 && self->closed) {
			p2.h = buf1[0].x;
			p2.v = buf1[0].y;

		} else {
			p2.h = buf1[edge + 1].x;
			p2.v = buf1[edge + 1].y;
		}

		for (int32_t vi = 0; other_count > vi; ++vi) {
			vertex.h = buf2[vi].x;
			vertex.v = buf2[vi].y;

			if (PolyCtrlPointNearSegment(self, &p1, &p2, &vertex, thresh))
				return true;
		}
	}

	other_count = other->edge_count;

	for (int32_t edge = 0; other_count > edge; ++edge) {
		p1.h = buf2[edge].x;
		p1.v = buf2[edge].y;

		if (edge + 1 == other_count && other_closed) {
			p2.h = buf2[0].x;
			p2.v = buf2[0].y;

		} else {
			p2.h = buf2[edge + 1].x;
			p2.v = buf2[edge + 1].y;
		}

		for (int32_t vi = 0; self->point_count > vi; ++vi) {
			vertex.h = buf1[vi].x;
			vertex.v = buf1[vi].y;

			if (PolyCtrlPointNearSegment(self, &p1, &p2, &vertex, thresh))
				return true;
		}
	}

	return false;
}

void poly_ctrl_draw_invert(struct shape *self, struct screen_dc *dc)
{
	struct poly_ctrl *p = (struct poly_ctrl *) self;
	HDC hdc = dc->hdc;

	if (p->closed) {
		HRGN rgn = CreatePolygonRgn((const POINT *) p->points, p->point_count, ALTERNATE);
		HGDIOBJ old_obj = SelectObject(hdc, rgn);
		InvertRgn(hdc, rgn);
		SelectObject(hdc, old_obj);
		DeleteObject(rgn);

	} else {
		SetROP2(hdc, R2_NOT);
		SHAPE_OPS(self)->DrawOutline(self, dc);
		SetROP2(hdc, R2_COPYPEN);
	}
}

void PolyCtrlSetPoints(struct poly_ctrl *self, int32_t point_count, const struct point *points, bool closed)
{
	if (self->points) {
		MemFree(self->points);
		self->points = NULL;
	}

	self->closed = closed;
	self->point_count = point_count;
	self->edge_count = point_count;

	if (self->point_count < 1)
		return;

	if (!self->closed)
		--self->edge_count;

	PolyCtrlBuildPoints(self, points);
}

void PolyCtrlBuildPoints(struct poly_ctrl *self, const struct point *origin)
{
	if (!self->point_count)
		return;

	struct rect bounds = RectMakeOrigin(origin[0], 0, 0);
	self->points = MemAlloc((size_t) self->point_count, sizeof(struct poly_point));
	struct poly_point *pts = self->points;

	for (int32_t i = 0; self->point_count > i; ++i) {
		pts[i].x = origin[i].h;
		pts[i].y = origin[i].v;

		if (pts[i].x < bounds.left)
			bounds.left = pts[i].x;

		if (pts[i].x > bounds.right)
			bounds.right = pts[i].x;

		if (pts[i].y < bounds.top)
			bounds.top = pts[i].y;

		if (pts[i].y > bounds.bottom)
			bounds.bottom = pts[i].y;
	}

	SHAPE_OPS(self)->SetRect(&self->base, &bounds);
}

bool PolyCtrlNearestPoints(struct poly_ctrl *self, struct poly_ctrl *other, int32_t *best_this, int32_t *best_other,
	int32_t *prev_this, int32_t *prev_other, int32_t *pdx, int32_t *pdy, int32_t thresh)
{
	int32_t other_count = other->point_count;

	if (self->point_count < 1 || other->point_count < 1)
		return false;

	struct poly_point *this_pts = self->points;
	struct poly_point *other_pts = other->points;
	int32_t best_this_l, best_other_l, prev_this_l, prev_other_l;
	best_this_l = best_other_l = prev_this_l = prev_other_l = -1;

	if (PolyCtrlEdgesNear(self, other, this_pts, other_pts, thresh)) {
		uint32_t best_dist = (uint32_t) -1;

		for (int32_t ti = 0; self->point_count > ti; ++ti) {
			for (int32_t oi = 0; other_count > oi; ++oi) {
				int32_t dx = other_pts[oi].x - this_pts[ti].x;
				int32_t abs_dx = dx;
				int32_t dy = other_pts[oi].y - this_pts[ti].y;
				int32_t abs_dy = dy;

				if (abs_dx < 0)
					abs_dx = 0 - abs_dx;

				if (abs_dy < 0)
					abs_dy = 0 - abs_dy;

				uint32_t cur_dist = (uint32_t) (abs_dy * abs_dy + abs_dx * abs_dx);

				if (best_dist >= cur_dist) {
					prev_this_l = best_this_l;
					prev_other_l = best_other_l;
					best_dist = cur_dist;
					best_this_l = ti;
					best_other_l = oi;
					*pdx = dx;
					*pdy = dy;
				}
			}
		}
	}

	*best_this = best_this_l;
	*prev_this = prev_this_l;
	*best_other = best_other_l;
	*prev_other = prev_other_l;

	return best_this_l != -1;
}

void poly_ctrl_move(struct poly_ctrl *self, const struct point *delta)
{
	if (self->point_count < 1)
		return;

	struct rect bounds = SHAPE_OPS(self)->GetRectByVal(&self->base);
	bounds.left += delta->h;
	bounds.right += delta->h;
	bounds.top += delta->v;
	bounds.bottom += delta->v;
	SHAPE_OPS(self)->SetRect(&self->base, &bounds);
}

bool PolyCtrlMovePoint(struct poly_ctrl *self, const struct point *delta, int32_t idx)
{
	if (self->point_count < 1 || self->point_count <= idx)
		return false;

	struct poly_point *pts = self->points;
	pts[idx].x += delta->h;
	pts[idx].y += delta->v;
	struct rect bounds = RectMake(pts[0].y, pts[0].x, pts[0].y, pts[0].x);

	for (int32_t i = 1; self->point_count > i; ++i) {
		if (pts[i].x < bounds.left)
			bounds.left = pts[i].x;

		if (pts[i].x > bounds.right)
			bounds.right = pts[i].x;

		if (pts[i].y < bounds.top)
			bounds.top = pts[i].y;

		if (pts[i].y > bounds.bottom)
			bounds.bottom = pts[i].y;
	}

	shape_set_rect(&self->base, &bounds);

	return true;
}

void poly_ctrl_draw(struct shape *self, struct screen_dc *dc)
{
	struct poly_ctrl *p = (struct poly_ctrl *) self;

	if (!p->closed) {
		SHAPE_OPS(self)->DrawOutline(self, dc);
		return;
	}

	HDC hdc = dc->hdc;
	ScreenDcSetBrush(dc, self->brushColor);
	ScreenDcSetPen(dc, self->brushColor, self->penWidth, self->penStyle);
	Polygon(hdc, (const POINT *) p->points, p->point_count);
}

bool PolyCtrlContainsPoint(struct poly_ctrl *self, const struct point *pt)
{
	if (self->point_count < 3)
		return false;

	HRGN rgn = CreatePolygonRgn((const POINT *) self->points, self->point_count, ALTERNATE);
	bool inside = PtInRegion(rgn, pt->h, pt->v) != 0;
	DeleteObject(rgn);

	return inside;
}

int32_t PolyCtrlFindNearEdge(struct poly_ctrl *self, const struct point *pt, int32_t threshold)
{
	struct point p1, p2;
	int32_t result = -1;
	int32_t thresh = threshold;

	if (self->point_count < 1)
		return -1;

	if (thresh < 0)
		thresh = self->base.penWidth;

	struct poly_point *pts = self->points;

	for (int32_t edge = 0; self->edge_count > edge && result == -1; ++edge) {
		p1.h = pts[edge].x;
		p1.v = pts[edge].y;

		if (self->edge_count == edge + 1 && self->closed) {
			p2.h = pts[0].x;
			p2.v = pts[0].y;

		} else {
			p2.h = pts[edge + 1].x;
			p2.v = pts[edge + 1].y;
		}

		if (PolyCtrlPointNearSegment(self, &p1, &p2, pt, thresh))
			result = edge;
	}

	return result;
}

bool PolyCtrlDeleteVertex(struct poly_ctrl *self, int32_t idx)
{
	if (self->point_count < 1 || self->point_count <= idx)
		return false;

	struct poly_point *old_points = self->points;

	if (idx == 0) {
		old_points += 1;
		--self->point_count;
		idx = -1;
	}

	int32_t dst_count = 0;
	struct poly_point *new_points = MemAlloc((size_t) self->point_count, sizeof(struct poly_point));
	new_points[0] = old_points[0];
	struct rect bounds = RectMake(new_points[0].y, new_points[0].x, new_points[0].y, new_points[0].x);
	++dst_count;

	for (int32_t src = 1; self->point_count > src; ++src) {
		if (src != idx) {
			new_points[dst_count] = old_points[src];

			if (new_points[dst_count].x < bounds.left)
				bounds.left = new_points[dst_count].x;

			if (new_points[dst_count].x > bounds.right)
				bounds.right = new_points[dst_count].x;

			if (new_points[dst_count].y < bounds.top)
				bounds.top = new_points[dst_count].y;

			if (new_points[dst_count].y > bounds.bottom)
				bounds.bottom = new_points[dst_count].y;

			++dst_count;
		}
	}

	MemFree(self->points);
	self->points = new_points;
	SHAPE_OPS(self)->SetRect(&self->base, &bounds);
	self->edge_count = dst_count;
	self->point_count = self->edge_count;

	if (!self->closed)
		--self->edge_count;

	return true;
}

bool PolyCtrlGetEdge(struct poly_ctrl *self, int32_t idx, struct point *start, struct point *end)
{
	struct point unused_start, unused_end;
	int32_t next = idx + 1;

	if (self->edge_count <= idx)
		return false;

	if (!start)
		start = &unused_start;

	if (!end)
		end = &unused_end;

	if (self->point_count == next)
		next = 0;

	struct poly_point *pts = self->points;
	start->h = pts[idx].x;
	start->v = pts[idx].y;
	end->h = pts[next].x;
	end->v = pts[next].y;

	return true;
}

bool PolyCtrlVerticesNear(struct poly_ctrl *self, struct poly_ctrl *other, int32_t thresh)
{
	int32_t unused, dy = 0, dx = 0;

	if (self->point_count < 1 || other->point_count < 1)
		return false;

	bool found = PolyCtrlNearestPoints(self, other, &unused, &unused, &unused, &unused, &dx, &dy, thresh);

	if (found) {
		struct point delta = PointMake(dx, dy);
		POLY_CTRL_OPS(self)->Move(self, &delta);
	}

	return found;
}

bool PolyCtrlGetPoint(struct poly_ctrl *self, int32_t idx, struct point *out)
{
	struct point unused;

	if (self->point_count <= idx)
		return false;

	if (!out)
		out = &unused;

	struct poly_point *buf = self->points;
	out->h = buf[idx].x;
	out->v = buf[idx].y;

	return true;
}

bool PolyCtrlEdgesNear2(struct poly_ctrl *self, struct poly_ctrl *other, int32_t thresh)
{
	if (self->point_count < 1 || other->point_count < 1)
		return false;

	return PolyCtrlEdgesNear(self, other, self->points, other->points, thresh);
}

bool PolyCtrlCombine(struct poly_ctrl *self, struct poly_ctrl *other, int32_t thresh)
{
	int32_t best_this, best_other, this_end, other_end, dx = 0, dy = 0;
	int32_t step = 1;
	int32_t other_last = other->edge_count - 1;

	if (self->point_count < 1 || other->point_count < 1)
		return false;

	bool found = PolyCtrlNearestPoints(self, other, &best_this, &best_other, &this_end, &other_end, &dx, &dy, thresh);

	if (!found)
		return false;

	struct point delta = PointMake(dx, dy);
	POLY_CTRL_OPS(self)->Move(self, &delta);

	struct poly_point *this_pts = self->points;
	struct poly_point *other_pts = other->points;
	this_end = best_this + 1;

	if (self->point_count <= this_end)
		this_end = 0;

	other_end = best_other - 1;

	if (other_end < 0)
		other_end = other_last;

	int32_t index = best_other + 1;

	if (other_last < index)
		index = 0;

	if (this_pts[this_end].x == other_pts[index].x && other_pts[index].y == this_pts[this_end].y) {
		other_end = index;
		step = -1;

	} else if (other_pts[other_end].x != this_pts[this_end].x || other_pts[other_end].y != this_pts[this_end].y) {
		this_end = best_this;
		other_end = best_other;
	}

	index = this_end;
	int32_t other_scan = other_end;

	do {
		this_end = index;
		other_end = other_scan;
		++index;

		if (self->point_count <= index)
			index = 0;

		other_scan -= step;

		if (other_scan < 0)
			other_scan = other_last;

		if (other_last < other_scan)
			other_scan = 0;
	} while (this_pts[index].x == other_pts[other_scan].x && this_pts[index].y == other_pts[other_scan].y);

	int32_t dst_count = 0;
	struct poly_point *new_points = MemAlloc(2 * (size_t) self->point_count + (size_t) other->point_count,
		sizeof(struct poly_point));
	new_points[0] = this_pts[0];
	++dst_count;

	if (best_this > 0) {
		for (index = 1; best_this > index; ++index)
			new_points[dst_count++] = this_pts[index];
	}

	index = best_other;

	do {
		new_points[dst_count++] = other_pts[index];
		index += step;

		if (index < 0)
			index = other_last;

		if (other_last < index)
			index = 0;
	} while (other_end != index);

	if (self->point_count > this_end) {
		for (index = this_end; self->point_count > index; ++index)
			new_points[dst_count++] = this_pts[index];
	}

	MemFree(self->points);
	self->points = new_points;
	self->point_count = dst_count;

	return true;
}


const struct line_ctrl_ops LINE_CTRL_OPS_TABLE = {
	{
		{
			{line_ctrl_destroy},
			poly_ctrl_draw_brush20,
			poly_ctrl_draw_brush12,
			poly_ctrl_draw_outline,
			poly_ctrl_draw_invert,
			poly_ctrl_draw,
			poly_ctrl_get_rect_by_val,
			poly_ctrl_set_rect,
			shape_make_region,
		},
		poly_ctrl_move,
	},
};

void LineCtrlInit(struct line_ctrl *self, const struct point *p0, const struct point *p1, int32_t pen_color,
	int32_t brush_color, int32_t alt_brush_color, int32_t pen_width)
{
	PolyCtrlInit(&self->base, 0, NULL, false, pen_color, brush_color, alt_brush_color, pen_width);
	self->base.base.base.ops = &LINE_CTRL_OPS_TABLE.base.base.base;

	struct point pts[2];
	pts[0] = *p0;
	pts[1] = *p1;
	PolyCtrlSetPoints(&self->base, 2, pts, false);
}

struct line_ctrl *LineCtrlCreate(const struct point *p0, const struct point *p1, int32_t pen_color,
	int32_t brush_color, int32_t alt_brush_color, int32_t pen_width)
{
	struct line_ctrl *self = MemAlloc(1, sizeof(struct line_ctrl));
	LineCtrlInit(self, p0, p1, pen_color, brush_color, alt_brush_color, pen_width);

	return self;
}

void LineCtrlDestroy(struct line_ctrl **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void line_ctrl_destroy(struct object *self)
{
	self->ops = &POLY_CTRL_OPS_TABLE.base.base;
	poly_ctrl_destroy(self);
}
