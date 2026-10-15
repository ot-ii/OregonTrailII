#include "../replay.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <windows.h>

#include "../../base/geom.h"
#include "../../base/ini.h"
#include "../../base/memory.h"
#include "../../base/rng.h"
#include "../../base/str.h"

// The recording is a stream of records, each tagged with what it answers:
//   'T' value pos n      from clock read `n` within queue check `pos` on, the clock reads `value`
//   'p' count            the next `count` message-queue checks find nothing
//   'P' ...              a message-queue check found this message
//   'Q' kind value count the next `count` queries of `kind` return `value`
//   'B' kind have len .. a query of `kind` returned this block of bytes (or nothing)
//   'S' pos ...          Windows delivered this message from one of its own loops, before queue check `pos`
//   'K' pos timer        a timer fired inside one of Windows' own loops, before queue check `pos`
//   'C' pos ...          a checkpoint of the game's state at queue check `pos`
//   'Z' scale            the window scale the recording started at
// Each kind is read back in the order the game asks for it, so how the records of different kinds interleave in the
// file does not matter -- only their order within a kind.
//
// "External" moments are the only ones where Windows, not the game, decides what happens next: inside the real
// queue check (messages other threads and the system send, e.g. switching apps) and while Windows runs one of its
// own loops for a non-client click (menus, moving the window, the close box).  Messages that reach a game window in
// those moments are recorded ('S'), and a replay ignores the real ones and delivers the recorded ones instead.
// Everything else the game does to itself -- creating windows, focusing, the notifications its own actions cause --
// simply happens again.
//
// The records are packed structs, little-endian.

#pragma pack(push, 1)
struct p_rec {
	uint16_t hwnd, flags;
	uint32_t msg, w, l;
	int32_t x, y;
	uint32_t time;
};

struct s_rec {
	uint32_t pos;
	uint16_t hwnd, flags;
	uint32_t msg, w, l;
};

struct c_rec {
	uint32_t pos, hash;
	int32_t day, cash;
	uint32_t rng;
};
#pragma pack(pop)

#define F_TIMER     1
#define F_LWND      2
#define NO_WINDOW   0xFFFF
#define CHECK_EVERY 4000000

static const uint32_t VERSION = 2;

static int32_t g_mode = -1;
static DWORD g_thread;
static bool g_live;
static int32_t g_clock_offset;
static int32_t g_external;
static int32_t g_injecting;
static uint32_t g_peeks;
static uint32_t g_until = 0xFFFFFFFF;
static bool g_fast;
static char g_first_arg[MAX_PATH];
static char g_log_path[MAX_PATH];

static HWND *g_windows;
static size_t g_windows_n, g_windows_cap;
static HHOOK g_cbt;

struct timer {
	UINT_PTR real;
	TIMERPROC proc;
};

static struct timer *g_timers;
static size_t g_timers_n, g_timers_cap;

static FILE *g_out;
static uint8_t *g_buf;
static size_t g_buf_n, g_buf_cap;
static DWORD g_last_flush;
static uint32_t g_t_value, g_t_count;
static uint32_t g_t_peek, g_t_nth;
static uint32_t g_p_none;
static uint16_t g_q_kind;
static int32_t g_q_value;
static uint32_t g_q_count;

struct run {
	uint32_t value, pos, nth;
};

struct q_item {
	uint16_t kind;
	uint8_t bytes, have;
	int32_t value;
	uint32_t count;
	uint8_t *data;
	size_t data_n;
};

struct p_item {
	uint32_t none;
	struct p_rec rec;
};

struct rp_event {
	char type;
	struct s_rec rec;
	uint16_t timer;
};

static struct run *g_t;
static size_t g_t_n, g_t_cap;
static struct p_item *g_p;
static size_t g_p_n, g_p_cap;
static struct q_item *g_q;
static size_t g_q_n, g_q_cap;
static struct rp_event *g_ev;
static size_t g_ev_n, g_ev_cap;
static struct c_rec *g_c;
static size_t g_c_n, g_c_cap;
static size_t g_ti, g_pi, g_qi, g_evi, g_ci;
static uint32_t g_p_used, g_q_used;
static uint32_t g_first_tick;
static DWORD g_real_start;
static bool g_diverged;
static HWND g_status;
static void (*g_checkpoint_fn)(void);
static FILE *g_trace;
static int32_t g_replay_scale;

static void (*g_sound_stop)(int32_t handle);
static void (*g_scaler_present_all)(void);
static void (*g_scaler_force)(int32_t scale);

static VOID CALLBACK timer_trampoline(HWND hwnd, UINT msg, UINT_PTR id, DWORD time);
static int32_t query_impl(int32_t kind, int32_t value);

static void *array_grow(void *items, size_t *cap, size_t count, size_t size)
{
	if (count < *cap)
		return items;

	*cap = *cap ? *cap * 2 : 16;

	return MemRealloc(items, *cap, size);
}

static void trace(const char *fmt, ...)
{
	va_list a;
	if (!g_trace)
		return;
	va_start(a, fmt);
	vfprintf(g_trace, fmt, a);
	va_end(a);
	fputc(10, g_trace);
}

static void rp_log(const char *fmt, ...)
{
	FILE *f;
	va_list a;
	if (!g_log_path[0] || (f = fopen(g_log_path, "a")) == NULL)
		return;
	va_start(a, fmt);
	vfprintf(f, fmt, a);
	va_end(a);
	fputc('\n', f);
	fclose(f);
}

static bool on_game_thread(void)
{
	return g_mode > 0 && !g_live && GetCurrentThreadId() == g_thread;
}

static bool game_class(HWND h)
{
	static const char *const names[] = {"MECCWClass", "MECCDClass", "Edit", "ComboBox", "ComboLBox", "ListBox",
		"Button", "Static", "ScrollBar", "MCIWndClass", "OTIIDebugState", "OTIITwitchOptions", "#32770", NULL};
	char cls[64];
	if (!GetClassNameA(h, cls, sizeof(cls)))
		return false;
	for (int32_t i = 0; names[i]; ++i)
		if (!_stricmp(cls, names[i]))
			return true;
	return false;
}

static LRESULT CALLBACK cbt_hook(int code, WPARAM w, LPARAM l)
{
	if (code == HCBT_CREATEWND && GetCurrentThreadId() == g_thread && game_class((HWND) w)) {
		g_windows = array_grow(g_windows, &g_windows_cap, g_windows_n, sizeof(HWND));
		g_windows[g_windows_n++] = (HWND) w;
	}
	return CallNextHookEx(g_cbt, code, w, l);
}

static uint16_t window_index(HWND h)
{
	if (!h)
		return NO_WINDOW;
	for (size_t i = g_windows_n; i-- > 0;)
		if (g_windows[i] == h)
			return (uint16_t) i;
	return NO_WINDOW;
}

static HWND window_at(uint16_t i)
{
	return i < g_windows_n && IsWindow(g_windows[i]) ? g_windows[i] : NULL;
}

static int32_t timer_index(UINT_PTR real)
{
	for (size_t i = g_timers_n; i-- > 0;)
		if (g_timers[i].real == real)
			return (int32_t) i;
	return -1;
}

static void put(const void *p, size_t n)
{
	if (g_buf_n + n > g_buf_cap) {
		size_t cap = g_buf_cap ? g_buf_cap : 4096;
		while (cap < g_buf_n + n)
			cap *= 2;
		g_buf = MemRealloc(g_buf, cap, 1);
		g_buf_cap = cap;
	}
	memcpy(g_buf + g_buf_n, p, n);
	g_buf_n += n;
}

static void put_tag(char tag)
{
	put(&tag, 1);
}

static void flush_runs(void)
{
	if (g_p_none) {
		put_tag('p');
		put(&g_p_none, 4);
		g_p_none = 0;
	}
	if (g_q_count) {
		put_tag('Q');
		put(&g_q_kind, 2);
		put(&g_q_value, 4);
		put(&g_q_count, 4);
		g_q_count = 0;
	}
}

static void write_out(bool runs)
{
	if (!g_out)
		return;
	if (runs)
		flush_runs();
	if (g_buf_n) {
		fwrite(g_buf, 1, g_buf_n, g_out);
		g_buf_n = 0;
	}
	fflush(g_out);
}

static void maybe_flush(void)
{
	DWORD now = GetTickCount();
	if (g_buf_n > 65536 || now - g_last_flush > 1000) {
		g_last_flush = now;
		write_out(true);
	}
}

static void flush_at_exit(void)
{
	if (g_mode == RP_RECORD)
		write_out(true);
}

static void prune_old(const char *dir, const char *base)
{
	char pattern[MAX_PATH], path[MAX_PATH];
	WIN32_FIND_DATAA fd;
	HANDLE h;
	char (*names)[MAX_PATH] = NULL;
	size_t n = 0, cap = 0;

	StrPrintf(pattern, "%s\\%s_*.otrp", dir, base);
	if ((h = FindFirstFileA(pattern, &fd)) == INVALID_HANDLE_VALUE)
		return;
	do {
		names = array_grow(names, &cap, n, sizeof(*names));
		StrCopy(names[n], fd.cFileName);
		n++;
	} while (FindNextFileA(h, &fd));
	FindClose(h);

	for (size_t a = 0; a < n; ++a)
		for (size_t b = a + 1; b < n; ++b)
			if (strcmp(names[b], names[a]) < 0) {
				char tmp[MAX_PATH];
				memcpy(tmp, names[a], sizeof(tmp));
				memcpy(names[a], names[b], sizeof(tmp));
				memcpy(names[b], tmp, sizeof(tmp));
			}
	for (size_t i = 0; i + 30 < n; ++i) {
		StrPrintf(path, "%s\\%s", dir, names[i]);
		DeleteFileA(path);
	}
	MemFree(names);
}

static void start_recording(void)
{
	char exe[MAX_PATH], dir[MAX_PATH], base[MAX_PATH], path[MAX_PATH], flag[8];
	char *slash, *dot;
	SYSTEMTIME t;
	uint32_t v = VERSION, n;

	if (!GetModuleFileNameA(NULL, exe, MAX_PATH))
		return;
	exe[MAX_PATH - 1] = 0;
	StrCopy(dir, exe);
	if ((slash = strrchr(dir, '\\')) == NULL)
		return;
	StrCopy(base, slash + 1);
	*slash = 0;
	if ((dot = strrchr(base, '.')) != NULL)
		*dot = 0;
	if (IniReadLayered(IniDataPath(), IniPath(), "replay", "record", flag, sizeof(flag)) && flag[0] == '0')
		return;
	StrCat(dir, "\\replays");
	CreateDirectoryA(dir, NULL);
	prune_old(dir, base);
	GetLocalTime(&t);
	StrPrintf(path, "%s\\%s_%04d%02d%02d_%02d%02d%02d.otrp", dir, base, t.wYear, t.wMonth, t.wDay, t.wHour,
		t.wMinute, t.wSecond);
	if ((g_out = fopen(path, "wb")) == NULL)
		return;
	atexit(flush_at_exit);
	put("OTRP", 4);
	put(&v, 4);
	n = (uint32_t) strlen(exe);
	put(&n, 4);
	put(exe, n);
	write_out(false);
}


static void go_live(const char *why)
{
	if (g_live)
		return;
	uint32_t value = g_t_n == 0 ? 0 : (g_ti < g_t_n ? g_t[g_ti].value : g_t[g_t_n - 1].value);
	g_clock_offset = (int32_t) (value - (uint32_t) GetTickCount());
	g_live = true;
	rp_log("replay: live from queue check %u (%s)", g_peeks, why);
	if (g_status)
		SetWindowTextA(g_status, "Replay finished -- live");
}

static bool load_replay(const char *path)
{
	FILE *f;
	uint8_t *d;
	size_t i = 0, n;
	long size;

	if ((f = fopen(path, "rb")) == NULL)
		return false;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	d = MemAlloc(size > 0 ? (size_t) size : 1, 1);
	n = size > 0 ? fread(d, 1, (size_t) size, f) : 0;
	fclose(f);
	if (n < 12 || memcmp(d, "OTRP", 4) || memcmp(d + 4, &VERSION, 4)) {
		MemFree(d);
		return false;
	}
	i = 8;
	{
		uint32_t len;
		memcpy(&len, d + i, 4);
		i += 4 + (size_t) len;
	}
#define NEED(k) if (i + (k) > n) break
	while (i < n) {
		char tag = (char) d[i++];
		if (tag == 'T') {
			struct run r;
			NEED(12);
			memcpy(&r.value, d + i, 4);
			memcpy(&r.pos, d + i + 4, 4);
			memcpy(&r.nth, d + i + 8, 4);
			i += 12;
			g_t = array_grow(g_t, &g_t_cap, g_t_n, sizeof(*g_t));
			g_t[g_t_n++] = r;

		} else if (tag == 'p') {
			struct p_item p;
			NEED(4);
			memset(&p, 0, sizeof(p));
			memcpy(&p.none, d + i, 4);
			i += 4;
			g_p = array_grow(g_p, &g_p_cap, g_p_n, sizeof(*g_p));
			g_p[g_p_n++] = p;

		} else if (tag == 'P') {
			struct p_item p;
			NEED(sizeof(struct p_rec));
			p.none = 0;
			memcpy(&p.rec, d + i, sizeof(struct p_rec));
			i += sizeof(struct p_rec);
			g_p = array_grow(g_p, &g_p_cap, g_p_n, sizeof(*g_p));
			g_p[g_p_n++] = p;

		} else if (tag == 'Q') {
			struct q_item q;
			NEED(10);
			memset(&q, 0, sizeof(q));
			memcpy(&q.kind, d + i, 2);
			memcpy(&q.value, d + i + 2, 4);
			memcpy(&q.count, d + i + 6, 4);
			q.bytes = 0;
			q.have = 1;
			i += 10;
			g_q = array_grow(g_q, &g_q_cap, g_q_n, sizeof(*g_q));
			g_q[g_q_n++] = q;

		} else if (tag == 'B') {
			struct q_item q;
			uint32_t len;
			NEED(7);
			memset(&q, 0, sizeof(q));
			memcpy(&q.kind, d + i, 2);
			q.have = d[i + 2];
			memcpy(&len, d + i + 3, 4);
			i += 7;
			NEED(len);
			q.bytes = 1;
			q.value = 0;
			q.count = 1;
			q.data_n = len;
			q.data = len ? MemDup(d + i, len) : NULL;
			i += len;
			g_q = array_grow(g_q, &g_q_cap, g_q_n, sizeof(*g_q));
			g_q[g_q_n++] = q;

		} else if (tag == 'S') {
			struct rp_event e;
			NEED(sizeof(struct s_rec));
			e.type = 'S';
			memcpy(&e.rec, d + i, sizeof(struct s_rec));
			e.timer = 0;
			i += sizeof(struct s_rec);
			g_ev = array_grow(g_ev, &g_ev_cap, g_ev_n, sizeof(*g_ev));
			g_ev[g_ev_n++] = e;

		} else if (tag == 'K') {
			struct rp_event e;
			NEED(6);
			memset(&e, 0, sizeof(e));
			e.type = 'K';
			memcpy(&e.rec.pos, d + i, 4);
			memcpy(&e.timer, d + i + 4, 2);
			i += 6;
			g_ev = array_grow(g_ev, &g_ev_cap, g_ev_n, sizeof(*g_ev));
			g_ev[g_ev_n++] = e;

		} else if (tag == 'Z') {
			int32_t scale;
			NEED(4);
			memcpy(&scale, d + i, 4);
			i += 4;
			if (!g_replay_scale)
				g_replay_scale = scale;

		} else if (tag == 'C') {
			struct c_rec c;
			NEED(sizeof(struct c_rec));
			memcpy(&c, d + i, sizeof(struct c_rec));
			i += sizeof(struct c_rec);
			g_c = array_grow(g_c, &g_c_cap, g_c_n, sizeof(*g_c));
			g_c[g_c_n++] = c;

		} else {
			break;
		}
	}
#undef NEED
	MemFree(d);
	if (g_t_n)
		g_first_tick = g_t[0].value;
	g_real_start = GetTickCount();
	rp_log("replay: %s -- %u clock runs, %u queue entries, %u queries, %u events, %u checkpoints", path,
		(uint32_t) g_t_n, (uint32_t) g_p_n, (uint32_t) g_q_n, (uint32_t) g_ev_n, (uint32_t) g_c_n);
	return true;
}

static LRESULT CALLBACK status_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
	return DefWindowProcA(h, m, w, l);
}

static void show_status(void)
{
	WNDCLASSA wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.lpfnWndProc = status_proc;
	wc.hInstance = GetModuleHandleA(NULL);
	wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
	wc.lpszClassName = "OTIIReplayStatus";
	RegisterClassA(&wc);
	g_status = CreateWindowExA(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, "OTIIReplayStatus", "Replay", WS_OVERLAPPED | WS_CAPTION,
		0, 0, 420, 40, NULL, NULL, GetModuleHandleA(NULL), NULL);
	if (g_status)
		ShowWindow(g_status, SW_SHOWNOACTIVATE);
}

static void update_status(void)
{
	static DWORD last;
	char text[128];
	DWORD now = GetTickCount();
	if (!g_status || now - last < 500)
		return;
	last = now;
	StrPrintf(text, "Replay: message %u of about %u%s%s", g_peeks, (uint32_t) g_p_n,
		g_fast ? " (fast)" : "", g_diverged ? " -- DIVERGED, see the log" : "");
	SetWindowTextA(g_status, text);
}

static const struct q_item *next_query(int32_t kind)
{
	while (g_qi < g_q_n && !g_q[g_qi].bytes && g_q_used >= g_q[g_qi].count) {
		++g_qi;
		g_q_used = 0;
	}
	if (g_qi >= g_q_n) {
		go_live("recording ends (queries)");
		return NULL;
	}
	if (g_q[g_qi].kind != kind) {
		rp_log("replay: DIVERGED at queue check %u: the game asked for query %d, the recording has %d", g_peeks, kind,
			(int32_t) g_q[g_qi].kind);
		g_diverged = true;
		go_live("diverged");
		return NULL;
	}
	return &g_q[g_qi];
}

static void parse_command_line(char *replay, size_t size)
{
	const char *p = GetCommandLineA();
	char tok[MAX_PATH];
	int32_t argi = 0, want = 0;

	replay[0] = 0;
	while (*p) {
		size_t n = 0;
		while (*p == ' ' || *p == '\t')
			++p;
		if (!*p)
			break;
		if (*p == '"') {
			++p;
			while (*p && *p != '"' && n < sizeof(tok) - 1)
				tok[n++] = *p++;
			if (*p == '"')
				++p;

		} else {
			while (*p && *p != ' ' && *p != '\t' && n < sizeof(tok) - 1)
				tok[n++] = *p++;
		}
		tok[n] = 0;
		if (argi++ == 0)
			continue;
		if (want == 1) {
			StrCopyN(replay, size, tok);
			want = 0;

		} else if (want == 2) {
			g_until = (uint32_t) strtoul(tok, NULL, 10);
			want = 0;

		} else if (!_stricmp(tok, "/replay") || !_stricmp(tok, "-replay")) {
			want = 1;

		} else if (!_stricmp(tok, "/until") || !_stricmp(tok, "-until")) {
			want = 2;

		} else if (!_stricmp(tok, "/fast") || !_stricmp(tok, "-fast")) {
			g_fast = true;

		} else if (tok[0] != '/' && tok[0] != '-' && !g_first_arg[0]) {
			StrCopy(g_first_arg, tok);
		}
	}
}

static void init(void)
{
	char replay[MAX_PATH], exe[MAX_PATH], *dot;

	g_mode = RP_OFF;
	g_thread = GetCurrentThreadId();
	if (GetModuleFileNameA(NULL, exe, MAX_PATH)) {
		exe[MAX_PATH - 1] = 0;
		StrCopy(g_log_path, exe);
		if ((dot = strrchr(g_log_path, '.')) != NULL && !strchr(dot, '\\'))
			*dot = 0;
		StrCat(g_log_path, "_replay.txt");
	}
	parse_command_line(replay, sizeof(replay));
	{
		char env[8], path[MAX_PATH];
		if (GetEnvironmentVariableA("OTRP_TRACE", env, sizeof(env)) && env[0] == '1') {
			StrPrintf(path, "%.*s_%s.trace.txt", (int) (strlen(g_log_path) - 11), g_log_path,
				replay[0] ? "replay" : "record");
			g_trace = fopen(path, "w");
		}
	}
	g_cbt = SetWindowsHookExA(WH_CBT, cbt_hook, NULL, g_thread);
	if (replay[0]) {
		if (load_replay(replay)) {
			g_mode = RP_REPLAY;
			if (g_replay_scale && g_scaler_force)
				g_scaler_force(g_replay_scale);
			show_status();

		} else {
			rp_log("replay: cannot read %s -- running normally", replay);
			MessageBoxA(NULL, "The replay file could not be read; the game starts normally.", "Oregon Trail II", MB_OK);
		}
		return;
	}
	start_recording();
	if (g_out)
		g_mode = RP_RECORD;
}

static bool is_input_message(UINT m)
{
	return (m >= WM_KEYFIRST && m <= WM_KEYLAST) || (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST) ||
		(m >= WM_NCMOUSEMOVE && m <= WM_NCMBUTTONDBLCLK) || m == WM_TIMER || m == WM_COMMAND ||
		m == WM_SYSCOMMAND || m == WM_QUIT || m == WM_USER + 0x200 || m >= WM_USER;
}

static bool is_non_client(UINT m)
{
	return (m >= WM_NCMOUSEMOVE && m <= WM_NCMBUTTONDBLCLK) || m == WM_SYSKEYDOWN || m == WM_SYSKEYUP ||
		m == WM_SYSCHAR || m == WM_SYSDEADCHAR || m == WM_NCMOUSEHOVER || m == WM_NCMOUSELEAVE;
}

static bool is_logic_message(UINT m)
{
	return m == WM_COMMAND || m == WM_CLOSE || m == WM_ACTIVATEAPP || m == WM_ACTIVATE || m == WM_CHAR ||
		m == WM_KEYDOWN || m == WM_KEYUP || m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK ||
		m == WM_RBUTTONDOWN || m == WM_RBUTTONUP || m == WM_MOUSEMOVE;
}

static uint32_t map_lparam(UINT m, LPARAM l, uint16_t *flags)
{
	if ((m == WM_COMMAND || m == WM_ACTIVATE) && l) {
		uint16_t idx = window_index((HWND) l);
		*flags |= F_LWND;
		return idx;
	}
	if (m == WM_ACTIVATEAPP)
		return 0;
	return (uint32_t) l;
}

static LPARAM unmap_lparam(uint16_t flags, uint32_t l)
{
	if (flags & F_LWND)
		return l == NO_WINDOW ? 0 : (LPARAM) window_at((uint16_t) l);
	return (LPARAM) l;
}

int32_t RpMode(void)
{
	if (g_mode < 0)
		init();
	return g_live ? RP_OFF : g_mode;
}

void RpFlush(void)
{
	if (g_mode == RP_RECORD)
		write_out(true);
}

void RpTraceRand(int32_t lo, int32_t hi, int32_t count, int32_t result)
{
	if (g_trace)
		trace("R %d %d %d = %d +%X at %u", lo, hi, count, result, 0u, g_peeks);
}

const char *RpFirstArgument(void)
{
	RpMode();
	return g_first_arg[0] ? g_first_arg : NULL;
}

uint32_t RpGetTickCount(void)
{
	DWORD real;
	uint32_t nth;

	RpMode();
	real = GetTickCount();
	if (g_live)
		return (uint32_t) real + (uint32_t) g_clock_offset;
	if (!on_game_thread())
		return real;
	if (g_external) {
		if (g_mode == RP_RECORD)
			return g_t_count ? g_t_value : real;
		return g_ti < g_t_n ? g_t[g_ti].value : real;
	}
	if (g_t_peek != g_peeks) {
		g_t_peek = g_peeks;
		g_t_nth = 0;
	}
	nth = g_t_nth++;
	if (g_mode == RP_RECORD) {
		if (!g_t_count || g_t_value != real) {
			uint32_t value = real;
			put_tag('T');
			put(&value, 4);
			put(&g_peeks, 4);
			put(&nth, 4);
			g_t_value = real;
			g_t_count = 1;
			trace("T %u at %u", value, g_peeks);
		}
		return real;
	}
	if (!g_t_n)
		return real;
	while (g_ti + 1 < g_t_n &&
			(g_t[g_ti + 1].pos < g_peeks || (g_t[g_ti + 1].pos == g_peeks && g_t[g_ti + 1].nth <= nth))) {
		++g_ti;
		trace("T %u at %u", g_t[g_ti].value, g_peeks);
		if (!g_fast) {
			DWORD target = g_real_start + (g_t[g_ti].value - g_first_tick);
			DWORD now;
			while ((int32_t) (target - (now = GetTickCount())) > 0)
				Sleep((int32_t) (target - now) > 20 ? 20 : target - now);
		}
	}
	return g_t[g_ti].value;
}

uint32_t RpClockNow(void)
{
	DWORD real;

	RpMode();
	real = GetTickCount();
	if (g_live)
		return (uint32_t) real + (uint32_t) g_clock_offset;
	if (!on_game_thread())
		return real;
	if (g_mode == RP_RECORD)
		return g_t_count ? g_t_value : real;
	if (g_mode == RP_REPLAY)
		return g_ti < g_t_n ? g_t[g_ti].value : real;
	return real;
}

int32_t GameTicks(void)
{
	return (int32_t) (RpGetTickCount() / 17);
}

static VOID CALLBACK timer_trampoline(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
	int32_t i = timer_index(id);
	if (i < 0)
		return;
	if (on_game_thread()) {
		if (g_mode == RP_REPLAY && !g_injecting)
			return;

		if (g_mode == RP_RECORD && g_external) {
			uint16_t t = (uint16_t) i;
			trace("K %u timer %u", g_peeks, (uint32_t) t);
			put_tag('K');
			put(&g_peeks, 4);
			put(&t, 2);
			g_external = 0;
			g_timers[i].proc(hwnd, msg, id, time);
			g_external = 1;
			return;
		}
	}
	g_timers[i].proc(hwnd, msg, id, time);
}

uintptr_t RpSetTimer(void *hwnd, uintptr_t id, uint32_t elapse, void *proc)
{
	struct timer t;
	TIMERPROC tp = (TIMERPROC) (uintptr_t) proc;

	RpMode();
	if (hwnd || !proc || GetCurrentThreadId() != g_thread || g_mode == RP_OFF)
		return SetTimer((HWND) hwnd, id, elapse, tp);
	t.proc = tp;
	t.real = SetTimer(NULL, 0, elapse, timer_trampoline);
	if (t.real) {
		g_timers = array_grow(g_timers, &g_timers_cap, g_timers_n, sizeof(*g_timers));
		g_timers[g_timers_n++] = t;
	}
	trace("timer %u created at %u", (uint32_t) g_timers_n - 1, g_peeks);
	return t.real;
}

static void inject(const struct rp_event *e)
{
	if (e->type == 'K')
		trace("K %u timer %u", e->rec.pos, (uint32_t) e->timer);
	else
		trace("S %u msg %04X hwnd %u w %u l %u", e->rec.pos, e->rec.msg, (uint32_t) e->rec.hwnd, e->rec.w, e->rec.l);
	++g_injecting;
	if (e->type == 'K') {
		if (e->timer < g_timers_n)
			g_timers[e->timer].proc(NULL, WM_TIMER, g_timers[e->timer].real, RpGetTickCount());

	} else {
		HWND h = window_at(e->rec.hwnd);
		if (h)
			SendMessageA(h, e->rec.msg, (WPARAM) e->rec.w, unmap_lparam(e->rec.flags, e->rec.l));
	}
	--g_injecting;
}

bool RpPeekMessage(void *msg_v, void *hwnd_v, uint32_t first, uint32_t last, uint32_t remove)
{
	MSG *msg = msg_v;
	HWND hwnd = hwnd_v;
	BOOL got;

	RpMode();
	if (!on_game_thread())
		return PeekMessageA(msg, hwnd, first, last, remove) != 0;

	if (g_scaler_present_all)
		g_scaler_present_all();
	if (g_mode == RP_RECORD) {
		uint32_t index = g_peeks;
		if (g_checkpoint_fn && index % CHECK_EVERY == 0)
			g_checkpoint_fn();
		g_external = 1;
		got = PeekMessageA(msg, hwnd, first, last, remove);
		g_external = 0;
		g_peeks = index + 1;
		if (!got) {
			++g_p_none;

		} else {
			struct p_rec r;
			if (g_p_none) {
				put_tag('p');
				put(&g_p_none, 4);
				g_p_none = 0;
			}
			memset(&r, 0, sizeof(r));
			r.msg = msg->message;
			r.hwnd = window_index(msg->hwnd);
			r.w = (uint32_t) msg->wParam;
			r.l = (uint32_t) msg->lParam;
			if (!msg->hwnd && msg->message == WM_TIMER && msg->lParam == (LPARAM) timer_trampoline) {
				int32_t t = timer_index(msg->wParam);
				r.flags |= F_TIMER;
				r.w = (uint32_t) (t < 0 ? 0xFFFF : t);
				r.l = 0;

			} else {
				r.l = map_lparam(msg->message, msg->lParam, &r.flags);
			}
			r.x = msg->pt.x;
			r.y = msg->pt.y;
			r.time = msg->time;
			put_tag('P');
			put(&r, sizeof(r));
			trace("P %u msg %04X hwnd %u flags %u w %u l %u rng %u", index, r.msg, (uint32_t) r.hwnd,
				(uint32_t) r.flags, r.w, r.l, (uint32_t) RngCalls());
		}
		if ((index & 0xFFFF) == 0)
			maybe_flush();
		return got != 0;
	}
	if (g_checkpoint_fn && g_peeks % CHECK_EVERY == 0)
		g_checkpoint_fn();

	{
		MSG real;
		for (;;) {
			BOOL any;
			g_external = 1;
			any = PeekMessageA(&real, NULL, 0, 0, PM_REMOVE);
			g_external = 0;
			if (!any)
				break;
			bool game = real.hwnd && window_index(real.hwnd) != NO_WINDOW;
			if (real.message == WM_PAINT && game) {
				RedrawWindow(real.hwnd, NULL, NULL, RDW_VALIDATE | RDW_NOFRAME | RDW_NOERASE);

			} else if (!is_input_message(real.message) && real.hwnd && !game) {
				TranslateMessage(&real);
				DispatchMessageA(&real);
			}
		}
	}

	while (g_evi < g_ev_n && g_ev[g_evi].rec.pos <= g_peeks)
		inject(&g_ev[g_evi++]);

	if (g_peeks >= g_until) {
		go_live("/until reached");
		return PeekMessageA(msg, hwnd, first, last, remove) != 0;
	}
	update_status();
	++g_peeks;
	while (g_pi < g_p_n && g_p[g_pi].none && g_p_used >= g_p[g_pi].none) {
		++g_pi;
		g_p_used = 0;
	}
	if (g_pi >= g_p_n) {
		go_live("recording ends (messages)");
		return PeekMessageA(msg, hwnd, first, last, remove) != 0;
	}
	if (g_p[g_pi].none) {
		++g_p_used;
		return false;
	}
	{
		const struct p_rec *r = &g_p[g_pi++].rec;
		g_p_used = 0;
		trace("P %u msg %04X hwnd %u flags %u w %u l %u rng %u", g_peeks - 1, r->msg, (uint32_t) r->hwnd,
			(uint32_t) r->flags, r->w, r->l, (uint32_t) RngCalls());
		memset(msg, 0, sizeof(*msg));
		msg->message = r->msg;
		msg->time = r->time;
		msg->pt.x = r->x;
		msg->pt.y = r->y;
		if (r->flags & F_TIMER) {
			msg->hwnd = NULL;
			msg->wParam = r->w;
			msg->lParam = (LPARAM) timer_trampoline;

		} else {
			msg->hwnd = r->hwnd == NO_WINDOW ? NULL : window_at(r->hwnd);
			msg->wParam = r->w;
			msg->lParam = unmap_lparam(r->flags, r->l);
			if (r->hwnd != NO_WINDOW && !msg->hwnd)
				msg->message = WM_NULL;
		}
		return true;
	}
}

intptr_t RpDispatchMessage(const void *msg_v)
{
	const MSG *msg = msg_v;
	LRESULT r;

	RpMode();
	if (!on_game_thread())
		return DispatchMessageA(msg);
	if (g_mode == RP_REPLAY) {
		if (!msg->hwnd && msg->message == WM_TIMER && msg->lParam == (LPARAM) timer_trampoline) {
			if (msg->wParam < g_timers_n) {
				++g_injecting;
				g_timers[msg->wParam].proc(NULL, WM_TIMER, g_timers[msg->wParam].real, msg->time);
				--g_injecting;
			}
			return 0;
		}
		if (is_non_client(msg->message) || msg->message == WM_NULL)
			return 0;
		++g_injecting;
		r = DispatchMessageA(msg);
		--g_injecting;
		return r;
	}

	if (is_non_client(msg->message)) {
		g_external = 1;
		r = DispatchMessageA(msg);
		g_external = 0;
		return r;
	}
	return DispatchMessageA(msg);
}

int32_t RpWindowEnter(void *hwnd, uint32_t msg, uintptr_t wparam, intptr_t lparam)
{
	int32_t token = g_external;

	RpMode();
	if (!on_game_thread() || !is_logic_message(msg))
		return token;
	if (g_mode == RP_REPLAY)
		return g_external ? -1 : token;
	if (g_external) {
		struct s_rec s;
		memset(&s, 0, sizeof(s));
		s.pos = g_peeks;
		s.hwnd = window_index((HWND) hwnd);
		s.msg = msg;
		s.w = (uint32_t) wparam;
		s.l = map_lparam(msg, (LPARAM) lparam, &s.flags);
		put_tag('S');
		put(&s, sizeof(s));
		trace("S %u msg %04X hwnd %u w %u l %u", s.pos, s.msg, (uint32_t) s.hwnd, s.w, s.l);
		g_external = 0;
	}
	return token;
}

void RpWindowLeave(int32_t token)
{
	if (token >= 0)
		g_external = token;
}

void RpSetCheckpoint(void (*fn)(void))
{
	g_checkpoint_fn = fn;
}

static POINT g_virtual_cursor;
static DWORD g_virtual_cursor_until;

void RpSetVirtualCursor(int32_t x, int32_t y, int32_t ms)
{
	g_virtual_cursor.x = x;
	g_virtual_cursor.y = y;
	g_virtual_cursor_until = GetTickCount() + (DWORD) ms;
}

bool RpGetCursorPos(struct point *pt)
{
	POINT p;
	BOOL ok;

	if (g_virtual_cursor_until && (int32_t) (GetTickCount() - g_virtual_cursor_until) < 0) {
		p = g_virtual_cursor;
		ok = TRUE;

	} else {
		p.x = pt->h;
		p.y = pt->v;
		ok = GetCursorPos(&p);
	}
	int32_t x = RpQuery(RPQ_CURSOR, p.x), y = RpQuery(RPQ_CURSOR, p.y);
	pt->h = x;
	pt->v = y;
	return ok != 0;
}

int32_t RpGetKeyState(int32_t key)
{
	return (int16_t) RpQuery(RPQ_KEYSTATE, GetKeyState(key));
}

uint32_t RpGetPrivateProfileInt(const char *app, const char *key, int32_t def, const char *file)
{
	return (uint32_t) RpQuery(RPQ_INI_INT, (int32_t) GetPrivateProfileIntA(app, key, def, file));
}

uint32_t RpGetPrivateProfileString(const char *app, const char *key, const char *def, char *out, uint32_t size,
	const char *file)
{
	return RpIniString(out, size, GetPrivateProfileStringA(app, key, def, out, size, file));
}

uint32_t RpIniString(char *out, uint32_t size, uint32_t n)
{
	if (!size)
		return 0;

	RpMode();
	if (on_game_thread() && g_mode == RP_REPLAY) {
		char *live = MemAlloc(size, 1);
		memcpy(live, out, size);
		if (!RpQueryBytes(RPQ_INI_STRING, out, (int32_t) size, true))
			out[0] = 0;
		if (g_live && !out[0])
			memcpy(out, live, size);
		MemFree(live);
		out[size - 1] = 0;
		return (uint32_t) strlen(out);
	}
	RpQueryBytes(RPQ_INI_STRING, out, (int32_t) (n + 1 < size ? n + 1 : size), true);
	return n;
}

bool RpWritePrivateProfileString(const char *app, const char *key, const char *value, const char *file)
{
	RpMode();
	if (on_game_thread() && g_mode == RP_REPLAY)
		return true;
	return WritePrivateProfileStringA(app, key, value, file) != 0;
}

int32_t RpMessageBox(void *hwnd, const char *text, const char *caption, uint32_t type)
{
	RpMode();
	if (on_game_thread() && g_mode == RP_REPLAY) {
		int32_t r = RpQuery(RPQ_MESSAGEBOX, 0);
		if (!g_live)
			return r;
		return MessageBoxA((HWND) hwnd, text, caption, type);
	}
	{
		int32_t saved = g_external, r;
		if (on_game_thread() && g_mode == RP_RECORD)
			g_external = 1;
		r = MessageBoxA((HWND) hwnd, text, caption, type);
		g_external = saved;
		return RpQuery(RPQ_MESSAGEBOX, r);
	}
}

int32_t RpTrackPopupMenu(void *menu, uint32_t flags, int32_t x, int32_t y, void *hwnd)
{
	RpMode();
	if (on_game_thread() && g_mode == RP_REPLAY)
		return 0;

	int32_t saved = g_external;

	if (on_game_thread() && g_mode == RP_RECORD)
		g_external = 1;

	int32_t r = TrackPopupMenu((HMENU) menu, flags, x, y, 0, (HWND) hwnd, NULL);

	g_external = saved;

	return r;
}

int32_t RpSoundHandle(int32_t handle)
{
	int32_t started = RpQuery(RPQ_SOUND_STARTED, handle != 0);

	if (RpMode() == RP_REPLAY) {
		if (!started && handle) {
			if (g_sound_stop)
				g_sound_stop(handle);
			return 0;
		}
		if (started && !handle)
			return 0x3FFFFFFF;
	}
	return handle;
}

uint32_t RpTime(void)
{
	return (uint32_t) RpQuery(RPQ_TIME, (int32_t) time(NULL));
}

int32_t RpQuery(int32_t kind, int32_t value)
{
	int32_t r = query_impl(kind, value);
	if (g_trace && on_game_thread() && kind != RPQ_CURSOR)
		trace("Q %d = %d", kind, r);
	return r;
}

static int32_t query_impl(int32_t kind, int32_t value)
{
	RpMode();
	if (!on_game_thread() || g_external)
		return value;
	if (g_mode == RP_RECORD) {
		if (g_q_count && g_q_kind == kind && g_q_value == value) {
			++g_q_count;

		} else {
			if (g_q_count) {
				put_tag('Q');
				put(&g_q_kind, 2);
				put(&g_q_value, 4);
				put(&g_q_count, 4);
			}
			g_q_kind = (uint16_t) kind;
			g_q_value = value;
			g_q_count = 1;
		}
		return value;
	}
	{
		const struct q_item *q = next_query(kind);
		if (!q || q->bytes) {
			if (q && q->bytes) {
				rp_log("replay: DIVERGED at queue check %u: query %d expected a value, the recording has bytes",
					g_peeks, kind);
				g_diverged = true;
				go_live("diverged");
			}
			return value;
		}
		++g_q_used;
		return q->value;
	}
}

bool RpQueryBytes(int32_t kind, void *buf, int32_t size, bool have)
{
	RpMode();
	if (!on_game_thread() || g_external)
		return have;
	if (g_mode == RP_RECORD) {
		uint16_t k = (uint16_t) kind;
		uint8_t h = (uint8_t) (have ? 1 : 0);
		uint32_t len = have && size > 0 ? (uint32_t) size : 0;
		if (g_q_count) {
			put_tag('Q');
			put(&g_q_kind, 2);
			put(&g_q_value, 4);
			put(&g_q_count, 4);
			g_q_count = 0;
		}
		put_tag('B');
		put(&k, 2);
		put(&h, 1);
		put(&len, 4);
		if (len)
			put(buf, len);
		return have;
	}
	{
		const struct q_item *q = next_query(kind);
		if (!q)
			return have;
		if (!q->bytes) {
			rp_log("replay: DIVERGED at queue check %u: query %d expected bytes, the recording has a value", g_peeks,
				kind);
			g_diverged = true;
			go_live("diverged");
			return have;
		}
		++g_qi;
		g_q_used = 0;
		if (q->have && size > 0) {
			size_t n = q->data_n < (size_t) size ? q->data_n : (size_t) size;
			if (n)
				memcpy(buf, q->data, n);
		}
		return q->have != 0;
	}
}

void RpCheckpoint(uint32_t hash, int32_t day, int32_t cash, uint32_t rng_calls)
{
	RpMode();
	if (!on_game_thread())
		return;
	if (g_mode == RP_RECORD) {
		struct c_rec c;
		c.pos = g_peeks;
		c.hash = hash;
		c.day = day;
		c.cash = cash;
		c.rng = rng_calls;
		put_tag('C');
		put(&c, sizeof(c));
		return;
	}
	while (g_ci < g_c_n && g_c[g_ci].pos < g_peeks)
		++g_ci;
	if (g_ci < g_c_n && g_c[g_ci].pos == g_peeks) {
		const struct c_rec *c = &g_c[g_ci++];
		if (!g_diverged && (c->hash != hash || c->day != day || c->cash != cash || c->rng != rng_calls)) {
			g_diverged = true;
			rp_log("replay: DIVERGED by queue check %u: recorded day %d cash %d rand calls %u state %08X, "
				"replayed day %d cash %d rand calls %u state %08X", g_peeks, c->day, c->cash, c->rng, c->hash, day,
				cash, rng_calls, hash);
		}
	}
}

void RpNoteScale(int32_t scale)
{
	int32_t value = scale;

	if (g_mode != RP_RECORD || !g_out)
		return;
	put_tag('Z');
	put(&value, 4);
}

void RpSetSoundStop(void (*stop)(int32_t handle))
{
	g_sound_stop = stop;
}

void RpSetScalerHooks(void (*present_all)(void), void (*force)(int32_t scale))
{
	g_scaler_present_all = present_all;
	g_scaler_force = force;
	if (force && g_mode == RP_REPLAY && g_replay_scale)
		force(g_replay_scale);
}

int32_t RpReplayScale(void)
{
	RpMode();
	return g_mode == RP_REPLAY ? g_replay_scale : 0;
}
