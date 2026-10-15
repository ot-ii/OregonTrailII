#include "../debugscreen.h"

#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "../../base/memory.h"
#include "../../ui/scaler.h"

static HWND g_dbg_wnd = 0;
static HWND g_dbg_edit = 0;
static HFONT g_dbg_font = 0;

static void refresh(void)
{
	const char *text = DebugScreenText();
	int len = GetWindowTextLengthA(g_dbg_edit);
	char *old = MemAlloc((size_t) len + 1, 1);

	GetWindowTextA(g_dbg_edit, old, len + 1);
	old[len] = 0;
	if (strcmp(old, text ? text : "")) {
		int first = (int) SendMessageA(g_dbg_edit, EM_GETFIRSTVISIBLELINE, 0, 0);
		SendMessageA(g_dbg_edit, WM_SETREDRAW, FALSE, 0);
		SetWindowTextA(g_dbg_edit, text);
		SendMessageA(g_dbg_edit, EM_LINESCROLL, 0, first);
		SendMessageA(g_dbg_edit, WM_SETREDRAW, TRUE, 0);
		InvalidateRect(g_dbg_edit, 0, TRUE);
	}
	MemFree(old);
}

static LRESULT CALLBACK dbg_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
		case WM_CREATE:
			g_dbg_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
				WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL |
				ES_AUTOHSCROLL, 0, 0, 100, 100, hwnd, 0, GetModuleHandleA(0), 0);
			g_dbg_font = CreateFontA(-ScalerDpiPx(hwnd, 13), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0,
				FIXED_PITCH | FF_MODERN, "Consolas");
			SendMessageA(g_dbg_edit, WM_SETFONT, (WPARAM) g_dbg_font, TRUE);
			SetTimer(hwnd, 1, 1000, 0);
			refresh();
			return 0;
		case WM_SIZE:
			MoveWindow(g_dbg_edit, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
			return 0;
		case WM_TIMER:
			refresh();
			return 0;
		case WM_CLOSE:
			DestroyWindow(hwnd);
			return 0;
		case WM_DESTROY:
			KillTimer(hwnd, 1);
			if (g_dbg_font)
				DeleteObject(g_dbg_font);
			g_dbg_font = 0;
			g_dbg_wnd = 0;
			g_dbg_edit = 0;
			return 0;
	}
	return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void DebugScreenToggle(void)
{
	static int registered = 0;

	if (g_dbg_wnd) {
		DestroyWindow(g_dbg_wnd);
		return;
	}
	if (!registered) {
		WNDCLASSA wc;
		ZeroMemory(&wc, sizeof(wc));
		wc.lpfnWndProc = dbg_wndproc;
		wc.hInstance = GetModuleHandleA(0);
		wc.hCursor = LoadCursor(0, IDC_ARROW);
		wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
		wc.lpszClassName = "OTIIDebugState";
		RegisterClassA(&wc);
		registered = 1;
	}
	g_dbg_wnd = CreateWindowExA(0, "OTIIDebugState", "Oregon Trail II - Debug State", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1000, 760, 0, 0, GetModuleHandleA(0), 0);
	if (g_dbg_wnd) {
		ShowWindow(g_dbg_wnd, SW_SHOWNOACTIVATE);
		UpdateWindow(g_dbg_wnd);
	}
}
