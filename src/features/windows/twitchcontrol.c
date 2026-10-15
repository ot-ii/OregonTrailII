#include "../twitchcontrol.h"

#ifdef OTII_TWITCH

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "../controlstate.h"
#include "../../base/date.h"
#include "../../dialogs/datelistbuilder.h"
#include "../../app/event.h"
#include "../../base/file.h"
#include "../../ui/framewin.h"
#include "../../game/game.h"
#include "../../game/gamecmd.h"
#include "../../screens/healthwin.h"
#include "../../base/ini.h"
#include "../../app/replay.h"
#include "../../base/rng.h"
#include "../../ui/scaler.h"
#include "../../base/str.h"
#include "../twitchirc.h"
#include "../../ui/window.h"
#include "../../ui/windowpos.h"

#ifdef OTII_TEST
#include "../../../tests/testframework.h"
#endif

#define TICK_MS 250
#define CLICK_GAP_MS 1000
#define WORD_LEN 32
#define MAX_WORDS 48
#define RECENT 8
#define ACTIONS 5
#define TEXT_SIZE 32768

static uintptr_t g_timer;
static bool g_busy;
static uint32_t g_next_click;
static struct control_state g_st;

static char g_recent[RECENT][640];
static int32_t g_recent_n;
static char g_actions[ACTIONS][256];
static int32_t g_actions_n;

static void *g_ng_hwnd;
static int32_t g_ng_step;
static int32_t g_ng_wait;

static HWND g_wnd;
static HWND g_edit;
static HFONT g_font;

static char g_target[64];

static void ini_setting(const char *key, const char *fallback, char *out, size_t size)
{
	IniGetString("twitch", key, "\x01", out, size);
	if (!strcmp(out, "\x01")) {
		StrCopyN(out, size, fallback);
		IniSetString("twitch", key, fallback);
	}
}

static void push(char (*list)[640], int32_t *n, int32_t max, const char *line)
{
	memmove(list[1], list[0], sizeof(list[0]) * (size_t) (max - 1));
	StrCopyN(list[0], sizeof(list[0]), line);
	if (*n < max)
		++*n;
}

static void add_action(const char *line)
{
	SYSTEMTIME t;

	GetLocalTime(&t);
	memmove(g_actions[1], g_actions[0], sizeof(g_actions[0]) * (ACTIONS - 1));
	StrPrintf(g_actions[0], "%02d:%02d:%02d  %s", t.wHour, t.wMinute, t.wSecond, line);
	if (g_actions_n < ACTIONS)
		++g_actions_n;
}

static void obs_write(const char *file, const char *text)
{
	char dir[MAX_PATH];
	char path[MAX_PATH];

	if (RpMode() == RP_REPLAY)
		return;

	ini_setting("obs_dir", "", dir, sizeof(dir));

	if (!dir[0]) {
		FileGetExeDir(dir, sizeof(dir));

		size_t len = strlen(dir);
		if (len && dir[len - 1] == '\\')
			dir[len - 1] = 0;
	}

	StrPrintf(path, "%s\\%s", dir, file);
	FileWriteAll(path, text, strlen(text));
}

static int32_t difficulty(void)
{
	char text[16];

	ini_setting("difficulty", "1", text, sizeof(text));
	int32_t n = atoi(text);
	return n < 1 ? 1 : n;
}

static void write_difficulty(int32_t n)
{
	char text[32];

	StrPrintf(text, "Difficulty: %d", n);
	obs_write("difficulty.txt", text);
}

static void update_target(void)
{
	static const int32_t CODE[4] = {1, 2, 4, 3};
	const char *name = NULL;
	char text[80];

	if (!g_game || !g_game->start_date || g_game->destination < DEST_WILLAMETTE ||
		g_game->destination > DEST_SALT_LAKE)
		return;

	for (int32_t i = 0; g_destination_options[i].name; ++i) {
		if (g_destination_options[i].code10 == CODE[g_game->destination]) {
			name = g_destination_options[i].name;
			if (g_destination_options[i].min_ctx <= g_game->start_date->year &&
				g_destination_options[i].max_ctx >= g_game->start_date->year)
				break;
		}
	}

	if (!name)
		return;

	if (!strncmp(name, "the ", 4))
		name += 4;

	StrPrintf(text, "Target: %s", name);
	if (strcmp(text, g_target)) {
		StrCopy(g_target, text);
		obs_write("target.txt", text);
	}
}

static int32_t words(const char *s, char (*w)[WORD_LEN], int32_t max)
{
	int32_t n = 0;

	for (;;) {
		int32_t k = 0;

		while (*s && !IsCharAlphaNumericA(*s))
			++s;

		if (!*s || n == max)
			return n;

		while (*s && IsCharAlphaNumericA(*s)) {
			if (k < WORD_LEN - 1)
				w[n][k++] = *s;
			++s;
		}

		w[n][k] = 0;
		CharLowerA(w[n]);
		++n;
	}
}

static bool allowed(const struct control_option *o)
{
	return o->command != CMD_LOAD_GAME && o->command != CMD_SAVE_GAME && o->command != CMD_SAVE_GAME_AS &&
		!(o->command == GCMD_CHOICE && o->code == NODE_CMD_SAVE_DIARY);
}

static bool set_up_screen(const struct control_state *st)
{
	return st->screen == CONTROL_DIALOG && (!strcmp(st->title, "Character Creation") || !strcmp(st->title, "Prologue"));
}

static bool word_eq(const char *typed, const char *label)
{
	size_t a = strlen(typed);
	size_t b = strlen(label);

	if (!strcmp(typed, label))
		return true;

	if (b > a && !strncmp(typed, label, a))
		return !strcmp(label + a, "s") || !strcmp(label + a, "es") || !strcmp(label + a, "en");

	if (a > b && !strncmp(typed, label, b))
		return !strcmp(typed + b, "s") || !strcmp(typed + b, "es") || !strcmp(typed + b, "en");

	return false;
}

static int32_t covers(char (*mw)[WORD_LEN], int32_t m, const char *label)
{
	char lw[MAX_WORDS][WORD_LEN];
	int32_t n = words(label, lw, MAX_WORDS);

	if (n < m)
		return 0;

	for (int32_t j = 0; j < m; ++j) {
		int32_t k;

		for (k = 0; k < n && !word_eq(mw[j], lw[k]); ++k)
			;

		if (k == n)
			return 0;
	}

	return n;
}

static bool fillable(const struct control_field *f)
{
	return !f->is_choice && f->label[0];
}

static const char *field_name(const struct control_field *f)
{
	return strncmp(f->label, "Quantity: ", 10) ? f->label : f->label + 10;
}

static int32_t match_field(const struct control_state *st, const char *text, char *value, size_t size)
{
	char mw[16][WORD_LEN];
	int32_t m = words(text, mw, 16);
	int32_t best = -1;
	int32_t best_len = 0;
	bool tie = false;
	int32_t n;

	if (m < 2 || strspn(mw[m - 1], "0123456789") != strlen(mw[m - 1]) || strlen(mw[m - 1]) > 5)
		return -1;

	StrCopyN(value, size, mw[m - 1]);

	for (int32_t i = 0; i < st->field_count; ++i) {
		if (!fillable(&st->fields[i]) || (n = covers(mw, m - 1, field_name(&st->fields[i]))) == 0)
			continue;

		if (best < 0 || n < best_len) {
			best = i;
			best_len = n;
			tie = false;

		} else if (n == best_len && strcmp(st->fields[best].label, st->fields[i].label)) {
			tie = true;
		}
	}

	return tie ? -1 : best;
}

static int32_t match_choice(const struct control_state *st, const char *text, bool need_label, int32_t *entry)
{
	static char names[64][64];
	char mw[16][WORD_LEN];
	int32_t m = words(text, mw, 16);
	int32_t best = -1;
	int32_t best_entry = -1;
	int32_t best_len = 0;
	bool tie = false;
	int32_t n;

	if (!m)
		return -1;

	for (int32_t i = 0; i < st->field_count; ++i) {
		if (!st->fields[i].is_choice)
			continue;

		int32_t count = ControlFieldChoices(st, i, names, 64);

		for (int32_t k = need_label ? 1 : 0; k < (need_label ? m : 1); ++k) {
			int32_t label_len = 0;

			if (need_label && (!st->fields[i].label[0] || (label_len = covers(mw, k, st->fields[i].label)) == 0))
				continue;

			for (int32_t e = 0; e < count; ++e) {
				if ((n = covers(mw + k, m - k, names[e])) == 0)
					continue;

				n += label_len;

				if (best < 0 || n < best_len) {
					best = i;
					best_entry = e;
					best_len = n;
					tie = false;

				} else if (n == best_len && (best != i || best_entry != e)) {
					tie = true;
				}
			}
		}
	}

	if (tie || best < 0)
		return -1;

	*entry = best_entry;
	return best;
}

struct pick {
	const char *label;
	int32_t option;
	HWND close;
	bool side;
};

static HWND health_hwnd(void)
{
	return g_health_window ? (HWND) ((struct window *) g_health_window)->hwnd : NULL;
}

static int32_t build_picks(const struct control_state *st, struct pick *p, int32_t max)
{
	int32_t i;
	int32_t n = 0;

	for (i = 0; i < st->side_option_first && n < max; ++i) {
		if (allowed(&st->options[i])) {
			struct pick q = {st->options[i].label, i, NULL, false};
			p[n++] = q;
		}
	}

	if (g_parent_win && st->hwnd && st->hwnd != g_parent_win->base.hwnd && n < max) {
		struct pick q = {"Close window", -1, (HWND) st->hwnd, false};
		p[n++] = q;
	}

	for (i = st->side_option_first; i < st->option_count && n < max; ++i) {
		if (allowed(&st->options[i])) {
			struct pick q = {st->options[i].label, i, NULL, true};
			p[n++] = q;
		}
	}

	if (health_hwnd() && IsWindowVisible(health_hwnd()) && n < max) {
		struct pick q = {"Close Health Status", -1, health_hwnd(), true};
		p[n++] = q;
	}

	return n;
}

static int32_t match(const struct pick *p, int32_t count, const char *text)
{
	char mw[16][WORD_LEN];
	int32_t m = words(text, mw, 16);
	int32_t best = -1;
	int32_t best_len = 0;
	bool tie = false;

	if (!m)
		return -1;

	if (m == 1 && strspn(mw[0], "0123456789") == strlen(mw[0])) {
		int32_t k = atoi(mw[0]);
		return k >= 1 && k <= count ? k - 1 : -1;
	}

	for (int32_t i = 0; i < count; ++i) {
		int32_t n = covers(mw, m, p[i].label);
		if (!n)
			continue;

		if (best < 0 || n < best_len) {
			best = i;
			best_len = n;
			tie = false;

		} else if (n == best_len && strcmp(p[best].label, p[i].label)) {
			tie = true;
		}
	}

	return tie ? -1 : best;
}

static const struct control_option *find_label(const struct control_state *st, const char *label)
{
	for (int32_t i = 0; i < st->side_option_first; ++i)
		if (!_strnicmp(st->options[i].label, label, strlen(label)))
			return &st->options[i];

	return NULL;
}

static void set_up(const struct control_state *st)
{
	const struct control_option *o;

	if (st->hwnd != g_ng_hwnd) {
		g_ng_hwnd = st->hwnd;
		g_ng_step = 0;
		g_ng_wait = 0;
	}

	if (g_ng_wait > 0) {
		--g_ng_wait;
		return;
	}

	if (!strcmp(st->title, "Prologue")) {
		if ((o = find_label(st, "Continue")) != NULL) {
			ControlClick(st, o->index);
			add_action("(prologue skipped)");
		}

		g_ng_wait = 12;
		return;
	}

	switch (g_ng_step) {
		case 0:
			if ((o = find_label(st, "Random")) != NULL)
				ControlClick(st, o->index);

			g_ng_step = 1;
			return;

		case 1:
			for (int32_t i = 0; i < st->field_count; ++i)
				if (!strcmp(st->fields[i].label, "Leader's name") && !st->fields[i].text[0])
					ControlSetField(st, i, "Chat");

			g_ng_step = 2;
			return;

		default:
			if ((o = find_label(st, "Travel the Trail")) != NULL) {
				ControlClick(st, o->index);
				add_action("(new game set up: Random, Travel the Trail!)");
			}

			g_ng_step = 1;
			g_ng_wait = 12;
			return;
	}
}

static bool set_choice(int32_t field, int32_t entry, const char *who, const char *text)
{
	static char names[64][64];
	char pick[16];
	char line[640];
	int32_t count = ControlFieldChoices(&g_st, field, names, 64);

	StrPrintf(pick, "#%d", entry);
	if (entry >= count || !ControlSetField(&g_st, field, pick))
		return false;

	StrPrintf(line, "%s: \"%s\" -> %s = %s", who, text, g_st.fields[field].label[0] ? g_st.fields[field].label :
		"list", names[entry]);
	add_action(line);
	return true;
}

static void CALLBACK tick(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
	(void) hwnd;
	(void) msg;
	(void) id;
	(void) time;

	struct twitch_message m;
	bool have_state = false;
	bool picked = false;

	if (g_busy)
		return;

	g_busy = true;

	while (RpQueryBytes(RPQ_CHAT, &m, sizeof(m), TwitchPoll(&m))) {
		char line[640];
		const char *who = m.display_name[0] ? m.display_name : m.login;

		if (strcmp(m.command, "PRIVMSG") && strcmp(m.command, "USERNOTICE"))
			continue;

		StrPrintf(line, "%s%s%s: %s", who, m.reward_id[0] ? " [channel points]" : "", m.bits ? " [bits]" : "",
			m.text);
		push(g_recent, &g_recent_n, RECENT, line);

		if (picked || strcmp(m.command, "PRIVMSG") || m.reward_id[0] || (int32_t) (RpGetTickCount() - g_next_click) < 0)
			continue;

		if (!have_state) {
			if (!ControlGetState(&g_st))
				continue;

			have_state = true;
		}

		if (set_up_screen(&g_st))
			continue;

		{
			char value[8];
			int32_t f = match_field(&g_st, m.text, value, sizeof(value));

			if (f >= 0) {
				ControlSetField(&g_st, f, value);
				StrPrintf(line, "%s: \"%s\" -> %s = %s", who, m.text, field_name(&g_st.fields[f]), value);
				add_action(line);
				have_state = false;
				continue;
			}
		}

		{
			int32_t entry;
			int32_t f = match_choice(&g_st, m.text, true, &entry);

			if (f >= 0 && set_choice(f, entry, who, m.text)) {
				have_state = false;
				continue;
			}
		}

		{
			struct pick picks[72];
			int32_t count = build_picks(&g_st, picks, 72);
			int32_t entry;
			int32_t f;
			int32_t i;

			if ((i = match(picks, count, m.text)) < 0) {
				if ((f = match_choice(&g_st, m.text, false, &entry)) >= 0 && set_choice(f, entry, who, m.text))
					have_state = false;

				continue;
			}

			StrPrintf(line, "%s: \"%s\" -> %s", who, m.text, picks[i].label);
			add_action(line);

			if (picks[i].option >= 0)
				ControlClick(&g_st, picks[i].option);
			else
				PostMessageA(picks[i].close, WM_CLOSE, 0, 0);
		}

		g_next_click = RpGetTickCount() + CLICK_GAP_MS;
		picked = true;
	}

	if (!picked && ControlGetState(&g_st) && set_up_screen(&g_st))
		set_up(&g_st);

	update_target();
	g_busy = false;
}

struct text {
	char buf[TEXT_SIZE];
	size_t len;
};

static void put(struct text *s, const char *str)
{
	size_t n = strlen(str);

	if (s->len + n >= sizeof(s->buf))
		n = sizeof(s->buf) - 1 - s->len;

	memcpy(s->buf + s->len, str, n);
	s->len += n;
	s->buf[s->len] = 0;
}

static void add(struct text *s, const char *fmt, ...)
{
	char buf[1200];
	va_list a;

	va_start(a, fmt);
	vsnprintf(buf, sizeof(buf), fmt, a);
	va_end(a);
	buf[sizeof(buf) - 1] = 0;
	put(s, buf);
}

static void build_text(struct text *s)
{
	static struct control_state st;
	static char names[64][64];
	int32_t i;
	int32_t n;
	int32_t k;

	s->len = 0;
	s->buf[0] = 0;
	put(s, "Type an option's words (whole words, any part of it) or its number.\r\n\r\n");

	if (!ControlGetState(&st)) {
		put(s, "(no game window)\r\n");

	} else if (set_up_screen(&st)) {
		add(s, "%s: setting up the new game -- chat waits.\r\n", st.title);

	} else {
		add(s, "%s\r\n", st.title[0] ? st.title : "(untitled)");

		for (i = 0, n = 0; i < st.field_count; ++i) {
			if (fillable(&st.fields[i])) {
				if (!n++)
					put(s, "Amounts -- type the name and a number (\"mules 6\"):\r\n");

				add(s, "  %-28s %s\r\n", field_name(&st.fields[i]), st.fields[i].text[0] ? st.fields[i].text : "0");
			}
		}

		for (i = 0, k = 0; i < st.field_count; ++i) {
			if (st.fields[i].is_choice) {
				int32_t count = ControlFieldChoices(&st, i, names, 64);

				if (!k++)
					put(s, "Lists -- type the list's name and an entry (\"rations filling\"):\r\n");

				++n;
				add(s, "  %s: %s  [", st.fields[i].label[0] ? st.fields[i].label : "(list)", st.fields[i].text);

				for (int32_t e = 0; e < count; ++e)
					add(s, "%s%s", e ? " / " : "", names[e]);

				put(s, "]\r\n");
			}
		}

		if (n)
			put(s, "Options:\r\n");

		struct pick picks[72];
		int32_t count = build_picks(&st, picks, 72);

		for (i = 0; i < count; ++i) {
			if (picks[i].side && (!i || !picks[i - 1].side))
				put(s, "\r\nHealth Status window:\r\n");

			add(s, "  %2d  %s\r\n", i + 1, picks[i].label);
		}

		if (!count)
			put(s, "  (nothing to pick right now)\r\n");
	}
}

static void refresh(void)
{
	static struct text text;
	static char old[TEXT_SIZE];

	build_text(&text);
	GetWindowTextA(g_edit, old, (int) sizeof(old));

	if (strcmp(old, text.buf)) {
		int32_t first = (int32_t) SendMessageA(g_edit, EM_GETFIRSTVISIBLELINE, 0, 0);

		SendMessageA(g_edit, WM_SETREDRAW, FALSE, 0);
		SetWindowTextA(g_edit, text.buf);
		SendMessageA(g_edit, EM_LINESCROLL, 0, first);
		SendMessageA(g_edit, WM_SETREDRAW, TRUE, 0);
		InvalidateRect(g_edit, NULL, TRUE);
	}
}

static void save_window_rect(HWND hwnd)
{
	static const char *const keys[4] = {"options_x", "options_y", "options_width", "options_height"};
	WINDOWPLACEMENT wp;
	int32_t values[4];

	if (RpMode() == RP_REPLAY)
		return;

	wp.length = sizeof(wp);
	if (!GetWindowPlacement(hwnd, &wp))
		return;

	values[0] = wp.rcNormalPosition.left;
	values[1] = wp.rcNormalPosition.top;
	values[2] = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
	values[3] = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;

	for (int32_t i = 0; i < 4; ++i)
		IniSetInt("twitch", keys[i], values[i]);
}

static bool restore_window_rect(HWND hwnd)
{
	RECT r;
	int32_t x = IniGetInt("twitch", "options_x", -32768);
	int32_t y = IniGetInt("twitch", "options_y", -32768);
	int32_t w = IniGetInt("twitch", "options_width", 0);
	int32_t h = IniGetInt("twitch", "options_height", 0);

	if (x == -32768 || y == -32768 || w < 100 || h < 60)
		return false;

	SetRect(&r, x, y, x + w, y + h);
	if (!MonitorFromRect(&r, MONITOR_DEFAULTTONULL))
		return false;

	SetWindowPos(hwnd, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
	return true;
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg) {
		case WM_CREATE:
			g_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
				WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0, 0, 100, 100, hwnd,
				NULL, GetModuleHandleA(NULL), NULL);
			g_font = CreateFontA(-ScalerDpiPx(hwnd, 12), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0,
				FIXED_PITCH | FF_MODERN, "Consolas");
			SendMessageA(g_edit, WM_SETFONT, (WPARAM) g_font, TRUE);
			SetTimer(hwnd, 1, 1000, NULL);
			refresh();
			return 0;

		case WM_SIZE:
			MoveWindow(g_edit, 0, 0, LOWORD(lparam), HIWORD(lparam), TRUE);
			return 0;

		case WM_TIMER:
			refresh();
			return 0;

		case WM_EXITSIZEMOVE:
			save_window_rect(hwnd);
			return 0;

		case WM_CLOSE:
			DestroyWindow(hwnd);
			return 0;

		case WM_DESTROY:
			save_window_rect(hwnd);
			KillTimer(hwnd, 1);
			if (g_font)
				DeleteObject(g_font);

			g_font = NULL;
			g_wnd = NULL;
			g_edit = NULL;
			return 0;
	}

	return DefWindowProcA(hwnd, msg, wparam, lparam);
}

void TwitchControlToggleWindow(void)
{
	static bool registered;
	HWND main = g_parent_win ? (HWND) g_parent_win->base.hwnd : NULL;

	if (g_wnd) {
		DestroyWindow(g_wnd);
		return;
	}

	if (!registered) {
		WNDCLASSA wc;

		ZeroMemory(&wc, sizeof(wc));
		wc.lpfnWndProc = wnd_proc;
		wc.hInstance = GetModuleHandleA(NULL);
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
		wc.lpszClassName = "OTIITwitchOptions";
		RegisterClassA(&wc);
		registered = true;
	}

	g_wnd = CreateWindowExA(0, "OTIITwitchOptions", "Twitch Chat Options", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
		CW_USEDEFAULT, ScalerDpiPx(main, 465), ScalerDpiPx(main, 560), NULL, NULL, GetModuleHandleA(NULL), NULL);

	if (g_wnd) {
		if (!restore_window_rect(g_wnd) && g_parent_win)
			WindowPlaceBeside(g_wnd, main, false, true);

		ShowWindow(g_wnd, SW_SHOWNOACTIVATE);
		UpdateWindow(g_wnd);
	}
}

void TwitchControlStart(void)
{
	char channel[64];
	char reward[64];

	ini_setting("channel", "carlsagan42", channel, sizeof(channel));
	ini_setting("party_reward_id", "3a84c19e-96af-4ee7-9248-b9c5adebf942", reward, sizeof(reward));
#ifdef OTII_TEST
	{
		char server[256];

		IniGetString("twitch", "server", "", server, sizeof(server));
		TwitchSetServer(server);
		if (strstr(TestCommandLine(), "/test="))
			return;
	}
#endif
	TwitchStart(channel, reward);
	write_difficulty(difficulty());

	if (!g_timer)
		g_timer = RpSetTimer(NULL, 0, TICK_MS, (void *) tick);

	if (!g_wnd)
		TwitchControlToggleWindow();
}

#ifdef OTII_TEST
bool TwitchControlTestConnect(const char *channel, const char *party_reward_id, const char *server)
{
	if (!TwitchSetServer(server) || !TwitchStart(channel, party_reward_id))
		return false;

	if (!g_timer)
		g_timer = RpSetTimer(NULL, 0, TICK_MS, (void *) tick);

	if (!g_wnd)
		TwitchControlToggleWindow();

	return true;
}

void TwitchControlTestDisconnect(void)
{
	TwitchControlStop();
	TwitchStop();
	TwitchSetServer("");
}

const char *TwitchControlTestOptionsText(void)
{
	static struct text text;

	build_text(&text);
	return text.buf;
}
#endif

void TwitchControlStop(void)
{
	if (g_timer) {
		KillTimer(NULL, g_timer);
		g_timer = 0;
	}

	if (g_wnd)
		DestroyWindow(g_wnd);
}

int32_t TwitchControlLastPicks(char (*lines)[256], int32_t max)
{
	int32_t i;

	for (i = 0; i < g_actions_n && i < max; ++i)
		StrCopyN(lines[i], sizeof(lines[i]), g_actions[i]);

	return i;
}

int32_t TwitchControlNewGameDifficulty(void)
{
	int32_t n = difficulty();

	write_difficulty(n);
	return n > 10 ? 10 : n;
}

void TwitchControlScoreReached(void)
{
	char text[16];
	char seed[16];

	StrPrintf(seed, "%u", RngSeed());
	ini_setting("last_scored_seed", "", text, sizeof(text));
	if (!strcmp(text, seed))
		return;

	int32_t n = difficulty() + 1;
	StrPrintf(text, "%d", n);

	if (RpMode() != RP_REPLAY) {
		IniSetString("twitch", "difficulty", text);
		IniSetString("twitch", "last_scored_seed", seed);
	}

	write_difficulty(n);
	StrPrintf(text, "difficulty %d", n);
	add_action(text);
}

int32_t TwitchControlRecentChat(char (*lines)[640], int32_t max)
{
	int32_t i;

	for (i = 0; i < g_recent_n && i < max; ++i)
		StrCopyN(lines[i], sizeof(lines[i]), g_recent[i]);

	return i;
}

#endif
