#include "../font.h"

#include <string.h>

#include <windows.h>

#include "../../base/memory.h"
#include "../../base/str.h"

const struct font_ops FONT_OPS_TABLE = {
	{font_destroy},
};

void FontInit(struct font *self, int32_t font_id, void *hfont, int32_t point_size, int32_t style)
{
	ObjectInit(&self->base);
	self->base.ops = &FONT_OPS_TABLE.base;

	self->font_id = font_id;
	self->hfont = hfont;
	self->style = style;
	self->point_size = point_size;
}

struct font *FontCreate(int32_t font_id, void *hfont, int32_t point_size, int32_t style)
{
	struct font *self = MemAlloc(1, sizeof(struct font));
	FontInit(self, font_id, hfont, point_size, style);

	return self;
}

void FontDestroy(struct font **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void font_destroy(struct object *obj)
{
	struct font *self = (struct font *) obj;

	if (self->hfont)
		DeleteObject((HGDIOBJ) self->hfont);

	object_destroy(obj);
}

void *FontCreateHandle(const char *face_name, int32_t point_size, int32_t style)
{
	LOGFONTA log_font;

	HDC screen_dc = GetDC(NULL);
	GetDeviceCaps(screen_dc, LOGPIXELSX);
	ReleaseDC(NULL, screen_dc);
	memset(&log_font, 0, sizeof(log_font));

	point_size = (int32_t) ((double) point_size * 1.33);

	log_font.lfHeight = -point_size;

	if (style & 0x20) {
		log_font.lfWidth = 2 * point_size / 3;

	} else {
		log_font.lfWidth = 0;
	}

	if (style & 1) {
		log_font.lfWeight = 700;

	} else {
		log_font.lfWeight = 400;
	}

	log_font.lfItalic = (BYTE) (style & 2);
	log_font.lfUnderline = (BYTE) (style & 4);
	StrCopy(log_font.lfFaceName, face_name);

	return CreateFontIndirectA(&log_font);
}
