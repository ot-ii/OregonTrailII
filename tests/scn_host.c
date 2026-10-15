#include "testframework.h"

#ifdef OTII_TEST

#include "../src/base/date.h"
#include "../src/game/game.h"
#include "../src/game/member.h"
#include "../src/game/party.h"

TEST_GAME(scn_host, game_is_up)
{
	int32_t alive = 0;

	CHECK(g_game != NULL);
	if (!g_game)
		return;
	CHECK(g_game->party != NULL);
	CHECK(g_game->date != NULL);
	CHECK(g_game->cur_node != NULL);
	CHECK_RANGE(g_game->date->year, 1840, 1860);
	CHECK_RANGE(g_game->party->member_count, 1, 6);
	for (int32_t i = 0; i < g_game->party->member_count && i < 6; ++i)
		if (g_game->party->members[i] && g_game->party->members[i]->state == MEMBER_ALIVE)
			++alive;
	CHECK(alive >= 1);
	TNote("node %d, %d-%02d-%02d, %d alive, cash %.2f, food %d lb", g_game->cur_node_id, g_game->date->year,
		g_game->date->month, g_game->date->day, alive, g_game->party->cash, PartyGetFoodPounds(g_game->party));
}

const struct test_reg TESTS_scn_host[] = {
	TEST_GAME_ENTRY(scn_host, game_is_up),
	TEST_END
};

#endif
