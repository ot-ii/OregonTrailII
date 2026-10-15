#include "../testframework.h"
#include "../testsupport.h"

#ifdef OTII_TEST

#include <windows.h>

#include "../../src/game/ailment.h"
#include "../../src/features/autopilot.h"
#include "../../src/base/date.h"
#include "../../src/game/game.h"
#include "../../src/base/ini.h"
#include "../../src/game/invitem.h"
#include "../../src/game/invlist.h"
#include "../../src/base/list.h"
#include "../../src/game/member.h"
#include "../../src/game/party.h"
#include "../../src/app/replay.h"

static void wait_out_cache(void)
{
	Sleep(210);
	RpGetTickCount();
}

static int32_t days(int32_t level)
{
	wait_out_cache();
	return AutoPilotTestFoodDaysAt(level);
}

static int32_t count_all(struct inv_list *list)
{
	int32_t count = 0;

	for (struct list_node *n = list->base.head; n; n = n->next)
		count += InvItemGetCount(InvItemFromNode(n), false);
	return count;
}

static int32_t essentials(void)
{
	return TestItemCount("rifle") + TestItemCount("pistol") + TestItemCount("shotgun") +
		TestItemCount("spare wagon wheel") + TestItemCount("spare wagon axle") + TestItemCount("spare wagon tongue") +
		TestItemCount("spare ox yoke");
}

static struct inv_item *first_animal(struct party *p)
{
	return InvItemFromNode(p->animals.base.head);
}

TEST_GAME(unit_autopilot, meal_helping)
{
	CHECK(TestSave("autopilot_base"));
	for (int32_t level = 0; level <= 2; ++level) {
		struct party *p = g_game->party;
		int32_t want = PartyMemberCount(p, true) * (level == 0 ? 1 : level == RATIONS_MEAGER ? 2 : 3);
		if (!InvListHasGroup(&p->inventory, IGRP_COOKWARE)) {
			want = want * 7 / 6;
			if (!want)
				want = 1;
		}
		if (p->skills[SKILL_COOKING]) {
			want = want * 5 / 6;
			if (!want)
				want = 1;
		}
		TCase("rations %d", level);
		CHECK_EQ(AutoPilotTestMealHelping(level), want);
	}
}

TEST_GAME(unit_autopilot, food_days)
{
	int32_t d0, d1, d2, empty, seasoning_only, more;

	TestLoad("autopilot_base");
	d2 = days(RATIONS_FILLING);
	d1 = days(RATIONS_MEAGER);
	d0 = days(RATIONS_BARE_BONES);
	TNote("the start wagon: %d days filling, %d meager, %d bare bones", d2, d1, d0);
	CHECK(d2 > 0);
	CHECK(d1 >= d2);
	CHECK(d0 >= d1);

	TCase("empty wagon");
	InvListClear(&g_game->party->inventory);
	empty = days(RATIONS_FILLING);
	CHECK_EQ(empty, 0);

	TCase("only pepper and coffee");
	TestSetItem("pepper", 20);
	TestSetItem("coffee beans", 20);
	seasoning_only = days(RATIONS_FILLING);
	CHECK_EQ(seasoning_only, 0);

	TCase("flour added");
	TestSetItem("flour", 30);
	more = days(RATIONS_FILLING);
	CHECK(more > 0);
	TestSetItem("flour", 60);
	CHECK(days(RATIONS_FILLING) >= more);
	TestLoad("autopilot_base");
}

TEST_GAME(unit_autopilot, short_endgame_and_late)
{
	int32_t food;

	TestLoad("autopilot_base");
	food = days(RATIONS_FILLING);
	for (int32_t left = -1; left <= 120; left += 7) {
		int32_t want = food < 20 || (left >= 0 && left <= 60 && food < left + 5);
		TCase("%d days of food, %d to go", food, left);
		wait_out_cache();
		CHECK_EQ(AutoPilotTestFoodShortEndgame(left), want);
	}
	{
		struct date *d = g_game->date;
		int32_t m = d->month, day = d->day;
		d->month = MONTH_OCTOBER;
		d->day = 1;
		TCase("October 1");
		CHECK(!AutoPilotTestArrivingLate(14));
		CHECK(AutoPilotTestArrivingLate(15));
		CHECK(!AutoPilotTestArrivingLate(-1));
		d->month = MONTH_MAY;
		TCase("May 1");
		CHECK(!AutoPilotTestArrivingLate(150));
		CHECK(AutoPilotTestArrivingLate(200));
		d->month = m;
		d->day = day;
	}
}

TEST_GAME(unit_autopilot, stuck_breakdown_trades)
{
	struct party *p;
	struct inv_item *ox;
	int32_t have;

	TestLoad("autopilot_base");
	p = g_game->party;
	ox = first_animal(p);
	if (!ox) {
		TNote("no draft animals in the start game");
		return;
	}
	have = 0;
	for (struct list_node *n = p->animals.base.head; n; n = n->next)
		have += InvItemGetCount(InvItemFromNode(n), true);
	for (struct list_node *n = p->spare_animals.base.head; n; n = n->next)
		have += InvItemGetCount(InvItemFromNode(n), true);
	TNote("%d draft animals", have);
	for (int32_t refusals = 0; refusals <= 7; ++refusals) {
		for (int32_t qty = have - 4 > 1 ? have - 4 : 1; qty < have; ++qty) {
			int32_t left = have - qty, want = left >= (refusals >= 3 ? 2 : 4) &&
				(refusals >= 6 || AutoPilotTestCanPullWithout(ox, qty));
			TCase("%d animals, asked for %d, %d refusals so far", have, qty, refusals);
			CHECK_EQ(!!AutoPilotTestStuckTradeOk(ox, qty, refusals), want);
		}
	}
	TestLoad("autopilot_base");
}

TEST_GAME(unit_autopilot, stuck_in_mud_lightens_enough)
{
	struct party *p;
	int32_t excess, dumped, food_before, kept;

	CHECK(TestLoad("autopilot_mud_stuck"));
	p = g_game->party;
	PartyCalcPullingMode(p);
	excess = AutoPilotTestLoadExcess();
	TNote("%d oxen, %d lb of food, %d lb over the easy load", count_all(&p->animals), PartyGetFoodPounds(p), excess);
	CHECK(AutoPilotTestStrained());
	CHECK(excess > 0);
	food_before = PartyGetFoodPounds(p);
	kept = essentials();
	dumped = AutoPilotTestDump(excess);
	TNote("dumped %d lb; food %d -> %d lb", dumped, food_before, PartyGetFoodPounds(p));
	CHECK(!AutoPilotTestStrained());
	CHECK(AutoPilotTestLoadExcess() <= 0);
	TCase("the guns and the spare parts stay");
	CHECK_EQ(essentials(), kept);
	TestLoad("autopilot_mud_stuck");
}

TEST_GAME(unit_autopilot, breakdown_trade_keeps_a_team)
{
	struct party *p;
	struct inv_item *ox;
	int32_t have;

	CHECK(TestLoad("autopilot_mud_stuck"));
	p = g_game->party;
	ox = first_animal(p);
	CHECK(ox != NULL);
	if (!ox)
		return;
	InvItemAddUnits(ox, 3);
	PartyCalcPullingMode(p);
	have = count_all(&p->animals) + count_all(&p->spare_animals);
	for (int32_t refusals = 0; refusals <= 5; refusals += 5)
		for (int32_t qty = 1; qty < have; ++qty) {
			TCase("%d oxen, asked for %d, %d refusals", have, qty, refusals);
			if (!AutoPilotTestStuckTradeOk(ox, qty, refusals))
				continue;
			InvItemRemoveUnits(ox, qty);
			PartyCalcPullingMode(p);
			AutoPilotTestDump(AutoPilotTestLoadExcess());
			CHECK(!AutoPilotTestStrained());
			TestLoad("autopilot_mud_stuck");
			p = g_game->party;
			ox = first_animal(p);
			InvItemAddUnits(ox, 3);
			PartyCalcPullingMode(p);
		}
	TCase("");
	TestLoad("autopilot_mud_stuck");
}

TEST_GAME(unit_autopilot, quarantine_waits_out_the_fever)
{
	struct member *m;

	TestLoad("autopilot_base");
	m = g_game->party->members[1];
	CHECK(m != NULL);
	if (!m)
		return;
	TCase("nobody ill: the general week at a landmark with no way on");
	CHECK(AutoPilotTestKeepWaiting(3));
	CHECK(!AutoPilotTestKeepWaiting(7));
	TCase("typhoid, no symptoms yet");
	MemberAddAilment(m, AILMENT_TYPHOID_FEVER, STAGE_INCUBATION);
	CHECK(!AutoPilotTestKeepWaiting(7));
	TestLoad("autopilot_base");
	TCase("typhoid with symptoms");
	m = g_game->party->members[1];
	MemberAddAilment(m, AILMENT_TYPHOID_FEVER, STAGE_ACUTE);
	CHECK(AutoPilotTestKeepWaiting(7));
	CHECK(AutoPilotTestKeepWaiting(60));
	TCase("");
	TestLoad("autopilot_base");
}

TEST(unit_autopilot, menu_checks_follow_the_state)
{
	HMENU menu = CreatePopupMenu();
	int running = AutoPilotRunning();

	AppendMenuA(menu, MF_STRING | MF_CHECKED, AUTOPILOT_MENU_ID, "Autopilot");
	AutoPilotAppendMenu(menu);
	CheckMenuItem(menu, AUTOPILOT_MENU_ID, MF_BYCOMMAND | (running ? MF_UNCHECKED : MF_CHECKED));
	CheckMenuItem(menu, AUTOPILOT_NEXT_GAME_MENU_ID, MF_BYCOMMAND | MF_CHECKED);
	AutoPilotMenuUpdate(menu);
	CHECK_EQ(!!(GetMenuState(menu, AUTOPILOT_MENU_ID, MF_BYCOMMAND) & MF_CHECKED), !!running);
	CHECK_EQ(!!(GetMenuState(menu, AUTOPILOT_NEXT_GAME_MENU_ID, MF_BYCOMMAND) & MF_CHECKED),
		!!GetPrivateProfileIntA("autopilot", "new_game_after_end", 0, IniPath()));
	DestroyMenu(menu);
}

TEST_GAME(unit_autopilot, wanted_settings)
{
	int32_t pace, rations;

	TestLoad("autopilot_base");
	wait_out_cache();
	pace = AutoPilotTestWantedPace();
	rations = AutoPilotTestWantedRations();
	CHECK(pace == PACE_8_HOURS || pace == PACE_10_HOURS);
	CHECK_RANGE(rations, 0, 2);
	TNote("start game: pace %d, rations %d", pace, rations);
	TCase("an empty wagon: the food is short, so not filling rations (unless the trip's length is unknown)");
	InvListClear(&g_game->party->inventory);
	wait_out_cache();
	if (AutoPilotTestDaysToGo() >= 0)
		CHECK(AutoPilotTestWantedRations() != RATIONS_FILLING);
	TestLoad("autopilot_base");
}

TEST(unit_autopilot, stuck_trade_gives_gun_after_long_wait)
{
	CHECK(AutoPilotTestStuckGunOk(40, 22, 0, -1, 0));
	CHECK(!AutoPilotTestStuckGunOk(30, 22, 0, -1, 0));
	CHECK(!AutoPilotTestStuckGunOk(30, 22, 19, -1, 0));
	CHECK(AutoPilotTestStuckGunOk(30, 22, 20, -1, 0));
	CHECK(AutoPilotTestStuckGunOk(1, 22, 20, -1, 0));
	CHECK(!AutoPilotTestStuckGunOk(0, 22, 20, -1, 0));
	CHECK(!AutoPilotTestStuckGunOk(35, -1, 0, -1, 0));
	CHECK(AutoPilotTestStuckGunOk(40, -1, 0, -1, 0));
	CHECK(AutoPilotTestStuckGunOk(12, 22, 0, 6, 1));
	CHECK(!AutoPilotTestStuckGunOk(10, 22, 0, 6, 1));
	CHECK(!AutoPilotTestStuckGunOk(12, 22, 0, 6, 0));
}

TEST_GAME(unit_autopilot, towns_that_sell_guns)
{
	int32_t landmarks, days_away, towns;

	towns = AutoPilotTestGunTowns(&landmarks, &days_away);
	TNote("%d of %d landmarks ahead sell a gun; the first is %d days away", towns, landmarks, days_away);
	CHECK(landmarks > 20);
	CHECK(towns >= 3);
	CHECK(days_away >= 0);
}

const struct test_reg TESTS_unit_autopilot[] = {
	TEST_GAME_ENTRY(unit_autopilot, meal_helping),
	TEST_GAME_ENTRY(unit_autopilot, food_days),
	TEST_GAME_ENTRY(unit_autopilot, short_endgame_and_late),
	TEST_GAME_ENTRY(unit_autopilot, stuck_breakdown_trades),
	TEST_GAME_ENTRY(unit_autopilot, stuck_in_mud_lightens_enough),
	TEST_GAME_ENTRY(unit_autopilot, breakdown_trade_keeps_a_team),
	TEST_GAME_ENTRY(unit_autopilot, quarantine_waits_out_the_fever),
	TEST_ENTRY(unit_autopilot, menu_checks_follow_the_state),
	TEST_GAME_ENTRY(unit_autopilot, wanted_settings),
	TEST_ENTRY(unit_autopilot, stuck_trade_gives_gun_after_long_wait),
	TEST_GAME_ENTRY(unit_autopilot, towns_that_sell_guns),
	TEST_END
};

#endif
