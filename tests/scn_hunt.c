#include "testframework.h"
#include "testsupport.h"

#ifdef OTII_TEST

#include <string.h>

#include "../src/game/ailment.h"
#include "../src/base/cstr.h"
#include "../src/game/game.h"
#include "../src/dialogs/huntweapondlg.h"
#include "../src/game/invitem.h"
#include "../src/game/invlist.h"
#include "../src/base/list.h"
#include "../src/game/member.h"
#include "../src/game/msg.h"
#include "../src/game/msgcontroller.h"
#include "../src/game/msgqueue.h"
#include "../src/game/party.h"
#include "../src/base/rng.h"

static int32_t take_ailment(struct member *m, int32_t id)
{
	int32_t found = 0;

	for (int32_t i = 0; i < 64; ++i) {
		struct ailment *a = OblistDataAt(m->nodes, i, false);
		if (!a)
			break;
		if (a->def && a->def->id == id) {
			a->stage = STAGE_NONE;
			found = 1;
		}
	}
	if (found)
		MemberPurgeExpiredAilments(m);
	m->health = m->max_health;
	m->state = MEMBER_ALIVE;
	return found;
}

static struct inv_item *find_named(const char *name)
{
	struct cstr *s = CStrCreateFrom(name);
	struct inv_item *it = InvListFindByName(&g_game->party->inventory, s);
	CStrDestroy(&s);
	return it;
}

TEST_GAME(scn_hunt, accidental_gunshot)
{
	int32_t n = TStatRuns() * 10;

	CHECK(TestSave("hunt_base"));
	for (int32_t sharp = 0; sharp <= 1; ++sharp)
		for (int32_t sheath = 0; sheath <= 1; ++sheath) {
			struct party *p;
			struct member *leader;
			struct inv_item *sheath_item;
			int32_t hits = 0;
			int32_t chance = (sharp ? 10 : 500) / (sheath ? 10 : 1);
			TCase("sharpshooting %d, sheath %d", sharp, sheath);
			TestLoad("hunt_base");
			p = g_game->party;
			leader = PartyGetMember(p, 0);
			p->skills[SKILL_SHARPSHOOTING] = sharp != 0;
			sheath_item = InvListFindByType(&p->inventory, g_item_rifle_or_shotgun_sheath);
			if (sheath && !sheath_item) {
				TNote("the start game has no sheath: case skipped");
				continue;
			}
			if (!sheath && sheath_item)
				InvItemRemoveUnits(sheath_item, InvItemGetCount(sheath_item, true));
			take_ailment(leader, AILMENT_ACCIDENTAL_GUNSHOT);
			RngSetSeed(TSeed() + 10 * (uint32_t) sharp + (uint32_t) sheath);
			for (int32_t i = 0; i < n; ++i) {
				MsgControllerRollDailyHazards(GetAuxMsgqueue());
				hits += take_ailment(leader, AILMENT_ACCIDENTAL_GUNSHOT);
				take_ailment(leader, AILMENT_ANIMAL_BITE);
				take_ailment(leader, AILMENT_ANIMAL_MAULING);
			}
			CHECK_RATE(hits, n, chance / 10000.0);
		}
	TestLoad("hunt_base");
}

TEST_GAME(scn_hunt, bite_and_mauling)
{
	int32_t n = TStatRuns() * 5, bites = 0, maulings = 0;
	struct member *leader;

	TestLoad("hunt_base");
	leader = PartyGetMember(g_game->party, 0);
	if (!GameIsEventAllowed(g_game, MSG_ANIMAL_BITE) && !GameIsEventAllowed(g_game, MSG_ANIMAL_MAULING)) {
		TNote("neither is allowed at the start game's node");
		return;
	}
	RngSetSeed(TSeed() + 77);
	for (int32_t i = 0; i < n; ++i) {
		MsgControllerRollDailyHazards(GetAuxMsgqueue());
		take_ailment(leader, AILMENT_ACCIDENTAL_GUNSHOT);
		bites += take_ailment(leader, AILMENT_ANIMAL_BITE);
		maulings += take_ailment(leader, AILMENT_ANIMAL_MAULING);
	}
	if (GameIsEventAllowed(g_game, MSG_ANIMAL_BITE))
		CHECK_RATE(bites, n, 0.03);
	if (GameIsEventAllowed(g_game, MSG_ANIMAL_MAULING))
		CHECK_RATE(maulings, n, 0.01);
	TestLoad("hunt_base");
}

TEST_GAME(scn_hunt, missing_reasons)
{
	static const struct {
		int32_t powder, rifle, pistol, shotgun, bullets, shot;
		int32_t missing;
		const char *text;
	} cases[] = {
		{1, 1, 0, 0, 1, 0, 0, 0},
		{0, 1, 0, 0, 1, 0, 1, "You do not have any gunpowder, so you cannot go hunting."},
		{1, 0, 0, 0, 1, 1, 2, "You do not have any firearms, so you cannot go hunting."},
		{0, 0, 0, 0, 0, 0, 3, "You do not have any firearms or gunpowder, so you cannot go hunting."},
		{1, 1, 0, 0, 0, 0, 4, "You do not have any bullets, so you cannot go hunting."},
		{1, 0, 1, 0, 0, 1, 4, "You do not have any bullets, so you cannot go hunting."},
		{0, 0, 1, 0, 0, 0, 5, "You do not have any bullets or gunpowder, so you cannot go hunting."},
		{1, 0, 0, 1, 0, 0, 8, "You do not have any shot, so you cannot go hunting."},
		{0, 0, 0, 1, 1, 0, 9, "You do not have any shot or gunpowder, so you cannot go hunting."},
		{1, 1, 0, 1, 1, 0, 0, 0},
		{1, 0, 0, 1, 0, 1, 0, 0},
	};

	CHECK(TestSave("hunt_base"));
	for (int32_t i = 0; i < (int32_t) (sizeof(cases) / sizeof(cases[0])); ++i) {
		struct cstr *text;
		TCase("case %d", i);
		TestLoad("hunt_base");
		TestSetItem("gunpowder", cases[i].powder);
		TestSetItem("rifle", cases[i].rifle);
		TestSetItem("pistol", cases[i].pistol);
		TestSetItem("shotgun", cases[i].shotgun);
		TestSetItem("bullets", cases[i].bullets);
		TestSetItem("shot", cases[i].shot);
		InvListPurgeEmpty(&g_game->party->inventory);
		CHECK_EQ(HuntMissingItems(), cases[i].missing);
		text = HuntMissingText(cases[i].missing);
		if (!cases[i].text) {
			CHECK(!text);

		} else {
			CHECK(text);
			if (text)
				CHECK_MSG(!strcmp(CStrGet(text), cases[i].text), "got \"%s\"", CStrGet(text));
		}
		CStrDestroy(&text);
	}
	TestLoad("hunt_base");
}

TEST_GAME(scn_hunt, ammo_used_and_purged)
{
	struct inv_item *bullets, *shot, *powder;
	int32_t shots, powder_before, shot_before;

	CHECK(TestSave("hunt_base"));
	TestLoad("hunt_base");
	TestSetItem("gunpowder", 5);
	TestSetItem("bullets", 1);
	TestSetItem("shot", 1);
	bullets = find_named("bullets");
	shot = find_named("shot");
	powder = find_named("gunpowder");
	CHECK(bullets && shot && powder);
	if (!bullets || !shot || !powder)
		return;
	shots = InvItemTotalPortions(bullets);
	powder_before = InvItemTotalPortions(powder);
	shot_before = InvItemTotalPortions(shot);
	CHECK(powder_before > shots);
	HuntUseAmmo(shots, WEAPON_RIFLE);
	CHECK(!find_named("bullets"));
	powder = find_named("gunpowder");
	CHECK(powder && InvItemTotalPortions(powder) == powder_before - shots);
	shot = find_named("shot");
	CHECK(shot && InvItemTotalPortions(shot) == shot_before);

	TCase("shotgun");
	HuntUseAmmo(1, WEAPON_SHOTGUN);
	shot = find_named("shot");
	CHECK(shot_before > 1 ? shot && InvItemTotalPortions(shot) == shot_before - 1 : !shot);

	TCase("no shots");
	powder = find_named("gunpowder");
	powder_before = powder ? InvItemTotalPortions(powder) : 0;
	HuntUseAmmo(0, WEAPON_RIFLE);
	powder = find_named("gunpowder");
	CHECK((powder ? InvItemTotalPortions(powder) : 0) == powder_before);
	TestLoad("hunt_base");
}

const struct test_reg TESTS_scn_hunt[] = {
	TEST_GAME_ENTRY(scn_hunt, accidental_gunshot),
	TEST_GAME_ENTRY(scn_hunt, bite_and_mauling),
	TEST_GAME_ENTRY(scn_hunt, missing_reasons),
	TEST_GAME_ENTRY(scn_hunt, ammo_used_and_purged),
	TEST_END
};

#endif
