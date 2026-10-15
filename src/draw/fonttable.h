#pragma once

#include <stdbool.h>
#include <stdint.h>

struct font;
struct font_rsrc;
struct obarray;

struct font_table_entry {
	char face_name[64];
	uint8_t font_id;
	int8_t point_size;
	uint8_t style;
	uint8_t unread_43;
};

struct obarray *FontTableCreate(void);
struct font *FontTableFindById(struct obarray *table, int32_t id);
int32_t InitFontTable(struct font_rsrc *font_rsrc);
void CleanupFontTable(void);

extern struct obarray *g_font_table;
extern int32_t g_dc_count;
extern bool g_fonts_inited;
