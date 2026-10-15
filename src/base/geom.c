#include "geom.h"

struct point PointMake(int32_t h, int32_t v)
{
	struct point p;
	p.v = v;
	p.h = h;

	return p;
}

bool PointEqual(struct point a, struct point b)
{
	return a.v == b.v && a.h == b.h;
}

struct rect RectMake(int32_t top, int32_t left, int32_t bottom, int32_t right)
{
	struct rect r;
	r.top = top;
	r.left = left;
	r.bottom = bottom;
	r.right = right;

	return r;
}

struct rect RectMakeSize(int32_t w, int32_t h)
{
	return RectMake(0, 0, h, w);
}

struct rect RectMakeOrigin(struct point origin, int32_t w, int32_t h)
{
	return RectMake(origin.v, origin.h, origin.v + h, origin.h + w);
}

struct rect RectMakePoints(struct point a, struct point b)
{
	struct rect r;

	if (a.h < b.h) {
		r.left = a.h;
		r.right = b.h;

	} else {
		r.left = b.h;
		r.right = a.h;
	}

	if (a.v < b.v) {
		r.top = a.v;
		r.bottom = b.v;

	} else {
		r.top = b.v;
		r.bottom = a.v;
	}

	return r;
}

int32_t RectWidth(const struct rect *r)
{
	return r->right - r->left;
}

int32_t RectHeight(const struct rect *r)
{
	return r->bottom - r->top;
}

struct point RectTopLeft(const struct rect *r)
{
	return PointMake(r->left, r->top);
}

struct point RectSize(const struct rect *r)
{
	return PointMake(r->right - r->left, r->bottom - r->top);
}

bool RectEqual(const struct rect *a, const struct rect *b)
{
	return a->top == b->top && a->left == b->left && a->bottom == b->bottom && a->right == b->right;
}

bool RectContains(const struct rect *r, const struct rect *other)
{
	return r->top <= other->top && r->bottom >= other->bottom && r->left <= other->left && r->right >= other->right;
}

bool RectIsInside(const struct rect *r, const struct rect *other)
{
	return RectContains(other, r);
}

bool RectContainsPoint(const struct rect *r, struct point p)
{
	return p.h >= r->left && p.h < r->right && p.v >= r->top && p.v < r->bottom;
}

struct rect RectUnionOf(const struct rect *a, const struct rect *b)
{
	return RectMake(b->top < a->top ? b->top : a->top, b->left < a->left ? b->left : a->left,
		b->bottom > a->bottom ? b->bottom : a->bottom, b->right > a->right ? b->right : a->right);
}

struct rect RectIntersectOf(const struct rect *a, const struct rect *b)
{
	if (b->bottom < a->top || a->bottom < b->top || a->left > b->right || b->left > a->right)
		return RectMake(0, 0, 0, 0);

	return RectMake(b->top > a->top ? b->top : a->top, b->left > a->left ? b->left : a->left,
		b->bottom < a->bottom ? b->bottom : a->bottom, b->right < a->right ? b->right : a->right);
}

void RectUnionWith(struct rect *r, const struct rect *other)
{
	if (r->top > other->top)
		r->top = other->top;

	if (r->left > other->left)
		r->left = other->left;

	if (r->bottom < other->bottom)
		r->bottom = other->bottom;

	if (r->right < other->right)
		r->right = other->right;
}

void RectIntersectWith(struct rect *r, const struct rect *other)
{
	*r = RectIntersectOf(r, other);
}

struct rect RectUnion(const struct rect *r, const struct rect *other)
{
	struct rect out;
	out.left = other->left <= r->left ? other->left : r->left;
	out.top = other->top <= r->top ? other->top : r->top;
	out.right = other->right < r->right ? r->right : other->right;
	out.bottom = other->bottom < r->bottom ? r->bottom : other->bottom;

	return out;
}

bool RectIntersect(const struct rect *r, const struct rect *other, struct rect *out)
{
	struct rect x;
	x.left = r->left >= other->left ? r->left : other->left;
	x.top = r->top >= other->top ? r->top : other->top;
	x.right = r->right > other->right ? other->right : r->right;
	x.bottom = r->bottom > other->bottom ? other->bottom : r->bottom;

	if (x.left < x.right && x.bottom > x.top) {
		*out = x;
		return true;
	}

	*out = RectMake(0, 0, 0, 0);

	return false;
}

void RectInflate(struct rect *r, int32_t d)
{
	r->top -= d;
	r->left -= d;
	r->bottom += d;
	r->right += d;
}

void RectDeflate(struct rect *r, int32_t d)
{
	RectInflate(r, -d);
}

void RectOffset(struct rect *r, int32_t dh, int32_t dv)
{
	r->top += dv;
	r->bottom += dv;
	r->left += dh;
	r->right += dh;
}

void RectGrowToPoint(struct rect *r, struct point p)
{
	if (r->left > p.h)
		r->left = p.h;

	if (p.h > r->right)
		r->right = p.h;

	if (r->top > p.v)
		r->top = p.v;

	if (r->bottom < p.v)
		r->bottom = p.v;
}
