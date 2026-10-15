#include "../region.h"

#include <windows.h>

#include "../../base/memory.h"
#include "../screendc.h"

const struct region_ops REGION_OPS_TABLE = {
	{region_destroy},
};

static void region_base_init(struct region *self)
{
	ObjectInit(&self->base);
	self->base.ops = &REGION_OPS_TABLE.base;
}

void RegionInit(struct region *self)
{
	region_base_init(self);

	self->hrgn = NULL;
	self->state = 0;
	self->corner_h = 0;
	self->corner_w = self->corner_h;
	self->hrgn = CreateRectRgn(0, 0, 0, 0);
}

void RegionInitCopy(struct region *self, struct region *src)
{
	region_base_init(self);

	self->state = src->state;
	self->corner_w = src->corner_w;
	self->corner_h = src->corner_h;
	self->hrgn = CreateRectRgn(0, 0, 0, 0);
	CombineRgn((HRGN) self->hrgn, (HRGN) src->hrgn, (HRGN) self->hrgn, RGN_COPY);
}

void RegionInitRect(struct region *self, struct rect *rect, int32_t state, int32_t cw, int32_t ch)
{
	region_base_init(self);

	self->hrgn = NULL;
	self->state = state;
	self->corner_w = cw;
	self->corner_h = ch;

	switch (state) {
		case 0:
			self->hrgn = CreateRectRgn(0, 0, 0, 0);
			break;
		case 1:
			self->hrgn = CreateRectRgn(rect->left, rect->top, rect->right, rect->bottom);
			break;
		case 2:
			self->hrgn = CreateRoundRectRgn(rect->left, rect->top, rect->right, rect->bottom, cw, ch);
			break;
		default:
			self->hrgn = CreateRectRgn(0, 0, 0, 0);
			break;
	}
}

void RegionInitDc(struct region *self, int32_t unused_x, int32_t kind, struct screen_dc *dc)
{
	region_base_init(self);

	self->hrgn = NULL;
	self->state = kind;
	self->corner_w = 0;
	self->corner_h = 0;
	HDC discarded = (HDC) dc->hdc;

	switch (kind) {
		case 3:
			self->hrgn = CreateRectRgn(0, 0, 0, 0);

			if (discarded)
				GetClipRgn(discarded, (HRGN) self->hrgn);

			break;
		default:
			self->hrgn = CreateRectRgn(0, 0, 0, 0);
			break;
	}
}

struct region *RegionCreate(void)
{
	struct region *self = MemAlloc(1, sizeof(struct region));
	RegionInit(self);

	return self;
}

struct region *RegionCreateCopy(struct region *src)
{
	struct region *self = MemAlloc(1, sizeof(struct region));
	RegionInitCopy(self, src);

	return self;
}

struct region *RegionCreateRect(struct rect *rect, int32_t state, int32_t cw, int32_t ch)
{
	struct region *self = MemAlloc(1, sizeof(struct region));
	RegionInitRect(self, rect, state, cw, ch);

	return self;
}

struct region *RegionCreateDc(int32_t unused_x, int32_t kind, struct screen_dc *dc)
{
	struct region *self = MemAlloc(1, sizeof(struct region));
	RegionInitDc(self, unused_x, kind, dc);

	return self;
}

void RegionDestroy(struct region **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void region_destroy(struct object *obj)
{
	struct region *self = (struct region *) obj;

	if (self->hrgn)
		DeleteObject((HGDIOBJ) self->hrgn);

	object_destroy(obj);
}

void RegionSetState(struct region *self, int32_t code)
{
	if (code == 1) {
		self->state = 0;

	} else if (code == 2) {
		self->state = 1;
	}
}

void RegionUnion(struct region *self, struct region *other)
{
	if (!other)
		return;

	self->state = 4;
	int32_t combined = CombineRgn((HRGN) self->hrgn, (HRGN) other->hrgn, (HRGN) self->hrgn, RGN_OR);
	RegionSetState(self, combined);
}

void RegionSubtract(struct region *self, struct region *other)
{
	if (!other)
		return;

	self->state = 4;
	int32_t combined = CombineRgn((HRGN) self->hrgn, (HRGN) self->hrgn, (HRGN) other->hrgn, RGN_DIFF);
	RegionSetState(self, combined);
}

void RegionIntersect(struct region *self, struct region *other)
{
	if (!other)
		return;

	self->state = 4;
	int32_t combined = CombineRgn((HRGN) self->hrgn, (HRGN) other->hrgn, (HRGN) self->hrgn, RGN_AND);
	RegionSetState(self, combined);
}

int32_t RegionPaint(struct region *self, struct screen_dc *dc, int32_t brush)
{
	ScreenDcSetBrush(dc, brush);

	return PaintRgn((HDC) dc->hdc, (HRGN) self->hrgn);
}

void RegionResetState(struct region *self, struct region *other)
{
	if (!other)
		return;

	self->state = 4;
	RegionSetState(self, 0);

	if (self->hrgn && other->hrgn)
		CombineRgn((HRGN) self->hrgn, (HRGN) other->hrgn, NULL, RGN_COPY);
}

struct region *RegionAssign(struct region *self, struct region *other)
{
	self->state = other->state;
	self->corner_w = other->corner_w;
	self->corner_h = other->corner_h;
	CombineRgn((HRGN) self->hrgn, (HRGN) other->hrgn, (HRGN) self->hrgn, RGN_COPY);

	return self;
}

struct region *RegionAssignRect(struct region *self, struct rect *rect)
{
	self->state = 1;
	self->corner_h = 0;
	self->corner_w = self->corner_h;

	struct region tmp;
	RegionInitRect(&tmp, rect, 1, 0, 0);
	CombineRgn((HRGN) self->hrgn, (HRGN) tmp.hrgn, (HRGN) self->hrgn, RGN_COPY);
	region_destroy(&tmp.base);

	return self;
}

struct region *RegionUnionInplace(struct region *self, struct region *other)
{
	RegionUnion(self, other);

	return self;
}

struct region *RegionFromRect(struct region *self, struct rect *rect)
{
	struct region tmp;
	RegionInitRect(&tmp, rect, 1, 0, 0);
	RegionUnion(self, &tmp);
	region_destroy(&tmp.base);

	return self;
}

struct region *RegionSubtractInplace(struct region *self, struct region *other)
{
	RegionSubtract(self, other);

	return self;
}

struct region *RegionSubtractRect(struct region *self, struct rect *rect)
{
	struct region tmp;
	RegionInitRect(&tmp, rect, 1, 0, 0);
	RegionSubtract(self, &tmp);
	region_destroy(&tmp.base);

	return self;
}

void RegionUnionOf(struct region *out, struct region *src, struct region *other)
{
	RegionInitCopy(out, src);
	RegionUnion(out, other);
}

void RegionUnionRect(struct region *out, struct rect *rect, struct region *other)
{
	RegionInitRect(out, rect, 1, 0, 0);
	RegionUnion(out, other);
}

void RegionUnionRect2(struct region *out, struct region *other, struct rect *rect)
{
	RegionInitRect(out, rect, 1, 0, 0);
	RegionUnion(out, other);
}

void RegionDiffOf(struct region *out, struct region *src, struct region *other)
{
	RegionInitCopy(out, src);
	RegionSubtract(out, other);
}

void RegionDiffRect(struct region *out, struct rect *rect, struct region *other)
{
	RegionInitRect(out, rect, 1, 0, 0);
	RegionSubtract(out, other);
}

void RegionDiffRect2(struct region *out, struct region *other, struct rect *rect)
{
	RegionInitRect(out, rect, 1, 0, 0);
	RegionSubtract(out, other);
}

bool RegionEquals(struct region *lhs, struct region *rhs)
{
	return EqualRgn((HRGN) lhs->hrgn, (HRGN) rhs->hrgn) != 0;
}

bool RegionNotEquals(struct region *lhs, struct region *rhs)
{
	return EqualRgn((HRGN) lhs->hrgn, (HRGN) rhs->hrgn) == 0;
}

void RegionOnSave(struct region *self, struct datafile *file)
{
}

void RegionOnLoad(struct region *self, struct datafile *file)
{
}
