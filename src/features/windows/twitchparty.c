#include "../twitchparty.h"

#ifdef TWITCH_PARTY

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <windows.h>

#include "../../ui/framewin.h"
#include "../../base/ini.h"
#include "../../ui/menubar.h"
#include "../../dialogs/modaldialog.h"
#include "../../dialogs/newgamedlg.h"
#include "../../app/replay.h"
#include "../../ui/scaler.h"
#include "../../base/str.h"
#include "../twitchirc.h"
#include "../../ui/windowpos.h"

#define SECTION "twitch"
#define MAX_PICKS 64
#define KEEP_SELECTED 64

static HWND g_wnd;
static HWND g_list;
static HWND g_pick;
static HWND g_remove;
static HWND g_status;
static uintptr_t g_timer;
static bool g_started;
static char g_channel[64];
static struct twitch_message g_picks[MAX_PICKS];
static int32_t g_pick_count;
static char g_note[160];
static DWORD g_note_until;
static uint32_t g_shown;
static int32_t g_busy;

static void ini_setting(const char *key, char *out, size_t size)
{
	IniGetString(SECTION, key, "", out, size);
}

static void entry_text(const struct twitch_message *m, bool picked, wchar_t *out, int32_t size)
{
	wchar_t name[64];
	wchar_t text[256];
	const char *t = m->text;
	char trimmed[512];

	if (!m->display_name[0] || !MultiByteToWideChar(CP_UTF8, 0, m->display_name, -1, name, 64))
		MultiByteToWideChar(CP_UTF8, 0, m->login, -1, name, 64);

	while (*t == ' ')
		++t;

	const char *end = t + strlen(t);
	while (end > t && end[-1] == ' ')
		--end;

	int32_t n = (int32_t) (end - t) < (int32_t) sizeof(trimmed) - 1 ? (int32_t) (end - t) :
		(int32_t) sizeof(trimmed) - 1;
	memcpy(trimmed, t, (size_t) n);
	trimmed[n] = 0;

	if (!MultiByteToWideChar(CP_UTF8, 0, trimmed, -1, text, 256))
		text[0] = 0;

	_snwprintf_s(out, (size_t) size, _TRUNCATE, L"%s%s [%d] %s", picked ? L"\x2713 " : L"", name,
		TwitchPartyPlayCount(m->login), text);
}

static uint32_t signature(void)
{
	struct twitch_message m;
	uint32_t h = 2166136261u;
	int32_t i;

	for (i = 0; TwitchPartyPeek(i, &m); ++i) {
		for (const char *c = m.id; *c; ++c)
			h = (h ^ (uint8_t) *c) * 16777619u;

		h = (h ^ (uint32_t) ((TwitchPartyPicked(i) ? 1 : 0) + 2 + TwitchPartyPlayCount(m.login) * 7)) * 16777619u;
	}

	return h ^ (uint32_t) i;
}

static void refresh(bool force)
{
	struct twitch_message m;
	char status[256];
	char line[400];
	int32_t i;
	int32_t waiting = 0;
	int32_t picked = 0;

	if (!g_wnd)
		return;

	uint32_t sig = signature();

	if (force || sig != g_shown) {
		static char selected[KEEP_SELECTED][64];
		int32_t n_sel = 0;
		int32_t count = (int32_t) SendMessageW(g_list, LB_GETCOUNT, 0, 0);
		int32_t top = (int32_t) SendMessageW(g_list, LB_GETTOPINDEX, 0, 0);

		for (i = 0; i < count && n_sel < KEEP_SELECTED; ++i) {
			if (SendMessageW(g_list, LB_GETSEL, (WPARAM) i, 0) > 0) {
				int32_t idx = (int32_t) SendMessageW(g_list, LB_GETITEMDATA, (WPARAM) i, 0);
				if (TwitchPartyPeek(idx, &m))
					StrCopy(selected[n_sel++], m.id);
			}
		}

		SendMessageW(g_list, WM_SETREDRAW, FALSE, 0);
		SendMessageW(g_list, LB_RESETCONTENT, 0, 0);

		for (i = 0; TwitchPartyPeek(i, &m); ++i) {
			wchar_t text[400];

			entry_text(&m, TwitchPartyPicked(i), text, 400);
			int32_t at = (int32_t) SendMessageW(g_list, LB_ADDSTRING, 0, (LPARAM) text);
			SendMessageW(g_list, LB_SETITEMDATA, (WPARAM) at, (LPARAM) i);

			for (int32_t k = 0; k < n_sel; ++k)
				if (!strcmp(selected[k], m.id))
					SendMessageW(g_list, LB_SETSEL, TRUE, (LPARAM) at);
		}

		SendMessageW(g_list, LB_SETTOPINDEX, (WPARAM) top, 0);
		SendMessageW(g_list, WM_SETREDRAW, TRUE, 0);
		InvalidateRect(g_list, NULL, TRUE);
		g_shown = sig;
	}

	for (i = 0; TwitchPartyPeek(i, &m); ++i) {
		if (TwitchPartyPicked(i))
			++picked;
		else
			++waiting;
	}

	TwitchStatus(status, sizeof(status));
	bool note = g_note[0] && (int32_t) (GetTickCount() - g_note_until) < 0;
	StrPrintf(line, "%s\r\n%d waiting, %d picked%s%s", status, waiting, picked, note ? " -- " : "",
		note ? g_note : "");
	SetWindowTextA(g_status, line);
	EnableWindow(g_pick, NewGameDlgRunning() != NULL);
	EnableWindow(g_remove, picked > 0);
}

static void pick_selected(void)
{
	int32_t count = (int32_t) SendMessageW(g_list, LB_GETCOUNT, 0, 0);

	for (int32_t i = 0; i < count && g_pick_count < MAX_PICKS; ++i) {
		if (SendMessageW(g_list, LB_GETSEL, (WPARAM) i, 0) > 0) {
			int32_t idx = (int32_t) SendMessageW(g_list, LB_GETITEMDATA, (WPARAM) i, 0);
			struct twitch_message m;

			if (TwitchPartyPeek(idx, &m) && !TwitchPartyPicked(idx))
				g_picks[g_pick_count++] = m;
		}
	}

	SendMessageW(g_list, LB_SETSEL, FALSE, -1);
}

static void note(const char *text)
{
	StrCopy(g_note, text);
	g_note_until = GetTickCount() + 5000;
}

static void CALLBACK tick(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
	(void) hwnd;
	(void) msg;
	(void) id;
	(void) time;

	struct twitch_message m;

	if (g_busy)
		return;

	++g_busy;
	memset(&m, 0, sizeof(m));
	bool have = g_pick_count > 0;
	if (have)
		m = g_picks[0];

	if (RpQueryBytes(RPQ_PARTY_ENTRY, &m, sizeof(m), have)) {
		int32_t slot = NewGamePartyPick(NewGameDlgRunning(), &m);

		if (have) {
			memmove(g_picks, g_picks + 1, sizeof(g_picks[0]) * (size_t) (g_pick_count - 1));
			--g_pick_count;
		}

		if (slot >= 0) {
			TwitchPartyMarkPicked(m.id);

		} else if (NewGameDlgRunning()) {
			note("the party is full (5 companions)");
			g_pick_count = 0;

		} else {
			note("open the New Game screen to pick");
			g_pick_count = 0;
		}

		refresh(true);

	} else if (g_wnd) {
		refresh(false);
	}

	--g_busy;
}

static void layout(void)
{
	RECT r;
	int32_t pad = ScalerDpiPx(g_wnd, 8);
	int32_t bh = ScalerDpiPx(g_wnd, 28);
	int32_t bw = ScalerDpiPx(g_wnd, 120);
	int32_t sh = ScalerDpiPx(g_wnd, 36);

	GetClientRect(g_wnd, &r);
	int32_t w = r.right - 2 * pad;
	int32_t h = r.bottom - 4 * pad - bh - sh;
	MoveWindow(g_status, pad, pad, w, sh, TRUE);
	MoveWindow(g_list, pad, 2 * pad + sh, w, h > 20 ? h : 20, TRUE);
	MoveWindow(g_pick, pad, r.bottom - pad - bh, bw, bh, TRUE);
	MoveWindow(g_remove, 2 * pad + bw, r.bottom - pad - bh, bw, bh, TRUE);
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
		case WM_CREATE: {
			LOGFONTA lf;

			memset(&lf, 0, sizeof(lf));
			lf.lfHeight = -ScalerDpiPx(hwnd, 14);
			StrCopy(lf.lfFaceName, "Segoe UI");
			HFONT font = CreateFontIndirectA(&lf);
			g_status = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hwnd,
				(HMENU) (INT_PTR) TWITCH_PARTY_STATUS, GetModuleHandleA(NULL), NULL);
			g_list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL |
				LBS_EXTENDEDSEL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 0, 0, 10, 10, hwnd,
				(HMENU) (INT_PTR) TWITCH_PARTY_LIST, GetModuleHandleA(NULL), NULL);
			g_pick = CreateWindowExA(0, "BUTTON", "Pick", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 10, 10, hwnd,
				(HMENU) (INT_PTR) TWITCH_PARTY_PICK, GetModuleHandleA(NULL), NULL);
			g_remove = CreateWindowExA(0, "BUTTON", "Remove played", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 10,
				10, hwnd, (HMENU) (INT_PTR) TWITCH_PARTY_REMOVE, GetModuleHandleA(NULL), NULL);
			SendMessageA(g_status, WM_SETFONT, (WPARAM) font, 0);
			SendMessageA(g_list, WM_SETFONT, (WPARAM) font, 0);
			SendMessageA(g_pick, WM_SETFONT, (WPARAM) font, 0);
			SendMessageA(g_remove, WM_SETFONT, (WPARAM) font, 0);
			SetPropA(hwnd, "font", font);
			return 0;
		}

		case WM_SIZE:
			layout();
			return 0;

		case WM_COMMAND:
			if (LOWORD(wp) == TWITCH_PARTY_PICK) {
				pick_selected();
				return 0;
			}

			if (LOWORD(wp) == TWITCH_PARTY_REMOVE) {
				TwitchPartyRemovePicked();
				refresh(true);
				return 0;
			}

			if (LOWORD(wp) == TWITCH_PARTY_LIST && HIWORD(wp) == LBN_DBLCLK) {
				pick_selected();
				return 0;
			}

			break;

		case WM_EXITSIZEMOVE:
			if (RpMode() != RP_REPLAY) {
				RECT r;

				GetWindowRect(hwnd, &r);
				IniSetInt(SECTION, "window_x", r.left);
				IniSetInt(SECTION, "window_y", r.top);
			}

			return 0;

		case WM_CLOSE:
			ShowWindow(hwnd, SW_HIDE);
			return 0;

		case WM_DESTROY: {
			HFONT font = (HFONT) RemovePropA(hwnd, "font");
			if (font)
				DeleteObject(font);

			g_wnd = NULL;
			g_list = NULL;
			g_pick = NULL;
			g_remove = NULL;
			g_status = NULL;
			return 0;
		}
	}

	return DefWindowProcW(hwnd, msg, wp, lp);
}

static void open_window(void)
{
	static bool registered;
	HWND main = g_parent_win ? (HWND) g_parent_win->base.hwnd : NULL;
	char title[128];

	if (g_wnd)
		return;

	if (!registered) {
		WNDCLASSW wc;

		memset(&wc, 0, sizeof(wc));
		wc.lpfnWndProc = wnd_proc;
		wc.hInstance = GetModuleHandleA(NULL);
		wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1);
		wc.lpszClassName = L"OTIITwitchRedemptions";
		RegisterClassW(&wc);
		registered = true;
	}

	g_wnd = CreateWindowExW(0, L"OTIITwitchRedemptions", L"Twitch Redemptions", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
		CW_USEDEFAULT, ScalerDpiPx(main, 360), ScalerDpiPx(main, 480), NULL, NULL, GetModuleHandleA(NULL), NULL);
	if (!g_wnd)
		return;

	StrPrintf(title, "Twitch Redemptions -- %s", g_channel);
	SetWindowTextA(g_wnd, title);

	int32_t x = IniGetInt(SECTION, "window_x", -32768);
	int32_t y = IniGetInt(SECTION, "window_y", -32768);
	RECT r;

	GetWindowRect(g_wnd, &r);
	OffsetRect(&r, x - r.left, y - r.top);

	if (x != -32768 && y != -32768 && MonitorFromRect(&r, MONITOR_DEFAULTTONULL)) {
		SetWindowPos(g_wnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

	} else if (main) {
		WindowPlaceBeside(g_wnd, main, false, true);
		GetWindowRect(g_wnd, &r);
		if (!MonitorFromRect(&r, MONITOR_DEFAULTTONULL) || r.left < GetSystemMetrics(SM_XVIRTUALSCREEN))
			WindowPlaceBeside(g_wnd, main, true, true);
	}

	layout();
	refresh(true);
	ShowWindow(g_wnd, SW_SHOWNOACTIVATE);
}

static bool begin(const char *channel, const char *reward_id, const char *server)
{
	if (!TwitchSetServer(server))
		return false;

	StrCopy(g_channel, channel);

	g_started = RpQuery(RPQ_PARTY_STARTED, TwitchStart(channel, reward_id) ? 1 : 0) != 0;
	if (!g_started)
		return false;

	if (!g_timer)
		g_timer = RpSetTimer(NULL, 0, 250, (void *) tick);

	open_window();
	return true;
}

bool TwitchPartySettings(const char *channel, const char *reward_id, const char *server)
{
	(void) server;
	return channel && channel[0] && reward_id && reward_id[0];
}

void TwitchPartyStart(void)
{
	char channel[64];
	char reward[64];
	char server[256];

	ini_setting("channel", channel, sizeof(channel));
	ini_setting("party_reward_id", reward, sizeof(reward));
	ini_setting("server", server, sizeof(server));
	if (!TwitchPartySettings(channel, reward, server))
		return;

#ifndef OTII_TEST
	if (RpMode() == RP_REPLAY) {
		if (!TwitchSetServer(server))
			return;

		StrCopy(g_channel, channel);
		if (!RpQuery(RPQ_PARTY_STARTED, 0))
			return;

		g_timer = RpSetTimer(NULL, 0, 250, (void *) tick);
		open_window();
		return;
	}

	begin(channel, reward, server);
#endif
}

void TwitchPartyStop(void)
{
	if (g_timer)
		KillTimer(NULL, g_timer);

	g_timer = 0;

	if (g_wnd)
		DestroyWindow(g_wnd);

	if (g_started)
		TwitchStop();

	g_started = false;
	g_pick_count = 0;
}

bool TwitchPartyConnect(const char *channel, const char *reward_id, const char *server)
{
	if (!TwitchPartySettings(channel, reward_id, server))
		return false;

	return begin(channel, reward_id, server);
}

void TwitchPartyDisconnect(void)
{
	TwitchPartyStop();
	TwitchSetServer("");
}

void *TwitchPartyWindow(void)
{
	return g_wnd;
}

void TwitchPartyShowWindow(void)
{
	if (!g_started)
		return;

	if (!g_wnd)
		open_window();

	if (g_wnd) {
		ShowWindow(g_wnd, IsIconic(g_wnd) ? SW_RESTORE : SW_SHOW);
		SetWindowPos(g_wnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		refresh(true);
	}
}

void TwitchPartyMenuUpdate(void *popup)
{
	HMENU menu = (HMENU) popup;

	if (!menu || GetMenuState(menu, DEBUG_SCREEN_MENU_ID, MF_BYCOMMAND) == (UINT) -1)
		return;

	bool present = GetMenuState(menu, TWITCH_PARTY_MENU_ID, MF_BYCOMMAND) != (UINT) -1;

	if (g_started && !present)
		InsertMenuA(menu, DEBUG_SCREEN_MENU_ID, MF_BYCOMMAND | MF_STRING, TWITCH_PARTY_MENU_ID, "Twitch Redemptions");
	else if (!g_started && present)
		DeleteMenu(menu, TWITCH_PARTY_MENU_ID, MF_BYCOMMAND);
}

#endif
