#include "../fonttable.h"

#include <string.h>

#include <windows.h>

#include "../../base/byteorder.h"
#include "../font.h"
#include "../fontrsrc.h"
#include "../../base/list.h"
#include "../../base/object.h"
#include "../../base/str.h"

struct obarray *g_font_table = NULL;
int32_t g_dc_count = 0;
bool g_fonts_inited = false;

struct obarray *FontTableCreate(void)
{
	return ObarrayCreate(ObjectDelete);
}

struct font *FontTableFindById(struct obarray *table, int32_t id)
{
	if (!table)
		return NULL;

	int32_t count = ObarrayCount(table);

	for (int32_t index = 0; index < count; ++index) {
		struct font *obj = ObarrayGetAt(table, index);

		if (obj->font_id == id)
			return obj;
	}

	return NULL;
}

int32_t InitFontTable(struct font_rsrc *font_rsrc)
{
	LOGFONTA logfont;
	int32_t status = 0;

	if (!g_fonts_inited) {
		g_font_table = FontTableCreate();
		logfont.lfHeight = 16;
		logfont.lfWidth = 7;
		logfont.lfEscapement = 0;
		logfont.lfOrientation = 0;
		logfont.lfWeight = FW_BOLD;
		logfont.lfItalic = 0;
		logfont.lfUnderline = 0;
		logfont.lfStrikeOut = 0;
		logfont.lfCharSet = ANSI_CHARSET;
		logfont.lfOutPrecision = OUT_DEFAULT_PRECIS;
		logfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
		logfont.lfQuality = DEFAULT_QUALITY;
		logfont.lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
		StrCopy(logfont.lfFaceName, "MS Sans Serif");
		HFONT hfont = CreateFontIndirectA(&logfont);
		struct font *font;

		if (hfont) {
			font = FontCreate(0, hfont, 12, 0);

		} else {
			font = FontCreate(0, GetStockObject(SYSTEM_FONT), 12, 0);
		}

		ObarrayAddUnique(g_font_table, font);

		if (font_rsrc) {
			const uint8_t *table_rsrc = font_rsrc->mem;

			if (table_rsrc) {
				int32_t entry_count = ReadLe16(table_rsrc);
				const uint8_t *p = table_rsrc + 2;

				for (int32_t index = 0; entry_count > index; ++index) {
					struct font_table_entry entry;
					memcpy(&entry, p, sizeof(entry));

					char face_name[sizeof(entry.face_name) + 1];
					memcpy(face_name, entry.face_name, sizeof(entry.face_name));
					face_name[sizeof(entry.face_name)] = '\0';

					void *created_font = FontCreateHandle(face_name, entry.point_size, entry.style);

					if (created_font) {
						font = FontCreate(entry.font_id, created_font, entry.point_size, entry.style);
						ObarrayAddUnique(g_font_table, font);
					}

					p += sizeof(entry);
				}

			} else {
				status = 1;
			}

		} else {
			status = 1;
		}

		g_fonts_inited = true;
	}

	return status;
}

void CleanupFontTable(void)
{
	if (!g_dc_count && g_fonts_inited)
		ObarrayDestroy(&g_font_table);
}
