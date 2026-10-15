#include "../testframework.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <string.h>

#include <windows.h>
#include <vfw.h>

#include "../../src/app/app.h"
#include "../../src/features/controlstate.h"
#include "../../src/base/cstr.h"
#include "../../src/base/object.h"
#include "../../src/base/path.h"
#include "../../src/base/str.h"
#include "../../src/ui/framewin.h"
#include "../../src/game/game.h"
#include "../../src/base/geom.h"
#include "../../src/base/ini.h"
#include "../../src/base/memory.h"
#include "../../src/ui/scaler.h"
#include "../../src/draw/screendc.h"
#include "../../src/draw/windc.h"
#include "../../src/ui/window.h"
#include "../../src/ui/windowpos.h"
#include "../testsupport.h"

static HWND main_hwnd(void)
{
	return g_parent_win ? (HWND) g_parent_win->base.hwnd : NULL;
}

static int largest_fit_now(HWND hwnd, int gw, int gh)
{
	RECT w, c;
	MONITORINFO mi;

	GetWindowRect(hwnd, &w);
	GetClientRect(hwnd, &c);
	mi.cbSize = sizeof(mi);
	GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
	return ScalerLargestFit(gw, gh, (w.right - w.left) - c.right, (w.bottom - w.top) - c.bottom,
		mi.rcWork.right - mi.rcWork.left, mi.rcWork.bottom - mi.rcWork.top);
}

static void screen_to_window(HWND hwnd, POINT *p)
{
	struct point pt = {p->y, p->x};

	ScalerScreenToWindow(hwnd, &pt);
	p->x = pt.h;
	p->y = pt.v;
}

static void screen_to_dc(HDC dc, POINT *p)
{
	struct point pt = {p->y, p->x};

	ScalerScreenToDc(dc, &pt);
	p->x = pt.h;
	p->y = pt.v;
}

static void dc_to_screen(HDC dc, POINT *p)
{
	struct point pt = {p->y, p->x};

	ScalerDcToScreen(dc, &pt);
	p->x = pt.h;
	p->y = pt.v;
}

static void check_scale(HWND hwnd, int n, int gw, int gh)
{
	RECT c;
	HDC mem = (HDC) g_parent_win->base.windc->base.hdc, win;
	POINT org = {0, 0}, p;
	int k, j, x, y;
	const int PX = 7, PY = 9;

	GetClientRect(hwnd, &c);
	CHECK_EQ(c.right, gw * n);
	CHECK_EQ(c.bottom, gh * n);
	CHECK_EQ(ScalerScale(), n);

	SetPixelV(mem, PX, PY, RGB(255, 0, 0));
	SetPixelV(mem, PX + 1, PY, RGB(0, 0, 255));
	GdiFlush();
	ScalerPresentAll();
	win = GetDC(hwnd);
	for (k = 0; k < n; ++k)
		for (j = 0; j < n; ++j) {
			CHECK_EQ(GetPixel(win, PX * n + k, PY * n + j), RGB(255, 0, 0));
			CHECK_EQ(GetPixel(win, (PX + 1) * n + k, PY * n + j), RGB(0, 0, 255));
		}
	ReleaseDC(hwnd, win);

	ClientToScreen(hwnd, &org);
	for (x = 0; x < gw; x += 97)
		for (y = 0; y < gh; y += 61)
			for (k = 0; k < n; k += n > 1 ? n - 1 : 1) {
				p.x = org.x + x * n + k;
				p.y = org.y + y * n + k;
				screen_to_window(hwnd, &p);
				CHECK(p.x == x && p.y == y);
				p.x = org.x + x * n + k;
				p.y = org.y + y * n + k;
				screen_to_dc(mem, &p);
				CHECK(p.x == x && p.y == y);
			}
	p.x = 100;
	p.y = 50;
	dc_to_screen(mem, &p);
	CHECK_EQ(p.x, org.x + 100 * n);
	CHECK_EQ(p.y, org.y + 50 * n);
	p.x = org.x - 1;
	p.y = org.y - 1;
	screen_to_window(hwnd, &p);
	CHECK_EQ(p.x, -1);
	CHECK_EQ(p.y, -1);
}

struct best_monitor {
	RECT work;
	LONG area;
};

static BOOL CALLBACK pick_monitor(HMONITOR mon, HDC dc, LPRECT rc, LPARAM data)
{
	struct best_monitor *best = (struct best_monitor *) data;
	MONITORINFO mi;
	LONG area;

	(void) dc;
	(void) rc;
	mi.cbSize = sizeof(mi);
	if (GetMonitorInfoA(mon, &mi)) {
		area = (mi.rcWork.right - mi.rcWork.left) * (mi.rcWork.bottom - mi.rcWork.top);
		if (area > best->area) {
			best->area = area;
			best->work = mi.rcWork;
		}
	}
	return TRUE;
}

static void signature(const struct control_state *st, char *out, size_t size)
{
	out[0] = 0;
	for (int i = 0; i < st->option_count; ++i) {
		strncat_s(out, size, st->options[i].label, _TRUNCATE);
		strncat_s(out, size, "; ", _TRUNCATE);
	}
}

TEST_GAME(scn_scaler, every_scale)
{
	HWND hwnd = main_hwnd();
	HWND edit;
	HFONT font;
	LOGFONTA lf, got;
	int32_t gw = 0, gh = 0;
	int start, fit, n;
	RECT was;

	CHECK(hwnd != NULL);
	if (!hwnd)
		return;
	CHECK(ScalerGameSize(hwnd, &gw, &gh));
	if (!gw || !gh)
		return;
	start = ScalerScale();
	{
		struct best_monitor best;
		RECT w;

		best.area = 0;
		EnumDisplayMonitors(NULL, NULL, pick_monitor, (LPARAM) &best);
		GetWindowRect(hwnd, &was);
		if (best.area) {
			SetWindowPos(hwnd, NULL, best.work.left + 20, best.work.top + 20, 0, 0,
				SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
			TestPump(300);
			GetWindowRect(hwnd, &w);
			TNote("moved to the monitor at %d,%d (%d x %d work area)", (int) best.work.left, (int) best.work.top,
				(int) (best.work.right - best.work.left), (int) (best.work.bottom - best.work.top));
		}
		check_scale(hwnd, ScalerScale(), gw, gh);
	}
	fit = largest_fit_now(hwnd, gw, gh);
	TNote("main window %d x %d game pixels, starting at %dx, room for %dx on this monitor", gw, gh, start, fit);

	edit = CreateWindowExA(0, "EDIT", "scaled", WS_CHILD | WS_VISIBLE | WS_BORDER, 40, 30, 120, 22, hwnd, NULL,
		GetModuleHandleA(NULL), NULL);
	memset(&lf, 0, sizeof(lf));
	lf.lfHeight = -13;
	strcpy_s(lf.lfFaceName, sizeof(lf.lfFaceName), "Arial");
	font = CreateFontIndirectA(&lf);
	SendMessageA(edit, WM_SETFONT, (WPARAM) font, 0);
	ScalerChild(edit, 40, 30, 120, 22, font);
	CHECK(TestSave("scaler_base"));

	for (n = 1; n <= SCALER_MAX; ++n) {
		RECT r;

		TCase("%dx", n);
		if (n > fit) {
			TNote("%dx does not fit this monitor: skipped", n);
			continue;
		}
		ScalerSet(n, false);
		TestPump(200);
		check_scale(hwnd, n, gw, gh);

		GetWindowRect(edit, &r);
		MapWindowPoints(NULL, hwnd, (POINT *) &r, 2);
		CHECK_EQ(r.left, 40 * n);
		CHECK_EQ(r.top, 30 * n);
		CHECK_EQ(r.right - r.left, 120 * n);
		CHECK_EQ(r.bottom - r.top, 22 * n);
		GetObjectA((HFONT) SendMessageA(edit, WM_GETFONT, 0, 0), sizeof(got), &got);
		CHECK_EQ(got.lfHeight, -13 * n);
		{
			HDC win;
			COLORREF inside;

			RedrawWindow(edit, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
			ScalerPresent(hwnd, true);
			win = GetDC(hwnd);
			inside = GetPixel(win, r.left + (r.right - r.left) - 4 * n, r.top + (r.bottom - r.top) / 2);
			ReleaseDC(hwnd, win);
			CHECK_MSG(inside == GetSysColor(COLOR_WINDOW), "%dx: the edit box shows %06lX, not its own background",
				n, (unsigned long) inside);
		}

		if (ScalerCursorScale() != n) {
			TNote("%dx: Scale Mouse Pointer is off in OREGONII.INI: the cursor is not checked", n);

		} else {
			int cw = GetSystemMetrics(SM_CXCURSOR), ch = GetSystemMetrics(SM_CYCURSOR), bytes = cw / 8 * ch, i;
			unsigned char *and_plane = MemAlloc(1, (size_t) bytes), *xor_plane = MemAlloc(1, (size_t) bytes);
			HCURSOR base, shown;
			ICONINFO ii;
			BITMAP bm;

			for (i = 0; i < bytes; ++i) {
				and_plane[i] = 0xFF;
				xor_plane[i] = 0;
			}
			and_plane[0] = 0x0F;
			xor_plane[0] = 0xA0;
			base = CreateCursor(GetModuleHandleA(NULL), 3, 5, cw, ch, and_plane, xor_plane);
			shown = (HCURSOR) ScalerCursor(base);
			CHECK(GetIconInfo(shown, &ii));
			CHECK_EQ(ii.xHotspot, 3 * n);
			CHECK_EQ(ii.yHotspot, 5 * n);
			GetObjectA(ii.hbmMask, sizeof(bm), &bm);
			CHECK_EQ(bm.bmWidth, cw * n);
			CHECK_EQ(bm.bmHeight, 2 * ch * n);
			{
				HDC mdc = CreateCompatibleDC(NULL);
				HGDIOBJ old = SelectObject(mdc, ii.hbmMask);

				for (int k2 = 0; k2 < n; ++k2) {
					CHECK_EQ(GetPixel(mdc, 1 * n + k2, k2) & 0xFFFFFF, 0);
					CHECK_EQ(GetPixel(mdc, 4 * n + k2, k2) & 0xFFFFFF, 0xFFFFFF);
					CHECK_EQ(GetPixel(mdc, 0 * n + k2, ch * n + k2) & 0xFFFFFF, 0xFFFFFF);
					CHECK_EQ(GetPixel(mdc, 1 * n + k2, ch * n + k2) & 0xFFFFFF, 0);
				}
				SelectObject(mdc, old);
				DeleteDC(mdc);
			}
			DeleteObject(ii.hbmMask);
			if (ii.hbmColor)
				DeleteObject(ii.hbmColor);
			if (n > 1)
				CHECK(shown != base);
			ScalerCursorForget(base);
			DestroyCursor(base);
			MemFree(and_plane);
			MemFree(xor_plane);

			shown = (HCURSOR) ScalerCursor(LoadCursorA(NULL, IDC_ARROW));
			if (n > 1 && GetIconInfo(shown, &ii)) {
				GetObjectA(ii.hbmColor ? ii.hbmColor : ii.hbmMask, sizeof(bm), &bm);
				CHECK_EQ(bm.bmWidth, 32 * n);
				DeleteObject(ii.hbmMask);
				if (ii.hbmColor)
					DeleteObject(ii.hbmColor);
			}
		}

		{
			struct control_state *st = MemAlloc(1, sizeof(*st));
			int i, pick = -1;

			TestLoad("scaler_base");
			TestPump(200);
			if (ControlGetState(st) && st->screen == CONTROL_LANDMARK)
				for (i = 0; i < st->option_count && pick < 0; ++i)
					if (st->options[i].button && strstr(st->options[i].label, "Supplies"))
						pick = i;
			if (pick < 0) {
				char labels[512] = "";
				for (i = 0; i < st->option_count; ++i) {
					strncat_s(labels, sizeof(labels), st->options[i].label, _TRUNCATE);
					strncat_s(labels, sizeof(labels), "; ", _TRUNCATE);
				}
				TNote("%dx: screen %d, no supplies/map option to click among: %s", n, st->screen, labels);

			} else {
				char before[512], after[512];

				signature(st, before, sizeof(before));
				ControlClick(st, pick);
				TestPump(1500);
				ControlGetState(st);
				signature(st, after, sizeof(after));
				CHECK_MSG(strcmp(before, after) != 0, "%dx: clicking Supplies changed nothing (still: %s)", n, after);
				if (n == 1)
					TNote("Supplies opens a screen with: %s", after);
				TestLoad("scaler_base");
			}
			MemFree(st);
		}
	}
	TCase("");
	DestroyWindow(edit);
	DeleteObject(font);
	ScalerSet(start, false);
	SetWindowPos(hwnd, NULL, was.left, was.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	TestPump(300);
	{
		RECT c;
		GetClientRect(hwnd, &c);
		CHECK_EQ(c.right, gw * start);
	}
}

static BOOL CALLBACK pick_negative(HMONITOR mon, HDC dc, LPRECT rc, LPARAM data)
{
	RECT *found = (RECT *) data;
	MONITORINFO mi;

	(void) dc;
	(void) rc;
	mi.cbSize = sizeof(mi);
	if (GetMonitorInfoA(mon, &mi) && (mi.rcWork.left < 0 || mi.rcWork.top < 0) && found->right == found->left) {
		*found = mi.rcWork;
		return FALSE;
	}
	return TRUE;
}

static int ini_int(const char *key)
{
	return (int) GetPrivateProfileIntA("window", key, -32768, IniPath());
}

static void ini_restore(const char *key, int value)
{
	char text[16];

	if (value == -32768) {
		WritePrivateProfileStringA("window", key, NULL, IniPath());
		return;
	}
	_snprintf_s(text, sizeof(text), _TRUNCATE, "%d", value);
	WritePrivateProfileStringA("window", key, text, IniPath());
}

TEST_GAME(scn_scaler, negative_monitor)
{
	HWND hwnd = main_hwnd();
	RECT neg = {0, 0, 0, 0}, was, w, c;
	int32_t gw = 0, gh = 0;
	int start, n, fit, saved_x = ini_int("x"), saved_y = ini_int("y");

	CHECK(hwnd != NULL);
	if (!hwnd || !ScalerGameSize(hwnd, &gw, &gh))
		return;
	EnumDisplayMonitors(NULL, NULL, pick_negative, (LPARAM) &neg);
	if (neg.right == neg.left) {
		TNote("no monitor left of or above the main one: skipped");
		return;
	}
	TNote("monitor at %d,%d (%d x %d work area)", (int) neg.left, (int) neg.top, (int) (neg.right - neg.left),
		(int) (neg.bottom - neg.top));
	start = ScalerScale();
	GetWindowRect(hwnd, &was);
	SetWindowPos(hwnd, NULL, neg.left + 20, neg.top + 20, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	TestPump(300);
	fit = largest_fit_now(hwnd, gw, gh);

	for (n = 1; n <= SCALER_MAX; ++n) {
		int x, y, gx, gy, r, bad = 0;

		TCase("%dx", n);
		if (n > fit) {
			TNote("%dx does not fit this monitor: skipped", n);
			continue;
		}
		ScalerSet(n, false);
		SetWindowPos(hwnd, NULL, neg.left + 20, neg.top + 20, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
		TestPump(200);
		GetWindowRect(hwnd, &w);
		CHECK_EQ(w.left, neg.left + 20);
		CHECK_EQ(w.top, neg.top + 20);
		check_scale(hwnd, n, gw, gh);

		{
			TITLEBARINFO tb;
			LRESULT hit;
			tb.cbSize = sizeof(tb);
			if (GetTitleBarInfo(hwnd, &tb) && tb.rcTitleBar.bottom > tb.rcTitleBar.top) {
				x = (tb.rcTitleBar.left + tb.rcTitleBar.right) / 2;
				y = (tb.rcTitleBar.top + tb.rcTitleBar.bottom) / 2;
				CHECK(y < 0 || neg.top >= 0);
				hit = SendMessageA(hwnd, WM_NCHITTEST, 0, MAKELPARAM((WORD) (short) x, (WORD) (short) y));
				CHECK_MSG(hit == HTCAPTION, "the title bar at %d,%d answers %d, not HTCAPTION", x, y, (int) hit);

			} else {
				TNote("%dx: no title bar reported", n);
			}
		}

		{
			SIZE sz = {0, 0};
			UINT dpi = GetDpiForWindow(hwnd);
			GetWindowRect(hwnd, &w);
			CHECK(SendMessageA(hwnd, 0x02E4, dpi, (LPARAM) &sz));
			CHECK_EQ(sz.cx, w.right - w.left);
			CHECK_EQ(sz.cy, w.bottom - w.top);
			CHECK(SendMessageA(hwnd, 0x02E4, dpi * 3 / 2, (LPARAM) &sz));
			CHECK(sz.cx > gw * n && sz.cy > gh * n);
		}

		for (gy = 0; gy < gh; gy += 37)
			for (gx = 0; gx < gw; gx += 41)
				for (r = 0; r < n; ++r) {
					POINT p = {gx * n + r, gy * n + (n - 1 - r)};
					ClientToScreen(hwnd, &p);
					screen_to_window(hwnd, &p);
					if (p.x != gx || p.y != gy)
						++bad;
				}
		CHECK_EQ(bad, 0);
	}
	TCase("");

	{
		int h, top;
		RECT suggested;

		GetWindowRect(hwnd, &w);
		h = w.bottom - w.top;
		top = neg.bottom - h / 2;
		SendMessageA(hwnd, WM_ENTERSIZEMOVE, 0, 0);
		SetWindowPos(hwnd, NULL, neg.left + 20, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
		TestPump(300);
		GetWindowRect(hwnd, &w);
		TCase("dragging, straddling two monitors");
		CHECK_EQ(w.left, neg.left + 20);
		CHECK_EQ(w.top, top);
		suggested = w;
		SendMessageA(hwnd, WM_DPICHANGED, MAKEWPARAM(96, 96), (LPARAM) &suggested);
		TestPump(200);
		GetWindowRect(hwnd, &w);
		TCase("a DPI change while dragging");
		CHECK_EQ(w.left, suggested.left);
		CHECK_EQ(w.top, suggested.top);
		GetClientRect(hwnd, &c);
		CHECK_EQ(c.right, gw * ScalerScale());
		CHECK_EQ(c.bottom, gh * ScalerScale());
		SendMessageA(hwnd, WM_EXITSIZEMOVE, 0, 0);
		TestPump(200);
		GetWindowRect(hwnd, &w);
		TCase("the drag ends");
		CHECK_EQ(w.top, suggested.top);
	}

	{
		TCase("position saved and restored");
		SetWindowPos(hwnd, NULL, neg.left + 40, neg.top + 30, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
		TestPump(100);
		WindowPosSave(hwnd);
		CHECK_EQ(ini_int("x"), neg.left + 40);
		CHECK_EQ(ini_int("y"), neg.top + 30);
		SetWindowPos(hwnd, NULL, was.left, was.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
		TestPump(100);
		CHECK(WindowPosRestore(hwnd));
		GetWindowRect(hwnd, &w);
		CHECK_EQ(w.left, neg.left + 40);
		CHECK_EQ(w.top, neg.top + 30);
		ini_restore("x", saved_x);
		ini_restore("y", saved_y);
	}
	TCase("");

	ScalerSet(start, false);
	SetWindowPos(hwnd, NULL, was.left, was.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	TestPump(300);
}

TEST_GAME(scn_scaler, other_window_position)
{
	int saved_x = ini_int("health_x"), saved_y = ini_int("health_y"), main_x = ini_int("x"), main_y = ini_int("y");
	HWND w = CreateWindowExA(0, "STATIC", "position test", WS_POPUP | WS_CAPTION | WS_VISIBLE, 0, 0, 200, 150, NULL,
		NULL, GetModuleHandleA(NULL), NULL);
	RECT work, r;

	CHECK(w != NULL);
	if (!w)
		return;
	SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
	SetWindowPos(w, NULL, work.left + 123, work.top + 77, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	WindowPosSaveAs(w, "health");
	CHECK_EQ(ini_int("health_x"), work.left + 123);
	CHECK_EQ(ini_int("health_y"), work.top + 77);
	CHECK_EQ(ini_int("x"), main_x);
	CHECK_EQ(ini_int("y"), main_y);
	SetWindowPos(w, NULL, work.left + 300, work.top + 200, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	CHECK(WindowPosRestoreAs(w, "health"));
	GetWindowRect(w, &r);
	CHECK_EQ(r.left, work.left + 123);
	CHECK_EQ(r.top, work.top + 77);
	TCase("nothing saved: left where it is");
	ini_restore("health_x", -32768);
	ini_restore("health_y", -32768);
	CHECK(!WindowPosRestoreAs(w, "health"));
	TCase("");
	DestroyWindow(w);
	ini_restore("health_x", saved_x);
	ini_restore("health_y", saved_y);
}

TEST_GAME(scn_scaler, playing_video_not_covered)
{
	HWND hwnd = main_hwnd(), video;
	RECT r;
	int cx, cy, mode = 0, tries;

	char file[512];

	CHECK(hwnd != NULL);
	{
		struct path *rsrc = APP_OPS(g_app)->GetResourcePath(g_app);
		struct cstr *dir = PathCopyPath(rsrc);
		ObjectDelete(rsrc);
		char joined[512];
		StrCopy(joined, CStrGet(dir));
		StrCat(joined, "AVI\\4000.AVI");
		CStrDestroy(&dir);
		if (!GetFullPathNameA(joined, sizeof(file), file, NULL) || !GetShortPathNameA(file, file, sizeof(file)))
			StrCopy(file, joined);
	}
	video = MCIWndCreateA(hwnd, GetModuleHandleA(NULL), WS_CHILD | WS_VISIBLE | MCIWNDF_NOPLAYBAR | MCIWNDF_NOMENU |
		MCIWNDF_NOERRORDLG, file);
	CHECK_MSG(video != NULL, "the video window opens");
	if (!video)
		return;
	SetWindowPos(video, HWND_TOP, 40, 40, 0, 0, SWP_NOSIZE);
	GetWindowRect(video, &r);
	MapWindowPoints(NULL, hwnd, (POINT *) &r, 2);
	cx = (r.left + r.right) / 2;
	cy = (r.top + r.bottom) / 2;
	TNote("video at %ld,%ld - %ld,%ld", r.left, r.top, r.right, r.bottom);
	CHECK_EQ(ScalerTestCovers(hwnd, cx, cy), 1);
	MCIWndPlay(video);
	for (tries = 0; tries < 40 && (mode = (int) MCIWndGetMode(video, NULL, 0)) != MCI_MODE_PLAY; ++tries)
		TestPump(25);
	CHECK_EQ(mode, MCI_MODE_PLAY);
	CHECK_EQ(ScalerTestCovers(hwnd, cx, cy), 0);
	CHECK_EQ(ScalerTestCovers(hwnd, r.left - 5, cy), 1);
	MCIWndStop(video);
	TestPump(50);
	CHECK_EQ(ScalerTestCovers(hwnd, cx, cy), 1);
	MCIWndDestroy(video);
	TestPump(50);
}

TEST_GAME(scn_scaler, rest_shows_each_day)
{
	int32_t before, shown;

	CHECK(g_game != NULL);
	if (!g_game)
		return;
	CHECK(TestSave("scaler_rest"));
	TestPump(100);
	before = ScalerTestPresents();
	GameRest(g_game, 72, 1);
	shown = ScalerTestPresents() - before;
	TNote("the window was shown %d times during a three-day rest", shown);
	CHECK(shown >= 3);
	TestLoad("scaler_rest");
}

const struct test_reg TESTS_scn_scaler[] = {
	TEST_GAME_ENTRY(scn_scaler, every_scale),
	TEST_GAME_ENTRY(scn_scaler, negative_monitor),
	TEST_GAME_ENTRY(scn_scaler, other_window_position),
	TEST_GAME_ENTRY(scn_scaler, playing_video_not_covered),
	TEST_GAME_ENTRY(scn_scaler, rest_shows_each_day),
	TEST_END
};

#endif
