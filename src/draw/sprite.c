#include "sprite.h"

#include <stddef.h>
#include <string.h>

#include "../base/byteorder.h"
#include "canvas.h"
#include "../base/memory.h"
#include "../data/rsrc.h"

const struct sprite_ops SPRITE_OPS_TABLE = {
	{
		{
			{
				{sprite_destroy},
				media_base_free,
				media_base_release_lock,
				media_base_is_unlocked,
				media_base_clone,
				media_base_release_shared,
				media_base_release_source,
			},
			drawable_draw_to,
			drawable_get_size,
			obj_base_slot9,
			obj_base_slot10,
			drawable_get_rect,
		},
		sprite_draw_at,
		drawable_draw,
		sprite_get_size_at,
		drawable_get_rect_at,
	},
	sprite_slot16,
	sprite_decode_row,
	sprite_decode_clipped_row,
	sprite_hit_test,
	sprite_byteswap_resource,
};

void SpriteInit(struct sprite *self, struct rsrc *image)
{
	DrawableInit(&self->base, NULL);
	self->base.base.base.base.ops = &SPRITE_OPS_TABLE.base.base.base.base;

	self->base.base.base.size = sizeof(struct sprite);
	self->image = image;
	self->base.item_count = SpriteGetFrameCount(self);
}

struct sprite *SpriteCreate(struct rsrc *image)
{
	struct sprite *self = MemAlloc(1, sizeof(struct sprite));
	SpriteInit(self, image);

	return self;
}

void SpriteDestroy(struct sprite **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void sprite_destroy(struct object *self)
{
	self->ops = &DRAWABLE_OPS_TABLE.base.base.base;
	drawable_destroy(self);
}

static uint32_t swap32(uint32_t x)
{
	return ((x & 0xff) << 24) | ((x & 0xff00) << 8) | ((x & 0xff0000) >> 8) | ((x & 0xff000000) >> 24);
}

static uint32_t swap16(uint32_t x)
{
	return ((x & 0xff) << 8) | ((x & 0xff00) >> 8);
}

int32_t sprite_byteswap_resource(struct sprite *self)
{
	uint8_t *base = RsrcData(self->image);
	uint8_t *cursor = base;

	WriteLe32(cursor, swap32(ReadLeU32(cursor)));
	uint32_t count = ReadLeU32(cursor) >> 2;
	cursor += 4;
	count--;

	while (count != 0) {
		WriteLe32(cursor, swap32(ReadLeU32(cursor)));
		cursor += 4;
		count--;
	}

	count = ReadLeU32(base) >> 2;

	while (count != 0) {
		cursor = base + ReadLeU32(base + 4 * (count - 1));

		WriteLe16(cursor, (int32_t) swap16(ReadLeU16(cursor)));
		int32_t row_count = ReadLe16(cursor);
		WriteLe16(cursor + 2, (int32_t) swap16(ReadLeU16(cursor + 2)));
		cursor += 4;

		while (row_count != 0) {
			WriteLe32(cursor, swap32(ReadLeU32(cursor)));
			cursor += 4;
			row_count--;
		}

		count--;
	}

	return SpriteGetFrameCount(self);
}

int32_t SpriteGetFrameCount(struct sprite *self)
{
	self->base.item_count = (int32_t) (ReadLeU32(RsrcData(self->image)) >> 2);

	return self->base.item_count;
}

int32_t sprite_slot16(struct sprite *self, int32_t a2, int32_t a3, int32_t a4, int32_t a5)
{
	(void) self;
	(void) a2;
	(void) a3;
	(void) a4;
	(void) a5;

	return 0;
}

int32_t sprite_draw_at(struct drawable *self, struct canvas *dc, struct point *pos, int32_t index, int32_t y,
	int32_t flag)
{
	(void) y;
	(void) flag;

	struct sprite *s = (struct sprite *) self;

	int32_t stride = dc->bits.stride;
	CanvasLock(dc);

	uint8_t *dst_bits = dc->bits.bits;
	stride &= 0x7FFF;

	uint8_t *data = RsrcData(s->image);
	const uint8_t *frame_base = data + ReadLe32(data + 4 * index);
	int32_t frame_height = ReadLe16(frame_base);
	int32_t frame_width = ReadLe16(frame_base + 2);
	frame_base += 4;

	struct rect dst_rect = RectMake(0, 0, frame_height, frame_width);
	dst_rect.left += pos->h;
	dst_rect.right += pos->h;
	dst_rect.top += pos->v;
	dst_rect.bottom += pos->v;

	struct rect clip_bounds = CanvasGetBounds(dc);
	struct rect draw_rect;
	RectIntersect(&clip_bounds, &dst_rect, &draw_rect);

	if (draw_rect.right - draw_rect.left && draw_rect.bottom - draw_rect.top) {
		int32_t clip_width = 0;
		int32_t skip = 0;
		int32_t first_row = draw_rect.top - dst_rect.top;

		if (dst_rect.right > clip_bounds.right) {
			clip_width = draw_rect.right - draw_rect.left;
			skip = 0;

		} else if (dst_rect.left < clip_bounds.left) {
			clip_width = draw_rect.right - draw_rect.left;
			skip = dst_rect.right - dst_rect.left - clip_width;
		}

		bool done = false;

		if (skip || clip_width) {
			const uint8_t *row_offsets = frame_base + first_row * 4;
			uint8_t *dst = dst_bits + dst_rect.left + (intptr_t) stride * draw_rect.top;

			for (int32_t row = draw_rect.top; draw_rect.bottom > row && !done; ++row) {
				const uint8_t *row_ptr = frame_base + ReadLe32(row_offsets);
				row_offsets += 4;
				done = SPRITE_OPS(s)->DecodeClippedRow(s, row_ptr, dst, skip, clip_width) != 0;
				dst += stride;
			}

		} else {
			const uint8_t *row_ptr = frame_base + ReadLe32(frame_base + 4 * first_row);
			uint8_t *dst = dst_bits + dst_rect.left + (intptr_t) stride * draw_rect.top;

			for (int32_t row = draw_rect.top; draw_rect.bottom > row && !done; ++row) {
				done = SPRITE_OPS(s)->DecodeRow(s, &row_ptr, dst) != 0;
				dst += stride;
			}
		}
	}

	CanvasUnlock(dc);

	return 0;
}

int32_t sprite_decode_clipped_row(struct sprite *self, const uint8_t *src, uint8_t *dst, int32_t skip,
	int32_t width)
{
	(void) self;

	int32_t end_col = skip + width;
	int32_t col = 0;
	uint8_t carry = 0;

	while (skip > col) {
		uint8_t run = *src;
		src++;

		if (run) {
			if (run == 255) {
				return 1;

			} else if (run == 254) {
				return 0;
			}

			--run;

			if (col + run > skip) {
				carry = (uint8_t) (col + run - skip);
				run = (uint8_t) (run - carry);
				col += run;
				dst += run;
				src += run;

			} else {
				col += run;
				dst += run;
				src += run;
			}

		} else {
			run = *src;
			src++;
			dst += run;
			col += run;
		}
	}

	while (carry--) {
		++col;
		*dst = *src;
		src++;
		dst++;
	}

	while (end_col > col) {
		uint8_t run = *src;
		src++;

		if (run) {
			if (run == 255) {
				return 1;

			} else if (run == 254) {
				return 0;
			}

			--run;

			if (col + run > end_col) {
				carry = (uint8_t) (col + run - end_col);
				run = (uint8_t) (run - carry);
			}

			col += run;

			while (run--) {
				*dst = *src;
				src++;
				dst++;
			}

		} else {
			run = *src;
			src++;
			dst += run;
			col += run;
		}
	}

	return 0;
}

int32_t sprite_decode_row(struct sprite *self, const uint8_t **src_ptr, uint8_t *dst)
{
	(void) self;

	const uint8_t *src = *src_ptr;
	int32_t result;

	while (1) {
		uint8_t run = *src;
		src++;

		if (run != 0) {
			if (run == 0xFF) {
				result = 1;
				break;

			} else if (run == 0xFE) {
				result = 0;
				break;
			}

			run--;

			uint8_t dword_count = run >> 2;

			while (dword_count--) {
				memcpy(dst, src, 4);
				dst += 4;
				src += 4;
			}

			run = run & 3;

			while (run--)
				*dst++ = *src++;

		} else {
			run = *src++;
			dst += run;
		}
	}

	*src_ptr = src;

	return result;
}

int32_t sprite_get_size_at(struct drawable *self, struct point *frame, int32_t b)
{
	struct sprite *s = (struct sprite *) self;

	uint8_t *data = RsrcData(s->image);
	const uint8_t *cursor = data + ReadLeU32(data + 4 * b);

	frame->v = ReadLe16(cursor);
	frame->h = ReadLe16(cursor + 2);

	return 0;
}

bool sprite_hit_test(struct sprite *self, uint32_t y, uint32_t x, uint32_t frame)
{
	bool hit = false;

	uint8_t *frames = RsrcData(self->image);
	const uint8_t *base = frames + ReadLeU32(frames + 4 * frame);
	uint32_t width = ReadLeU16(base);
	uint32_t height = ReadLeU16(base + 2);

	if (width <= x || height <= y)
		return hit;

	base += 4;
	const uint8_t *row = base + ReadLeU32(base + 4 * x);
	bool done = false;
	uint32_t col = 0;

	while (col < x && !done) {
		uint8_t pix = *row++;

		if (pix) {
			if (pix == 255) {
				done = true;

			} else if (pix == 254) {
				done = true;

			} else {
				pix--;

				if (col < x && col + pix >= x) {
					hit = true;
					return hit;
				}

				col = col + pix;
				row += pix;
			}

		} else {
			pix = *row;
			row++;
			col = col + pix;
		}
	}

	return hit;
}
