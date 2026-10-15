#pragma once

#include <stdbool.h>
#include <stdint.h>

struct point {
	int32_t v;
	int32_t h;
};

struct rect {
	int32_t top;
	int32_t left;
	int32_t bottom;
	int32_t right;
};

struct point PointMake(int32_t h, int32_t v);
bool PointEqual(struct point a, struct point b);

struct rect RectMake(int32_t top, int32_t left, int32_t bottom, int32_t right);
struct rect RectMakeSize(int32_t w, int32_t h);
struct rect RectMakeOrigin(struct point origin, int32_t w, int32_t h);
struct rect RectMakePoints(struct point a, struct point b);

int32_t RectWidth(const struct rect *r);
int32_t RectHeight(const struct rect *r);
struct point RectTopLeft(const struct rect *r);
struct point RectSize(const struct rect *r);

bool RectEqual(const struct rect *a, const struct rect *b);
bool RectContains(const struct rect *r, const struct rect *other);
bool RectIsInside(const struct rect *r, const struct rect *other);
bool RectContainsPoint(const struct rect *r, struct point p);

struct rect RectUnionOf(const struct rect *a, const struct rect *b);
struct rect RectIntersectOf(const struct rect *a, const struct rect *b);
void RectUnionWith(struct rect *r, const struct rect *other);
void RectIntersectWith(struct rect *r, const struct rect *other);
struct rect RectUnion(const struct rect *r, const struct rect *other);
bool RectIntersect(const struct rect *r, const struct rect *other, struct rect *out);
void RectInflate(struct rect *r, int32_t d);
void RectDeflate(struct rect *r, int32_t d);
void RectOffset(struct rect *r, int32_t dh, int32_t dv);
void RectGrowToPoint(struct rect *r, struct point p);
