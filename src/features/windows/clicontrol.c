#include "../clicontrol.h"

#ifdef CLI_CONTROL

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "../controlstate.h"
#include "../../app/replay.h"
#include "../../base/str.h"

#define POLL_MS 50

static CRITICAL_SECTION g_cs;
static HANDLE g_done;
static char g_cmd[512];
static int32_t g_pending;
static char g_out[65536];
static int32_t g_outlen;
static uint32_t g_seq;
static char g_reply[65536];
static HANDLE g_console = INVALID_HANDLE_VALUE;

static void out(const char *fmt, ...)
{
	va_list a;
	int32_t n;

	if (g_outlen >= (int32_t) sizeof(g_out) - 2)
		return;

	va_start(a, fmt);
	n = vsnprintf(g_out + g_outlen, sizeof(g_out) - g_outlen, fmt, a);
	va_end(a);

	if (n > 0)
		g_outlen = g_outlen + n < (int32_t) sizeof(g_out) - 1 ? g_outlen + n : (int32_t) sizeof(g_out) - 1;
}

static const char *screen_name(int32_t s)
{
	switch (s) {
		case CONTROL_TRAVEL: return "no choices (travelling / title / town street)";
		case CONTROL_EVENT: return "event";
		case CONTROL_LANDMARK: return "landmark";
		case CONTROL_DIALOG: return "dialog";
		case CONTROL_TOWN: return "town street -- walk with the Turn/Walk options, click people and shops";
	}

	return "no game window";
}

static void show_field(const struct control_field *f, int32_t i)
{
	char label[80];

	label[0] = 0;
	if (f->label[0])
		StrPrintf(label, " (%s)", f->label);

	if (f->is_choice) {
		out("   field %d%s: [%s]  (drop-down, %d entries)\n", i, label, f->text, f->choice_count);

	} else {
		out("   field %d%s: \"%s\"\n", i, label, f->text);
	}
}

static void show_state(void)
{
	static struct control_state st;
	int32_t i;

	ControlGetState(&st);
	out("\n== %s  [%s]\n", st.title[0] ? st.title : "(none)", screen_name(st.screen));
	if (st.has_game)
		out("   %d/%02d/%02d  at %s%s\n", st.year, st.month, st.day, st.location,
			st.event_id >= 0 ? "  (event pending)" : "");
	for (i = 0; i < st.info_count; ++i)
		out("   * %s\n", st.info[i]);
	for (i = 0; i < st.side_text_first; ++i)
		out("   | %s\n", st.text[i]);
	for (i = 0; i < st.table_count; ++i)
		out("   # %s\n", st.table[i]);
	for (i = 0; i < st.field_count; ++i)
		show_field(&st.fields[i], i);
	for (i = 0; i < st.side_option_first; ++i)
		out("   %2d) %s\n", i, st.options[i].label);
	if (!st.side_option_first)
		out("   (nothing to click -- 'wait' to let the game run, 'at x y' to click the scene)\n");
	if (st.side_text_first < st.text_count || st.side_option_first < st.option_count) {
		out("   -- Health Status window --\n");
		for (i = st.side_text_first; i < st.text_count; ++i)
			out("   | %s\n", st.text[i]);
		for (i = st.side_option_first; i < st.option_count; ++i)
			out("   %2d) %s\n", i, st.options[i].label);
	}
}

static void execute_shoot(struct control_state *st, const char *arg)
{
	int32_t i, pick = -1, best = -1, nth = *arg ? atoi(arg) : -1, seen = 0;

	ControlGetState(st);
	for (i = 0; i < st->option_count; ++i)
		if (!strcmp(st->options[i].label, "Reload"))
			break;
	if (i < st->option_count) {
		out("the gun is not loaded -- use option %d (Reload) first\n", i);
		return;
	}

	for (i = 0; i < st->option_count; ++i) {
		const char *pct;
		int32_t chance;

		if (st->options[i].button || strncmp(st->options[i].label, "Shoot:", 6))
			continue;
		pct = strstr(st->options[i].label, "about ");
		chance = pct ? atoi(pct + 6) : 0;
		if (nth >= 0 ? seen == nth : chance > best) {
			pick = i;
			best = chance;
		}
		++seen;
	}

	if (pick >= 0 && ControlClick(st, pick)) {
		out("fired: %s\n", st->options[pick].label + 7);

	} else if (!seen) {
		out("nothing to shoot at right now -- 'wait 1' and try again, or 'Rotate' to look another way\n");

	} else {
		out("no target %d (there are %d; 'shoot' picks the best)\n", nth, seen);
	}
}

static void execute_help(void)
{
	out("Commands:\n"
		"  state | s | <enter>   show the window: text, fields and numbered options\n"
		"  <n> | click <n>       click option n\n"
		"  at <x> <y>            click a point in the window\n"
		"  type <n> <text>       fill field n; for a drop-down, pick the entry starting with <text>\n"
		"  type <n> #<i>         pick entry i of drop-down n\n"
		"  list <n>              show the entries of drop-down n\n"
		"  close                 close the top dialog\n"
		"  menu                  list the menu items\n"
		"  menu <id>             invoke a menu item (Save Game, ...)\n"
		"  wait [seconds]        let the game run (default 3), then show the state\n"
		"  shoot [n]             hunting: fire at the best target (or target n, counting from 0), aimed\n"
		"                        when the command runs\n"
		"  export_animals [dir]  save every hunting animal's frames as .bmp sheets (default folder: animals)\n"
		"  debug                 the game's internal state (party ages, ailments, immunities, food,\n"
		"                        weather, event state, inventory, seed) -- as Help > Debug State\n"
		"  dump                  button diagnostics\n"
		"  help | ?              this list\n"
		"  quit | exit           close the command line (the game keeps running)\n");
}

static void execute(char *line)
{
	static struct control_state st;
	char *arg;
	int32_t n;

	while (*line == ' ')
		++line;
	arg = strchr(line, ' ');
	if (arg) {
		*arg++ = 0;

	} else {
		arg = line + strlen(line);
	}

	if (!*line || !strcmp(line, "state") || !strcmp(line, "s")) {
		show_state();

	} else if (!strcmp(line, "click") || (line[0] >= '0' && line[0] <= '9')) {
		n = atoi(line[0] >= '0' && line[0] <= '9' ? line : arg);
		ControlGetState(&st);
		if (ControlClick(&st, n)) {
			out("clicked %d: %s\n", n, st.options[n].label);

		} else {
			out("no option %d (there are %d)\n", n, st.option_count);
		}

	} else if (!strcmp(line, "at")) {
		int x = 0, y = 0;

		ControlGetState(&st);
		if (sscanf(arg, "%d %d", &x, &y) == 2 && ControlClickAt(&st, x, y)) {
			out("clicked (%d, %d) on %s\n", x, y, st.title);

		} else {
			out("usage: at <x> <y>\n");
		}

	} else if (!strcmp(line, "type")) {
		char *text = strchr(arg, ' ');

		n = atoi(arg);
		ControlGetState(&st);
		if (text && ControlSetField(&st, n, text + 1)) {
			ControlGetState(&st);
			if (n < st.field_count && !st.fields[n].is_choice && strcmp(st.fields[n].text, text + 1)) {
				out("field %d = \"%s\" -- the game changed it from \"%s\" (a store limits a quantity to what "
					"you can afford and what it has)\n", n, st.fields[n].text, text + 1);

			} else {
				out("field %d = \"%s\"\n", n, n < st.field_count ? st.fields[n].text : text + 1);
			}

		} else {
			out("usage: type <field> <text | #n>   (there are %d fields; 'list <field>' shows a drop-down)\n",
				st.field_count);
		}

	} else if (!strcmp(line, "menu")) {
		int32_t ids[64];
		static char names[64][64];
		int32_t count = ControlMenuItems(ids, names, 64), i;

		if (*arg) {
			n = atoi(arg);
			for (i = 0; i < count && ids[i] != n; ++i)
				;
			if (i < count && ControlMenu(n)) {
				out("menu %d: %s\n", n, names[i]);

			} else {
				out("menu item %d is not available now\n", n);
			}

		} else {
			for (i = 0; i < count; ++i)
				out("   %5d  %s\n", ids[i], names[i]);
		}

	} else if (!strcmp(line, "close")) {
		ControlGetState(&st);
		if (st.screen == CONTROL_DIALOG && ControlClose(&st)) {
			out("closed %s\n", st.title);

		} else {
			out("nothing to close (the main window stays open; use the menu to quit)\n");
		}

	} else if (!strcmp(line, "list")) {
		static char names[64][64];
		int32_t count, i;

		ControlGetState(&st);
		count = ControlFieldChoices(&st, atoi(arg), names, 64);
		if (!count)
			out("field '%s' is not a drop-down\n", arg);
		for (i = 0; i < count; ++i)
			out("   #%-3d %s\n", i, names[i]);

	} else if (!strcmp(line, "shoot")) {
		execute_shoot(&st, arg);

	} else if (!strcmp(line, "export_animals")) {
		static char report[16000];

		n = ControlExportAnimals(*arg ? arg : "animals", report, sizeof(report));
		out("%s%d picture sheets written\n", report, n);

	} else if (!strcmp(line, "debug")) {
		static char text[60000];

		ControlDebugText(text, sizeof(text));
		out("%s", text);

	} else if (!strcmp(line, "dump")) {
		static char dump[12000];

		ControlDump(dump, sizeof(dump));
		out("%s", dump);

	} else if (!strcmp(line, "help") || !strcmp(line, "?")) {
		execute_help();

	} else {
		out("unknown command '%s' -- try 'help'\n", line);
	}
}

static void CALLBACK poll(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
	char line[512];
	uint32_t seq = 0;
	bool have;

	(void) hwnd;
	(void) msg;
	(void) id;
	(void) time;

	line[0] = 0;
	EnterCriticalSection(&g_cs);
	have = g_pending != 0;
	if (have) {
		StrCopy(line, g_cmd);
		seq = g_seq;
		g_pending = 0;
	}
	LeaveCriticalSection(&g_cs);

	if (!RpQueryBytes(RPQ_CLI_LINE, line, sizeof(line), have))
		return;
	line[sizeof(line) - 1] = 0;
	g_outlen = 0;
	g_out[0] = 0;
	execute(line);

	EnterCriticalSection(&g_cs);
	if (seq == g_seq) {
		memcpy(g_reply, g_out, (size_t) g_outlen + 1);
		SetEvent(g_done);
	}
	LeaveCriticalSection(&g_cs);
}

static void print(const char *text)
{
	static wchar_t wide[65536];
	int n = MultiByteToWideChar(1252, 0, text, -1, wide, 65536);
	DWORD written;

	if (g_console == INVALID_HANDLE_VALUE)
		g_console = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, 0, NULL);

	fflush(stdout);
	if (n > 1)
		WriteConsoleW(g_console, wide, (DWORD) (n - 1), &written, NULL);
}

static bool run(const char *cmd)
{
	EnterCriticalSection(&g_cs);
	StrCopy(g_cmd, cmd);
	++g_seq;
	g_pending = 1;
	ResetEvent(g_done);
	LeaveCriticalSection(&g_cs);

	if (WaitForSingleObject(g_done, 10000) != WAIT_OBJECT_0) {
		EnterCriticalSection(&g_cs);
		g_pending = 0;
		++g_seq;
		LeaveCriticalSection(&g_cs);
		printf("(the game did not answer -- is it busy or closed?)\n");
		return false;
	}

	print(g_reply);

	return true;
}

static DWORD WINAPI reader(LPVOID param)
{
	char line[512];

	(void) param;

	printf("Oregon Trail II -- command line control.  'help' for commands, <enter> to show the game.\n");
	for (;;) {
		char *nl;

		printf("> ");
		fflush(stdout);
		if (!fgets(line, sizeof(line), stdin))
			break;
		if ((nl = strpbrk(line, "\r\n")) != NULL)
			*nl = 0;
		if (!strcmp(line, "quit") || !strcmp(line, "exit"))
			break;
		if (!strncmp(line, "wait", 4)) {
			int32_t sec = atoi(line + 4);
			Sleep((DWORD) (sec > 0 ? sec : 3) * 1000);
			run("state");
			continue;
		}
		if (run(line) && (!strncmp(line, "click", 5) || !strncmp(line, "at ", 3) || !strncmp(line, "menu ", 5) ||
				!strcmp(line, "close") || !strncmp(line, "shoot", 5) || (line[0] >= '0' && line[0] <= '9'))) {
			Sleep(800);
			run("state");
		}
	}
	printf("command line control closed; the game keeps running.\n");
	FreeConsole();

	return 0;
}

void CliControlStart(void)
{
	DWORD tid;
	FILE *f;

	InitializeCriticalSection(&g_cs);
	g_done = CreateEventA(NULL, TRUE, FALSE, NULL);
	if (!AllocConsole())
		return;
	SetConsoleTitleA("Oregon Trail II -- command line");
	SetConsoleOutputCP(1252);
	SetConsoleCP(1252);
	if (freopen_s(&f, "CONIN$", "r", stdin) != 0)
		f = NULL;
	if (freopen_s(&f, "CONOUT$", "w", stdout) != 0)
		f = NULL;
	RpSetTimer(NULL, 0, POLL_MS, (void *) poll);
	HANDLE thread = CreateThread(NULL, 0, reader, NULL, 0, &tid);
	if (thread)
		CloseHandle(thread);
}

#endif
