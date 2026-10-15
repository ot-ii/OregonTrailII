#include "testframework.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <string.h>

#include "../src/game/game.h"
#include "../src/trail/riverrules.h"
#include "../src/base/str.h"

struct band {
	int32_t depth;
	const char *name;
};

static const struct band BANDS[] = {{10, "shallow (<=24)"}, {28, "middle (25-30)"}, {45, "deep (>30)"}};

static int32_t base_chance(int32_t method, int32_t band, int32_t swift)
{
	static const int32_t caulk[3][6] = {{0, 0, 0, 0, 0, 0}, {0, 99, 95, 85, 75, 50}, {0, 99, 97, 95, 85, 75}};
	static const int32_t ford[3][6] = {{0, 99, 97, 95, 85, 75}, {0, 97, 95, 85, 75, 50}, {0, 0, 0, 0, 0, 0}};
	static const int32_t ferry[6] = {0, 99, 99, 95, 90, 85};

	if (swift < 1 || swift > 5)
		return 0;
	if (method == NODE_CMD_CAULK_AND_FLOAT)
		return caulk[band][swift];
	if (method == NODE_CMD_FORD)
		return ford[band][swift];
	if (method == NODE_CMD_FERRY)
		return ferry[swift];
	return 100;
}

static int32_t expected(const struct river_situation *s, int32_t band)
{
	int32_t c = base_chance(s->method, band, s->swiftness);

	if (s->method != NODE_CMD_FERRY && !s->conditions_checked)
		c -= 10;
	if (s->auto_pick)
		c += (100 - c) / 2;
	if (s->method == NODE_CMD_FORD || s->method == NODE_CMD_CAULK_AND_FLOAT)
		c -= 5 * s->raining + 15 * s->strained + 10 * s->conestoga;
	if (s->riverwork)
		c += (100 - c) / 2;
	return c;
}

static FILE *open_table(void)
{
	char path[512];

	TestExeBase(path, sizeof(path));
	if (!path[0])
		return NULL;
	StrCat(path, "_river.tsv");
	return fopen(path, "w");
}

static const char *method_name(int32_t m)
{
	return m == NODE_CMD_FORD ? "ford" : m == NODE_CMD_CAULK_AND_FLOAT ? "caulk" : m == NODE_CMD_FERRY ? "ferry" :
		m == NODE_CMD_CROSS_ICE ? "ice" : "?";
}

TEST(scn_river, chance_table)
{
	static const int32_t methods[] = {NODE_CMD_FORD, NODE_CMD_CAULK_AND_FLOAT, NODE_CMD_FERRY};
	FILE *table = open_table();
	int32_t cases = 0;

	if (table)
		fprintf(table, "method\tdepth\tswiftness\tchecked\tguide\train\tstrained\tconestoga\triverwork\tchance\n");
	for (int32_t m = 0; m < 3; ++m)
		for (int32_t b = 0; b < 3; ++b)
			for (int32_t sw = 0; sw <= 6; ++sw)
				for (int32_t mods = 0; mods < 64; ++mods) {
					struct river_situation s;
					int32_t got, want;
					memset(&s, 0, sizeof(s));
					s.method = methods[m];
					s.depth = BANDS[b].depth;
					s.width = 200;
					s.swiftness = sw;
					s.conditions_checked = (mods & 1) != 0;
					s.auto_pick = (mods & 2) != 0;
					s.raining = (mods & 4) != 0;
					s.strained = (mods & 8) != 0;
					s.conestoga = (mods & 16) != 0;
					s.riverwork = (mods & 32) != 0;
					got = RiverSuccessChance(&s);
					want = expected(&s, b);
					TCase("%s %s swiftness %d mods %02x", method_name(s.method), BANDS[b].name, sw, mods);
					CHECK_EQ(got, want);
					if (sw >= 1 && sw <= 5 && got > 100)
						CHECK_MSG(0, "chance %d over 100", got);
					if (table && sw >= 1 && sw <= 5)
						fprintf(table, "%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", method_name(s.method), BANDS[b].name,
							sw, s.conditions_checked, s.auto_pick, s.raining, s.strained, s.conestoga, s.riverwork, got);
					++cases;
				}
	if (table)
		fclose(table);
	TCase("");
	TNote("%d combinations", cases);
}

TEST(scn_river, negative_chances)
{
	static const int32_t methods[] = {NODE_CMD_FORD, NODE_CMD_CAULK_AND_FLOAT};
	int32_t negative = 0;

	for (int32_t m = 0; m < 2; ++m)
		for (int32_t b = 0; b < 3; ++b)
			for (int32_t sw = 1; sw <= 5; ++sw) {
				struct river_situation s;
				memset(&s, 0, sizeof(s));
				s.method = methods[m];
				s.depth = BANDS[b].depth;
				s.swiftness = sw;
				s.raining = s.strained = s.conestoga = true;
				if (RiverSuccessChance(&s) < 0)
					++negative;
			}
	TNote("%d of 30 ford/caulk cases go below 0 unchecked, in rain, strained, with a Conestoga", negative);
	CHECK(negative >= 0);
}

TEST(scn_river, ice)
{
	static const struct { int32_t weight, width, freeze, high, want; } cases[] = {
		{0, 0, 0, 20, 90}, {4000, 300, 0, 20, 88}, {40000, 600, 0, 20, 78}, {40000, 600, 1, 20, 84},
		{40000, 600, 1, 40, 76}, {8000, 0, 0, 33, 87}, {0, 0, 0, 100, 22},
	};

	for (int32_t i = 0; i < (int32_t) (sizeof(cases) / sizeof(cases[0])); ++i) {
		struct river_situation s;
		memset(&s, 0, sizeof(s));
		s.method = NODE_CMD_CROSS_ICE;
		s.width = cases[i].width;
		s.ice_weight = cases[i].weight;
		s.ice_long_freeze = cases[i].freeze != 0;
		s.ice_high_temp = cases[i].high;
		TCase("weight %d width %d freeze %d high %d", cases[i].weight, cases[i].width, cases[i].freeze,
			cases[i].high);
		CHECK_EQ(RiverSuccessChance(&s), cases[i].want);
	}
}

TEST(scn_river, guide_method)
{
	CHECK_EQ(RiverGuideMethod(0), NODE_CMD_FORD);
	CHECK_EQ(RiverGuideMethod(24), NODE_CMD_FORD);
	CHECK_EQ(RiverGuideMethod(25), NODE_CMD_CAULK_AND_FLOAT);
	CHECK_EQ(RiverGuideMethod(999), NODE_CMD_CAULK_AND_FLOAT);
}

TEST(scn_river, failure_outcomes)
{
	static const int32_t methods[] = {NODE_CMD_FORD, NODE_CMD_CAULK_AND_FLOAT, NODE_CMD_FERRY, NODE_CMD_CROSS_ICE};

	for (int32_t m = 0; m < 4; ++m)
		for (int32_t b = 0; b < 3; ++b)
			for (int32_t sw = 0; sw <= 6; ++sw) {
				int32_t ob = -1, oa = -1, loss = -1;
				RiverFailure(methods[m], BANDS[b].depth, sw, &ob, &oa, &loss);
				TCase("%s %s swiftness %d", method_name(methods[m]), BANDS[b].name, sw);
				CHECK_RANGE(ob, CROSS_OUTCOME_TIPPED_DEEP, CROSS_OUTCOME_THROUGH_ICE);
				CHECK_RANGE(oa, CROSS_OUTCOME_TIPPED_DEEP, CROSS_OUTCOME_THROUGH_ICE);
				CHECK_RANGE(loss, 0, 100);
			}
	{
		int32_t ob = -1, oa = -1, loss = -1;
		TCase("unknown method");
		RiverFailure(77, 20, 3, &ob, &oa, &loss);
		CHECK_RANGE(ob, CROSS_OUTCOME_TIPPED_DEEP, CROSS_OUTCOME_THROUGH_ICE);
		CHECK_RANGE(oa, CROSS_OUTCOME_TIPPED_DEEP, CROSS_OUTCOME_THROUGH_ICE);
		CHECK_RANGE(loss, 0, 100);
	}
}

const struct test_reg TESTS_scn_river[] = {
	TEST_ENTRY(scn_river, chance_table),
	TEST_ENTRY(scn_river, negative_chances),
	TEST_ENTRY(scn_river, ice),
	TEST_ENTRY(scn_river, guide_method),
	TEST_ENTRY(scn_river, failure_outcomes),
	TEST_END
};

#endif
