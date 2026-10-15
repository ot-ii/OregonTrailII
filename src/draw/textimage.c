#include "textimage.h"

#include <stddef.h>
#include <stdint.h>

#include "../base/byteorder.h"
#include "canvas.h"
#include "../base/file.h"
#include "../base/memory.h"
#include "../base/str.h"

#define BIH_SIZE        0
#define BIH_WIDTH       4
#define BIH_HEIGHT      8
#define BIH_PLANES      12
#define BIH_BIT_COUNT   14

#define FILE_HEADER_SIZE 0xE

struct canvas *MakeTextImage(const char *name)
{
	char filename[100];
	uint8_t file_header[FILE_HEADER_SIZE];

	StrPrintf(filename, "%s.bmp", name);

	struct file *file = FileOpen(filename, FILE_READ);

	if (!file)
		return NULL;

	int32_t got = FileRead(file, file_header, FILE_HEADER_SIZE);

	if (got != FILE_HEADER_SIZE || ReadLe32(file_header + 2) < FILE_HEADER_SIZE + 0x400 + BIH_BIT_COUNT + 2) {
		FileClose(&file);
		return NULL;
	}

	int32_t buf_size = ReadLe32(file_header + 2) - FILE_HEADER_SIZE;
	uint8_t *raw = MemAlloc((size_t) buf_size, 1);
	FileRead(file, raw, buf_size);
	FileClose(&file);

	int32_t pal_off = (int32_t) ReadLeU16(raw + BIH_SIZE);
	int32_t width = ReadLe32(raw + BIH_WIDTH);
	int32_t height = ReadLe32(raw + BIH_HEIGHT);
	int32_t planes = (int32_t) ReadLeU16(raw + BIH_PLANES);
	int32_t bit_count = (int32_t) ReadLeU16(raw + BIH_BIT_COUNT);

	uint8_t *pixel_data = raw + 0x400 + pal_off;
	int32_t stride = ((planes * bit_count * width) / 8 + 3) & 0xFFFC;
	int32_t data_len = height * stride;

	if (height < 0 || stride <= 0 || (int64_t) 0x400 + pal_off + data_len > buf_size) {
		MemFree(raw);
		return NULL;
	}

	uint8_t *pixels = MemAlloc((size_t) data_len, 1);
	uint8_t *out_cursor = pixels;
	uint8_t *src_row = pixel_data + data_len - stride;

	for (int32_t row = height - 1; row >= 0; --row) {
		for (int32_t col = 0; stride > col; ++col) {
			*out_cursor = src_row[col];
			++out_cursor;
		}

		src_row -= stride;
	}

	struct canvas *canvas = CanvasCreate();
	canvas->bits.rect = RectMake(0, 0, height, width);
	canvas->bits.stride = stride;
	canvas->bits.valid = 1;
	canvas->bits.bits = NULL;
	canvas->dib = pixels;
	canvas->dib_size = data_len;
	canvas->bits.depth = bit_count;

	MemFree(raw);

	return canvas;
}
