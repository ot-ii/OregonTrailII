#include "testframework.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <string.h>

#include "../src/game/game.h"
#include "../src/trail/hillrules.h"
#include "../src/game/invlist.h"
#include "../src/screens/mainui.h"
#include "../src/game/msgcontroller.h"
#include "../src/trail/node.h"
#include "../src/game/party.h"
#include "../src/base/rng.h"
#include "../src/base/str.h"
#include "../src/game/weather.h"
#include "testsupport.h"

struct method {
	int32_t code;
	bool uphill;
	const char *name;
	int32_t chance[6];
};

static const struct method METHODS[] = {
	{NODE_CMD_GO_UP, true, "go up", {0, 97, 75, 50, 0, 0}},
	{NODE_CMD_DOUBLE_TEAM, true, "double-team", {0, 97, 92, 75, 50, 0}},
	{NODE_CMD_ROPES_AND_CHAINS, true, "ropes/chains up", {0, 75, 75, 97, 92, 85}},
	{NODE_CMD_GO_DOWN, false, "go down", {0, 97, 75, 50, 0, 0}},
	{NODE_CMD_LOCK_WHEELS, false, "lock the wheels", {0, 97, 92, 75, 50, 0}},
	{NODE_CMD_ANCHOR_WAGON, false, "anchor the wagon", {0, 97, 92, 87, 75, 50}},
	{NODE_CMD_ROPES_AND_CHAINS, false, "ropes/chains down", {0, 75, 75, 75, 95, 90}},
};
#define METHOD_COUNT ((int32_t) (sizeof(METHODS) / sizeof(METHODS[0])))

static FILE *open_table(void)
{
	char path[512];

	TestExeBase(path, sizeof(path));
	if (!path[0])
		return NULL;
	StrCat(path, "_hill.tsv");
	return fopen(path, "w");
}

TEST(scn_hill, chance_table)
{
	FILE *table = open_table();

	if (table)
		fprintf(table, "method\tsteepness\tchecked\tstrained\tmud_or_snow\tconestoga\tchance\n");
	for (int32_t m = 0; m < METHOD_COUNT; ++m)
		for (int32_t st = 0; st <= 6; ++st)
			for (int32_t mods = 0; mods < 16; ++mods) {
				struct hill_situation s;
				int32_t want = st >= 1 && st <= 5 ? METHODS[m].chance[st] : 0, got;
				s.method = METHODS[m].code;
				s.uphill = METHODS[m].uphill;
				s.steepness = st;
				s.conditions_checked = (mods & 1) != 0;
				s.strained = (mods & 2) != 0;
				s.muddy_or_snow = (mods & 4) != 0;
				s.conestoga = (mods & 8) != 0;
				want -= (s.conditions_checked ? 0 : 10) + 25 * s.strained + 25 * s.muddy_or_snow + 15 * s.conestoga;
				got = HillSuccessChance(&s);
				TCase("%s steepness %d mods %x", METHODS[m].name, st, mods);
				CHECK_EQ(got, want);
				if (table && st >= 1 && st <= 5)
					fprintf(table, "%s\t%d\t%d\t%d\t%d\t%d\t%d\n", METHODS[m].name, st, s.conditions_checked,
						s.strained, s.muddy_or_snow, s.conestoga, got);
			}
	if (table)
		fclose(table);
}

TEST_GAME(scn_hill, real_rolls)
{
	int32_t n = TStatRuns() / 4;

	CHECK(TestSave("hill_base"));
	for (int32_t m = 0; m < METHOD_COUNT; ++m)
		for (int32_t st = 1; st <= 5; ++st) {
			int32_t ok = 0, chance, saved;
			struct hill_situation s;
			TCase("%s steepness %d", METHODS[m].name, st);
			TestLoad("hill_base");
			RngSetSeed(TSeed() + 100 * (uint32_t) m + (uint32_t) st);
			saved = g_game->cur_node->steepness;
			g_game->cur_node->steepness = st;
			g_conditions_checked = true;
			g_game->party->pull_mode = PULL_TEAM_EASY;
			s.method = METHODS[m].code;
			s.uphill = METHODS[m].uphill;
			s.steepness = st;
			s.conditions_checked = true;
			s.strained = false;
			s.muddy_or_snow = MainUiGetCatFlag(CAT_MUDDY_TRAIL) || g_game->weather->snow_depth;
			s.conestoga = InvListFindByType(&g_game->party->wagons, g_item_conestoga) != NULL;
			chance = HillSuccessChance(&s);
			for (int32_t i = 0; i < n; ++i)
				if (METHODS[m].uphill ? EventRollAilment2(METHODS[m].code, g_game->cur_node_id) :
						EventRollAilment(METHODS[m].code, g_game->cur_node_id))
					++ok;
			CHECK_RATE(ok, n, (chance < 0 ? 0 : chance > 100 ? 100 : chance) / 100.0);
			g_game->cur_node->steepness = saved;
		}
	TestLoad("hill_base");
}

TEST_GAME(scn_hill, impossible_steepness)
{
	static const int32_t bad[] = {0, 6, -1, 100};

	TestSave("hill_base");
	for (int32_t i = 0; i < 4; ++i) {
		struct test_state after;
		char when[64];
		TCase("steepness %d", bad[i]);
		TestLoad("hill_base");
		g_game->cur_node->steepness = bad[i];
		for (int32_t k = 0; k < 5; ++k) {
			EventRollAilment2(NODE_CMD_GO_UP, g_game->cur_node_id);
			EventRollAilment(NODE_CMD_GO_DOWN, g_game->cur_node_id);
		}
		TestPump(200);
		TestCapture(&after);
		StrPrintf(when, "steepness %d", bad[i]);
		TestCheckInvariants(&after, when);
	}
	TestLoad("hill_base");
}

const struct test_reg TESTS_scn_hill[] = {
	TEST_ENTRY(scn_hill, chance_table),
	TEST_GAME_ENTRY(scn_hill, real_rolls),
	TEST_GAME_ENTRY(scn_hill, impossible_steepness),
	TEST_END
};

#endif
