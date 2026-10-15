#include "../cursor.h"

#include <stddef.h>

#include <windows.h>

#include "../../app/app.h"
#include "../../data/assets.h"
#include "../../base/byteorder.h"
#include "../../base/geom.h"
#include "../../base/log.h"
#include "../../base/memory.h"
#include "../../app/replay.h"
#include "../../data/rsrc.h"
#include "../scaler.h"

const struct cursor_ops CURSOR_OPS_TABLE = {
	{cursor_destroy},
	cursor_set,
	cursor_reset_arrow,
};

static void cursor_init_base(struct cursor *self)
{
	ObjectInit(&self->base);
	self->base.ops = &CURSOR_OPS_TABLE.base;
}

void CursorInit(struct cursor *self)
{
	cursor_init_base(self);
	self->hcursor = LoadCursorA(NULL, IDC_ARROW);
	self->hwnd = NULL;
	self->cursor_id = 0;
}

void CursorInitKind(struct cursor *self, int32_t kind)
{
	cursor_init_base(self);

	switch (kind) {
		case 0:
			self->hcursor = LoadCursorA(NULL, IDC_ARROW);
			break;
		case 1:
			self->hcursor = LoadCursorA(NULL, IDC_IBEAM);
			break;
		case 2:
			self->hcursor = LoadCursorA(NULL, IDC_CROSS);
			break;
		case 3:
			self->hcursor = LoadCursorA(NULL, IDC_WAIT);
			break;
		default:
			self->hcursor = LoadCursorA(NULL, IDC_ARROW);
			break;
	}

	self->hwnd = NULL;
	self->cursor_id = kind;
}

struct cursor_record {
	int32_t image[16];
	int32_t mask[16];
	int32_t hot_v;
	int32_t hot_h;
};

static void cursor_record_read(const uint8_t *data, struct cursor_record *out)
{
	for (int32_t i = 0; i < 16; ++i) {
		out->image[i] = ReadLe16(data + 2 * i);
		out->mask[i] = ReadLe16(data + 32 + 2 * i);
	}

	out->hot_v = ReadLe16(data + 64);
	out->hot_h = ReadLe16(data + 66);
}

void CursorInitLib(struct cursor *self, struct assets *lib, int32_t id)
{
	cursor_init_base(self);
	self->hwnd = NULL;

	if (!lib) {
		LogError(LOG_ERROR, "Constructor called with NULL CMECCLib*");
		self->hcursor = LoadCursorA(NULL, IDC_ARROW);
		self->cursor_id = 0;
		return;
	}

	self->cursor_id = id;
	struct rsrc *rsrc = AssetsGetElement(lib, "CURSOR", id, false);

	if (!rsrc) {
		LogError(LOG_ERROR, "MECCLib failed to get cursor");
		self->hcursor = LoadCursorA(NULL, IDC_ARROW);
		self->cursor_id = 0;
		return;
	}

	RsrcLoadAndUnpack(rsrc);

	struct cursor_record rec;
	cursor_record_read(RsrcData(rsrc), &rec);
	int width = GetSystemMetrics(SM_CXCURSOR);
	int height = GetSystemMetrics(SM_CYCURSOR);
	int plane_size = width / 8 * height;
	uint8_t *and_plane = MemAlloc((size_t) plane_size, 1);
	uint8_t *xor_plane = MemAlloc((size_t) plane_size, 1);

	for (int i = 0; plane_size > i; ++i) {
		and_plane[i] = 0xFF;
		xor_plane[i] = 0;
	}

	for (int i = 0; i < 16; ++i) {
		int32_t image = rec.image[i];
		int32_t mask = rec.mask[i];
		WriteLe16(&and_plane[width * i / 8], ~mask);
		WriteLe16(&xor_plane[width * i / 8], mask ^ image);
	}

	self->hcursor = CreateCursor((HINSTANCE) g_hinstance, rec.hot_h, rec.hot_v, width, height, and_plane, xor_plane);

	MemFree(and_plane);
	MemFree(xor_plane);
	RsrcDestroy(&rsrc);
}

static struct cursor *cursor_alloc(void)
{
	return MemAlloc(1, sizeof(struct cursor));
}

struct cursor *CursorCreate(void)
{
	struct cursor *self = cursor_alloc();
	CursorInit(self);

	return self;
}

struct cursor *CursorCreateKind(int32_t kind)
{
	struct cursor *self = cursor_alloc();
	CursorInitKind(self, kind);

	return self;
}

struct cursor *CursorCreateLib(struct assets *lib, int32_t id)
{
	struct cursor *self = cursor_alloc();
	CursorInitLib(self, lib, id);

	return self;
}

void CursorDestroy(struct cursor **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void cursor_destroy(struct object *obj)
{
	struct cursor *self = (struct cursor *) obj;

	if (GetCursor() == (HCURSOR) self->hcursor)
		cursor_reset_arrow(self);

	cursor_reset_arrow(self);
	ScalerCursorForget(self->hcursor);
	DestroyCursor((HCURSOR) self->hcursor);

	object_destroy(obj);
}

void cursor_set(struct cursor *self)
{
	if (!self->hcursor) {
		LogError(LOG_ERROR, "Set() called on invalid CCursor object");
		return;
	}

	struct point p = {0, 0};
	RpGetCursorPos(&p);

	POINT pt;
	pt.x = p.h;
	pt.y = p.v;
	self->hwnd = WindowFromPoint(pt);

	HCURSOR shown = (HCURSOR) ScalerCursor(self->hcursor);
	SetClassLongPtrA((HWND) self->hwnd, GCLP_HCURSOR, (LONG_PTR) shown);
	SetCursor(shown);
}

void cursor_reset_arrow(struct cursor *self)
{
	HCURSOR arrow = (HCURSOR) ScalerCursor(LoadCursorA(NULL, IDC_ARROW));

	if (self->hwnd) {
		SetClassLongPtrA((HWND) self->hwnd, GCLP_HCURSOR, (LONG_PTR) arrow);
		self->hwnd = NULL;
	}

	SetCursor(arrow);
}

void CursorHide(void)
{
	ShowCursor(FALSE);
}

void CursorShow(void)
{
	ShowCursor(TRUE);
}
