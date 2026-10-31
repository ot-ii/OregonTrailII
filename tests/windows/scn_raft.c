#include "../testframework.h"

#ifdef OTII_TEST

#include <string.h>

#include <windows.h>

#include "../../src/features/autopilot.h"
#include "../../src/app/evthandler.h"
#include "../../src/ui/framewin.h"
#include "../../src/game/game.h"
#include "../../src/base/geom.h"
#include "../../src/data/gfxdev.h"
#include "../../src/game/member.h"
#include "../../src/base/object.h"
#include "../../src/draw/palette.h"
#include "../../src/game/party.h"
#include "../../src/trail/fordwagon.h"
#include "../../src/trail/raftframe.h"
#include "../../src/trail/tilemap.h"
#include "../../src/ui/view.h"
#include "../../src/draw/viewsurface.h"
#include "../../src/ui/window.h"
#include "../testsupport.h"

struct raft_run {
	int32_t map;
	int32_t steps, finished, progress_bad, last_progress;
};

static struct raft_frame *raft_scene(int32_t map)
{
	struct rect whole = RectMake(0, 0, 442, 640), area = RectMake(20, 20, 420, 420);
	struct view *root = ViewCreateId(-2, &whole);
	struct raft_frame *raft = RaftFrameCreate(-2, &area, g_palette, g_data_lib, map, 1100, 0, 0);
	struct view *old;

	VIEW_OPS(root)->AddChild(root, &raft->base);
	old = WindowSetRootView(&g_parent_win->base, root);
	if (old)
		ObjectDelete(old);
	return raft;
}

static void run_raft(void *arg)
{
	struct raft_run *r = arg;
	struct raft_frame *raft = raft_scene(r->map);
	int32_t i;

	r->steps = r->finished = r->progress_bad = 0;
	for (i = 0; i < 4000 && !raft->finished; ++i) {
		int32_t p;
		EVT_HANDLER_OPS(raft)->OnIdleSpare((struct evt_handler *) raft, NULL);
		++r->steps;
		p = RAFT_FRAME_OPS(raft)->GetProgress(raft);
		if (p < 0 || p > 100)
			++r->progress_bad;
		r->last_progress = p;
		if ((i & 63) == 0)
			TestPump(1);
	}
	r->finished = raft->finished;
}

TEST_GAME(scn_raft, every_river_map)
{
	CHECK(TestSave("raft_base"));
	for (int32_t map = 1000; map <= 1003; ++map) {
		struct raft_run r;
		struct test_state s;
		memset(&r, 0, sizeof(r));
		r.map = map;
		r.last_progress = -1;
		TCase("raft map %d", map);
		TestLoad("raft_base");
		if (TGuarded(run_raft, &r)) {
			CHECK_EQ(r.progress_bad, 0);
			TNote("map %d: %d steps, %s, progress %d", map, r.steps, r.finished ? "finished" : "still going",
				r.last_progress);
		}
		TestPump(300);
		TestLoad("raft_base");
		TestCapture(&s);
		TestCheckInvariants(&s, "after rafting");
	}
	TestLoad("raft_base");
}

struct steered {
	int32_t map;
	int32_t finished, result, seconds, steps;
};

static void run_steered(void *arg)
{
	struct steered *r = arg;
	struct raft_frame *raft = raft_scene(r->map);
	DWORD start = GetTickCount();

	r->steps = 0;
	while (!raft->finished && GetTickCount() - start < 180000) {
		if (raft->base.owner_window && raft->tilemap)
			VIEW_SURFACE_OPS(raft->tilemap)->RenderPipeline((struct view_surface *) raft->tilemap,
				raft->base.owner_window->windc, true);
		++r->steps;
		TestPump(1);
	}
	r->finished = raft->finished;
	r->result = raft->result;
	r->seconds = (int32_t) ((GetTickCount() - start) / 1000);
}

TEST_GAME(scn_raft, autopilot_steers)
{
	struct steered r;

	CHECK(TestSave("raft_base"));
	TestLoad("raft_base");
	AutoPilotTestSteerRafts(true);
	for (int32_t map = 1000; map <= 1003; ++map) {
		memset(&r, 0, sizeof(r));
		r.map = map;
		if (TGuarded(run_steered, &r)) {
			TNote("map %d: %s after %d s (%d steps), damage %d", r.map, r.finished ? "reached the bank" : "still going",
				r.seconds, r.steps, r.result);
			CHECK_MSG(r.finished, "map %d: the raft did not reach the bank in 3 minutes", map);
			CHECK_MSG(r.result < 100, "map %d: the raft was wrecked (damage %d)", map, r.result);
		}
	}
	AutoPilotTestSteerRafts(false);
	TestPump(300);
	TestLoad("raft_base");
}

struct edges {
	int32_t map;
	int32_t start_h, start_v, max_h, max_v;
	int32_t width, height;
	bool out_left, out_down, along_bank;
};

static void run_edges(void *arg)
{
	struct edges *r = arg;
	struct raft_frame *raft = raft_scene(r->map);
	struct tile_map *map = raft->tilemap;
	struct spr_obj *obj = &raft->wagon->base.base;

	r->width = (int32_t) raft->grid_width;
	r->height = (int32_t) raft->grid_height;
	r->start_h = obj->region1.p2.h;
	r->start_v = obj->region1.p2.v;

	TILE_MAP_OPS(map)->ScrollTo(map, 100000, 100000);
	r->max_h = map->scroll_pos.h;
	r->max_v = map->scroll_pos.v;

	SPR_OBJ_OPS(obj)->SetAnchor(obj, 8 - map->scroll_pos.h, 16 * (r->height - 1) + 8 - map->scroll_pos.v);
	r->out_left = FordWagonCanMove(raft->wagon, -48, 0);
	r->out_down = FordWagonCanMove(raft->wagon, 0, 48);
	r->along_bank = FordWagonCanMove(raft->wagon, 0, -8);
}

TEST_GAME(scn_raft, map_edges)
{
	struct edges r;

	CHECK(TestSave("raft_base"));
	TestLoad("raft_base");
	for (int32_t map = 1000; map <= 1003; map++) {
		memset(&r, 0, sizeof(r));
		r.map = map;
		TCase("raft map %d", map);
		if (TGuarded(run_edges, &r)) {
			CHECK_MSG(r.start_h >= 0 && r.start_h < 400 && r.start_v >= 0 && r.start_v < 400,
				"map %d: the raft starts outside the view at %d,%d", map, r.start_h, r.start_v);
			CHECK_EQ(r.max_h, 16 * (r.width - 25));
			CHECK_EQ(r.max_v, 16 * (r.height - 25));
			if (map == 1000)
				CHECK(r.along_bank);
			CHECK_MSG(!r.out_left, "map %d: the raft can leave through the left edge", map);
			CHECK_MSG(!r.out_down, "map %d: the raft can leave through the bottom edge", map);
		}
	}
	TestPump(300);
	TestLoad("raft_base");
}

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

static struct member *leader(void)
{
	return g_game && g_game->party ? PartyGetMember(g_game->party, 0) : NULL;
}

TEST_GAME(scn_raft, autopilot_dalles)
{
	int32_t runs;
	bool done = false;
	DWORD start, quiet = 0;
	struct member *lead;

	CHECK(TestSave("raft_base"));
	CHECK_MSG(TestLoad("raft_dalles"), "no fixtures\\raft_dalles.ot2 next to the test build");
	runs = AutoPilotTestRaftRuns();
	AutoPilotStart();
	start = GetTickCount();
	while (!done && GetTickCount() - start < 600000) {
		run_game(250);
		lead = leader();
		if (lead && lead->state == MEMBER_DEAD)
			break;
		if (AutoPilotTestRaftRuns() > runs && !AutoPilotTestRaftSteering()) {
			if (!quiet)
				quiet = GetTickCount();
			else if (GetTickCount() - quiet > 20000)
				done = true;

		} else {
			quiet = 0;
		}
	}
	AutoPilotStop();
	lead = leader();
	TNote("%d raft trip(s) in %d s, leader %s", AutoPilotTestRaftRuns() - runs, (int32_t) ((GetTickCount() - start) / 1000),
		!lead ? "missing" : lead->state == MEMBER_DEAD ? "dead" : "alive");
	CHECK_MSG(AutoPilotTestRaftRuns() > runs, "the autopilot never took the raft");
	CHECK_MSG(lead && lead->state != MEMBER_DEAD, "the leader drowned");
	TestPump(300);
	TestLoad("raft_base");
}

const struct test_reg TESTS_scn_raft[] = {
	TEST_GAME_ENTRY(scn_raft, every_river_map),
	TEST_GAME_ENTRY(scn_raft, autopilot_steers),
	TEST_GAME_ENTRY(scn_raft, map_edges),
	TEST_GAME_ENTRY(scn_raft, autopilot_dalles),
	TEST_END
};

#endif
