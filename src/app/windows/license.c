#include "../license.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <share.h>

#include <windows.h>

#include "../../base/byteorder.h"
#include "../replay.h"
#include "../../base/str.h"

#define PRODUCT_PF_SIZE 0x160

struct product_info {
	int32_t unused_000[2];
	int32_t is_licensed_004;
	int32_t is_demo_006;
	int32_t unused_008[7];
	char copyright_016[21];
	char company_02B[20];
	char address_03F[41];
	char place_068[41];
	char telephone_091[41];
	char licensee_0ba[41];
	char empty_0e3[41];
	char name_10c[41];
	char product_135[43];
};

struct splash_dialog_data {
	HMODULE h_instance;
	uint32_t current_action;
	const char *error_text;
	int32_t height;
	int32_t width;
	ATOM class_id;
	HWND hwnd;
	UINT_PTR timer_id;
	int32_t no_animation;
	int32_t animation_start;
	int32_t animation_count;
	BOOL high_color;
	HGLOBAL bitmap_mem;
	int32_t x_offset;
	int32_t animation_percent;
	int32_t unknown_38;
	int32_t animation_x_target;
	int32_t animation_y_tweak_1;
	int32_t animation_y_tweak_2;
	int32_t animation_x_pos_1;
	int32_t animation_x_pos_2;
	int32_t animation_width;
	int32_t animation_step_1;
	int32_t animation_step_2_width;
	HGLOBAL logos[3];
	uint32_t animation_start_time;
	HDC base_dc;
	HDC comp_dc_1;
	HDC comp_dc_2;
	HPALETTE selected_palette;
	HPALETTE animation_palette;
	HBITMAP bitmap_1;
	HBITMAP bitmap_2;
};

struct splash_resource_data {
	struct splash_dialog_data *splash;
	int32_t image_index;
	HRSRC resource_info;
	HGLOBAL resource_handle;
	int32_t *bits;
	BITMAPINFO *bitmapinfo;
	HGLOBAL mask_mem;
	BITMAPINFO *mask_bitmapinfo;
};

static struct product_info g_product_info;
static struct splash_dialog_data g_splash_dialog_data;
static struct splash_dialog_data *g_splash_dialog_data_ptr;

static char g_resource_text_logo[8] = "LOGO1";
static const PALETTEENTRY SPLASH_DEFAULT_PALETTES[12] = {
	{0, 0, 0, PC_NOCOLLAPSE},
	{63, 63, 63, PC_NOCOLLAPSE},
	{127, 127, 127, PC_NOCOLLAPSE},
	{191, 191, 191, PC_NOCOLLAPSE},
	{255, 255, 255, PC_NOCOLLAPSE},
	{222, 0, 0, PC_NOCOLLAPSE},
	{255, 102, 0, PC_NOCOLLAPSE},
	{255, 255, 0, PC_NOCOLLAPSE},
	{0, 102, 0, PC_NOCOLLAPSE},
	{0, 154, 255, PC_NOCOLLAPSE},
	{51, 0, 154, PC_NOCOLLAPSE},
	{104, 18, 255, PC_NOCOLLAPSE},
};

#define IDENT_ID_OK     1
#define IDENT_ID_CANCEL 2
#define IDENT_ID_NAME   101
#define IDENT_ID_ORG    102

static int32_t splash_get_current_state(void);
static const char *splash_initialize(struct splash_dialog_data *ctx);
static void splash_draw_bitmap_1(struct splash_dialog_data *ctx, HDC hdc);
static void splash_perform_state(struct splash_dialog_data *ctx, HDC hdc);
static void splash_set_state(struct splash_dialog_data *ctx, int32_t action);
static void splash_draw_text(struct splash_dialog_data *ctx);
static void splash_draw_background_rect_bitmap(struct splash_dialog_data *ctx, HBITMAP bitmap);
static void splash_draw_outline_helper(struct splash_dialog_data *ctx, HDC hdc);
static void splash_draw_outline(struct splash_dialog_data *ctx, HBITMAP hbitmap);
static void splash_draw_rainbow(struct splash_dialog_data *ctx, HBITMAP hbitmap);
static LRESULT CALLBACK splash_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static const char *read_product_pf(struct splash_dialog_data *gd);
static const char *write_product_pf(void);
static void identalog_enable_items(HWND dlg);
static INT_PTR CALLBACK ident_dialog_func(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static bool splash_load_bitmap_resource(struct splash_dialog_data *ctx, int32_t image_index);
static void splash_delete_resource_data(HGLOBAL mem);
static void splash_draw_image_phase1(HGLOBAL resource, HBITMAP bitmap);
static void splash_initialize_phase2_image(struct splash_dialog_data *ctx, int32_t image_index);
static HGLOBAL splash_fill_bitmapinfo(struct splash_dialog_data *ctx);
static int32_t splash_calculate_fade_color(int32_t input);
static void splash_circle_draw(int8_t *image_data, int32_t offset, int32_t width_height, int32_t offset_min_1,
	int32_t animation_start, int32_t animation_count);
static void splash_palette_set_circle_colors(PALETTEENTRY *entries, int32_t count, int32_t pct);
static void splash_draw_phase_circle(struct splash_dialog_data *ad);
static void splash_draw_logo_on_circle(struct splash_dialog_data *data, BYTE *bits, BITMAPINFO *bitmapinfo);

static HMODULE splash_get_hinstance(void)
{
	return GetModuleHandleA(NULL);
}

void LicenseInitialize(int32_t unused_arg_0)
{
	(void) unused_arg_0;

	g_splash_dialog_data.h_instance = splash_get_hinstance();
	read_product_pf(&g_splash_dialog_data);

	if (g_splash_dialog_data.error_text) {
		RpMessageBox(NULL, g_splash_dialog_data.error_text, NULL, MB_SYSTEMMODAL);
		exit(0);
	}
}

int32_t LicenseGetType(void)
{
	LicenseInitialize(0);

	if (g_product_info.is_demo_006 != 0)
		return LICENSE_DEMO;

	return LICENSE_FULL;
}

void LicenseGetLicenseeInformation(struct product_strings *dst)
{
	LicenseInitialize(0);

	StrCopy(dst->not_implemented_0, "not implemented (yet)");
	StrCopy(dst->name_20, g_product_info.name_10c);
	StrCopy(dst->licensee_40, g_product_info.licensee_0ba);
	StrCopy(dst->not_implemented_60, "not implemented (yet)");
}

bool SplashDisplayAndWait(void)
{
	MSG msg;
	struct splash_dialog_data *splash = &g_splash_dialog_data;
	splash->h_instance = splash_get_hinstance();
	splash->error_text = splash_initialize(splash);

	if (splash->error_text) {
		splash_set_state(splash, 8);
		SplashHandleErrorDisplay();
		return false;
	}

	while (splash_get_current_state() != 7) {
		if (RpPeekMessage(&msg, NULL, WM_TIMER, WM_TIMER, PM_REMOVE) ||
			RpPeekMessage(&msg, NULL, WM_USER, 0x7FFF, PM_REMOVE))
			RpDispatchMessage(&msg);
	}

	return g_splash_dialog_data.error_text == NULL;
}

const char *SplashGetErrorText(void)
{
	return g_splash_dialog_data.error_text;
}

static int32_t splash_get_current_state(void)
{
	return (int32_t) g_splash_dialog_data.current_action;
}

static const char *splash_initialize(struct splash_dialog_data *ctx)
{
	WNDCLASSA wndclass;
	int32_t i;
	int32_t palette_entry_count = 256;

	uint8_t lp_storage[sizeof(LOGPALETTE) + 255 * sizeof(PALETTEENTRY)];
	LOGPALETTE *lp = (LOGPALETTE *) lp_storage;

	if (!GetClassInfoA(ctx->h_instance, "MECCSplash", &wndclass)) {
		wndclass.style = CS_BYTEALIGNCLIENT | CS_OWNDC;
		wndclass.lpfnWndProc = splash_wnd_proc;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = sizeof(struct splash_dialog_data *);
		wndclass.hInstance = ctx->h_instance;
		wndclass.hIcon = NULL;
		wndclass.hCursor = NULL;
		wndclass.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
		wndclass.lpszMenuName = NULL;
		wndclass.lpszClassName = "MECCSplash";

		ctx->class_id = RegisterClassA(&wndclass);
	}

	if (!GetClassInfoA(ctx->h_instance, "MECCSplash", &wndclass))
		return "System error.";

	DWORD style = WS_POPUP;
	ctx->hwnd = CreateWindowExA(0, "MECCSplash", "", style, 100, 100, 500, 300, NULL, NULL, ctx->h_instance, ctx);

	if (!ctx->hwnd)
		return "System error.";

	SetWindowLongPtrA(ctx->hwnd, 0, (LONG_PTR) ctx);

	ctx->height = 300;
	ctx->width = 500;

	ctx->base_dc = GetDC(ctx->hwnd);

	if (!ctx->base_dc)
		return "System error.";

	int32_t screen_width = GetDeviceCaps(ctx->base_dc, HORZRES);
	int32_t screen_height = GetDeviceCaps(ctx->base_dc, VERTRES);
	MoveWindow(ctx->hwnd, (screen_width - 500) / 2, (screen_height - 300) / 2, 500, 300, FALSE);

	ctx->animation_y_tweak_1 = 0;
	ctx->animation_y_tweak_2 = 42;
	ctx->animation_x_target = ctx->width / 2;
	ctx->animation_width = 300;
	ctx->animation_step_1 = 225;
	ctx->animation_step_2_width = 200;

	if (!splash_load_bitmap_resource(ctx, 0))
		return "Can't load required resources.";

	if (!splash_load_bitmap_resource(ctx, 1))
		return "Can't load required resources.";

	if (!splash_load_bitmap_resource(ctx, 2))
		return "Can't load required resources.";

	ctx->comp_dc_1 = CreateCompatibleDC(ctx->base_dc);

	if (!ctx->comp_dc_1)
		return "System error.";

	ctx->comp_dc_2 = CreateCompatibleDC(ctx->base_dc);

	if (!ctx->comp_dc_2)
		return "System error.";

	ctx->bitmap_2 = CreateCompatibleBitmap(ctx->base_dc, ctx->animation_x_target, ctx->height);

	if (!ctx->bitmap_2)
		return "Out of memory.";

	ctx->bitmap_1 = CreateCompatibleBitmap(ctx->base_dc, ctx->width, ctx->height);

	if (!ctx->bitmap_1)
		return "Out of memory.";

	ctx->high_color = GetDeviceCaps(ctx->base_dc, BITSPIXEL) >= 8 ? TRUE : FALSE;

	ctx->animation_percent = 100;
	ctx->x_offset = 148;
	ctx->unknown_38 = 2;

	int32_t supports_palette = GetDeviceCaps(ctx->base_dc, RASTERCAPS) & RC_PALETTE;
	ctx->no_animation = 0;
	lp->palVersion = 0x300;
	lp->palNumEntries = 256;

	for (i = 0; i < 12; i++)
		lp->palPalEntry[i] = SPLASH_DEFAULT_PALETTES[i];

	ctx->animation_start = i;

	for (; i < 256; i++) {
		BYTE gray = (BYTE) ((47 * i) % 255);
		lp->palPalEntry[i].peFlags = PC_NOCOLLAPSE;
		lp->palPalEntry[i].peRed = gray;
		lp->palPalEntry[i].peGreen = gray;
		lp->palPalEntry[i].peBlue = gray;
	}

	ctx->animation_palette = CreatePalette(lp);

	if (!ctx->animation_palette)
		return "System error.";

	ctx->selected_palette = SelectPalette(ctx->base_dc, ctx->animation_palette, FALSE);

	if (supports_palette != 0)
		palette_entry_count = (int32_t) RealizePalette(ctx->base_dc);

	if (ctx->high_color) {
		if (ctx->no_animation != 0) {
			ctx->animation_count = palette_entry_count - ctx->animation_start;
			AnimatePalette(ctx->animation_palette, (UINT) ctx->animation_start, (UINT) ctx->animation_count,
				&lp->palPalEntry[ctx->animation_start]);

		} else {
			ctx->animation_count = 220;
		}

	} else {
		ctx->no_animation = 0;
		ctx->animation_count = 0;
	}

	ctx->bitmap_mem = splash_fill_bitmapinfo(ctx);
	splash_draw_background_rect_bitmap(ctx, ctx->bitmap_1);
	splash_draw_image_phase1(ctx->logos[0], ctx->bitmap_1);
	splash_draw_outline(ctx, ctx->bitmap_1);
	splash_draw_background_rect_bitmap(ctx, ctx->bitmap_2);
	splash_draw_rainbow(ctx, ctx->bitmap_2);
	splash_draw_image_phase1(ctx->logos[0], ctx->bitmap_2);
	splash_draw_outline(ctx, ctx->bitmap_2);
	splash_set_state(ctx, 0);
	ShowWindow(ctx->hwnd, SW_SHOW);
	UpdateWindow(ctx->hwnd);

	ctx->timer_id = SetTimer(ctx->hwnd, 1, 1, NULL);

	if (ctx->timer_id == 0)
		return "System error.";

	splash_set_state(ctx, 0);

	return NULL;
}

void SplashHandleErrorDisplay(void)
{
	MSG msg;
	struct splash_dialog_data *ctx = &g_splash_dialog_data;

	while (splash_get_current_state() != 8) {
		if (RpPeekMessage(&msg, NULL, WM_TIMER, WM_TIMER, PM_REMOVE) ||
			RpPeekMessage(&msg, NULL, WM_USER, 0x7FFF, PM_REMOVE))
			RpDispatchMessage(&msg);
	}

	if (ctx->base_dc) {
		if (ctx->selected_palette)
			SelectPalette(ctx->base_dc, ctx->selected_palette, FALSE);

		if (ctx->animation_palette)
			DeleteObject(ctx->animation_palette);

		ReleaseDC(ctx->hwnd, ctx->base_dc);
	}

	if (ctx->comp_dc_1)
		DeleteDC(ctx->comp_dc_1);

	if (ctx->comp_dc_2)
		DeleteDC(ctx->comp_dc_2);

	if (ctx->bitmap_mem)
		GlobalFree(ctx->bitmap_mem);

	if (ctx->bitmap_1)
		DeleteObject(ctx->bitmap_1);

	if (ctx->bitmap_2)
		DeleteObject(ctx->bitmap_2);

	splash_delete_resource_data(ctx->logos[0]);
	splash_delete_resource_data(ctx->logos[1]);
	splash_delete_resource_data(ctx->logos[2]);

	if (ctx->timer_id != 0)
		KillTimer(ctx->hwnd, 1);

	if (ctx->hwnd)
		DestroyWindow(ctx->hwnd);

	if (ctx->class_id != 0)
		UnregisterClassA("MECCSplash", ctx->h_instance);

	ctx->hwnd = NULL;
}

static void splash_draw_bitmap_1(struct splash_dialog_data *ctx, HDC hdc)
{
	HGDIOBJ old_bitmap = SelectObject(ctx->comp_dc_1, ctx->bitmap_1);
	BitBlt(hdc, 0, 0, 500, 300, ctx->comp_dc_1, 0, 0, SRCCOPY);
	SelectObject(ctx->comp_dc_1, old_bitmap);
}

static void splash_perform_state(struct splash_dialog_data *ctx, HDC hdc)
{
	HGDIOBJ hobj;
	HDC dc = hdc;

	if (!dc)
		dc = ctx->base_dc;

	switch (ctx->current_action) {
		case 0:
			splash_draw_bitmap_1(ctx, dc);
			splash_set_state(ctx, 1);
			break;
		case 4:
			if (ctx->animation_start_time == (uint32_t) -1) {
				splash_draw_bitmap_1(ctx, dc);
				ctx->animation_start_time = RpGetTickCount();

			} else {
				if (RpGetTickCount() >= ctx->animation_start_time + 100)
					splash_set_state(ctx, 3);
			}
			break;
		case 1: {
			int32_t new_x_1 = (int32_t) ((RpGetTickCount() - ctx->animation_start_time) *
				(uint32_t) ctx->animation_x_target / (uint32_t) ctx->animation_width);

			if (ctx->animation_x_target < new_x_1)
				new_x_1 = ctx->animation_x_target;

			if (ctx->animation_x_pos_1 >= new_x_1) {
				if (ctx->animation_start_time + (uint32_t) ctx->animation_width + (uint32_t) ctx->animation_step_1 <=
					RpGetTickCount())
					splash_set_state(ctx, 2);
				break;
			}

			hobj = SelectObject(ctx->comp_dc_1, ctx->bitmap_2);
			BitBlt(dc, ctx->animation_x_pos_1, 0, new_x_1 - ctx->animation_x_pos_1, 300, ctx->comp_dc_1,
				ctx->animation_x_pos_1, 0, SRCCOPY);
			SelectObject(ctx->comp_dc_1, hobj);
			ctx->animation_x_pos_1 = new_x_1;
			break;
		}
		case 2: {
			int32_t new_x_2 = (int32_t) ((RpGetTickCount() - ctx->animation_start_time) *
				(uint32_t) ctx->animation_x_target / (uint32_t) ctx->animation_step_2_width);

			if (ctx->animation_x_target < new_x_2)
				new_x_2 = ctx->animation_x_target;

			if (ctx->animation_x_pos_2 >= new_x_2)
				break;

			hobj = SelectObject(ctx->comp_dc_1, ctx->bitmap_1);
			BitBlt(dc, ctx->animation_x_pos_2, 0, new_x_2 - ctx->animation_x_pos_2, 300, ctx->comp_dc_1,
				ctx->animation_x_pos_2, 0, SRCCOPY);
			SelectObject(ctx->comp_dc_1, hobj);

			ctx->animation_x_pos_2 = new_x_2;

			if (ctx->animation_x_target <= new_x_2)
				splash_set_state(ctx, 4);
			break;
		}
		case 3: {
			if (ctx->animation_start_time == (uint32_t) -1) {
				ctx->animation_start_time = RpGetTickCount();

				if (ctx->no_animation != 0)
					splash_draw_bitmap_1(ctx, dc);
			}

			int32_t elapsed = (int32_t) (RpGetTickCount() - ctx->animation_start_time);
			int32_t new_pct;

			if (elapsed <= 750) {
				new_pct = (750 - elapsed) * 100 / 750;

			} else {
				new_pct = 0;
			}

			if (ctx->animation_percent > new_pct) {
				ctx->animation_percent = new_pct;

				if (ctx->animation_percent < 0)
					ctx->animation_percent = 0;

				if (ctx->animation_percent >= 0)
					splash_draw_phase_circle(ctx);
			}

			if (ctx->animation_percent <= 0)
				splash_set_state(ctx, 5);
			break;
		}
		case 7:
			if (RpGetTickCount() >= ctx->animation_start_time + 5000) {
				splash_set_state(ctx, 8);
				break;
			}
			// FALL THROUGH INTENTIONAL!
		case 5:
		case 6:
		case 8:
			if (hdc) {
				splash_draw_outline_helper(ctx, dc);
				splash_draw_phase_circle(ctx);
				splash_draw_text(ctx);
			}
			return;
	}
}

static void splash_set_state(struct splash_dialog_data *ctx, int32_t action)
{
	ctx->current_action = (uint32_t) action;

	switch (action) {
		case 1:
			ctx->animation_start_time = RpGetTickCount();
			ctx->animation_x_pos_1 = 0;
			break;
		case 2:
			ctx->animation_x_pos_2 = 0;
			ctx->animation_start_time = RpGetTickCount();
			break;
		case 3:
			ctx->animation_start_time = (uint32_t) -1;
			ctx->animation_percent = 100;

			if (ctx->no_animation != 0)
				splash_draw_image_phase1(ctx->logos[2], ctx->bitmap_1);

			if (ctx->high_color == FALSE) {
				splash_draw_image_phase1(ctx->logos[2], ctx->bitmap_1);
				splash_draw_bitmap_1(ctx, ctx->base_dc);
				splash_set_state(ctx, 5);
			}
			break;
		case 5:
			if (!read_product_pf(ctx)) {
				splash_set_state(ctx, 6);

			} else {
				splash_set_state(ctx, 7);
			}
			break;
		case 6:
			if (!ctx->error_text) {
				splash_draw_text(ctx);
				ctx->animation_start_time = RpGetTickCount();
				splash_set_state(ctx, 7);
			}
			break;
		case 4:
			ctx->animation_start_time = (uint32_t) -1;
			splash_draw_image_phase1(ctx->logos[1], ctx->bitmap_1);

			if (!ctx->no_animation)
				splash_initialize_phase2_image(ctx, 2);
			break;
		case 7:
		case 8:
			break;
	}
}

static void text_out(HDC dc, int32_t x, int32_t y, const char *text)
{
	TextOutA(dc, x, y, text, (int) strlen(text));
}

static void splash_draw_text(struct splash_dialog_data *ctx)
{
	int32_t x = 270;
	int32_t y = 200;
	SetTextColor(ctx->base_dc, RGB(255, 255, 255));
	SetTextAlign(ctx->base_dc, TA_TOP);
	SetBkMode(ctx->base_dc, TRANSPARENT);

	if (g_product_info.is_demo_006) {
		text_out(ctx->base_dc, x, y, "Demo Version");

	} else {
		text_out(ctx->base_dc, x, y, "Licensed to:");
		x += 20;
		y += 25;

		if (g_product_info.is_licensed_004) {
			text_out(ctx->base_dc, x, y, g_product_info.licensee_0ba);

		} else {
			text_out(ctx->base_dc, x, y, g_product_info.name_10c);
			y += 25;
			text_out(ctx->base_dc, x, y, g_product_info.licensee_0ba);
		}
	}
}

static void splash_draw_background_rect_bitmap(struct splash_dialog_data *ctx, HBITMAP bitmap)
{
	HDC dc = ctx->comp_dc_1;
	HGDIOBJ old_bitmap = SelectObject(dc, bitmap);
	HPALETTE old_pal = SelectPalette(dc, ctx->animation_palette, FALSE);

	RECT rc;
	rc.left = 0;
	rc.top = rc.left;
	rc.bottom = 300;
	rc.right = 500;

	FillRect(ctx->comp_dc_1, &rc, (HBRUSH) GetStockObject(BLACK_BRUSH));

	if (ctx->no_animation != 0) {
		int32_t x_dest = 250 - ctx->x_offset;
		int32_t y_dest = 150 - ctx->x_offset;
		int32_t width = 2 * ctx->x_offset;
		int32_t height = 2 * ctx->x_offset;
		BITMAPINFO *bmi = (BITMAPINFO *) GlobalLock(ctx->bitmap_mem);
		void *bits = &bmi->bmiColors[256];
		StretchDIBits(dc, x_dest, y_dest, width, height, 0, 0, width, height, bits, bmi, DIB_PAL_COLORS, SRCCOPY);
		GlobalUnlock(ctx->bitmap_mem);
	}

	SelectObject(dc, old_bitmap);
	SelectPalette(dc, old_pal, FALSE);
}

static void splash_draw_outline_helper(struct splash_dialog_data *ctx, HDC hdc)
{
	HGDIOBJ black_pen = SelectObject(hdc, GetStockObject(BLACK_PEN));

	LOGPEN pen;
	pen.lopnStyle = PS_SOLID;
	pen.lopnWidth.y = 1;
	pen.lopnWidth.x = pen.lopnWidth.y;

	int32_t y_1 = 0;
	int32_t x_1 = y_1;
	int32_t x_2 = ctx->width - 1;
	int32_t y_2 = ctx->height - 1;

	MoveToEx(hdc, x_1, y_2, NULL);
	LineTo(hdc, x_2, y_2);
	LineTo(hdc, x_2, y_1);
	LineTo(hdc, x_1, y_1);

	y_2--;
	LineTo(hdc, x_1, y_2);

	x_2--;
	LineTo(hdc, x_2, y_2);

	y_1++;
	LineTo(hdc, x_2, y_1);

	SelectObject(hdc, black_pen);

	pen.lopnColor = GetNearestColor(hdc, 0xAAAAAA);
	HPEN gray_pen = CreatePenIndirect(&pen);
	SelectObject(hdc, gray_pen);

	x_1++;
	LineTo(hdc, x_1, y_1);

	y_2--;
	LineTo(hdc, x_1, y_2);

	x_2--;
	LineTo(hdc, x_2, y_2);

	y_1++;
	LineTo(hdc, x_2, y_1);

	SelectObject(hdc, black_pen);
	DeleteObject(gray_pen);

	pen.lopnColor = GetNearestColor(hdc, 0x555555);
	gray_pen = CreatePenIndirect(&pen);
	SelectObject(hdc, gray_pen);

	x_1++;
	LineTo(hdc, x_1, y_1);

	y_2--;
	LineTo(hdc, x_1, y_2);

	x_2--;
	LineTo(hdc, x_2, y_2);

	y_1++;
	LineTo(hdc, x_2, y_1);

	SelectObject(hdc, black_pen);
	DeleteObject(gray_pen);
}

static void splash_draw_outline(struct splash_dialog_data *ctx, HBITMAP hbitmap)
{
	HDC dc = ctx->comp_dc_1;
	HGDIOBJ old = SelectObject(dc, hbitmap);
	splash_draw_outline_helper(ctx, dc);
	SelectObject(dc, old);
}

static void splash_draw_rainbow(struct splash_dialog_data *ctx, HBITMAP hbitmap)
{
	HDC dc = ctx->comp_dc_1;
	HGDIOBJ old = SelectObject(dc, hbitmap);
	HPALETTE old_pal = SelectPalette(dc, ctx->animation_palette, FALSE);
	int mode = ALTERNATE;
	POINT pts[3];

	for (int32_t i = 0; i < 7; i++) {
		pts[0].x = 0;
		pts[0].y = ctx->animation_y_tweak_2 * i + ctx->animation_y_tweak_1;
		pts[1].x = ctx->animation_x_target;
		pts[1].y = ctx->height / 2;
		pts[2].x = pts[0].x;
		pts[2].y = pts[0].y + ctx->animation_y_tweak_2;

		HRGN rgn = CreatePolygonRgn(pts, 3, mode);
		HBRUSH brush = CreateSolidBrush(((uint32_t) (i + 5) & 0xFFFF) | 0x1000000);
		FillRgn(dc, rgn, brush);
		DeleteObject(brush);
		DeleteObject(rgn);
	}

	SelectObject(dc, old);
	SelectPalette(dc, old_pal, FALSE);
}

static LRESULT CALLBACK splash_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	LRESULT retval = 0;
	PAINTSTRUCT paint;
	struct splash_dialog_data *ctx = (struct splash_dialog_data *) GetWindowLongPtrA(hwnd, 0);

	switch (msg) {
		case WM_PAINT:
			BeginPaint(hwnd, &paint);
			splash_perform_state(ctx, paint.hdc);
			EndPaint(hwnd, &paint);
			retval = DefWindowProcA(hwnd, msg, wparam, lparam);
			break;
		case WM_TIMER:
			splash_perform_state(ctx, NULL);
			break;
		case WM_KILLFOCUS:
		case WM_KEYDOWN:
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			if (ctx->current_action == 7)
				splash_set_state(ctx, 8);
			break;
		default:
			retval = DefWindowProcA(hwnd, msg, wparam, lparam);
			break;
	}

	return retval;
}

static void product_pf_retail_default(void)
{
	memset(&g_product_info, 0, sizeof(g_product_info));
	StrCopy(g_product_info.copyright_016, "Copyright 1995 MECC");
	StrCopy(g_product_info.company_02B, "MECC");
	StrCopy(g_product_info.address_03F, "6160 Summit Drive North");
	StrCopy(g_product_info.place_068, "Minneapolis, Minnesota  55430-4003");
	StrCopy(g_product_info.telephone_091, "(612)569-1500");
	StrCopy(g_product_info.name_10c, "Player");
	StrCopy(g_product_info.product_135, "Oregon Trail II for Windows");
}

static const struct {
	size_t offset;
	size_t size;
} PRODUCT_PF_TEXT[9] = {
	{0x016, 21}, {0x02B, 20}, {0x03F, 41}, {0x068, 41}, {0x091, 41}, {0x0BA, 41}, {0x0E3, 41}, {0x10C, 41},
	{0x135, 43},
};

static char *product_pf_text(struct product_info *info, int32_t i)
{
	char *fields[9] = {info->copyright_016, info->company_02B, info->address_03F, info->place_068,
		info->telephone_091, info->licensee_0ba, info->empty_0e3, info->name_10c, info->product_135};

	return fields[i];
}

static void product_pf_decode(const uint8_t *buf, struct product_info *info)
{
	info->unused_000[0] = ReadLe16(buf);
	info->unused_000[1] = ReadLe16(buf + 2);
	info->is_licensed_004 = ReadLe16(buf + 4);
	info->is_demo_006 = ReadLe16(buf + 6);

	for (int32_t i = 0; i < 7; i++)
		info->unused_008[i] = ReadLe16(buf + 8 + 2 * i);

	for (int32_t i = 0; i < 9; i++)
		memcpy(product_pf_text(info, i), buf + PRODUCT_PF_TEXT[i].offset, PRODUCT_PF_TEXT[i].size);
}

static void product_pf_encode(struct product_info *info, uint8_t *buf)
{
	WriteLe16(buf, info->unused_000[0]);
	WriteLe16(buf + 2, info->unused_000[1]);
	WriteLe16(buf + 4, info->is_licensed_004);
	WriteLe16(buf + 6, info->is_demo_006);

	for (int32_t i = 0; i < 7; i++)
		WriteLe16(buf + 8 + 2 * i, info->unused_008[i]);

	for (int32_t i = 0; i < 9; i++)
		memcpy(buf + PRODUCT_PF_TEXT[i].offset, product_pf_text(info, i), PRODUCT_PF_TEXT[i].size);
}

static const char *read_product_pf(struct splash_dialog_data *gd)
{
	uint8_t buf[PRODUCT_PF_SIZE];
	FILE *f = _fsopen("product.pf", "rb", _SH_DENYWR);

	if (!f) {
		product_pf_retail_default();
		return NULL;
	}

	size_t read_size = fread(buf, 1, PRODUCT_PF_SIZE, f);

	if (read_size != PRODUCT_PF_SIZE) {
		fclose(f);
		product_pf_retail_default();
		return NULL;
	}

	fclose(f);
	product_pf_decode(buf, &g_product_info);

	if (g_product_info.is_demo_006 != 0)
		return NULL;

	if (g_product_info.is_licensed_004 != 0) {
		if (g_product_info.licensee_0ba[0] == 0)
			gd->error_text = "This product needs to be idented with the MECC Site License Utility.";

		return gd->error_text;
	}

	if (g_product_info.name_10c[0] == 0) {
		INT_PTR res = DialogBoxParamA(gd->h_instance, "IDENTALOG", gd->hwnd, ident_dialog_func, (LPARAM) gd);

		if (res == -1)
			gd->error_text = "Could not display Ident dialog.";
	}

	return gd->error_text;
}

static const char *write_product_pf(void)
{
	uint8_t buf[PRODUCT_PF_SIZE];
	FILE *f = _fsopen("product.pf", "wb", _SH_DENYWR);

	if (!f)
		return "Unable to open PRODUCT.PF.";

	product_pf_encode(&g_product_info, buf);

	if (fwrite(buf, PRODUCT_PF_SIZE, 1, f) == 0) {
		fclose(f);
		return "Unable to write PRODUCT.PF.";
	}

	fclose(f);

	return NULL;
}

static void identalog_enable_items(HWND dlg)
{
	char tmp[44];
	GetDlgItemTextA(dlg, IDENT_ID_NAME, tmp, 40);
	HWND ok = GetDlgItem(dlg, IDENT_ID_OK);
	EnableWindow(ok, tmp[0] != 0);
}

static INT_PTR CALLBACK ident_dialog_func(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg) {
		case WM_INITDIALOG:
			g_splash_dialog_data_ptr = (struct splash_dialog_data *) lparam;
			identalog_enable_items(hwnd);
			return TRUE;

		case WM_COMMAND:
			switch (wparam) {
				case IDENT_ID_OK:
					GetDlgItemTextA(hwnd, IDENT_ID_NAME, g_product_info.name_10c, 40);
					GetDlgItemTextA(hwnd, IDENT_ID_ORG, g_product_info.licensee_0ba, 40);
					EndDialog(hwnd, 0);
					g_splash_dialog_data_ptr->error_text = write_product_pf();
					return FALSE;

				case IDENT_ID_CANCEL:
					g_splash_dialog_data_ptr->error_text = "This product needs to be registered.";
					EndDialog(hwnd, 0);
					splash_set_state(g_splash_dialog_data_ptr, 8);
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}

	identalog_enable_items(hwnd);

	return FALSE;
}

static bool splash_load_bitmap_resource(struct splash_dialog_data *ctx, int32_t image_index)
{
	int32_t image_size = 0;
	int32_t number_pixels = 0;
	HGLOBAL mem = GlobalAlloc(GHND, sizeof(struct splash_resource_data));

	if (!mem)
		return false;

	struct splash_resource_data *res_info = (struct splash_resource_data *) GlobalLock(mem);

	if (!res_info) {
		GlobalFree(mem);
		return false;
	}

	res_info->splash = ctx;
	res_info->image_index = image_index;

	g_resource_text_logo[4] = (char) (image_index + '1');

	res_info->resource_info = FindResourceA(ctx->h_instance, g_resource_text_logo, MAKEINTRESOURCEA(2));

	if (res_info->resource_info)
		res_info->resource_handle = LoadResource(ctx->h_instance, res_info->resource_info);

	if (res_info->resource_handle)
		res_info->bitmapinfo = (BITMAPINFO *) LockResource(res_info->resource_handle);

	if (res_info->bitmapinfo) {
		number_pixels = (int32_t) res_info->bitmapinfo->bmiHeader.biClrUsed;

		if (number_pixels == 0)
			number_pixels = 1 << (BYTE) res_info->bitmapinfo->bmiHeader.biBitCount;

		image_size = (int32_t) res_info->bitmapinfo->bmiHeader.biSize + number_pixels * 4;
		res_info->bits = (int32_t *) ((char *) res_info->bitmapinfo + image_size);
	}

	if (res_info->bitmapinfo)
		res_info->mask_mem = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T) image_size);

	if (res_info->mask_mem)
		res_info->mask_bitmapinfo = (BITMAPINFO *) GlobalLock(res_info->mask_mem);

	if (res_info->mask_bitmapinfo) {
		memcpy(res_info->mask_bitmapinfo, res_info->bitmapinfo, (size_t) image_size);

		RGBQUAD *pixel = (RGBQUAD *) (res_info->mask_bitmapinfo->bmiHeader.biSize +
			(uint8_t *) res_info->mask_bitmapinfo);

		for (int32_t i = 0; number_pixels > i; i++) {
			if ((pixel->rgbRed & pixel->rgbGreen & pixel->rgbBlue) != 0xFF) {
				pixel->rgbBlue = 0;
				pixel->rgbGreen = pixel->rgbBlue;
				pixel->rgbRed = pixel->rgbGreen;
			}

			pixel++;
		}
	}

	bool loaded = res_info->mask_bitmapinfo != NULL;
	GlobalUnlock(mem);

	if (loaded) {
		ctx->logos[image_index] = mem;
		return true;
	}

	splash_delete_resource_data(mem);

	return false;
}

static void splash_delete_resource_data(HGLOBAL mem)
{
	if (!mem)
		return;

	struct splash_resource_data *res_data = (struct splash_resource_data *) GlobalLock(mem);

	if (!res_data)
		return;

	res_data->splash->logos[res_data->image_index] = NULL;

	if (res_data->resource_handle)
		FreeResource(res_data->resource_handle);

	if (res_data->mask_mem) {
		GlobalUnlock(res_data->mask_mem);
		GlobalFree(res_data->mask_mem);
	}

	GlobalUnlock(mem);
	GlobalFree(mem);
}

static void splash_draw_image_phase1(HGLOBAL resource, HBITMAP bitmap)
{
	if (!resource)
		return;

	struct splash_resource_data *res_data = (struct splash_resource_data *) GlobalLock(resource);

	if (!res_data)
		return;

	struct splash_dialog_data *splash = res_data->splash;
	HGDIOBJ old_bitmap = SelectObject(splash->comp_dc_1, bitmap);
	LONG w = res_data->bitmapinfo->bmiHeader.biWidth;
	LONG h = res_data->bitmapinfo->bmiHeader.biHeight;
	int32_t x = (500 - w) / 2;
	int32_t y = (300 - h) / 2;

	StretchDIBits(splash->comp_dc_1, x, y, w, h, 0, 0, w, h, res_data->bits, res_data->mask_bitmapinfo,
		DIB_RGB_COLORS, MERGEPAINT);
	StretchDIBits(splash->comp_dc_1, x, y, w, h, 0, 0, w, h, res_data->bits, res_data->bitmapinfo,
		DIB_RGB_COLORS, SRCAND);

	SelectObject(splash->comp_dc_1, old_bitmap);
	GlobalUnlock(resource);
}

static void splash_initialize_phase2_image(struct splash_dialog_data *ctx, int32_t image_index)
{
	HGLOBAL mem = ctx->logos[image_index];
	struct splash_resource_data *res_data = (struct splash_resource_data *) GlobalLock(mem);
	splash_draw_logo_on_circle(ctx, (BYTE *) res_data->bits, res_data->bitmapinfo);
	GlobalUnlock(mem);
}

static HGLOBAL splash_fill_bitmapinfo(struct splash_dialog_data *ctx)
{
	int32_t offset = (uint8_t) ctx->x_offset;
	int32_t width_height = 2 * offset;
	int32_t bf_size = sizeof(BITMAPINFOHEADER) + 256 * 4;
	DWORD byte_count = (DWORD) (width_height * width_height + bf_size);

	HGLOBAL mem = GlobalAlloc(GHND, byte_count);

	if (!mem)
		return NULL;

	BITMAPINFO *bmi = (BITMAPINFO *) GlobalLock(mem);

	if (!bmi)
		return NULL;

	bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi->bmiHeader.biWidth = width_height;
	bmi->bmiHeader.biHeight = width_height;
	bmi->bmiHeader.biPlanes = 1;
	bmi->bmiHeader.biBitCount = 8;
	bmi->bmiHeader.biCompression = BI_RGB;
	bmi->bmiHeader.biSizeImage = 0;
	bmi->bmiHeader.biXPelsPerMeter = 120;
	bmi->bmiHeader.biYPelsPerMeter = 120;
	bmi->bmiHeader.biClrUsed = 0;
	bmi->bmiHeader.biClrImportant = 0;

	if (ctx->no_animation) {
		int16_t *colors = (int16_t *) bmi->bmiColors;

		for (int16_t i = 0; i < 256; i++)
			colors[i] = i;

	} else {
		GetPaletteEntries(ctx->animation_palette, 0, 255, (LPPALETTEENTRY) bmi->bmiColors);
	}

	int8_t *image_data = &((int8_t *) bmi)[bf_size];
	splash_circle_draw(image_data, offset * width_height - offset, width_height, offset - 1, ctx->animation_start,
		ctx->animation_count);

	GlobalUnlock(mem);

	return mem;
}

static int32_t splash_calculate_fade_color(int32_t input)
{
	if (input == 0)
		return 0;

	int32_t reduced = input;
	int32_t r = 1;

	while (reduced >= 4) {
		r *= 2;
		reduced >>= 2;
	}

	int32_t reduced_2 = input / r;

	while (reduced_2 + 1 < r || reduced_2 - 1 > r) {
		r = (r + reduced_2) / 2;
		reduced_2 = input / r;
	}

	return r;
}

static void splash_circle_draw(int8_t *image_data, int32_t offset, int32_t width_height, int32_t offset_min_1,
	int32_t animation_start, int32_t animation_count)
{
	int32_t rad_sq = offset_min_1 * offset_min_1;
	int32_t up_ctr = offset;
	int32_t dn_ctr = up_ctr;

	for (int32_t dy = 0; dy <= offset_min_1; dy++) {
		int32_t dy_sq = dy * dy;
		int32_t span_sq = rad_sq - dy_sq;
		int32_t dn_row = (width_height * dy) + offset;
		int32_t up_row = offset - (dy * width_height);

		for (int32_t dx = dy; dx * dx < span_sq; dx++) {
			int32_t fade = splash_calculate_fade_color((dx * dx) + dy_sq);
			int8_t color = (int8_t) (animation_start + ((offset_min_1 - fade) * animation_count) / offset_min_1);

			image_data[up_ctr - dx] = color;
			image_data[dn_ctr - dx] = image_data[up_ctr - dx];
			image_data[up_ctr + dx] = image_data[dn_ctr - dx];
			image_data[dn_ctr + dx] = image_data[up_ctr + dx];

			image_data[up_row - dy] = color;
			image_data[dn_row - dy] = image_data[up_row - dy];
			image_data[up_row + dy] = image_data[dn_row - dy];
			image_data[dn_row + dy] = image_data[up_row + dy];

			dn_row += width_height;
			up_row -= width_height;
		}

		dn_ctr += width_height;
		up_ctr -= width_height;
	}
}

static void splash_palette_set_circle_colors(PALETTEENTRY *entries, int32_t count, int32_t pct)
{
	PALETTEENTRY entry;
	double calc_pct = ((double) pct) / 100.0;
	calc_pct = (calc_pct + 1.0) * (calc_pct + 1.0) * (calc_pct + 1.0) * calc_pct * calc_pct / 8.0;
	calc_pct = (0.25 - ((calc_pct - 0.5) * (calc_pct - 0.5))) * 4.0;

	for (int32_t i = 0; i < count; i++) {
		double adj_pct = (double) (count - i) / count;
		double adj_pct_2;

		if (adj_pct < calc_pct) {
			adj_pct_2 = calc_pct - adj_pct;

		} else {
			adj_pct_2 = adj_pct - calc_pct;
		}

		double new_color = ((1.0 - ((calc_pct - adj_pct) * (calc_pct - adj_pct))) * 12.75) /
			(adj_pct_2 * adj_pct_2 * adj_pct_2 + 0.05);
		entry.peRed = entry.peGreen = entry.peBlue = (BYTE) new_color;
		entry.peFlags = PC_RESERVED;

		entries[i] = entry;
	}
}

static void splash_draw_phase_circle(struct splash_dialog_data *ad)
{
	PALETTEENTRY entries[256];
	int32_t percent = ad->animation_percent;
	int32_t color_count = ad->animation_count;

	if (ad->no_animation) {
		GetPaletteEntries(ad->animation_palette, (UINT) ad->animation_start, (UINT) ad->animation_count, entries);
		splash_palette_set_circle_colors(entries, color_count, percent);
		AnimatePalette(ad->animation_palette, (UINT) ad->animation_start, (UINT) ad->animation_count, entries);

	} else {
		int32_t dest_x = 250 - ad->x_offset;
		int32_t dest_y = 150 - ad->x_offset + 2;
		int32_t dest_w = 2 * ad->x_offset;
		int32_t dest_h = 2 * ad->x_offset - 5;

		BITMAPINFO *bmi = (BITMAPINFO *) GlobalLock(ad->bitmap_mem);
		splash_palette_set_circle_colors((PALETTEENTRY *) (bmi->bmiColors + ad->animation_start), color_count,
			percent);

		void *image_bits = (void *) &bmi->bmiColors[256];
		StretchDIBits(ad->base_dc, dest_x, dest_y, dest_w, dest_h, 0, 0, dest_w, dest_h, image_bits, bmi,
			DIB_RGB_COLORS, SRCCOPY);

		GlobalUnlock(ad->bitmap_mem);
	}
}

static void splash_draw_logo_on_circle(struct splash_dialog_data *data, BYTE *bits, BITMAPINFO *bitmapinfo)
{
	int8_t indexes[256];
	BITMAPINFO *src_image = (BITMAPINFO *) GlobalLock(data->bitmap_mem);
	int8_t *dst = (int8_t *) &src_image->bmiColors[0x100];
	int32_t dst_stride = src_image->bmiHeader.biWidth;
	RGBQUAD *src_colors = bitmapinfo->bmiColors;

	for (int32_t i = 0; i < 256; i++) {
		COLORREF color = RGB(src_colors[i].rgbRed, src_colors[i].rgbGreen, src_colors[i].rgbBlue);
		indexes[i] = (int8_t) GetNearestPaletteIndex(data->animation_palette, color);
	}

	int32_t x_offset = data->x_offset - (bitmapinfo->bmiHeader.biWidth / 2);
	int32_t y_offset = data->x_offset - (bitmapinfo->bmiHeader.biHeight / 2);
	int32_t src_stride = (bitmapinfo->bmiHeader.biWidth + 1) & 0xFFFFFFFE;
	dst += y_offset * dst_stride + x_offset;

	for (int32_t y = 0; y < bitmapinfo->bmiHeader.biHeight; y++) {
		for (int32_t x = 0; x < bitmapinfo->bmiHeader.biWidth; x++) {
			if (bits[x] != 0)
				dst[x] = indexes[bits[x]];
		}

		dst += dst_stride;
		bits += src_stride;
	}

	GlobalUnlock(data->bitmap_mem);
}
