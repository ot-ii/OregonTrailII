#include "../crowdcontrol.h"

#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>

#include "../crowdnet.h"
#include "../../base/file.h"
#include "../../ui/framewin.h"
#include "../../base/ini.h"
#include "../../app/replay.h"
#include "../../ui/scaler.h"
#include "../../base/str.h"
#include "../../ui/window.h"
#include "../../ui/windowpos.h"

static HWND g_wnd;

static const COLORREF CC_ROYAL = RGB(0x15, 0x11, 0x29);
static const COLORREF CC_PANEL = RGB(0x24, 0x21, 0x31);
static const COLORREF CC_PANEL_DARK = RGB(0x1B, 0x19, 0x2D);
static const COLORREF CC_SLATE = RGB(0x37, 0x3C, 0x5D);
static const COLORREF CC_RED = RGB(0xEA, 0x3D, 0x6A);
static const COLORREF CC_YELLOW = RGB(0xFF, 0xEC, 0x3B);
static const COLORREF CC_GREEN = RGB(0x1E, 0xE2, 0x9A);
static const COLORREF CC_BLUE = RGB(0x41, 0xB1, 0xF7);
static const COLORREF CC_WHITE = RGB(0xFA, 0xFA, 0xFA);
static const COLORREF CC_MUTED = RGB(0xA7, 0xA7, 0xCA);
static const COLORREF CC_GREY = RGB(0x6E, 0x6B, 0x86);

enum {
	ID_CONNECT = 101,
	ID_SESSION,
	ID_DISCONNECT,
	ID_PACK,
	ID_LIST,
};

struct window_parts {
	HFONT title;
	HFONT body;
	HFONT small_body;
	HFONT button;
	HBITMAP logo;
	HBRUSH royal;
	HBRUSH panel_dark;
	HWND list;
	bool fonts_loaded;
	int32_t list_version;
	HWND hover;
	RECT link;
	char guide[160];
	bool show_pack;
};

struct window_layout {
	int32_t sub_top;
	int32_t sub_h;
	int32_t desc_top;
	int32_t desc_h;
	int32_t link_top;
	int32_t link_h;
	int32_t panel_top;
	int32_t panel_h;
	int32_t head_h;
	int32_t buttons_top;
	int32_t buttons_h;
	int32_t title_top;
	int32_t title_h;
	int32_t list_top;
};

static const char DESCRIPTION[] = "Crowd Control lets your viewers (on any streaming platform) interact with the game "
	"by triggering effects from your unique Interact Link.";

static struct window_parts g_ui;
static HANDLE g_font_handles[2];
static DWORD g_auth_started;
static WNDPROC g_button_proc;


static void CALLBACK tick_proc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
	(void) hwnd;
	(void) msg;
	(void) id;
	(void) time;

	CrowdControlTick();
}

uintptr_t CrowdControlStartTimer(void)
{
	return RpSetTimer(NULL, 0, 250, (void *) tick_proc);
}

void CrowdControlStopTimer(uintptr_t timer)
{
	KillTimer(NULL, timer);
}

bool CrowdControlMainWindowEnabled(void)
{
	return g_parent_win && IsWindowEnabled((HWND) g_parent_win->base.hwnd);
}


static int32_t dip(HWND hwnd, int32_t px)
{
	return ScalerDpiPx(hwnd, px);
}

static bool load_brand_fonts(void)
{
	static const char *const names[2] = {"CC_FONT_REGULAR", "CC_FONT_BOLD"};
	int32_t loaded = 0;

	for (int32_t i = 0; i < 2; ++i) {
		DWORD count = 0;

		if (g_font_handles[i]) {
			++loaded;
			continue;
		}

		HRSRC r = FindResourceA(GetModuleHandleA(NULL), names[i], (LPCSTR) RT_RCDATA);
		HGLOBAL g = r ? LoadResource(GetModuleHandleA(NULL), r) : NULL;

		if (g)
			g_font_handles[i] = AddFontMemResourceEx(LockResource(g), SizeofResource(GetModuleHandleA(NULL), r), NULL,
				&count);
		if (g_font_handles[i])
			++loaded;
	}

	return loaded == 2;
}

static HFONT make_font(HWND hwnd, int32_t px, int32_t weight)
{
	return CreateFontA(-dip(hwnd, px), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, g_ui.fonts_loaded ? "Lexend" : "Segoe UI");
}

static int32_t font_height(HDC dc, HFONT font)
{
	TEXTMETRICA tm;

	SelectObject(dc, font);
	GetTextMetricsA(dc, &tm);

	return tm.tmHeight;
}

static void measure_layout(HWND hwnd, struct window_layout *l)
{
	RECT c;
	HDC dc = GetDC(hwnd);
	HGDIOBJ old = SelectObject(dc, g_ui.body);

	GetClientRect(hwnd, &c);

	RECT desc = {0, 0, c.right - 2 * dip(hwnd, 20), 0};

	l->sub_top = dip(hwnd, 14) + dip(hwnd, 60) + dip(hwnd, 4);
	l->sub_h = font_height(dc, g_ui.small_body);
	l->desc_top = l->sub_top + l->sub_h + dip(hwnd, 10);
	SelectObject(dc, g_ui.body);
	DrawTextA(dc, DESCRIPTION, -1, &desc, DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
	l->desc_h = desc.bottom - desc.top;
	l->link_top = l->desc_top + l->desc_h + dip(hwnd, 6);
	l->link_h = font_height(dc, g_ui.body);
	l->panel_top = l->link_top + l->link_h + dip(hwnd, 12);
	l->head_h = font_height(dc, g_ui.button);
	l->panel_h = dip(hwnd, 10) + l->head_h + dip(hwnd, 2) + font_height(dc, g_ui.small_body) + dip(hwnd, 10);
	l->buttons_top = l->panel_top + l->panel_h + dip(hwnd, 14);
	l->buttons_h = dip(hwnd, 42);
	l->title_top = l->buttons_top + l->buttons_h + dip(hwnd, 20);
	l->title_h = font_height(dc, g_ui.title);
	if (l->title_h < dip(hwnd, 26))
		l->title_h = dip(hwnd, 26);
	l->list_top = l->title_top + l->title_h + dip(hwnd, 6);

	SelectObject(dc, old);
	ReleaseDC(hwnd, dc);
}

static int32_t status(char *headline, int32_t hsize, char *detail, int32_t dsize, COLORREF *dot)
{
	char text[256];
	int32_t s = CrowdNetState(CrowdControlNet(), text, sizeof(text));

	if (g_auth_started && (s != CROWD_SIGN_IN || GetTickCount() - g_auth_started > 15 * 60 * 1000))
		g_auth_started = 0;
	StrCopyN(detail, (size_t) dsize, text);
	switch (s) {
		case CROWD_OFF:
			*dot = CC_GREY;
			StrCopyN(headline, (size_t) hsize, "Not available");
			if (!detail[0])
				StrCopyN(detail, (size_t) dsize, "Crowd Control is not set up: see [crowdcontrol] in OREGONII.INI.");
			break;
		case CROWD_CONNECTING:
			*dot = CC_YELLOW;
			StrCopyN(headline, (size_t) hsize, "Connecting...");
			break;
		case CROWD_SIGN_IN:
			if (g_auth_started) {
				*dot = CC_YELLOW;
				StrCopyN(headline, (size_t) hsize, "Waiting for authentication");

			} else {
				*dot = CC_GREY;
				StrCopyN(headline, (size_t) hsize, "Not connected");
			}
			break;
		case CROWD_SIGNED_IN:
			*dot = CC_BLUE;
			StrCopyN(headline, (size_t) hsize, "Connected");
			break;
		default:
			*dot = CC_GREEN;
			StrCopyN(headline, (size_t) hsize, "Session running");
			break;
	}
	if (strstr(text, "rror") || strstr(text, "ailed"))
		*dot = CC_RED;

	return s;
}

static void layout_buttons(HWND hwnd)
{
	char h[96];
	char d[256];
	COLORREF dot;
	int32_t s = status(h, sizeof(h), d, sizeof(d), &dot);
	bool connected = s == CROWD_SIGNED_IN || s == CROWD_SESSION;
	HWND connect = GetDlgItem(hwnd, ID_CONNECT);
	HWND session = GetDlgItem(hwnd, ID_SESSION);
	HWND disconnect = GetDlgItem(hwnd, ID_DISCONNECT);
	RECT c;
	struct window_layout l;
	int32_t x = dip(hwnd, 20);
	int32_t gap = dip(hwnd, 12);

	measure_layout(hwnd, &l);
	GetClientRect(hwnd, &c);

	int32_t y = l.buttons_top;
	int32_t bh = l.buttons_h;

	int32_t w = (c.right - 2 * x - gap) / 2;

	ShowWindow(connect, connected ? SW_HIDE : SW_SHOW);
	EnableWindow(connect, s == CROWD_SIGN_IN);
	ShowWindow(session, connected ? SW_SHOW : SW_HIDE);
	SetWindowTextA(session, s == CROWD_SESSION ? "Stop Session" : "Start Session");
	ShowWindow(disconnect, connected ? SW_SHOW : SW_HIDE);
	MoveWindow(connect, x, y, 2 * w + gap, bh, TRUE);
	MoveWindow(session, x, y, w, bh, TRUE);
	MoveWindow(disconnect, x + w + gap, y, w, bh, TRUE);
	InvalidateRect(session, NULL, FALSE);
}

static void fill_round(HDC dc, const RECT *r, int32_t radius, COLORREF fill, COLORREF edge)
{
	HBRUSH b = CreateSolidBrush(fill);
	HPEN p = CreatePen(PS_SOLID, 1, edge);
	HGDIOBJ ob = SelectObject(dc, b);
	HGDIOBJ op = SelectObject(dc, p);

	RoundRect(dc, r->left, r->top, r->right, r->bottom, radius, radius);
	SelectObject(dc, ob);
	SelectObject(dc, op);
	DeleteObject(b);
	DeleteObject(p);
}

static COLORREF shade(COLORREF c, int32_t pct)
{
	return RGB(GetRValue(c) * pct / 100, GetGValue(c) * pct / 100, GetBValue(c) * pct / 100);
}

static void draw_button(const DRAWITEMSTRUCT *di)
{
	HWND hwnd = GetParent(di->hwndItem);
	char text[64];
	char h[96];
	char d[256];
	COLORREF dot;
	COLORREF fill;
	COLORREF edge;
	COLORREF ink;
	int32_t s = status(h, sizeof(h), d, sizeof(d), &dot);
	bool pressed = (di->itemState & ODS_SELECTED) != 0;
	bool hover = di->hwndItem == g_ui.hover;
	bool disabled = (di->itemState & ODS_DISABLED) != 0;
	RECT r = di->rcItem;
	HBRUSH bg = CreateSolidBrush(CC_ROYAL);

	FillRect(di->hDC, &r, bg);
	DeleteObject(bg);
	GetWindowTextA(di->hwndItem, text, sizeof(text));
	switch (di->CtlID) {
		case ID_CONNECT:
			fill = CC_YELLOW;
			edge = CC_YELLOW;
			ink = CC_ROYAL;
			break;
		case ID_SESSION:
			fill = s == CROWD_SESSION ? CC_RED : CC_YELLOW;
			edge = fill;
			ink = s == CROWD_SESSION ? CC_WHITE : CC_ROYAL;
			break;
		case ID_PACK:
			fill = CC_PANEL;
			edge = CC_SLATE;
			ink = CC_MUTED;
			break;
		default:
			fill = CC_ROYAL;
			edge = CC_SLATE;
			ink = CC_WHITE;
			break;
	}
	if (disabled) {
		fill = CC_PANEL;
		edge = CC_SLATE;
		ink = CC_GREY;

	} else if (pressed) {
		fill = shade(fill, 80);

	} else if (hover) {
		edge = di->CtlID == ID_DISCONNECT || di->CtlID == ID_PACK ? CC_MUTED : CC_WHITE;
		if (di->CtlID == ID_DISCONNECT || di->CtlID == ID_PACK)
			fill = CC_PANEL;
	}
	fill_round(di->hDC, &r, dip(hwnd, 12), fill, edge);
	SetBkMode(di->hDC, TRANSPARENT);
	SetTextColor(di->hDC, ink);
	SelectObject(di->hDC, di->CtlID == ID_PACK ? g_ui.small_body : g_ui.button);
	if (pressed)
		OffsetRect(&r, 0, 1);
	DrawTextA(di->hDC, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static LRESULT CALLBACK button_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_MOUSEMOVE && g_ui.hover != hwnd) {
		TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, hwnd, 0};
		HWND was = g_ui.hover;

		g_ui.hover = hwnd;
		TrackMouseEvent(&t);
		InvalidateRect(hwnd, NULL, FALSE);
		if (was)
			InvalidateRect(was, NULL, FALSE);

	} else if (msg == WM_MOUSELEAVE && g_ui.hover == hwnd) {
		g_ui.hover = NULL;
		InvalidateRect(hwnd, NULL, FALSE);

	} else if (msg == WM_SETCURSOR) {
		SetCursor(LoadCursorA(NULL, (LPCSTR) IDC_HAND));
		return TRUE;
	}

	return CallWindowProcA(g_button_proc, hwnd, msg, wparam, lparam);
}

static void fill_list(void)
{
	int32_t count = CrowdControlLogCount();

	if (!g_ui.list || g_ui.list_version == CrowdControlLogVersion())
		return;

	g_ui.list_version = CrowdControlLogVersion();
	SendMessageA(g_ui.list, WM_SETREDRAW, FALSE, 0);
	SendMessageA(g_ui.list, LB_RESETCONTENT, 0, 0);
	for (int32_t i = count - 1; i >= 0; --i) {
		int32_t viewer_len;
		bool failed;

		SendMessageA(g_ui.list, LB_ADDSTRING, 0, (LPARAM) CrowdControlLogEntry(i, &viewer_len, &failed));
	}
	if (!count)
		SendMessageA(g_ui.list, LB_ADDSTRING, 0, (LPARAM) "");
	SendMessageA(g_ui.list, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(g_ui.list, NULL, TRUE);
}

static void draw_list_row(const DRAWITEMSTRUCT *di)
{
	HWND hwnd = GetParent(di->hwndItem);
	RECT r = di->rcItem;
	int32_t count = CrowdControlLogCount();
	int32_t entry = count - 1 - (int32_t) di->itemID;
	HBRUSH b = CreateSolidBrush((di->itemID & 1) ? CC_PANEL_DARK : CC_PANEL);
	int32_t pad = dip(hwnd, 10);

	FillRect(di->hDC, &r, b);
	DeleteObject(b);
	SetBkMode(di->hDC, TRANSPARENT);
	SelectObject(di->hDC, g_ui.body);
	r.left += pad;
	r.right -= pad;
	if (!count || (int32_t) di->itemID < 0 || entry < 0) {
		if (di->itemID == 0) {
			SetTextColor(di->hDC, CC_MUTED);
			DrawTextA(di->hDC, "No effects yet: they show here, with the viewer who sent them.", -1, &r,
				DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
		}
		return;
	}

	int32_t viewer_len;
	bool failed;
	const char *text = CrowdControlLogEntry(entry, &viewer_len, &failed);

	if (viewer_len > 0) {
		RECT measure = r;

		SetTextColor(di->hDC, CC_YELLOW);
		SelectObject(di->hDC, g_ui.button);
		DrawTextA(di->hDC, text, viewer_len, &measure, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_CALCRECT);
		DrawTextA(di->hDC, text, viewer_len, &r, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
		r.left += measure.right - measure.left;
		SelectObject(di->hDC, g_ui.body);
	}
	SetTextColor(di->hDC, failed ? CC_RED : CC_WHITE);
	DrawTextA(di->hDC, text + viewer_len, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
}

static void paint_window(HWND hwnd, HDC target)
{
	RECT client;
	RECT r;
	char headline[96];
	char detail[256];
	COLORREF dot;

	GetClientRect(hwnd, &client);

	int32_t w = client.right;
	HDC dc = CreateCompatibleDC(target);
	HBITMAP buffer = CreateCompatibleBitmap(target, client.right, client.bottom);
	HGDIOBJ old = SelectObject(dc, buffer);

	FillRect(dc, &client, g_ui.royal);
	SetBkMode(dc, TRANSPARENT);

	if (g_ui.logo) {
		HDC src = CreateCompatibleDC(dc);
		BITMAP bm;
		HGDIOBJ o = SelectObject(src, g_ui.logo);

		GetObjectA(g_ui.logo, sizeof(bm), &bm);
		SetStretchBltMode(dc, HALFTONE);
		SetBrushOrgEx(dc, 0, 0, NULL);
		StretchBlt(dc, dip(hwnd, 16), dip(hwnd, 14), dip(hwnd, 240), dip(hwnd, 60), src, 0, 0, bm.bmWidth,
			bm.bmHeight, SRCCOPY);
		SelectObject(src, o);
		DeleteDC(src);
	}
	struct window_layout l;

	measure_layout(hwnd, &l);
	SelectObject(dc, g_ui.small_body);
	SetTextColor(dc, CC_MUTED);
	SetRect(&r, dip(hwnd, 20), l.sub_top, w - dip(hwnd, 20), l.sub_top + l.sub_h);
	DrawTextA(dc, "Stream Interactions powered by Crowd Control", -1, &r, DT_SINGLELINE | DT_NOPREFIX);

	SelectObject(dc, g_ui.body);
	SetTextColor(dc, CC_WHITE);
	SetRect(&r, dip(hwnd, 20), l.desc_top, w - dip(hwnd, 20), l.desc_top + l.desc_h);
	DrawTextA(dc, DESCRIPTION, -1, &r, DT_WORDBREAK | DT_NOPREFIX);
	SetTextColor(dc, CC_BLUE);
	SetRect(&g_ui.link, dip(hwnd, 20), l.link_top, w - dip(hwnd, 20), l.link_top + l.link_h);
	r = g_ui.link;

	char line[200];

	StrPrintf(line, "Setup guide: %s", g_ui.guide);
	DrawTextA(dc, line, -1, &r, DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
	DrawTextA(dc, line, -1, &r, DT_SINGLELINE | DT_NOPREFIX);
	g_ui.link.right = r.right;

	status(headline, sizeof(headline), detail, sizeof(detail), &dot);
	SetRect(&r, dip(hwnd, 20), l.panel_top, w - dip(hwnd, 20), l.panel_top + l.panel_h);
	fill_round(dc, &r, dip(hwnd, 14), CC_PANEL, CC_SLATE);

	HBRUSH b = CreateSolidBrush(dot);
	HPEN p = CreatePen(PS_SOLID, 1, dot);
	HGDIOBJ ob = SelectObject(dc, b);
	HGDIOBJ op = SelectObject(dc, p);
	int32_t cx = r.left + dip(hwnd, 20);
	int32_t cy = r.top + dip(hwnd, 10) + l.head_h / 2;
	int32_t rad = dip(hwnd, 6);

	Ellipse(dc, cx - rad, cy - rad, cx + rad, cy + rad);
	SelectObject(dc, ob);
	SelectObject(dc, op);
	DeleteObject(b);
	DeleteObject(p);

	RECT t = r;

	t.left += dip(hwnd, 36);
	t.right -= dip(hwnd, 14);
	t.top += dip(hwnd, 10);
	SelectObject(dc, g_ui.button);
	SetTextColor(dc, CC_WHITE);
	DrawTextA(dc, headline, -1, &t, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
	t.top += l.head_h + dip(hwnd, 2);
	SelectObject(dc, g_ui.small_body);
	SetTextColor(dc, CC_MUTED);
	DrawTextA(dc, detail, -1, &t, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

	SelectObject(dc, g_ui.title);
	SetTextColor(dc, CC_WHITE);
	SetRect(&r, dip(hwnd, 20), l.title_top, w - dip(hwnd, 20), l.title_top + l.title_h);
	DrawTextA(dc, "Recent Effects", -1, &r, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

	BitBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
	SelectObject(dc, old);
	DeleteObject(buffer);
	DeleteDC(dc);
}

static void layout_window(HWND hwnd)
{
	RECT c;
	struct window_layout l;
	HWND pack = GetDlgItem(hwnd, ID_PACK);

	measure_layout(hwnd, &l);
	GetClientRect(hwnd, &c);
	layout_buttons(hwnd);
	ShowWindow(pack, g_ui.show_pack ? SW_SHOW : SW_HIDE);
	MoveWindow(pack, c.right - dip(hwnd, 20) - dip(hwnd, 130), l.title_top + (l.title_h - dip(hwnd, 26)) / 2,
		dip(hwnd, 130), dip(hwnd, 26), TRUE);
	MoveWindow(g_ui.list, dip(hwnd, 20), l.list_top, c.right - dip(hwnd, 40), c.bottom - l.list_top - dip(hwnd, 20),
		TRUE);
}

static void update_window_text(void)
{
	if (!g_wnd)
		return;

	layout_buttons(g_wnd);
	fill_list();
	InvalidateRect(g_wnd, NULL, FALSE);
}

static void dark_scroll_bars(HWND hwnd)
{
	typedef HRESULT (WINAPI *set_theme_func)(HWND, LPCWSTR, LPCWSTR);
	HMODULE ux = LoadLibraryA("uxtheme.dll");
	set_theme_func set = ux ? (set_theme_func) (void (*)(void)) GetProcAddress(ux, "SetWindowTheme") : NULL;

	if (set)
		set(hwnd, L"DarkMode_Explorer", NULL);
}

static void on_create(HWND hwnd)
{
	static const char *const labels[] = {"Connect Crowd Control", "Start Session", "Disconnect",
		"Write pack file"};
	BOOL dark = TRUE;
	COLORREF caption = CC_ROYAL;
	COLORREF caption_text = CC_WHITE;
	char pack[128];

	DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
	DwmSetWindowAttribute(hwnd, 35, &caption, sizeof(caption));
	DwmSetWindowAttribute(hwnd, 36, &caption_text, sizeof(caption_text));
	g_ui.fonts_loaded = load_brand_fonts();
	g_ui.title = make_font(hwnd, 18, FW_BOLD);
	g_ui.button = make_font(hwnd, 15, FW_BOLD);
	g_ui.body = make_font(hwnd, 14, FW_NORMAL);
	g_ui.small_body = make_font(hwnd, 12, FW_NORMAL);
	g_ui.logo = (HBITMAP) LoadImageA(GetModuleHandleA(NULL), "CC_LOGO", IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
	g_ui.royal = CreateSolidBrush(CC_ROYAL);
	g_ui.panel_dark = CreateSolidBrush(CC_PANEL_DARK);
	g_ui.list_version = -1;
	g_ui.hover = NULL;
	g_ui.show_pack = IniReadLayered(IniDataPath(), IniPath(), "crowdcontrol", "pack_button", pack, sizeof(pack)) &&
		atoi(pack) != 0;
	IniGetString("crowdcontrol", "pack_id", "", pack, sizeof(pack));
	if (pack[0]) {
		StrPrintf(g_ui.guide, "crowdcontrol.live/guides/%s", pack);

	} else {
		StrPrintf(g_ui.guide, "crowdcontrol.live%s", pack);
	}
	for (int32_t i = 0; i < 4; ++i) {
		HWND b = CreateWindowExA(0, "BUTTON", labels[i], WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 10, 10, hwnd,
			(HMENU) (INT_PTR) (ID_CONNECT + i), GetModuleHandleA(NULL), NULL);

		g_button_proc = (WNDPROC) SetWindowLongPtrA(b, GWLP_WNDPROC, (LONG_PTR) button_proc);
	}
	g_ui.list = CreateWindowExA(0, "LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_OWNERDRAWFIXED |
		LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT | LBS_NOSEL, 0, 0, 10, 10, hwnd, (HMENU) (INT_PTR) ID_LIST,
		GetModuleHandleA(NULL), NULL);
	dark_scroll_bars(g_ui.list);
	SetTimer(hwnd, 1, 1000, NULL);
	layout_window(hwnd);
	update_window_text();
}

static void on_command(HWND hwnd, int32_t id)
{
	switch (id) {
		case ID_CONNECT:
			CrowdNetSignIn(CrowdControlNet());
			g_auth_started = GetTickCount() | 1;
			break;
		case ID_SESSION: {
			char text[256];

			CrowdNetSession(CrowdControlNet(), CrowdNetState(CrowdControlNet(), text, sizeof(text)) != CROWD_SESSION);
			break;
		}
		case ID_DISCONNECT:
			CrowdNetSignOut(CrowdControlNet());
			g_auth_started = 0;
			break;
		case ID_PACK: {
			char path[MAX_PATH];
			char line[200];

			FileGetExeDir(path, sizeof(path));
			StrCat(path, "OregonTrailII.pack.json");
			if (CrowdControlWritePack(path)) {
				StrPrintf(line, "Pack file written: %s", path);

			} else {
				StrPrintf(line, "Could not write %s", path);
			}
			CrowdControlLog(line);
			break;
		}
	}
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg) {
		case WM_CREATE:
			on_create(hwnd);
			return 0;
		case WM_MEASUREITEM: {
			MEASUREITEMSTRUCT *mi = (MEASUREITEMSTRUCT *) lparam;

			mi->itemHeight = (UINT) dip(hwnd, 30);
			return TRUE;
		}
		case WM_DRAWITEM: {
			const DRAWITEMSTRUCT *di = (const DRAWITEMSTRUCT *) lparam;

			if (di->CtlID == ID_LIST) {
				draw_list_row(di);

			} else {
				draw_button(di);
			}
			return TRUE;
		}
		case WM_CTLCOLORLISTBOX:
			return (LRESULT) g_ui.panel_dark;
		case WM_ERASEBKGND:
			return 1;
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC dc = BeginPaint(hwnd, &ps);

			paint_window(hwnd, dc);
			EndPaint(hwnd, &ps);
			return 0;
		}
		case WM_SIZE:
			layout_window(hwnd);
			InvalidateRect(hwnd, NULL, FALSE);
			return 0;
		case WM_SETCURSOR: {
			POINT p;

			GetCursorPos(&p);
			ScreenToClient(hwnd, &p);
			if (LOWORD(lparam) == HTCLIENT && PtInRect(&g_ui.link, p)) {
				SetCursor(LoadCursorA(NULL, (LPCSTR) IDC_HAND));
				return TRUE;
			}
			break;
		}
		case WM_LBUTTONUP: {
			POINT p = {(short) LOWORD(lparam), (short) HIWORD(lparam)};
			char url[200];

			if (PtInRect(&g_ui.link, p)) {
				StrPrintf(url, "https://%s", g_ui.guide);
				ShellExecuteA(hwnd, "open", url, NULL, NULL, SW_SHOWNORMAL);
			}
			return 0;
		}
		case WM_TIMER:
			update_window_text();
			return 0;
		case WM_COMMAND:
			on_command(hwnd, LOWORD(wparam));
			update_window_text();
			return 0;
		case WM_CLOSE:
			if (g_parent_win && g_parent_win->base.hwnd) {
				PostMessageA((HWND) g_parent_win->base.hwnd, WM_COMMAND, CROWD_CONTROL_MENU_ID, 0);
				return 0;
			}
			break;
		case WM_DESTROY:
			KillTimer(hwnd, 1);
			DeleteObject(g_ui.title);
			DeleteObject(g_ui.button);
			DeleteObject(g_ui.body);
			DeleteObject(g_ui.small_body);
			if (g_ui.logo)
				DeleteObject(g_ui.logo);
			DeleteObject(g_ui.royal);
			DeleteObject(g_ui.panel_dark);
			memset(&g_ui, 0, sizeof(g_ui));
			g_wnd = NULL;
			return 0;
	}

	return DefWindowProcA(hwnd, msg, wparam, lparam);
}

void CrowdControlCloseWindow(void)
{
	if (g_wnd)
		DestroyWindow(g_wnd);
}

void CrowdControlToggleWindow(void)
{
	static bool registered;

	if (g_wnd) {
		DestroyWindow(g_wnd);
		return;
	}
	if (!registered) {
		WNDCLASSA wc;

		memset(&wc, 0, sizeof(wc));
		wc.lpfnWndProc = wnd_proc;
		wc.hInstance = GetModuleHandleA(NULL);
		wc.hCursor = LoadCursorA(NULL, (LPCSTR) IDC_ARROW);
		wc.hbrBackground = NULL;
		wc.hIcon = LoadIconA(wc.hInstance, MAKEINTRESOURCEA(2417));
		wc.lpszClassName = "OTIICrowdControl";
		RegisterClassA(&wc);
		registered = true;
	}

	HWND parent = g_parent_win ? (HWND) g_parent_win->base.hwnd : NULL;

	g_wnd = CreateWindowExA(0, "OTIICrowdControl", "Crowd Control", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
		CW_USEDEFAULT, ScalerDpiPx(parent, 600), ScalerDpiPx(parent, 640), NULL, NULL, GetModuleHandleA(NULL), NULL);
	if (g_wnd) {
		if (g_parent_win)
			WindowPlaceBeside(g_wnd, g_parent_win->base.hwnd, true, false);
		ShowWindow(g_wnd, SW_SHOWNOACTIVATE);
	}
}

void *CrowdControlWindowInfo(bool *fonts_loaded, bool *logo_loaded, int32_t *buttons_shown)
{
	*fonts_loaded = g_wnd ? g_ui.fonts_loaded : false;
	*logo_loaded = g_wnd && g_ui.logo != NULL;
	*buttons_shown = 0;
	for (int32_t i = ID_CONNECT; g_wnd && i <= ID_PACK; ++i)
		if (IsWindowVisible(GetDlgItem(g_wnd, i)))
			++*buttons_shown;

	return g_wnd;
}
