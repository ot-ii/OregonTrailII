#include "testframework.h"

#ifdef OTII_TEST

#include <string.h>

#include "../src/dialogs/convdlg.h"
#include "../src/base/date.h"
#include "../src/game/endgame.h"
#include "../src/game/game.h"
#include "../src/game/invlist.h"
#include "../src/game/member.h"
#include "../src/trail/node.h"
#include "../src/game/party.h"
#include "testsupport.h"

static const int32_t DEST_NODE[4] = {265, 506, 619, 753};

struct setup {
	int32_t saved_node_id;
	uint8_t saved_flags[1200];
};

static void begin(struct setup *s, int32_t dest)
{
	TestLoad("endgame_base");
	s->saved_node_id = g_game->cur_node->node_id;
	g_game->cur_node->node_id = DEST_NODE[dest];
	memcpy(s->saved_flags, g_slot_flags, sizeof(g_slot_flags));
	memset(g_slot_flags, 0, sizeof(g_slot_flags));
	for (int32_t i = 0; i < g_game->party->member_count; ++i)
		if (g_game->party->members[i]) {
			g_game->party->members[i]->state = MEMBER_ALIVE;
			g_game->party->members[i]->health_band = HEALTH_GOOD;
		}
	g_game->destination = (dest + 1) % 4;
	g_game->party->reputation = 50;
}

static void end(struct setup *s)
{
	g_game->cur_node->node_id = s->saved_node_id;
	memcpy(g_slot_flags, s->saved_flags, sizeof(g_slot_flags));
}

static int32_t reputation_bonus(int32_t r)
{
	if (r > 80)
		return 100;
	if (r > 60)
		return 75;
	if (r > 40)
		return 50;
	if (r > 20)
		return 25;
	if (r < -10)
		return -100;
	if (r < 0)
		return -75;
	if (r < 10)
		return -50;
	return 0;
}

TEST_GAME(scn_endgame, setup)
{
	CHECK(TestSave("endgame_base"));
}

TEST_GAME(scn_endgame, destinations)
{
	for (int32_t d = 0; d < 4; ++d) {
		struct setup s;
		TCase("destination %d (node %d)", d, DEST_NODE[d]);
		begin(&s, d);
		struct end_game *e = EndGameCreate(g_game);
		CHECK_EQ(e->destination, d);
		EndGameDestroy(&e);
		end(&s);
	}
}

TEST_GAME(scn_endgame, health_bands)
{
	static const int32_t band_pts[5] = {300, 350, 400, 450, 500};

	for (int32_t band = 0; band <= 4; ++band) {
		struct setup s;
		int32_t alive = 0;
		TCase("everyone in band %d", band);
		begin(&s, 0);
		for (int32_t i = 0; i < g_game->party->member_count; ++i)
			if (g_game->party->members[i]) {
				g_game->party->members[i]->health_band = band;
				++alive;
			}
		struct end_game *e = EndGameCreate(g_game);
		CHECK_EQ(e->pts_healthy + e->pts_fair + e->pts_poor, band_pts[band] * alive);
		CHECK_EQ(e->n_healthy, band == 4 ? alive : 0);
		CHECK_EQ(e->n_fair, band == 2 || band == 3 ? alive : 0);
		CHECK_EQ(e->n_poor, band <= 1 ? alive : 0);
		EndGameDestroy(&e);
		end(&s);
	}
	{
		struct setup s;
		TCase("the dead score nothing");
		begin(&s, 0);
		for (int32_t i = 1; i < g_game->party->member_count; ++i)
			if (g_game->party->members[i])
				g_game->party->members[i]->state = MEMBER_DEAD;
		struct end_game *e = EndGameCreate(g_game);
		CHECK_EQ(e->pts_healthy + e->pts_fair + e->pts_poor, 500);
		EndGameDestroy(&e);
		end(&s);
	}
}

TEST_GAME(scn_endgame, cash)
{
	static const double amounts[] = {0, 0.99, 2.5, 100, 1000.75, 32767.5, 81917, 81920, 1000000, 1e9};

	for (int32_t i = 0; i < (int32_t) (sizeof(amounts) / sizeof(amounts[0])); ++i) {
		struct setup s;
		int32_t want;
		TCase("$%.2f", amounts[i]);
		begin(&s, 0);
		g_game->party->cash = amounts[i];
		want = amounts[i] > 2e9 ? 32767 : 2 * (int32_t) amounts[i] / 5;
		if (want > 32767)
			want = 32767;
		struct end_game *e = EndGameCreate(g_game);
		CHECK_EQ(e->pts_cash, want);
		EndGameDestroy(&e);
		end(&s);
	}
}

TEST_GAME(scn_endgame, reputation)
{
	static const int32_t reps[] = {-100, -11, -10, -1, 0, 5, 9, 10, 15, 20, 21, 40, 41, 60, 61, 80, 81, 100};

	for (int32_t i = 0; i < (int32_t) (sizeof(reps) / sizeof(reps[0])); ++i) {
		struct setup s;
		TCase("reputation %d", reps[i]);
		begin(&s, 0);
		g_game->party->reputation = reps[i];
		struct end_game *e = EndGameCreate(g_game);
		CHECK_EQ(e->pts_misc, reputation_bonus(reps[i]));
		EndGameDestroy(&e);
		end(&s);
	}
}

TEST_GAME(scn_endgame, year_route_speed)
{
	for (int32_t y = 1840; y <= 1861; y += 3) {
		struct setup s;
		TCase("start year %d", y);
		begin(&s, 0);
		g_game->start_date->year = y;
		struct end_game *e = EndGameCreate(g_game);
		CHECK_EQ(e->pts_year, 10 * (1861 - y));
		EndGameDestroy(&e);
		end(&s);
	}
	for (int32_t town = 0; town < 4; ++town)
		for (int32_t dest = 0; dest < 4; ++dest) {
			int32_t k = 4 * town + dest;
			for (int32_t extra = -20; extra <= 40; extra += 30) {
				struct setup s;
				TCase("start town %d, destination %d, %+d days over the route's %d", town, dest, extra,
					g_route_days[k]);
				begin(&s, dest);
				g_game->start_town = town;
				g_game->days_traveled = g_route_days[k] + extra;
				struct end_game *e = EndGameCreate(g_game);
				CHECK_EQ(e->pts_route, g_route_pts[k]);
				CHECK_EQ(e->pts_speed, extra > 0 ? -extra : 0);
				EndGameDestroy(&e);
				end(&s);
			}
		}
}

TEST_GAME(scn_endgame, totals)
{
	static const int32_t mults[] = {10, 15, 20, 30, 35};

	for (int32_t i = 0; i < (int32_t) (sizeof(mults) / sizeof(mults[0])); ++i) {
		struct setup s;
		TCase("multiplier %d", mults[i]);
		begin(&s, 1);
		g_game->score_multiplier = mults[i];
		struct end_game *e = EndGameCreate(g_game);
		int32_t raw = e->pts_cash + e->pts_speed + e->pts_wagons + e->pts_handcarts + e->pts_healthy +
			e->pts_animals + e->pts_goods + e->pts_fair + e->pts_unused_skill_points + e->pts_misc + e->pts_poor +
			e->pts_year + e->pts_route;
		CHECK_EQ(e->raw_total, raw);
		CHECK_EQ(e->final_total, (int32_t) (raw * (mults[i] / 10.0) + 0.5));
		CHECK_EQ(e->acres, e->final_total / 20 + e->pts_year);
		CHECK_EQ(e->dollars, 20 * (e->pts_year / 5 + e->final_total / 200));
		CHECK_RANGE(e->lucky_number, 100, 999);
		CHECK(e->arrival_date && DateGreater(e->arrival_date, g_game->date));
		EndGameDestroy(&e);
		end(&s);
	}
}

TEST_GAME(scn_endgame, arrival_bonus)
{
	struct setup s;
	int32_t without, with_bonus;

	begin(&s, 2);
	g_game->player_level = 1;
	struct end_game *e = EndGameCreate(g_game);
	without = e->pts_misc;
	EndGameDestroy(&e);
	g_game->destination = 2;
	e = EndGameCreate(g_game);
	with_bonus = e->pts_misc;
	EndGameDestroy(&e);
	TNote("arrival bonus at the chosen destination: %d", with_bonus - without);
	CHECK(with_bonus - without >= 500);
	end(&s);
}

TEST_GAME(scn_endgame, goods)
{
	static const struct { const char *item; int32_t units, pts; } cases[] = {
		{"rifle", 2, 8}, {"shotgun", 1, 4}, {"pistol", 3, 12}, {"spare wagon wheel", 2, 6}, {"spare wagon axle", 1, 3},
		{"boots", 3, 6},
	};

	for (int32_t i = 0; i < (int32_t) (sizeof(cases) / sizeof(cases[0])); ++i) {
		struct setup s;
		int32_t base;
		TCase("%d %s", cases[i].units, cases[i].item);
		begin(&s, 0);
		InvListClear(&g_game->party->inventory);
		struct end_game *e = EndGameCreate(g_game);
		base = e->pts_goods;
		EndGameDestroy(&e);
		TestSetItem(cases[i].item, cases[i].units);
		if (TestItemCount(cases[i].item) != cases[i].units) {
			TNote("no item \"%s\" in the item table", cases[i].item);
			end(&s);
			continue;
		}
		e = EndGameCreate(g_game);
		CHECK_EQ(e->pts_goods - base, cases[i].pts);
		EndGameDestroy(&e);
		end(&s);
	}
}

TEST_GAME(scn_endgame, impossible)
{
	struct setup s;
	struct end_game *e;

	TCase("nobody alive");
	begin(&s, 0);
	for (int32_t i = 0; i < g_game->party->member_count; ++i)
		if (g_game->party->members[i])
			g_game->party->members[i]->state = MEMBER_DEAD;
	e = EndGameCreate(g_game);
	CHECK_EQ(e->pts_healthy + e->pts_fair + e->pts_poor, 0);
	EndGameDestroy(&e);
	end(&s);

	TCase("start in 1870");
	begin(&s, 0);
	g_game->start_date->year = 1870;
	e = EndGameCreate(g_game);
	CHECK_EQ(e->pts_year, -90);
	EndGameDestroy(&e);
	end(&s);

	TCase("5000 days");
	begin(&s, 0);
	g_game->days_traveled = 5000;
	e = EndGameCreate(g_game);
	CHECK(e->pts_speed < -4000);
	EndGameDestroy(&e);
	end(&s);

	TCase("a mountain of shot");
	begin(&s, 0);
	TestSetItem("shot", 400000);
	e = EndGameCreate(g_game);
	CHECK_RANGE(e->pts_goods, 1, 32767);
	EndGameDestroy(&e);
	end(&s);
	TestLoad("endgame_base");
}

const struct test_reg TESTS_scn_endgame[] = {
	TEST_GAME_ENTRY(scn_endgame, setup),
	TEST_GAME_ENTRY(scn_endgame, destinations),
	TEST_GAME_ENTRY(scn_endgame, health_bands),
	TEST_GAME_ENTRY(scn_endgame, cash),
	TEST_GAME_ENTRY(scn_endgame, reputation),
	TEST_GAME_ENTRY(scn_endgame, year_route_speed),
	TEST_GAME_ENTRY(scn_endgame, totals),
	TEST_GAME_ENTRY(scn_endgame, arrival_bonus),
	TEST_GAME_ENTRY(scn_endgame, goods),
	TEST_GAME_ENTRY(scn_endgame, impossible),
	TEST_END
};

#endif
