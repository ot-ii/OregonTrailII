#include "fontrsrc.h"

#include <stddef.h>

#include "../data/assets.h"
#include "../base/byteorder.h"
#include "../base/log.h"
#include "../base/memory.h"
#include "../data/rsrc.h"
#include "../base/str.h"

static const char *g_fonts_name = "FONTS";

const struct font_rsrc_ops FONT_RSRC_OPS_TABLE = {
	{font_rsrc_destroy},
};

void FontRsrcInit(struct font_rsrc *self, struct assets *lib, int32_t id)
{
	ObjectInit(&self->base);
	self->base.ops = &FONT_RSRC_OPS_TABLE.base;

	char msg_not_packed[100];
	char msg_bad_format[100];

	struct rsrc *rsrc = AssetsGetElement(lib, g_fonts_name, id, true);

	if (!rsrc)
		return;

	if (RsrcSize(rsrc) == 0) {
		RsrcDestroy(&rsrc);
		return;
	}

	switch (RsrcStorage(rsrc)) {
		case 1:
			StrPrintf(msg_not_packed, "Fonts resource %d not packed", id);
			LogError(LOG_ERROR, msg_not_packed);
			break;
		case 2:
		case 3:
			RsrcLoadAndUnpack(rsrc);
			break;
		default:
			StrPrintf(msg_bad_format, "Fonts resource %d, invalid format", id);
			LogError(LOG_ERROR, msg_bad_format);
			RsrcDestroy(&rsrc);
			return;
	}

	const uint8_t *src = RsrcData(rsrc);
	int32_t font_count = ReadLe16(src);
	src += 2;

	uint32_t count = (uint32_t) font_count & 0xFFFF;
	self->mem_size = (int32_t) (68 * count + 2);
	self->mem = MemAlloc((size_t) self->mem_size, 1);

	uint8_t *table = self->mem;
	WriteLe16(table, font_count);
	uint8_t *entries = table + 2;

	for (uint32_t font_index = 0; font_index < count; ++font_index) {
		src += 64;

		for (uint32_t byte_index = 0; byte_index < 64; ++byte_index)
			entries[68 * font_index + byte_index] = *src++;

		entries[68 * font_index + 64] = *src++;
		entries[68 * font_index + 65] = *src++;
		entries[68 * font_index + 66] = *src++;
		entries[68 * font_index + 67] = *src++;
	}

	RsrcDestroy(&rsrc);
}

struct font_rsrc *FontRsrcCreate(struct assets *lib, int32_t id)
{
	struct font_rsrc *self = MemAlloc(1, sizeof(struct font_rsrc));
	FontRsrcInit(self, lib, id);

	return self;
}

void FontRsrcDestroy(struct font_rsrc **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void font_rsrc_destroy(struct object *obj)
{
	struct font_rsrc *self = (struct font_rsrc *) obj;

	MemFree(self->mem);
	self->mem = NULL;

	object_destroy(obj);
}
