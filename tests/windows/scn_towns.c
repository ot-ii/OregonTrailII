#include "../testframework.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <string.h>

#include <windows.h>

#include "../../src/app/app.h"
#include "../../src/app/docmanager.h"
#include "../../src/app/event.h"
#include "../../src/app/evthandler.h"
#include "../../src/app/otiiapp.h"
#include "../../src/data/dataversion.h"
#include "../../src/data/docfile.h"
#include "../../src/dialogs/quickstartdlg.h"
#include "../../src/features/autopilot.h"
#include "../../src/features/controlstate.h"
#include "../../src/game/game.h"
#include "../../src/game/invlist.h"
#include "../../src/game/party.h"
#include "../../src/screens/sceneframe.h"
#include "../../src/trail/node.h"
#include "../../src/ui/framewin.h"
#include "../testsupport.h"

#define STATE_AT_NODE 2
#define STATE_SCENE 4

static const int32_t START_NODES[] = {1, 267, 289, 312, 313, 315};

static int32_t g_want_node;
static int32_t g_want_oxen;
static int32_t g_rolls;
static char g_titles[512];
static int32_t g_watch_seen;
static int32_t g_watch_pick;
static char g_watch_text[1024];

static void run_game(int32_t ms)
{
	DWORD until = GetTickCount() + (DWORD) ms;
	MSG msg;

	do {
		if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);

		} else {
			RunIdleProcessing(3);
		}
	} while ((LONG) (GetTickCount() - until) < 0);
}

static int32_t app_state(void)
{
	return ((struct otii_app *) g_app)->app_state;
}

static void describe(const char *when)
{
	static struct control_state st;

	if (!ControlGetState(&st)) {
		TNote("%s: no state", when);
		return;
	}
	TNote("%s: '%s' at '%s', node %d, state %d, scene %d, %d miles, %d days", when, st.title, st.location,
		g_game ? g_game->cur_node_id : -1, app_state(), g_scene_cur, g_game ? g_game->total_miles : -1,
		g_game ? g_game->days_traveled : -1);
}

static bool click_label(const char *label)
{
	static struct control_state st;

	if (!ControlGetState(&st))
		return false;
	for (int32_t i = 0; i < st.option_count; ++i)
		if (!strcmp(st.options[i].label, label)) {
			ControlClick(&st, st.options[i].index);
			return true;
		}
	return false;
}

static void CALLBACK drive_quick_start(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
	static struct control_state st;
	struct quick_start_choices *c = ((struct otii_app *) g_app)->new_game_choices;

	(void) hwnd;
	(void) msg;
	(void) id;
	(void) time;
	if (!ControlGetState(&st) || st.screen != CONTROL_DIALOG)
		return;
	if (!strstr(g_titles, st.title) && strlen(g_titles) + strlen(st.title) + 2 < sizeof(g_titles)) {
		if (g_titles[0])
			strcat_s(g_titles, sizeof(g_titles), "|");
		strcat_s(g_titles, sizeof(g_titles), st.title);
	}
	if (!c || !c->party)
		return;
	bool wanted = c->start_node == g_want_node && (!g_want_oxen || c->oxen == g_want_oxen);
	if (click_label(wanted || g_rolls > 2000 ? "Hit the Trail!" : "Change"))
		++g_rolls;
}

static bool quick_start(int32_t node, int32_t oxen)
{
	struct otii_app *app = (struct otii_app *) g_app;
	struct doc_file *doc = DocManagerGetSingleDoc(APP_OPS(g_app)->GetDocManager(g_app));

	if (doc)
		DocFileSetDirtyV(doc, false);
	OtiiAppHandleDocCommand(app, CMD_END_GAME, g_parent_win->base.hwnd);
	run_game(1000);
	for (int32_t wait = 0; wait < 20 && (g_game || app_state()); ++wait)
		run_game(500);
	if (g_game || app_state())
		describe("the game did not end");
	g_want_node = node;
	g_want_oxen = oxen;
	g_rolls = 0;
	g_titles[0] = 0;
	TestDismisser(false);
	UINT_PTR timer = SetTimer(NULL, 0, 50, drive_quick_start);
	bool clicked = click_label("Quick Start");
	if (!clicked)
		describe("no Quick Start button");
	CHECK_MSG(clicked, "no Quick Start button on the title screen");
	DWORD t0 = GetTickCount();
	while (clicked && GetTickCount() - t0 < 120000 && !(g_game && app_state() == STATE_AT_NODE))
		run_game(100);
	KillTimer(NULL, timer);
	TestDismisser(true);
	TNote("Quick Start button: dialogs '%s', %d rolls", g_titles, g_rolls);
	CHECK_MSG(!strcmp(g_titles, "Prologue"), "dialogs: %s", g_titles);
	bool ok = g_game && g_game->cur_node_id == node && app_state() == STATE_AT_NODE;
	CHECK_MSG(ok, "the game is not at node %d's menu", node);
	run_game(500);
	return ok;
}

static void CALLBACK watch(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
	static struct control_state st;
	static int32_t depth;

	(void) hwnd;
	(void) msg;
	(void) id;
	(void) time;
	if (depth || !ControlGetState(&st) || st.screen != CONTROL_DIALOG || !st.option_count)
		return;
	++depth;
	++g_watch_seen;
	if (strlen(g_watch_text) + 200 < sizeof(g_watch_text)) {
		strcat_s(g_watch_text, sizeof(g_watch_text), st.title);
		strcat_s(g_watch_text, sizeof(g_watch_text), ": ");
		strncat_s(g_watch_text, sizeof(g_watch_text), st.text_count ? st.text[0] : "", 150);
		strcat_s(g_watch_text, sizeof(g_watch_text), "|");
	}
	int32_t pick = g_watch_pick >= 0 && g_watch_pick < st.option_count ? g_watch_pick : st.option_count - 1;
	g_watch_pick = -1;
	ControlClick(&st, st.options[pick].index);
	--depth;
}

static UINT_PTR watch_start(int32_t pick)
{
	g_watch_seen = 0;
	g_watch_pick = pick;
	g_watch_text[0] = 0;
	TestDismisser(false);
	return SetTimer(NULL, 0, 300, watch);
}

static void watch_stop(UINT_PTR timer)
{
	KillTimer(NULL, timer);
	TestDismisser(true);
}

static bool walk_and_out(int32_t pick)
{
	UINT_PTR timer = watch_start(pick);
	bool offered = false;

	click_label("Go into town");
	run_game(2000);
	CHECK_EQ(app_state(), STATE_SCENE);
	for (int32_t step = 0; step < 30 && app_state() == STATE_SCENE; ++step) {
		if (!click_label("Walk forward") && !click_label("Walk on") && !click_label("Turn right"))
			click_label("Go back");
		run_game(1500);
		if (strstr(g_watch_text, "package"))
			offered = true;
	}
	watch_stop(timer);
	CHECK_EQ(app_state(), STATE_AT_NODE);
	return offered;
}

static volatile LONG g_beat;
static volatile LONG g_watchdog_on;

static void watchdog(void *arg)
{
	LONG last = -1;
	int32_t still = 0;

	(void) arg;
	while (g_watchdog_on) {
		Sleep(1000);
		LONG b = g_beat;
		still = b == last ? still + 1 : 0;
		last = b;
		if (still >= 90) {
			TCheck(false, "the game keeps running", __FILE__, __LINE__, "no message handled for 90 s");
			TestExitProcess(99);
		}
	}
}

static void CALLBACK beat(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
	(void) hwnd;
	(void) msg;
	(void) id;
	(void) time;
	InterlockedIncrement(&g_beat);
}

struct guard {
	UINT_PTR timer;
	struct test_thread *dog;
};

static void guard_start(struct guard *g)
{
	g_watchdog_on = 1;
	g->timer = SetTimer(NULL, 0, 200, beat);
	g->dog = TestThreadStart(watchdog, NULL);
}

static void guard_stop(struct guard *g)
{
	g_watchdog_on = 0;
	TestThreadJoin(&g->dog);
	KillTimer(NULL, g->timer);
}

static bool travel_to(int32_t node, int32_t ms)
{
	DWORD t0 = GetTickCount();

	AutoPilotStart();
	while (g_game && GetTickCount() - t0 < (DWORD) ms && !(g_game->cur_node_id == node && app_state() == STATE_AT_NODE))
		run_game(100);
	AutoPilotStop();
	run_game(1000);
	return g_game && g_game->cur_node_id == node && app_state() == STATE_AT_NODE;
}

static bool leave(void)
{
	return click_label("Continue") || click_label("Ready to go");
}

TEST_GAME(scn_towns, quick_start_no_package)
{
	if (!DataIs13()) {
		TNote("1.2 data: no Quick Start");
		return;
	}
	if (quick_start(315, 0)) {
		CHECK(g_game->train_wagons != -1);
		CHECK_MSG(!walk_and_out(0), "the package was offered: %s", g_watch_text);
	}
}

TEST_GAME(scn_towns, quick_start_no_package_then_leave)
{
	struct guard guard;

	if (!DataIs13()) {
		TNote("1.2 data: no Quick Start");
		return;
	}
	guard_start(&guard);
	for (int32_t k = 0; k < (int32_t) (sizeof(START_NODES) / sizeof(START_NODES[0])); ++k) {
		int32_t node = START_NODES[k];
		TCase("start node %d", node);
		if (!quick_start(node, 6))
			continue;
		CHECK_MSG(!walk_and_out(0), "node %d: the package was offered", node);
		TestSetItem("flour", 3000);
		CHECK(!PartyHasDraftAnimals(g_game->party));
		UINT_PTR timer = watch_start(-1);
		CHECK(leave());
		run_game(15000);
		watch_stop(timer);
		describe("after Continue");
		CHECK_MSG(strstr(g_watch_text, "too heavy") != NULL, "no message: '%s'", g_watch_text);
		CHECK_EQ(g_game->cur_node_id, node);
		CHECK_EQ(g_game->days_traveled, 0);
		CHECK_EQ(app_state(), STATE_AT_NODE);
		TestSetItem("flour", 0);
		InvListPurgeEmpty(&g_game->party->inventory);
		if (PartyHasDraftAnimals(g_game->party)) {
			timer = watch_start(-1);
			CHECK(leave());
			run_game(8000);
			watch_stop(timer);
			CHECK_MSG(g_game->cur_node_id != node || g_game->total_miles > 0, "still at node %d", node);
		}
	}
	TCase("");
	guard_stop(&guard);
}

TEST_GAME(scn_towns, quick_start_next_town)
{
	struct guard guard;

	if (!DataIs13()) {
		TNote("1.2 data: no Quick Start");
		return;
	}
	guard_start(&guard);
	if (quick_start(315, 0)) {
		CHECK(!walk_and_out(-1));
		CHECK(travel_to(325, 180000));
		describe("at Florence");
		UINT_PTR timer = watch_start(-1);
		click_label("Go into town");
		run_game(3000);
		CHECK_EQ(app_state(), STATE_SCENE);
		CHECK_EQ(g_scene_cur, LNodeGetLandmarkConv((struct lnode *) g_game->cur_node, g_game));
		click_label("Walk forward");
		run_game(3000);
		watch_stop(timer);
		describe("after a walk");
	}
	guard_stop(&guard);
}

static void load_into_fresh_session(const char *name, int32_t stale_scene)
{
	g_scene_cur = stale_scene;
	g_scene_prev = 0;
	g_scene_view = 0;
	UINT_PTR timer = watch_start(-1);
	CHECK(TestLoad(name));
	run_game(1500);
	watch_stop(timer);
}

TEST_GAME(scn_towns, force_save_in_town)
{
	static struct control_state st;

	if (DataIs13()) {
		if (!quick_start(START_NODES[TSeed() % 6], 0))
			return;
		TNote("starting town: node %d", g_game->cur_node_id);

	} else {
		CHECK(TestLoad("v12_node_325"));
		run_game(1000);
	}
	int32_t node = g_game->cur_node_id;
	UINT_PTR timer = watch_start(-1);
	click_label("Go into town");
	run_game(2000);
	CHECK_EQ(app_state(), STATE_SCENE);
	int32_t entrance = g_scene_cur;
	CHECK(entrance > 0);
	click_label("Turn left");
	run_game(1000);
	watch_stop(timer);
	CHECK_EQ(app_state(), STATE_SCENE);
	CHECK(TestSave("town_save"));
	for (int32_t stale = 0; stale < 2; ++stale) {
		TCase(stale ? "after another town's walk" : "a new session");
		load_into_fresh_session("town_save", stale ? (entrance == 500 ? 470 : 500) : 0);
		describe("loaded");
		CHECK_EQ(g_game->cur_node_id, node);
		CHECK_EQ(app_state(), STATE_SCENE);
		CHECK_EQ(g_scene_cur, entrance);
		CHECK(ControlGetState(&st) && st.screen == CONTROL_TOWN);
	}
	TCase("");
}

const struct test_reg TESTS_scn_towns[] = {
	TEST_GAME_ENTRY(scn_towns, quick_start_no_package),
	TEST_GAME_ENTRY(scn_towns, quick_start_no_package_then_leave),
	TEST_GAME_ENTRY(scn_towns, quick_start_next_town),
	TEST_GAME_ENTRY(scn_towns, force_save_in_town),
	TEST_END
};

#endif
