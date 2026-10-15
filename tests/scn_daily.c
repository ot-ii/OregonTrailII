#include "testframework.h"
#include "testsupport.h"

#ifdef OTII_TEST

#include <string.h>

#include "../src/game/game.h"
#include "../src/game/invitem.h"
#include "../src/game/invlist.h"
#include "../src/base/list.h"
#include "../src/game/member.h"
#include "../src/game/party.h"
#include "../src/game/weather.h"

static int32_t expected_drain(struct party *p, bool resting)
{
	int32_t d = 0;

	if (!InvListHasGroup(&p->inventory, IGRP_CLOTHING))
		d += 30;
	if (p->days_starving)
		d += 80;
	else if (p->ration_level == RATIONS_MEAGER)
		d += 45;
	else if (p->ration_level == 0)
		d += 60;
	if (WeatherGetLowTemp(g_game->weather, 0) < 10)
		d += 25;
	if (!resting) {
		if (g_game->travel_pace == PACE_10_HOURS)
			d += 45;
		if (g_game->travel_pace == PACE_EXHAUSTING)
			d += 60;
		if (WeatherSumPrecip(g_game->weather, 0))
			d += 20;
		if (WeatherGetHighTemp(g_game->weather, 0) > 95)
			d += 25;

	} else {
		d -= 50;
	}
	return d;
}

static int32_t meal_portions(struct party *p)
{
	static const int32_t groups[5] = {IGRP_SEASONING, IGRP_FOOD_OTHER, IGRP_FOOD_PRODUCE, IGRP_FOOD_MEAT,
		IGRP_FOOD_GRAIN};
	int32_t n = 0;

	for (struct list_node *node = p->inventory.base.head; node; node = node->next)
		for (int32_t g = 0; g < 5; ++g)
			if (InvItemInGroup(InvItemFromNode(node), groups[g])) {
				n += InvItemTotalPortions(InvItemFromNode(node));
				break;
			}
	return n;
}

TEST_GAME(scn_daily, health_strain)
{
	static const int32_t paces[] = {PACE_8_HOURS, PACE_10_HOURS, PACE_EXHAUSTING};
	int32_t cases = 0;

	CHECK(TestSave("daily_base"));
	for (int32_t rations = 0; rations <= 2; ++rations)
		for (int32_t pace = 0; pace < 3; ++pace)
			for (int32_t resting = 0; resting <= 1; ++resting)
				for (int32_t starving = 0; starving <= 1; ++starving) {
					struct party *p;
					int32_t want, before[6] = {0};
					struct test_state after;
					TestLoad("daily_base");
					p = g_game->party;
					p->ration_level = rations;
					g_game->travel_pace = paces[pace];
					g_game->travel_state = resting ? TRAVEL_RESTING : TRAVEL_MOVING;
					p->days_starving = starving;
					p->daily_health_drain = 0;
					for (int32_t i = 0; i < p->member_count && i < 6; ++i) {
						struct member *m = p->members[i];
						if (!m || m->state != MEMBER_ALIVE)
							continue;
						OblistClear(m->nodes);
						m->health = m->max_health * 6 / 10;
						m->hours_accum = 0;
						before[i] = m->health;
					}
					want = expected_drain(p, resting != 0);
					TCase("rations %d pace %d %s%s", rations, paces[pace], resting ? "resting" : "travelling",
						starving ? " starving" : "");
					PartyDailyHealthUpdate(p, 24);
					if (!starving)
						for (int32_t i = 0; i < p->member_count && i < 6; ++i) {
							struct member *m = p->members[i];
							int32_t change;
							if (!m || m->state != MEMBER_ALIVE)
								continue;
							change = m->health - before[i];
							CHECK_MSG(change >= m->stamina - want - 15 && change <= m->stamina - want + 15,
								"member %d: health %+d with stamina %d, strain %d", i, change, m->stamina, want);
						}
					CHECK_EQ(p->daily_health_drain, 0);
					TestCapture(&after);
					TestCheckInvariants(&after, "after a day");
					++cases;
				}
	TCase("");
	TNote("%d combinations", cases);
	TestLoad("daily_base");
}

TEST_GAME(scn_daily, food_eaten)
{
	TestSave("daily_base");
	for (int32_t rations = 0; rations <= 2; ++rations)
		for (int32_t cook = 0; cook <= 1; ++cook) {
			struct party *p;
			int32_t before, after, per, alive, want;
			TestLoad("daily_base");
			p = g_game->party;
			p->ration_level = rations;
			p->skills[SKILL_COOKING] = cook != 0;
			alive = PartyMemberCount(p, true);
			per = rations == 0 ? 1 : rations == RATIONS_MEAGER ? 2 : 3;
			want = alive * per;
			if (!InvListHasGroup(&p->inventory, IGRP_COOKWARE)) {
				want = want * 7 / 6;
				if (!want)
					want = 1;
			}
			if (cook) {
				want = want * 5 / 6;
				if (!want)
					want = 1;
			}
			TCase("rations %d, cookware %s, cooking skill %d", rations,
				InvListHasGroup(&p->inventory, IGRP_COOKWARE) ? "yes" : "no", cook);
			before = meal_portions(p);
			PartyConsumeDailyFood(p);
			after = meal_portions(p);
			TNote("%d alive, rations %d: %d portions eaten (a helping is %d)", alive, rations, before - after, want);
			CHECK(before - after >= want);
			CHECK(before - after <= 15 * want);
		}
	TestLoad("daily_base");
}

TEST_GAME(scn_daily, rest_with_plenty_raises_morale)
{
	struct party *p;
	int32_t before;

	TestSave("daily_base");
	TestLoad("daily_base");
	p = g_game->party;
	p->reputation = 40;
	g_game->travel_state = TRAVEL_RESTING;
	before = p->reputation;
	PartyDailyHealthUpdate(p, 24);
	if (PartyHasPlentyOfFood(p))
		CHECK_EQ(p->reputation, before + 2);
	else
		TNote("the start game does not have plenty of food");
	p->reputation = 100;
	PartyDailyHealthUpdate(p, 24);
	CHECK(p->reputation <= 100);
	TestLoad("daily_base");
}

TEST_GAME(scn_daily, impossible)
{
	struct party *p;
	struct test_state s;

	TestSave("daily_base");

	TCase("nobody alive");
	TestLoad("daily_base");
	p = g_game->party;
	for (int32_t i = 0; i < p->member_count; ++i)
		if (p->members[i])
			p->members[i]->state = MEMBER_DEAD;
	PartyDailyHealthUpdate(p, 24);
	TestCapture(&s);
	TestCheckInvariants(&s, "nobody alive");

	TCase("no food for 30 days");
	TestLoad("daily_base");
	p = g_game->party;
	InvListClear(&p->inventory);
	for (int32_t day = 0; day < 30; ++day) {
		p->daily_health_drain = 0;
		PartyDailyHealthUpdate(p, 24);
	}
	TestCapture(&s);
	TestCheckInvariants(&s, "30 days without food");
	CHECK(p->days_starving > 0);

	TCase("ration level 7");
	TestLoad("daily_base");
	p = g_game->party;
	p->ration_level = 7;
	PartyDailyHealthUpdate(p, 24);
	TestCapture(&s);
	TestCheckInvariants(&s, "ration level 7");

	TestLoad("daily_base");
}

const struct test_reg TESTS_scn_daily[] = {
	TEST_GAME_ENTRY(scn_daily, health_strain),
	TEST_GAME_ENTRY(scn_daily, food_eaten),
	TEST_GAME_ENTRY(scn_daily, rest_with_plenty_raises_morale),
	TEST_GAME_ENTRY(scn_daily, impossible),
	TEST_END
};

#endif
