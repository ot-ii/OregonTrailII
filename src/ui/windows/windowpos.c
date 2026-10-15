#include "../windowpos.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <dwmapi.h>

#include "../../base/ini.h"
#include "../../base/str.h"

static const char SECTION[] = "window";
#define NOT_SET (-32768)

static bool command_line_pos(int32_t *x, int32_t *y)
{
	const char *cmd = GetCommandLineA();
	const char *p = cmd ? strstr(cmd, "/pos=") : NULL;
	char *end;

	if (!p)
		return false;

	p += 5;
	long vx = strtol(p, &end, 10);

	if (end == p || *end != ',')
		return false;

	p = end + 1;
	long vy = strtol(p, &end, 10);

	if (end == p)
		return false;

	*x = (int32_t) vx;
	*y = (int32_t) vy;

	return true;
}

static void keys(const char *name, char *kx, char *ky, size_t size)
{
	if (name && name[0]) {
		StrPrintfN(kx, size, "%s_x", name);
		StrPrintfN(ky, size, "%s_y", name);

	} else {
		StrCopyN(kx, size, "x");
		StrCopyN(ky, size, "y");
	}
}

void WindowPosSave(void *hwnd)
{
	WindowPosSaveAs(hwnd, NULL);
}

bool WindowPosRestore(void *hwnd)
{
	return WindowPosRestoreAs(hwnd, NULL);
}

void WindowPosSaveAs(void *hwnd, const char *name)
{
	WINDOWPLACEMENT wp;
	char kx[48];
	char ky[48];

	if (!hwnd || !IsWindow((HWND) hwnd))
		return;

	int32_t px;
	int32_t py;

	if ((!name || !name[0]) && command_line_pos(&px, &py))
		return;

	wp.length = sizeof(wp);

	if (!GetWindowPlacement((HWND) hwnd, &wp))
		return;

	MONITORINFO mi;
	HMONITOR mon = MonitorFromRect(&wp.rcNormalPosition, MONITOR_DEFAULTTONEAREST);
	int32_t x = wp.rcNormalPosition.left;
	int32_t y = wp.rcNormalPosition.top;

	mi.cbSize = sizeof(mi);

	if (mon && GetMonitorInfoA(mon, &mi)) {
		x += mi.rcWork.left - mi.rcMonitor.left;
		y += mi.rcWork.top - mi.rcMonitor.top;
	}

	keys(name, kx, ky, sizeof(kx));
	IniSetInt(SECTION, kx, x);
	IniSetInt(SECTION, ky, y);
}

bool WindowPosRestoreAs(void *hwnd, const char *name)
{
	RECT r;
	RECT title;
	char kx[48];
	char ky[48];

	if (!hwnd)
		return false;

	keys(name, kx, ky, sizeof(kx));
	int32_t x = IniGetInt(SECTION, kx, NOT_SET);
	int32_t y = IniGetInt(SECTION, ky, NOT_SET);

	if (!name || !name[0])
		command_line_pos(&x, &y);

	if (x == NOT_SET || y == NOT_SET)
		return false;

	if (!GetWindowRect((HWND) hwnd, &r))
		return false;

	title.left = x;
	title.top = y;
	title.right = x + (r.right - r.left);
	title.bottom = y + GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYFRAME);

	if (!MonitorFromRect(&title, MONITOR_DEFAULTTONULL))
		return false;

	if (!SetWindowPos((HWND) hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE))
		return false;

	GetWindowRect((HWND) hwnd, &r);

	if (r.left != x || r.top != y)
		SetWindowPos((HWND) hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

	return true;
}

static void visible_rect(HWND hwnd, RECT *out)
{
	RECT dwm;

	GetWindowRect(hwnd, out);

	if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &dwm, sizeof(dwm))))
		return;

	*out = dwm;
}

void WindowPlaceBeside(void *wnd, void *anchor, bool right, bool match_height)
{
	RECT a;
	RECT v;
	RECT r;
	MONITORINFO mi;

	if (!wnd || !anchor || !IsWindow((HWND) wnd) || !IsWindow((HWND) anchor))
		return;

	visible_rect((HWND) anchor, &a);
	visible_rect((HWND) wnd, &v);
	GetWindowRect((HWND) wnd, &r);

	int32_t w = v.right - v.left;
	int32_t h = match_height ? a.bottom - a.top : v.bottom - v.top;
	int32_t x = right ? a.right : a.left - w;
	int32_t y = a.top;

	mi.cbSize = sizeof(mi);

	if (GetMonitorInfoA(MonitorFromWindow((HWND) anchor, MONITOR_DEFAULTTONEAREST), &mi)) {
		if (x + w > mi.rcWork.right)
			x = mi.rcWork.right - w;

		if (x < mi.rcWork.left)
			x = mi.rcWork.left;
	}

	SetWindowPos((HWND) wnd, NULL, x - (v.left - r.left), y - (v.top - r.top),
		w + (v.left - r.left) + (r.right - v.right), h + (v.top - r.top) + (r.bottom - v.bottom),
		SWP_NOZORDER | SWP_NOACTIVATE);
}
