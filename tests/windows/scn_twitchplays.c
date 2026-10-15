#include "../testframework.h"

#ifdef OTII_TEST

#ifdef OTII_TWITCH

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "../../src/app/evthandler.h"
#include "../../src/base/cstr.h"
#include "../../src/base/file.h"
#include "../../src/base/ini.h"
#include "../../src/base/memory.h"
#include "../../src/base/rng.h"
#include "../../src/base/str.h"
#include "../../src/features/controlstate.h"
#include "../../src/features/twitchcontrol.h"
#include "../../src/features/twitchirc.h"
#include "../../src/game/game.h"
#include "../../src/game/member.h"
#include "../../src/game/party.h"
#include "../../src/screens/healthwin.h"
#include "../../src/trail/node.h"
#include "../../src/ui/window.h"
#include "../testsupport.h"

static const char SERVER[] = "irc://127.0.0.1:6670";
static const char REWARD[] = "test-party-reward";

static bool wait_for(bool (*done)(void), int32_t ms)
{
	DWORD until = GetTickCount() + (DWORD) ms;

	while (!done() && (LONG) (GetTickCount() - until) < 0)
		TestPump(100);
	return done();
}

static bool connected(void)
{
	char s[256];

	TwitchStatus(s, sizeof(s));
	return !strncmp(s, "connected", 9);
}

static bool connect(void)
{
	TwitchTestReset();
	CHECK(TwitchControlTestConnect("testchan", REWARD, SERVER));
	CHECK_MSG(wait_for(connected, 15000), "no connection to the test server (is tools/twitch_testserver.py --test "
		"--port 6670 running?)");
	return connected();
}

static void disconnect(void)
{
	TwitchControlTestDisconnect();
	TwitchTestReset();
}

static void send_line(const char *kind, const char *login, const char *text)
{
	char line[400];

	if (!strcmp(kind, "TESTREDEEM"))
		StrPrintf(line, "TESTREDEEM %s %s %s :%s", login, login, REWARD, text);
	else
		StrPrintf(line, "TESTCHAT %s %s :%s", login, login, text);
	CHECK_MSG(TwitchSendRaw(line), "sending %s", line);
}

static struct control_state g_st;
static void (*g_drive)(const struct control_state *st);
static bool g_done;
static bool g_in_tick;
static int32_t g_phase;
static int32_t g_moves;
static int32_t g_sent;
static char g_login[16];
static char g_said[200];
static bool g_waiting;
static bool g_expect_none;
static DWORD g_deadline;
static DWORD g_next_at;
static DWORD g_phase_until;
static char g_seen[2048];

static DWORD now(void)
{
	return GetTickCount();
}

static bool picked(const char *login, char *line, size_t size)
{
	char lines[5][256];
	char key[40];
	int32_t n = TwitchControlLastPicks(lines, 5);

	StrPrintf(key, "  %s: \"", login);
	for (int32_t i = 0; i < n; ++i)
		if (strstr(lines[i], key)) {
			if (line)
				StrCopyN(line, size, lines[i]);
			return true;
		}
	return false;
}

static void say_kind(const char *kind, const char *text, bool expect_none)
{
	StrPrintf(g_login, "v%d", ++g_sent);
	StrCopy(g_said, text);
	send_line(kind, g_login, text);
	g_waiting = true;
	g_expect_none = expect_none;
	g_deadline = now() + (expect_none ? 2500u : 6000u);
	++g_moves;
}

static void say(const char *text)
{
	say_kind("TESTCHAT", text, false);
}

static void say_nothing(const char *text)
{
	say_kind("TESTCHAT", text, true);
}

static void phase(int32_t p, int32_t timeout_ms)
{
	g_phase = p;
	g_moves = 0;
	g_phase_until = now() + (DWORD) timeout_ms;
}

static bool phase_timed_out(const char *what)
{
	if ((LONG) (now() - g_phase_until) < 0)
		return false;
	{
		char opts[1024];

		opts[0] = 0;
		for (int32_t i = 0; i < g_st.option_count && strlen(opts) + strlen(g_st.options[i].label) + 4 < sizeof(opts);
			++i) {
			StrCat(opts, i ? ", " : "");
			StrCat(opts, g_st.options[i].label);
		}
		CHECK_MSG(0, "phase %d timed out: %s (screen %d \"%s\": %s; node %d, travel state %d)", g_phase, what,
			g_st.screen, g_st.title, opts, g_game && g_game->cur_node ? g_game->cur_node->node_id : -1,
			g_game ? g_game->travel_state : -1);
	}
	g_done = true;
	return true;
}

static void CALLBACK drive_tick(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
	(void) hwnd;
	(void) msg;
	(void) id;
	(void) time;

	if (g_done || g_in_tick)
		return;
	g_in_tick = true;

	if (g_waiting) {
		char line[256];
		bool got = picked(g_login, line, sizeof(line));

		if (g_expect_none) {
			CHECK_MSG(!got, "\"%s\" picked something: %s", g_said, line);
			if (got || (LONG) (now() - g_deadline) >= 0) {
				g_waiting = false;
				g_next_at = now() + 300;
			}

		} else if (got) {
			TNote("chat \"%s\": %s", g_said, line + 10);
			g_waiting = false;
			g_next_at = now() + 1300;

		} else if ((LONG) (now() - g_deadline) >= 0) {
			CHECK_MSG(0, "\"%s\" picked nothing (screen \"%s\")", g_said, g_st.title);
			g_waiting = false;
			g_next_at = now() + 300;
		}

	} else if ((LONG) (now() - g_next_at) >= 0 && ControlGetState(&g_st)) {
		if (g_st.title[0] && !strstr(g_seen, g_st.title) && strlen(g_seen) + strlen(g_st.title) + 2 < sizeof(g_seen)) {
			StrCat(g_seen, "|");
			StrCat(g_seen, g_st.title);
		}
		g_drive(&g_st);
	}

	g_in_tick = false;
}

static void run_driver(void (*drive)(const struct control_state *), int32_t ms)
{
	g_drive = drive;
	g_done = false;
	g_waiting = false;
	g_seen[0] = 0;
	g_next_at = now();
	phase(0, 30000);

	UINT_PTR timer = SetTimer(NULL, 0, 50, drive_tick);
	DWORD until = now() + (DWORD) ms;
	MSG msg;

	TestDismisser(false);
	while (!g_done && (LONG) (now() - until) < 0) {
		if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);

		} else {
			RunIdleProcessing(3);
		}
	}
	CHECK_MSG(g_done, "the scenario did not finish in %d s (phase %d, screen \"%s\")", ms / 1000, g_phase,
		g_st.title);
	g_done = true;
	KillTimer(NULL, timer);
	TestDismisser(true);
	TNote("screens: %s", g_seen);
}

static const struct control_option *option(const struct control_state *st, const char *prefix)
{
	for (int32_t i = 0; i < st->side_option_first; ++i)
		if (!_strnicmp(st->options[i].label, prefix, strlen(prefix)))
			return &st->options[i];
	return NULL;
}

static const struct control_option *option_with(const struct control_state *st, const char *part)
{
	for (int32_t i = 0; i < st->side_option_first; ++i)
		if (strstr(st->options[i].label, part))
			return &st->options[i];
	return NULL;
}

static int32_t field(const struct control_state *st, const char *label)
{
	for (int32_t i = 0; i < st->field_count; ++i)
		if (!_strnicmp(st->fields[i].label, label, strlen(label)))
			return i;
	return -1;
}

static bool set_up_screen(const struct control_state *st)
{
	return st->screen == CONTROL_DIALOG && (!strcmp(st->title, "Character Creation") || !strcmp(st->title, "Prologue"));
}

static bool answer(const struct control_state *st)
{
	static const char *const usual[] = {"OK", "Continue", "Yes", "Done", "Close", "So long."};

	if ((st->screen != CONTROL_DIALOG && st->screen != CONTROL_EVENT) || set_up_screen(st))
		return false;
	if (!strcmp(st->title, "Conversation")) {
		if (st->side_option_first > 0) {
			char n[8];
			StrPrintf(n, "%d", st->side_option_first);
			say(n);
			return true;
		}
		return false;
	}
	for (int32_t i = 0; i < (int32_t) (sizeof(usual) / sizeof(usual[0])); ++i)
		if (option(st, usual[i])) {
			say(usual[i]);
			return true;
		}
	for (int32_t i = 0; i < st->side_option_first; ++i) {
		const char *l = st->options[i].label;
		if (strcmp(l, "Map") && strcmp(l, "Journal") && strcmp(l, "Party health") && strcmp(l, "Supplies")) {
			say(l);
			return true;
		}
	}
	return false;
}

static bool read_obs(const char *file, char *out, size_t size)
{
	char path[MAX_PATH];
	size_t len;

	FileGetExeDir(path, sizeof(path));
	len = strlen(path);
	if (len && path[len - 1] != '\\')
		StrCat(path, "\\");
	StrCat(path, file);

	void *data = FileReadAll(path, &len);
	out[0] = 0;
	if (!data)
		return false;
	StrCopyN(out, size, (const char *) data);
	if (len < size)
		out[len] = 0;
	MemFree(data);
	return true;
}

static bool health_visible(void)
{
	return g_health_window && IsWindowVisible((HWND) ((struct window *) g_health_window)->hwnd);
}

static void drive_picks(const struct control_state *st)
{
	switch (g_phase) {
		case 0:
			CHECK_MSG(st->screen == CONTROL_LANDMARK, "screen %d \"%s\"", st->screen, st->title);
			{
				const char *text = TwitchControlTestOptionsText();
				CHECK_MSG(strstr(text, "   1  Map\r\n") && strstr(text, "  3  Party health\r\n"), "%s", text);
			}
			if (health_visible())
				say("close health status");
			phase(1, 10000);
			return;

		case 1:
			CHECK(!health_visible());
			say("party health");
			phase(2, 10000);
			return;

		case 2:
			CHECK(health_visible());
			{
				const char *text = TwitchControlTestOptionsText();
				CHECK_MSG(strstr(text, "Health Status window:") && strstr(text, "Close Health Status"), "%s", text);
			}
			say("close health status");
			phase(3, 10000);
			return;

		case 3:
			CHECK(!health_visible());
			say_kind("TESTREDEEM", "party health", true);
			phase(4, 10000);
			return;

		case 4:
			CHECK(!health_visible());
			CHECK_EQ(TwitchPartyPending(), 1);
			say_nothing("xyzzy plugh");
			phase(5, 10000);
			return;

		case 5:
			say_nothing("99");
			phase(6, 10000);
			return;

		case 6:
			say("1");
			phase(7, 10000);
			return;

		case 7:
			if (st->screen != CONTROL_DIALOG) {
				if (!phase_timed_out("the map"))
					g_next_at = now() + 200;
				return;
			}
			{
				const char *text = TwitchControlTestOptionsText();
				CHECK_MSG(strstr(text, "Close window"), "%s", text);
			}
			say("close window");
			phase(8, 10000);
			return;

		case 8:
			if (st->screen == CONTROL_DIALOG) {
				if (!phase_timed_out("the map closing"))
					g_next_at = now() + 200;
				return;
			}
			CHECK_EQ(st->screen, CONTROL_LANDMARK);
			g_done = true;
			return;
	}
}

TEST_GAME(scn_twitchplays, picks)
{
	if (!connect()) {
		disconnect();
		return;
	}
	run_driver(drive_picks, 90000);
	disconnect();
}

static char g_difficulty_shown[64];
static char g_store_field[64];
static char g_store_amount[8];
static double g_cash_before;
static bool g_seed_set;

static void check_new_party(void)
{
	struct party *p = g_game ? g_game->party : NULL;
	char text[64];

	CHECK(p != NULL);
	if (!p)
		return;
	TCase("the party");
	CHECK_EQ(p->member_count, 6);
	CHECK_MSG(!strcmp(CStrGet(p->members[0]->name), "Chat"), "leader %s", CStrGet(p->members[0]->name));
	CHECK_MSG(!strcmp(CStrGet(p->members[1]->name), "alice"), "slot 1 %s", CStrGet(p->members[1]->name));
	CHECK_EQ(p->members[1]->age, 34);
	CHECK_MSG(!strcmp(CStrGet(p->members[2]->name), "bob"), "slot 2 %s", CStrGet(p->members[2]->name));
	{
		bool carol = false;
		for (int32_t i = 3; i < p->member_count; ++i) {
			const char *name = CStrGet(p->members[i]->name);
			CHECK_MSG(strcmp(name, "alice") && strcmp(name, "bob"), "slot %d %s", i, name);
			if (!strcmp(name, "carol"))
				carol = true;
		}
		CHECK_MSG(carol, "carol, who chatted, is not in the party");
	}
	CHECK_EQ(TwitchPartyPending(), 0);

	TCase("the difficulty and the OBS files");
	CHECK_MSG(!strncmp(g_difficulty_shown, "Difficulty 4", 12), "the New Game screen showed \"%s\"",
		g_difficulty_shown);
	CHECK(read_obs("difficulty.txt", text, sizeof(text)));
	CHECK_MSG(!strcmp(text, "Difficulty: 4"), "difficulty.txt: %s", text);
	CHECK(read_obs("target.txt", text, sizeof(text)));
	CHECK_MSG(!strncmp(text, "Target: ", 8) && text[8], "target.txt: %s", text);
	{
		char lines[5][256];
		int32_t n = TwitchControlLastPicks(lines, 5);
		bool set_up = false;
		bool prologue = false;
		for (int32_t i = 0; i < n; ++i) {
			if (strstr(lines[i], "(new game set up: Random, Travel the Trail!)"))
				set_up = true;
			if (strstr(lines[i], "(prologue skipped)"))
				prologue = true;
		}
		CHECK(set_up);
		CHECK(prologue);
	}
	TCase("");
}

static void drive_new_game(const struct control_state *st)
{
	int32_t f;

	switch (g_phase) {
		case 0:
			ControlMenu(2010);
			phase(1, 20000);
			return;

		case 1:
			if (set_up_screen(st)) {
				phase(2, 30000);
				drive_new_game(st);
				return;
			}
			if (st->screen == CONTROL_DIALOG && option(st, "No")) {
				say("no");
				return;
			}
			if (!answer(st) && !phase_timed_out("the New Game screen"))
				g_next_at = now() + 100;
			return;

		case 2:
			if (!strcmp(st->title, "Character Creation")) {
				for (int32_t i = 0; i < st->field_count; ++i) {
					if (!strncmp(st->fields[i].text, "Difficulty ", 11))
						StrCopy(g_difficulty_shown, st->fields[i].text);
					if (st->fields[i].id == 9021 && !g_seed_set) {
						ControlSetField(st, i, "12345");
						g_seed_set = true;
					}
				}
				g_next_at = now() + 50;
				return;
			}
			if (set_up_screen(st) || !g_game || !g_game->party || !option(st, "Supplies")) {
				if (!phase_timed_out("the game on screen"))
					g_next_at = now() + 100;
				return;
			}
			check_new_party();
			phase(3, 120000);
			drive_new_game(st);
			return;

		case 3:
			if (option(st, "Buy: food")) {
				say("buy food");
				phase(4, 15000);
				return;
			}
			if (!strcmp(st->title, "Wagon Train")) {
				say("close window");
				return;
			}
			if (answer(st))
				return;
			if (option_with(st, "General Store")) {
				say("general store");
				return;
			}
			if (g_moves >= 40) {
				TNote("no general store found in %d moves: the store part is skipped", g_moves);
				phase(7, 30000);
				drive_new_game(st);
				return;
			}
			{
				static const char *const WALK[] = {"turn right", "walk forward", "turn right", "turn around",
					"walk forward", "turn left", "go back"};
				for (int32_t k = 0; k < 7; ++k) {
					const char *w = WALK[(g_moves + k) % 7];
					if (option(st, w)) {
						say(w);
						return;
					}
				}
			}
			if (!phase_timed_out("a store"))
				g_next_at = now() + 300;
			return;

		case 4:
			if (strcmp(st->title, "Purchase Inventory")) {
				if (!phase_timed_out("the store window"))
					g_next_at = now() + 200;
				return;
			}
			for (f = 0; f < st->field_count && (st->fields[f].is_choice || !st->fields[f].label[0]); ++f)
				;
			CHECK(f < st->field_count);
			if (f == st->field_count) {
				g_done = true;
				return;
			}
			{
				char line[160];
				const char *name = st->fields[f].label;
				if (!strncmp(name, "Quantity: ", 10))
					name += 10;
				StrCopy(g_store_field, st->fields[f].label);
				StrCopy(g_store_amount, "1");
				g_cash_before = g_game->party->cash;
				StrPrintf(line, "%s %s", name, g_store_amount);
				say(line);
			}
			phase(5, 15000);
			return;

		case 5:
			if (g_moves == 0 || g_moves == 2) {
				f = field(st, g_store_field);
				CHECK_MSG(f >= 0 && !strcmp(st->fields[f].text, g_store_amount), "%s = \"%s\"", g_store_field,
					f >= 0 ? st->fields[f].text : "(gone)");
			}
			if (g_moves == 0) {
				say("show items clothing");
				return;
			}
			if (g_moves == 1) {
				int32_t list = field(st, "Show items");
				CHECK_MSG(list >= 0 || st->field_count, "no list");
				for (int32_t i = 0; i < st->field_count; ++i)
					if (st->fields[i].is_choice)
						CHECK_MSG(!strcmp(st->fields[i].text, "clothing"), "the list shows %s", st->fields[i].text);
				say("show items food");
				return;
			}
			say("purchase");
			phase(6, 15000);
			return;

		case 6:
			if (!strcmp(st->title, "Purchase Inventory")) {
				if (!phase_timed_out("the purchase"))
					g_next_at = now() + 200;
				return;
			}
			if (answer(st))
				return;
			CHECK_MSG(g_game->party->cash < g_cash_before, "cash %.2f, before %.2f", g_game->party->cash,
				g_cash_before);
			phase(7, 30000);
			drive_new_game(st);
			return;

		case 7:
			if (answer(st))
				return;
			if (!option(st, "Back to travel")) {
				if (option(st, "Supplies") && g_moves < 3)
					say("supplies");
				else if (!phase_timed_out("the Supplies screen"))
					g_next_at = now() + 200;
				return;
			}
			phase(8, 30000);
			say("rations bare bones");
			return;

		case 8:
			CHECK_EQ(g_game->party->ration_level, RATIONS_BARE_BONES);
			phase(9, 30000);
			say("12 hours");
			return;

		case 9:
			CHECK_EQ(g_game->travel_pace, 2);
			phase(10, 30000);
			say("rations filling");
			return;

		case 10:
			CHECK_EQ(g_game->party->ration_level, RATIONS_FILLING);
			phase(11, 30000);
			say("back to travel");
			return;

		case 11:
			CHECK(!option(st, "Back to travel"));
			g_done = true;
			return;
	}
}

TEST_GAME(scn_twitchplays, new_game)
{
	char was[16];

	IniGetString("twitch", "difficulty", "1", was, sizeof(was));
	IniSetString("twitch", "difficulty", "4");
	IniSetString("twitch", "obs_dir", "");
	g_difficulty_shown[0] = 0;
	g_seed_set = false;
	if (!connect()) {
		disconnect();
		IniSetString("twitch", "difficulty", was);
		return;
	}
	send_line("TESTREDEEM", "alice", "34");
	send_line("TESTREDEEM", "bob", "hello");
	send_line("TESTCHAT", "carol", "hi everyone");
	TestPump(1500);
	CHECK_EQ(TwitchPartyPending(), 2);
	run_driver(drive_new_game, 300000);
	disconnect();
	IniSetString("twitch", "difficulty", was);
}

static int32_t g_river_node;
static int32_t g_date_before;
static struct test_state g_river_state;

static void drive_river(const struct control_state *st)
{
	struct test_state s;

	switch (g_phase) {
		case 0:
			CHECK_MSG(option(st, "Check the river conditions") != NULL, "not at a river: \"%s\"", st->title);
			if (!option(st, "Check the river conditions")) {
				g_done = true;
				return;
			}
			say("check the river conditions");
			phase(1, 15000);
			return;

		case 1:
			{
				bool shown = false;
				for (int32_t i = 0; i < st->text_count; ++i)
					if (strstr(st->text[i], "River Conditions") || strstr(st->text[i], "river"))
						shown = true;
				for (int32_t i = 0; i < st->info_count; ++i)
					if (strstr(st->info[i], "river") || strstr(st->info[i], "River"))
						shown = true;
				CHECK_MSG(shown, "no river conditions on screen");
			}
			TestCapture(&g_river_state);
			g_date_before = g_river_state.date_key;
			say("wait");
			phase(2, 15000);
			return;

		case 2:
			if (strcmp(st->title, "Resting")) {
				if (!answer(st) && !phase_timed_out("the Resting dialog"))
					g_next_at = now() + 200;
				return;
			}
			say("rest for 2");
			phase(3, 15000);
			return;

		case 3:
			{
				int32_t f = field(st, "Rest for");
				CHECK_MSG(f >= 0 && !strcmp(st->fields[f].text, "2"), "Rest for = %s", f >= 0 ? st->fields[f].text :
					"(none)");
			}
			say("ok");
			phase(4, 60000);
			return;

		case 4:
			if (!option(st, "Check the river conditions")) {
				if (!answer(st) && !phase_timed_out("back at the river"))
					g_next_at = now() + 200;
				return;
			}
			TestCapture(&s);
			CHECK_MSG(s.date_key - g_date_before >= 2, "rested %d day(s)", s.date_key - g_date_before);
			{
				static const char *const WAYS[][2] = {{"Ford the river", "ford"}, {"Take the ferry", "ferry"},
					{"Cross on the ice", "ice"}, {"Caulk the wagons", "caulk"}};
				for (int32_t i = 0; i < 4; ++i)
					if (option(st, WAYS[i][0])) {
						say(WAYS[i][1]);
						phase(5, 120000);
						return;
					}
			}
			CHECK_MSG(0, "no way across offered");
			g_done = true;
			return;

		case 5:
			if (answer(st))
				return;
			if (option(st, "Check the river conditions") || !g_game || !g_game->cur_node ||
				g_game->cur_node->node_id == g_river_node) {
				if (!phase_timed_out("the crossing"))
					g_next_at = now() + 300;
				return;
			}
			TNote("across: node %d -> %d", g_river_node, g_game->cur_node->node_id);
			g_done = true;
			return;
	}
}

TEST_GAME(scn_twitchplays, river)
{
	if (!TestLoad("v12_node_328")) {
		CHECK_MSG(0, "no fixture v12_node_328 (a river: the Elkhorn)");
		return;
	}
	TestPump(1500);
	g_river_node = g_game && g_game->cur_node ? g_game->cur_node->node_id : -1;
	if (!connect()) {
		disconnect();
		return;
	}
	run_driver(drive_river, 240000);
	disconnect();
}

TEST_GAME(scn_twitchplays, difficulty)
{
	char text[64];
	char seed[16];
	char was[16];

	IniGetString("twitch", "difficulty", "1", was, sizeof(was));
	IniSetString("twitch", "obs_dir", "");

	TCase("a score page raises the difficulty once per game");
	IniSetString("twitch", "difficulty", "2");
	IniSetString("twitch", "last_scored_seed", "");
	TwitchControlScoreReached();
	IniGetString("twitch", "difficulty", "", text, sizeof(text));
	CHECK_MSG(!strcmp(text, "3"), "difficulty=%s", text);
	StrPrintf(seed, "%u", RngSeed());
	IniGetString("twitch", "last_scored_seed", "", text, sizeof(text));
	CHECK_MSG(!strcmp(text, seed), "last_scored_seed=%s, seed %s", text, seed);
	CHECK(read_obs("difficulty.txt", text, sizeof(text)));
	CHECK_MSG(!strcmp(text, "Difficulty: 3"), "difficulty.txt: %s", text);
	TwitchControlScoreReached();
	IniGetString("twitch", "difficulty", "", text, sizeof(text));
	CHECK_MSG(!strcmp(text, "3"), "difficulty=%s", text);
	{
		char lines[5][256];
		CHECK(TwitchControlLastPicks(lines, 5) >= 1 && strstr(lines[0], "difficulty 3"));
	}

	TCase("the New Game screen's difficulty: 1 to 10");
	CHECK_EQ(TwitchControlNewGameDifficulty(), 3);
	IniSetString("twitch", "difficulty", "17");
	CHECK_EQ(TwitchControlNewGameDifficulty(), 10);
	CHECK(read_obs("difficulty.txt", text, sizeof(text)));
	CHECK_MSG(!strcmp(text, "Difficulty: 17"), "difficulty.txt: %s", text);
	IniSetString("twitch", "difficulty", "0");
	CHECK_EQ(TwitchControlNewGameDifficulty(), 1);
	IniSetString("twitch", "difficulty", "nonsense");
	CHECK_EQ(TwitchControlNewGameDifficulty(), 1);

	TCase("the options window keeps its place");
	if (connect()) {
		HWND w = FindWindowA("OTIITwitchOptions", NULL);
		RECT r;
		CHECK(w != NULL);
		if (w) {
			SetWindowPos(w, NULL, 120, 130, 480, 420, SWP_NOZORDER | SWP_NOACTIVATE);
			TwitchControlToggleWindow();
			CHECK(FindWindowA("OTIITwitchOptions", NULL) == NULL);
			CHECK_EQ(IniGetInt("twitch", "options_x", 0), 120);
			CHECK_EQ(IniGetInt("twitch", "options_y", 0), 130);
			CHECK_EQ(IniGetInt("twitch", "options_width", 0), 480);
			CHECK_EQ(IniGetInt("twitch", "options_height", 0), 420);
			TwitchControlToggleWindow();
			w = FindWindowA("OTIITwitchOptions", NULL);
			CHECK(w != NULL);
			if (w && GetWindowRect(w, &r)) {
				CHECK_EQ(r.left, 120);
				CHECK_EQ(r.top, 130);
				CHECK_EQ(r.right - r.left, 480);
			}
			TestPump(1200);
			if (w) {
				char shown[4096];
				HWND edit = FindWindowExA(w, NULL, "EDIT", NULL);
				GetWindowTextA(edit, shown, sizeof(shown));
				CHECK_MSG(!strncmp(shown, "Type an option's words", 22) && strstr(shown, "Map"), "%s", shown);
			}
		}
	}
	disconnect();
	TCase("");
	IniSetString("twitch", "difficulty", was);
}

const struct test_reg TESTS_scn_twitchplays[] = {
	TEST_GAME_ENTRY(scn_twitchplays, picks),
	TEST_GAME_ENTRY(scn_twitchplays, new_game),
	TEST_GAME_ENTRY(scn_twitchplays, river),
	TEST_GAME_ENTRY(scn_twitchplays, difficulty),
	TEST_END
};

#else

const struct test_reg TESTS_scn_twitchplays[] = {
	TEST_END
};

#endif

#endif
