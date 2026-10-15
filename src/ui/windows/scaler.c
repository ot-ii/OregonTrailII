#include "../scaler.h"

#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <mmsystem.h>
#include <vfw.h>

#include "../../base/geom.h"
#include "../../base/ini.h"
#include "../../base/memory.h"
#include "../../app/replay.h"
#include "../../base/str.h"

int32_t ScalerFloorDiv(int32_t value, int32_t scale)
{
	if (scale <= 1)
		return value;

	return value >= 0 ? value / scale : -((-value + scale - 1) / scale);
}

int32_t ScalerLargestFit(int32_t game_w, int32_t game_h, int32_t frame_w, int32_t frame_h, int32_t work_w,
	int32_t work_h)
{
	for (int32_t n = SCALER_MAX; n > 1; --n)
		if (game_w * n + frame_w <= work_w && game_h * n + frame_h <= work_h)
			return n;

	return 1;
}

struct surface {
	HWND hwnd;
	int game_w;
	int game_h;
	HDC mem;
	HBITMAP bmp;
	HBITMAP old_bmp;
};

#define MAX_SURFACES 64
static struct surface g_surface[MAX_SURFACES];
static int g_surfaces;

static int g_scale;
static int g_override;
static int g_forced;
static HWND g_dragging;
static HMENU g_size_menu;

struct scaled_font {
	HFONT original;
	int scale;
	HFONT scaled;
};

#define MAX_FONTS 64
static struct scaled_font g_font[MAX_FONTS];
static int g_fonts;

static const char SECTION[] = "window";

static int g_scale_cursor = -1;

struct scaled_cursor {
	HCURSOR base;
	int scale;
	HCURSOR scaled;
};

#define MAX_CURSORS 64
static struct scaled_cursor g_cursor[MAX_CURSORS];
static int g_cursors;
static HCURSOR g_cursor_last;

static struct surface *find_window_surface(HWND hwnd)
{
	for (int i = 0; i < g_surfaces; ++i)
		if (g_surface[i].hwnd == hwnd)
			return &g_surface[i];

	return NULL;
}

static struct surface *find_dc(HDC dc)
{
	for (int i = 0; dc && i < g_surfaces; ++i)
		if (g_surface[i].mem == dc)
			return &g_surface[i];

	return NULL;
}

static int scale(void)
{
	return g_scale > 0 ? g_scale : 1;
}

typedef UINT (WINAPI *DpiForWindowFn)(HWND);

static UINT window_dpi(HWND hwnd)
{
	static bool loaded;
	static DpiForWindowFn fn;

	if (!loaded) {
		fn = (DpiForWindowFn) (void *) GetProcAddress(GetModuleHandleA("user32.dll"), "GetDpiForWindow");
		loaded = true;
	}

	UINT dpi = fn && hwnd ? fn(hwnd) : 0;

	return dpi ? dpi : 96;
}

static void frame_and_work(HWND hwnd, int *frame_w, int *frame_h, RECT *work)
{
	RECT w;
	RECT c;
	MONITORINFO mi;

	GetWindowRect(hwnd, &w);
	GetClientRect(hwnd, &c);
	*frame_w = (w.right - w.left) - (c.right - c.left);
	*frame_h = (w.bottom - w.top) - (c.bottom - c.top);
	mi.cbSize = sizeof(mi);

	if (GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
		*work = mi.rcWork;

	} else {
		SetRect(work, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
	}
}

static struct surface *main_surface(void)
{
	for (int i = 0; i < g_surfaces; ++i)
		if (GetMenu(g_surface[i].hwnd))
			return &g_surface[i];

	return g_surfaces ? &g_surface[0] : NULL;
}

static int largest_fit(struct surface *s)
{
	int fw;
	int fh;
	RECT work;

	frame_and_work(s->hwnd, &fw, &fh, &work);

	return ScalerLargestFit(s->game_w, s->game_h, fw, fh, work.right - work.left, work.bottom - work.top);
}

static void fit_window(struct surface *s, bool keep_on_screen)
{
	int want_w = s->game_w * scale();
	int want_h = s->game_h * scale();
	RECT w;
	RECT c;

	for (int tries = 0; tries < 4; ++tries) {
		GetClientRect(s->hwnd, &c);

		if (c.right - c.left == want_w && c.bottom - c.top == want_h)
			break;

		GetWindowRect(s->hwnd, &w);
		SetWindowPos(s->hwnd, NULL, 0, 0, (w.right - w.left) + want_w - (c.right - c.left),
			(w.bottom - w.top) + want_h - (c.bottom - c.top), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	}

	if (keep_on_screen) {
		int fw;
		int fh;
		RECT work;

		frame_and_work(s->hwnd, &fw, &fh, &work);
		GetWindowRect(s->hwnd, &w);
		int x = w.left;
		int y = w.top;

		if (w.right > work.right)
			x -= w.right - work.right;

		if (w.bottom > work.bottom)
			y -= w.bottom - work.bottom;

		if (x < work.left)
			x = work.left;

		if (y < work.top)
			y = work.top;

		if (x != w.left || y != w.top)
			SetWindowPos(s->hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}
}

static HFONT scaled_font_for(HFONT original, int k)
{
	LOGFONTA lf;
	HFONT from = original ? original : (HFONT) GetStockObject(SYSTEM_FONT);

	if (k <= 1)
		return original;

	for (int i = 0; i < g_fonts; ++i)
		if (g_font[i].original == original && g_font[i].scale == k)
			return g_font[i].scaled;

	if (!GetObjectA(from, sizeof(lf), &lf))
		return original;

	lf.lfHeight *= k;
	lf.lfWidth *= k;

	if (g_fonts >= MAX_FONTS)
		return original;

	g_font[g_fonts].original = original;
	g_font[g_fonts].scale = k;
	g_font[g_fonts].scaled = CreateFontIndirectA(&lf);

	return g_font[g_fonts++].scaled;
}

static const char PROP_KEY[] = "SGK";
static const char PROP_X[] = "SGX";
static const char PROP_Y[] = "SGY";
static const char PROP_W[] = "SGW";
static const char PROP_H[] = "SGH";
static const char PROP_FONT[] = "SGF";
#define PROP_BIAS ((INT_PTR) 0x10000)

static void place_child(HWND child)
{
	int s = scale();
	int x = (int) ((INT_PTR) GetPropA(child, PROP_X) - PROP_BIAS);
	int y = (int) ((INT_PTR) GetPropA(child, PROP_Y) - PROP_BIAS);
	int w = (int) ((INT_PTR) GetPropA(child, PROP_W) - PROP_BIAS);
	int h = (int) ((INT_PTR) GetPropA(child, PROP_H) - PROP_BIAS);
	HFONT font = (HFONT) GetPropA(child, PROP_FONT);
	char cls[32];

	SetWindowPos(child, NULL, x * s, y * s, w * s, h * s, SWP_NOZORDER | SWP_NOACTIVATE);

	if (GetClassNameA(child, cls, sizeof(cls)) && !_stricmp(cls, "MCIWndClass"))
		return;

	if (font || s > 1 || SendMessageA(child, WM_GETFONT, 0, 0))
		SendMessageA(child, WM_SETFONT, (WPARAM) scaled_font_for(font, s), TRUE);
}

static BOOL CALLBACK place_child_enum(HWND child, LPARAM unused)
{
	(void) unused;

	if (GetPropA(child, PROP_KEY))
		place_child(child);

	return TRUE;
}

static bool video_playing(HWND child)
{
	return (int) SendMessageA(child, MCIWNDM_GETMODEA, 0, 0) == MCI_MODE_PLAY;
}

static void exclude_controls(HWND hwnd, HDC dc)
{
	RECT r;
	char cls[32];

	for (HWND child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
		if (!IsWindowVisible(child))
			continue;

		if (GetClassNameA(child, cls, sizeof(cls)) && !_stricmp(cls, "MCIWndClass") && !video_playing(child))
			continue;

		GetWindowRect(child, &r);
		MapWindowPoints(NULL, hwnd, (POINT *) &r, 2);
		ExcludeClipRect(dc, r.left, r.top, r.right, r.bottom);
	}
}

static int g_presents;

static void present(struct surface *s, bool full)
{
	RECT r;
	int k = scale();

	if (!s->mem)
		return;

	UINT flags = GetBoundsRect(s->mem, &r, DCB_RESET);

	if (!IsWindowVisible(s->hwnd) || IsIconic(s->hwnd))
		return;

	if (full) {
		SetRect(&r, 0, 0, s->game_w, s->game_h);

	} else {
		if (!(flags & DCB_SET) || IsRectEmpty(&r))
			return;

		LPtoDP(s->mem, (POINT *) &r, 2);
		InflateRect(&r, 1, 1);

		if (r.left < 0)
			r.left = 0;

		if (r.top < 0)
			r.top = 0;

		if (r.right > s->game_w)
			r.right = s->game_w;

		if (r.bottom > s->game_h)
			r.bottom = s->game_h;
	}

	int w = r.right - r.left;
	int h = r.bottom - r.top;

	if (w <= 0 || h <= 0)
		return;

	HDC dc = GetDC(s->hwnd);
	exclude_controls(s->hwnd, dc);
	++g_presents;

	if (k == 1) {
		BitBlt(dc, r.left, r.top, w, h, s->mem, r.left, r.top, SRCCOPY);

	} else {
		SetStretchBltMode(dc, COLORONCOLOR);
		StretchBlt(dc, r.left * k, r.top * k, w * k, h * k, s->mem, r.left, r.top, w, h, SRCCOPY);
	}

	SelectClipRgn(dc, NULL);
	ReleaseDC(s->hwnd, dc);
}

static int cursor_option(void)
{
	if (g_scale_cursor < 0)
		g_scale_cursor = IniGetInt(SECTION, "scale_cursor", 1) ? 1 : 0;

	return g_scale_cursor;
}

static int cursor_scale(void)
{
	return cursor_option() ? scale() : 1;
}

static int standard_cursor(HCURSOR c)
{
	static const LPCSTR ids[] = {IDC_ARROW, IDC_IBEAM, IDC_CROSS, IDC_WAIT};

	for (int i = 0; i < 4; ++i)
		if (c == LoadCursorA(NULL, ids[i]))
			return (int) (INT_PTR) ids[i];

	return 0;
}

static HCURSOR enlarge_monochrome(HCURSOR base, int k)
{
	ICONINFO ii;
	BITMAP bm;
	HBITMAP mask = NULL;
	HCURSOR made = NULL;

	if (!GetIconInfo(base, &ii))
		return NULL;

	if (ii.hbmColor || !GetObjectA(ii.hbmMask, sizeof(bm), &bm)) {
		if (ii.hbmColor)
			DeleteObject(ii.hbmColor);

		DeleteObject(ii.hbmMask);

		return NULL;
	}

	int w = bm.bmWidth;
	int h = bm.bmHeight;
	BITMAPINFO *bi = MemAlloc(1, sizeof(BITMAPINFOHEADER) + 2 * sizeof(RGBQUAD));
	DWORD *src = MemAlloc((size_t) w * h, 4);
	DWORD *dst = MemAlloc((size_t) w * k * h * k, 4);
	HDC screen = GetDC(NULL);

	if (bi && src && dst) {
		bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi->bmiHeader.biWidth = w;
		bi->bmiHeader.biHeight = -h;
		bi->bmiHeader.biPlanes = 1;
		bi->bmiHeader.biBitCount = 32;
		bi->bmiHeader.biCompression = BI_RGB;

		if (GetDIBits(screen, ii.hbmMask, 0, h, src, bi, DIB_RGB_COLORS) == h) {
			for (int y = 0; y < h * k; ++y)
				for (int x = 0; x < w * k; ++x)
					dst[(size_t) y * w * k + x] = src[(size_t) (y / k) * w + x / k];

			bi->bmiHeader.biWidth = w * k;
			bi->bmiHeader.biHeight = -h * k;
			mask = CreateBitmap(w * k, h * k, 1, 1, NULL);

			if (mask && SetDIBits(screen, mask, 0, h * k, dst, bi, DIB_RGB_COLORS) == h * k) {
				ICONINFO made_ii;

				made_ii.fIcon = FALSE;
				made_ii.xHotspot = ii.xHotspot * k;
				made_ii.yHotspot = ii.yHotspot * k;
				made_ii.hbmMask = mask;
				made_ii.hbmColor = NULL;
				made = (HCURSOR) CreateIconIndirect(&made_ii);
			}
		}
	}

	ReleaseDC(NULL, screen);

	if (mask)
		DeleteObject(mask);

	DeleteObject(ii.hbmMask);
	MemFree(bi);
	MemFree(src);
	MemFree(dst);

	return made;
}

static void reapply_cursor(void)
{
	HCURSOR shown = ScalerCursor(g_cursor_last ? g_cursor_last : LoadCursorA(NULL, IDC_ARROW));

	for (int i = 0; i < g_surfaces; ++i)
		SetClassLongPtrA(g_surface[i].hwnd, GCLP_HCURSOR, (LONG_PTR) shown);

	SetCursor(shown);
}

static int clamp(int n)
{
	return n < 1 ? 1 : n > SCALER_MAX ? SCALER_MAX : n;
}

static void choose_scale(struct surface *s)
{
	int n;

	if (g_override) {
		n = g_override;

	} else {
		n = IniGetInt(SECTION, "scale", 0);

		if (n <= 0)
			n = (int) ((window_dpi(s->hwnd) + 48) / 96);
	}

	if (g_forced) {
		g_scale = g_forced;
		return;
	}

	g_scale = 1;
	n = clamp(n);

	if (n > largest_fit(s))
		n = largest_fit(s);

	g_scale = n;
	RpNoteScale(g_scale);
}

typedef BOOL (WINAPI *AwarenessFn)(HANDLE);

void ScalerProcessInit(void)
{
	HMODULE user32 = GetModuleHandleA("user32.dll");
	AwarenessFn per_monitor = (AwarenessFn) (void *) GetProcAddress(user32, "SetProcessDpiAwarenessContext");
	const char *cmd = GetCommandLineA();
	const char *arg;

	if (!per_monitor || !per_monitor((HANDLE) -4))
		SetProcessDPIAware();

	if (cmd && (arg = strstr(cmd, "/scale=")) != NULL)
		g_override = clamp(atoi(arg + 7));

	RpSetScalerHooks(ScalerPresentAll, ScalerForce);
}

int32_t ScalerScale(void)
{
	return scale();
}

int32_t ScalerInitial(void)
{
	return scale();
}

void ScalerForce(int32_t k)
{
	g_forced = clamp(k);

	if (g_scale)
		ScalerSet(g_forced, false);
}

void ScalerWindowCreated(void *hwnd, int32_t game_w, int32_t game_h)
{
	if (!hwnd || g_surfaces >= MAX_SURFACES)
		return;

	struct surface *s = &g_surface[g_surfaces++];
	memset(s, 0, sizeof(*s));
	s->hwnd = (HWND) hwnd;
	s->game_w = game_w > 0 ? game_w : 1;
	s->game_h = game_h > 0 ? game_h : 1;

	if (!g_scale)
		choose_scale(s);

	fit_window(s, false);
	SetClassLongPtrA((HWND) hwnd, GCLP_HCURSOR,
		(LONG_PTR) ScalerCursor(g_cursor_last ? g_cursor_last : LoadCursorA(NULL, IDC_ARROW)));
}

void *ScalerAttach(void *hwnd, void *window_dc)
{
	struct surface *s = find_window_surface((HWND) hwnd);
	BITMAPINFO bi;
	void *bits = NULL;

	if (!s || !window_dc)
		return window_dc;

	if (GetDeviceCaps((HDC) window_dc, RASTERCAPS) & RC_PALETTE) {
		if (scale() != 1) {
			g_scale = 1;
			fit_window(s, false);
		}

		return window_dc;
	}

	memset(&bi, 0, sizeof(bi));
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
	bi.bmiHeader.biWidth = s->game_w;
	bi.bmiHeader.biHeight = -s->game_h;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	s->bmp = CreateDIBSection((HDC) window_dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
	s->mem = s->bmp ? CreateCompatibleDC((HDC) window_dc) : NULL;

	if (!s->mem) {
		if (s->bmp)
			DeleteObject(s->bmp);

		s->bmp = NULL;

		return window_dc;
	}

	s->old_bmp = (HBITMAP) SelectObject(s->mem, s->bmp);
	SetBoundsRect(s->mem, NULL, DCB_ENABLE | DCB_RESET);

	return s->mem;
}

void ScalerDetach(void *hwnd)
{
	struct surface *s = find_window_surface((HWND) hwnd);

	if (!s)
		return;

	if (s->mem) {
		SelectObject(s->mem, s->old_bmp);
		DeleteDC(s->mem);
	}

	if (s->bmp)
		DeleteObject(s->bmp);

	*s = g_surface[--g_surfaces];
}

void ScalerPresent(void *hwnd, bool full)
{
	struct surface *s = find_window_surface((HWND) hwnd);

	if (s)
		present(s, full);
}

void ScalerPresentAll(void)
{
	for (int i = 0; i < g_surfaces; ++i)
		present(&g_surface[i], false);
}

void ScalerScreenToDc(void *dc, struct point *pt)
{
	struct surface *s = find_dc((HDC) dc);
	POINT org = {0, 0};

	if (!s) {
		GetDCOrgEx((HDC) dc, &org);
		pt->h -= org.x;
		pt->v -= org.y;

		return;
	}

	ClientToScreen(s->hwnd, &org);
	pt->h = ScalerFloorDiv(pt->h - org.x, scale());
	pt->v = ScalerFloorDiv(pt->v - org.y, scale());
}

void ScalerDcToScreen(void *dc, struct point *pt)
{
	struct surface *s = find_dc((HDC) dc);
	POINT org = {0, 0};

	if (!s) {
		GetDCOrgEx((HDC) dc, &org);
		pt->h += org.x;
		pt->v += org.y;

		return;
	}

	ClientToScreen(s->hwnd, &org);
	pt->h = org.x + pt->h * scale();
	pt->v = org.y + pt->v * scale();
}

void ScalerScreenToWindow(void *hwnd, struct point *pt)
{
	POINT org = {0, 0};

	ClientToScreen((HWND) hwnd, &org);
	pt->h -= org.x;
	pt->v -= org.y;

	if (find_window_surface((HWND) hwnd)) {
		pt->h = ScalerFloorDiv(pt->h, scale());
		pt->v = ScalerFloorDiv(pt->v, scale());
	}
}

int32_t ScalerToGame(int32_t client_value)
{
	return ScalerFloorDiv(client_value, scale());
}

void ScalerRectToClient(struct rect *r)
{
	int k = scale();

	r->left *= k;
	r->top *= k;
	r->right *= k;
	r->bottom *= k;
}

void ScalerChild(void *child, int32_t x, int32_t y, int32_t w, int32_t h, void *font)
{
	HWND c = (HWND) child;

	if (!c)
		return;

	if (!font)
		font = (HFONT) SendMessageA(c, WM_GETFONT, 0, 0);

	SetPropA(c, PROP_KEY, (HANDLE) 1);
	SetPropA(c, PROP_X, (HANDLE) (x + PROP_BIAS));
	SetPropA(c, PROP_Y, (HANDLE) (y + PROP_BIAS));
	SetPropA(c, PROP_W, (HANDLE) (w + PROP_BIAS));
	SetPropA(c, PROP_H, (HANDLE) (h + PROP_BIAS));
	SetPropA(c, PROP_FONT, (HANDLE) font);

	if (scale() > 1)
		place_child(c);
}

void ScalerSet(int32_t k, bool remember)
{
	k = clamp(k);

	if (!g_forced && g_surfaces) {
		int fit = largest_fit(main_surface());

		if (k > fit)
			k = fit;
	}

	g_scale = k;

	for (int i = 0; i < g_surfaces; ++i) {
		struct surface *s = &g_surface[i];
		fit_window(s, true);
		EnumChildWindows(s->hwnd, place_child_enum, 0);
		InvalidateRect(s->hwnd, NULL, FALSE);
	}

	reapply_cursor();

	if (remember) {
		char value[8];

		StrPrintf(value, "%d", g_scale);
		IniSetString(SECTION, "scale", value);
	}
}

void ScalerMenuAppend(void *popup)
{
	char label[8];

	g_size_menu = CreatePopupMenu();

	for (int i = 1; i <= SCALER_MAX; ++i) {
		StrPrintf(label, "&%dx", i);
		AppendMenuA(g_size_menu, MF_STRING, SCALER_MENU_FIRST + i - 1, label);
	}

	AppendMenuA(g_size_menu, MF_SEPARATOR, 0, NULL);
	AppendMenuA(g_size_menu, MF_STRING, SCALER_MENU_CURSOR, "Scale Mouse &Pointer");
	AppendMenuA((HMENU) popup, MF_SEPARATOR, 0, NULL);
	AppendMenuA((HMENU) popup, MF_POPUP, (UINT_PTR) g_size_menu, "&Window Size");
}

void ScalerMenuUpdate(void *popup)
{
	if (!popup || (HMENU) popup != g_size_menu)
		return;

	int fit = g_surfaces ? largest_fit(main_surface()) : SCALER_MAX;

	for (int i = 1; i <= SCALER_MAX; ++i)
		EnableMenuItem(g_size_menu, SCALER_MENU_FIRST + i - 1, MF_BYCOMMAND | (i <= fit ? MF_ENABLED : MF_GRAYED));

	CheckMenuRadioItem(g_size_menu, SCALER_MENU_FIRST, SCALER_MENU_FIRST + SCALER_MAX - 1,
		SCALER_MENU_FIRST + scale() - 1, MF_BYCOMMAND);
	CheckMenuItem(g_size_menu, SCALER_MENU_CURSOR, MF_BYCOMMAND | (cursor_option() ? MF_CHECKED : MF_UNCHECKED));
}

bool ScalerMenuCommand(void *hwnd, int32_t id)
{
	(void) hwnd;

	if (id == SCALER_MENU_CURSOR) {
		g_scale_cursor = !cursor_option();
		IniSetString(SECTION, "scale_cursor", g_scale_cursor ? "1" : "0");
		reapply_cursor();

		return true;
	}

	if (id < SCALER_MENU_FIRST || id >= SCALER_MENU_FIRST + SCALER_MAX)
		return false;

	ScalerSet(id - SCALER_MENU_FIRST + 1, true);

	return true;
}

intptr_t ScalerDpiChanged(void *hwnd, uintptr_t wparam, intptr_t lparam)
{
	HWND wnd = (HWND) hwnd;
	struct surface *s = find_window_surface(wnd);
	const RECT *suggested = (const RECT *) lparam;

	(void) wparam;

	if (suggested) {
		SetWindowPos(wnd, NULL, suggested->left, suggested->top, suggested->right - suggested->left,
			suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
	}

	if (s) {
		fit_window(s, g_dragging != wnd);

		if (s == main_surface() && !g_forced && scale() > largest_fit(s))
			ScalerSet(largest_fit(s), false);

		InvalidateRect(wnd, NULL, FALSE);
	}

	return 0;
}

typedef BOOL (WINAPI *AdjustForDpiFn)(LPRECT, DWORD, BOOL, DWORD, UINT);

bool ScalerDpiScaledSize(void *hwnd, uint32_t dpi, int32_t *cx, int32_t *cy)
{
	static bool loaded;
	static AdjustForDpiFn adjust;

	if (!loaded) {
		adjust = (AdjustForDpiFn) (void *) GetProcAddress(GetModuleHandleA("user32.dll"), "AdjustWindowRectExForDpi");
		loaded = true;
	}

	HWND wnd = (HWND) hwnd;
	struct surface *s = find_window_surface(wnd);
	RECT w;
	RECT c;
	RECT now = {0, 0, 100, 100};
	RECT then = {0, 0, 100, 100};

	if (!s || !cx || !cy || !adjust || !dpi)
		return false;

	DWORD style = (DWORD) GetWindowLongA(wnd, GWL_STYLE);
	DWORD ex = (DWORD) GetWindowLongA(wnd, GWL_EXSTYLE);
	BOOL menu = GetMenu(wnd) != NULL;
	GetWindowRect(wnd, &w);
	GetClientRect(wnd, &c);

	if (!adjust(&now, style, menu, ex, window_dpi(wnd)) || !adjust(&then, style, menu, ex, dpi))
		return false;

	*cx = s->game_w * scale() + (w.right - w.left) - c.right + (then.right - then.left) - (now.right - now.left);
	*cy = s->game_h * scale() + (w.bottom - w.top) - c.bottom + (then.bottom - then.top) - (now.bottom - now.top);

	return true;
}

void ScalerSizeMove(void *hwnd, bool dragging)
{
	if (dragging) {
		g_dragging = (HWND) hwnd;

	} else if (g_dragging == (HWND) hwnd) {
		g_dragging = NULL;
	}
}

typedef int (WINAPI *MetricsForDpiFn)(int, UINT);

int32_t ScalerMetric(void *hwnd, int32_t index)
{
	static bool loaded;
	static MetricsForDpiFn fn;

	if (!loaded) {
		fn = (MetricsForDpiFn) (void *) GetProcAddress(GetModuleHandleA("user32.dll"), "GetSystemMetricsForDpi");
		loaded = true;
	}

	return fn ? fn(index, window_dpi((HWND) hwnd)) : GetSystemMetrics(index);
}

bool ScalerGameSize(void *hwnd, int32_t *w, int32_t *h)
{
	struct surface *s = find_window_surface((HWND) hwnd);

	if (!s)
		return false;

	*w = s->game_w;
	*h = s->game_h;

	return true;
}

int32_t ScalerDpiPx(void *hwnd, int32_t px)
{
	return MulDiv(px, (int) window_dpi((HWND) hwnd), 96);
}

void *ScalerCursor(void *base_cursor)
{
	HCURSOR base = (HCURSOR) base_cursor;
	int k = cursor_scale();
	HCURSOR made;

	g_cursor_last = base;

	if (!base || k <= 1)
		return base;

	for (int i = 0; i < g_cursors; ++i)
		if (g_cursor[i].base == base && g_cursor[i].scale == k)
			return g_cursor[i].scaled;

	int standard = standard_cursor(base);

	if (standard) {
		made = (HCURSOR) CopyImage(base, IMAGE_CURSOR, 32 * k, 32 * k, LR_COPYFROMRESOURCE);

		if (!made)
			made = (HCURSOR) CopyImage(base, IMAGE_CURSOR, 32 * k, 32 * k, 0);

	} else {
		made = enlarge_monochrome(base, k);
	}

	if (!made || g_cursors >= MAX_CURSORS) {
		if (made)
			DestroyCursor(made);

		return base;
	}

	g_cursor[g_cursors].base = base;
	g_cursor[g_cursors].scale = k;
	g_cursor[g_cursors].scaled = made;
	++g_cursors;

	return made;
}

void ScalerCursorForget(void *base_cursor)
{
	HCURSOR base = (HCURSOR) base_cursor;

	for (int i = 0; i < g_cursors;) {
		if (g_cursor[i].base == base) {
			DestroyCursor(g_cursor[i].scaled);
			g_cursor[i] = g_cursor[--g_cursors];

		} else {
			++i;
		}
	}

	if (g_cursor_last == base)
		g_cursor_last = NULL;
}

int32_t ScalerCursorScale(void)
{
	return cursor_scale();
}

#ifdef OTII_TEST
int32_t ScalerTestPresents(void)
{
	return g_presents;
}

int32_t ScalerTestCovers(void *hwnd, int32_t x, int32_t y)
{
	HDC dc = GetDC((HWND) hwnd);
	int32_t covered;

	if (!dc)
		return -1;

	exclude_controls((HWND) hwnd, dc);
	covered = PtVisible(dc, x, y) ? 1 : 0;
	SelectClipRgn(dc, NULL);
	ReleaseDC((HWND) hwnd, dc);
	return covered;
}
#endif
