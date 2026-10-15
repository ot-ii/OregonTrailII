#include "testframework.h"
#include "testsupport.h"

#ifdef OTII_TEST

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "../src/game/ailment.h"
#include "../src/data/assets.h"
#include "../src/base/cstr.h"
#include "../src/base/date.h"
#include "../src/base/file.h"
#include "../src/game/game.h"
#include "../src/data/gfxdev.h"
#include "../src/game/invitem.h"
#include "../src/game/invlist.h"
#include "../src/base/list.h"
#include "../src/game/member.h"
#include "../src/game/msg.h"
#include "../src/game/msgcontroller.h"
#include "../src/game/msgqueue.h"
#include "../src/trail/node.h"
#include "../src/game/party.h"
#include "../src/base/rng.h"
#include "../src/data/rsrc.h"
#include "../src/screens/sceneframe.h"
#include "../src/base/str.h"
#include "../src/data/strrsrc.h"

#define LAST_EVENT 102

struct event_info {
	char name[64];
	int32_t choices[8];
	int32_t choice_count;
	char labels[8][64];
};

static bool load_event(int32_t id, struct event_info *e)
{
	struct rsrc *probe;
	struct strrsrc *s;

	memset(e, 0, sizeof(*e));
	probe = g_text_lib ? AssetsGetElement(g_text_lib, "STRLST", 11000 + id, false) : NULL;
	if (!probe)
		return false;
	RsrcDestroy(&probe);
	s = StrRsrcLoad(g_text_lib, 11000 + id, true);
	for (int32_t i = 0; s && i < StrRsrcCount(s); ++i) {
		char line[160];
		int32_t index, cond;
		const char *label;
		StrRsrcCopyTo(s, i, line, sizeof(line));
		if (i == 0) {
			label = strchr(line, '\t');
			StrCopy(e->name, label ? label + 1 : line);
			continue;
		}
		label = strrchr(line, '\t');
		if (sscanf(line, "%" SCNd32 "\t%" SCNd32, &index, &cond) != 2 || index >= 1000 || e->choice_count >= 8 || !label ||
				label == strchr(line, '\t'))
			continue;
		e->choices[e->choice_count] = index;
		StrCopy(e->labels[e->choice_count], label ? label + 1 : "");
		++e->choice_count;
	}
	StrRsrcDestroy(&s);
	return true;
}

static FILE *open_table(void)
{
	char path[512];
	char name[128];

	FileGetExeDir(path, sizeof(path));
	FileGetExeName(name, sizeof(name));
	StrCat(path, name);
	StrCat(path, "_events.tsv");
	return fopen(path, "w");
}

struct run {
	int32_t id, choice;
	intptr_t member, subject;
	struct cstr *payload;
	int32_t handled;
	int32_t kind;
};

static struct member *some_member(void)
{
	struct party *p = g_game->party;

	for (int32_t i = p->member_count > 1 ? 1 : 0; i < p->member_count; ++i)
		if (p->members[i] && p->members[i]->state == MEMBER_ALIVE)
			return p->members[i];
	return p->members[0];
}

static struct ailment *add_ailment_for(int32_t msg_id, struct member *m)
{
	for (int32_t a = 1; a <= AILMENT_WOUND; ++a) {
		if (SceneFrameEventCodeToMsgId(NULL, a) != msg_id)
			continue;
		MemberAddAilment(m, a, STAGE_ACUTE);
		for (int32_t i = 0; i < 64; ++i) {
			struct ailment *ail = OblistDataAt(m->nodes, i, false);
			if (!ail)
				break;
			if (ail->def && ail->def->id == a)
				return ail;
		}
		return NULL;
	}
	return NULL;
}

#define FERRY_LANDMARK 19

static bool prepare(struct run *r)
{
	struct party *p = g_game->party;
	struct member *m = some_member();
	struct ailment *ail;

	r->member = 0;
	r->subject = 0;
	r->payload = NULL;
	r->kind = -1;
	switch (r->id) {
		case MSG_MISSING_ANIMAL:
		case MSG_MISSING_ANIMALS:
		case MSG_SICK_LIVESTOCK:
		case MSG_INJURED_LIVESTOCK:
		case MSG_ANIMAL_IN_QUICKSAND:
		case MSG_ANIMAL_DIED_ACCIDENT:
		case MSG_ANIMAL_IN_HOLE:
			r->subject = (intptr_t) InvItemFromNode(p->animals.base.head ? p->animals.base.head :
				p->spare_animals.base.head);
			if (!r->subject)
				return false;
			r->member = PartyCalcPullingMode(p);
			r->payload = CStrCreateFrom("1 ox");
			return true;

		case MSG_MISSING_PERSON:
			r->member = (intptr_t) m;
			m->state = MEMBER_MISSING;
			r->payload = CStrCreateCopy(m->name);
			CStrAppendBuf(r->payload, " is missing.");
			return true;

		case MSG_THEFT: {
			struct inv_list *lost = InvListCreate();
			struct cstr *flour = CStrCreateFrom("flour");
			InvListAdd(lost, flour, 10, 0);
			CStrDestroy(&flour);
			r->member = (intptr_t) lost;
			r->payload = InvListFormatContents(lost, true);
			return true;
		}

		case MSG_CAUGHT_FISH:
			r->member = 20;
			return true;

		case MSG_FERRY_DELAY:
			g_game->cur_node = (struct node_base *) LNodeCreate(FERRY_LANDMARK, true);
			g_game->cur_node_id = FERRY_LANDMARK;
			((struct lnode *) g_game->cur_node)->ferry_wait_days = 2;
			r->payload = CStrCreateFrom("The wait is 2 days!");
			return true;

		case MSG_MEMBER_DIED:
		case MSG_LEADER_DIED:
			r->member = (intptr_t) m;
			r->payload = CStrCreateFrom("died.");
			return true;

		case MSG_TIPPED_WAGON:
		case MSG_SWAMPED_WAGON:
		case MSG_FELL_THROUGH_ICE:
		case MSG_RAFTING_LOSSES:
		case MSG_TRAIN_SPLITTING:
		case MSG_WAGONS_JOIN:
			r->payload = CStrCreateFrom("some supplies");
			return true;
	}
	ail = add_ailment_for(r->id, m);
	if (ail) {
		r->member = (intptr_t) m;
		r->subject = (intptr_t) ail;
		r->payload = CStrCreateCopy(m->name);
		CStrAppendBuf(r->payload, " is ill.");
	}
	return true;
}

static void handle_one(void *arg)
{
	struct run *r = arg;
	struct msg *msg = MsgCreate(r->id, r->payload, r->member, r->subject);

	if (r->kind >= 0)
		msg->scene_variant = r->kind;

	msg->rsrc_index = r->choice;
	msg->choice_state = 1;
	MsgControllerHandleEvent(GetAuxMsgqueue(), msg);
	r->handled = msg->choice_state;
	if (msg->choice_state == 2)
		MsgDestroy(&msg);
}

static void run_case(int32_t id, int32_t choice, const char *label, FILE *table, const char *event_name)
{
	struct test_state before, after;
	char diff[1024], when[160];
	struct run r;

	TCase("event %d %s, choice %d %s", id, event_name, choice, label);
	if (!TestLoad("events_base")) {
		CHECK_MSG(0, "the base game did not load");
		return;
	}
	RngSetSeed(TSeed() + (uint32_t) (id * 64 + choice + 8));
	TestScreensClear();
	r.id = id;
	r.choice = choice;
	r.handled = -1;
	if (!prepare(&r)) {
		TNote("event %d: the party has nothing this event needs", id);
		return;
	}
	TestCapture(&before);
	if (!TGuarded(handle_one, &r))
		return;
	TestPump(250);
	TestCapture(&after);
	StrPrintf(when, "after event %d choice %d", id, choice);
	TestCheckInvariants(&after, when);
	TestDiff(&before, &after, diff, sizeof(diff));
	if (table)
		fprintf(table, "%d\t%s\t%d\t%s\t%d\t%s\t%s\n", id, event_name, choice, label, r.handled, diff,
			TestScreensSeen());
	if (table)
		fflush(table);
}

TEST_GAME(scn_events, every_choice)
{
	FILE *table = open_table();
	struct event_info e;
	int32_t events = 0, cases = 0;

	CHECK(TestSave("events_base"));
	if (table)
		fprintf(table, "event\tname\tchoice\tlabel\tstate_after\tchanged\tscreens\n");
	for (int32_t id = 0; id <= LAST_EVENT; ++id) {
		if (!load_event(id, &e)) {
			TCase("event %d", id);
			TNote("event %d has no STRLST %d (no menu)", id, 11000 + id);
			StrCopy(e.name, "(no menu)");
			run_case(id, 0, "(none)", table, e.name);
			++cases;
			continue;
		}
		++events;
		for (int32_t k = 0; k < e.choice_count; ++k) {
			run_case(id, e.choices[k], e.labels[k], table, e.name);
			++cases;
		}
	}
	if (table)
		fclose(table);
	TCase("");
	TNote("%d events with menus, %d cases", events, cases);
	TestLoad("events_base");
}

TEST_GAME(scn_events, animal_gone_before_answer)
{
	static const int32_t ids[] = {MSG_MISSING_ANIMAL, MSG_MISSING_ANIMALS, MSG_SICK_LIVESTOCK, MSG_INJURED_LIVESTOCK,
		MSG_ANIMAL_IN_QUICKSAND, MSG_ANIMAL_DIED_ACCIDENT, MSG_ANIMAL_IN_HOLE};

	TestSave("events_base");
	for (int32_t i = 0; i < (int32_t) (sizeof(ids) / sizeof(ids[0])); ++i) {
		struct event_info e;
		if (!load_event(ids[i], &e) || !e.choice_count) {
			e.choice_count = 1;
			e.choices[0] = 0;
			StrCopy(e.labels[0], "(no menu)");
		}
		for (int32_t k = 0; k < e.choice_count; ++k) {
			struct party *p;
			struct inv_item *animal;
			struct test_state s;
			struct run r;
			TCase("event %d %s, choice %d %s", ids[i], e.name, e.choices[k], e.labels[k]);
			TestLoad("events_base");
			p = g_game->party;
			animal = InvItemFromNode(p->animals.base.head);
			if (!animal) {
				TNote("no draft animals in the start game");
				return;
			}
			r.id = ids[i];
			r.choice = e.choices[k];
			r.handled = -1;
			r.member = PartyCalcPullingMode(p);
			r.subject = (intptr_t) animal;
			r.payload = CStrCreateFrom("1 ox");
			r.kind = 0;
			InvItemRemoveUnits(animal, InvItemGetCount(animal, true));
			PartyPurgeEmptyAnimals(p);
			if (!TGuarded(handle_one, &r))
				continue;
			TestPump(150);
			TestCapture(&s);
			TestCheckInvariants(&s, "after an animal event whose animal is gone");
		}
	}
	TestLoad("events_base");
}

TEST_GAME(scn_events, impossible_choices)
{
	static const int32_t bad[] = {-1, 99};
	struct event_info e;

	TestSave("events_base");
	for (int32_t id = 0; id <= LAST_EVENT; ++id) {
		if (!load_event(id, &e))
			continue;
		for (int32_t k = 0; k < 2; ++k)
			run_case(id, bad[k], "(not offered)", NULL, e.name);
		run_case(id, e.choice_count, "(one past the last)", NULL, e.name);
	}
	TestLoad("events_base");
}

TEST_GAME(scn_events, impossible_events)
{
	static const int32_t unknown[] = {103, 150, 999, 1003, 1004, 5000, -1};

	TestSave("events_base");
	for (int32_t i = 0; i < (int32_t) (sizeof(unknown) / sizeof(unknown[0])); ++i)
		run_case(unknown[i], 0, "(unknown event)", NULL, "?");
	for (int32_t id = 0; id <= LAST_EVENT; ++id) {
		struct test_state s;
		struct run r;
		TCase("event %d with everyone dead", id);
		if (!TestLoad("events_base"))
			return;
		r.id = id;
		r.choice = 0;
		r.handled = -1;
		for (int32_t m = 0; m < g_game->party->member_count && m < 6; ++m)
			if (g_game->party->members[m])
				g_game->party->members[m]->state = MEMBER_ALIVE;
		prepare(&r);
		for (int32_t m = 0; m < g_game->party->member_count && m < 6; ++m)
			if (g_game->party->members[m])
				g_game->party->members[m]->state = MEMBER_DEAD;
		if (!TGuarded(handle_one, &r))
			continue;
		CHECK_EQ(r.handled, 2);
		TestCapture(&s);
		CHECK_EQ(s.alive, 0);
	}
	TestLoad("events_base");
}

TEST_GAME(scn_events, gathering_pounds)
{
	int32_t found = 0, lo = 99, hi = 0, added = 0;

	CHECK(TestSave("events_base"));
	for (int32_t seed = 0; seed < 300; ++seed) {
		const char *seen, *at;
		int32_t n;
		struct run r;

		TestLoad("events_base");
		g_game->date->month = MONTH_AUGUST;
		g_game->party->skills[SKILL_BOTANY] = true;
		TestScreensClear();
		RngSetSeed(TSeed() + 5000 + (uint32_t) seed);
		r.id = MSG_SCURVY;
		r.choice = 4;
		r.handled = -1;
		if (!prepare(&r) || !r.subject) {
			TNote("no scurvy to treat");
			break;
		}
		if (!TGuarded(handle_one, &r))
			break;
		TestPump(50);
		seen = TestScreensSeen();
		at = strstr(seen, "You found ");
		if (at && sscanf(at, "You found %" SCNd32, &n) == 1) {
			++found;
			if (n < lo)
				lo = n;
			if (n > hi)
				hi = n;
			CHECK_MSG(strstr(at, n == 1 ? " pound of " : " pounds of ") != NULL, "%s", at);
		}
	}
	if (found) {
		TNote("scurvy search: %d finds, %d-%d pounds", found, lo, hi);
		CHECK_RANGE(lo, 2, 25);
		CHECK_RANGE(hi, 2, 25);

	} else {
		CHECK_MSG(0, "the scurvy search found nothing in 300 tries (the base game's place is desert or mountains?)");
	}

	for (int32_t seed = 0; seed < 100; ++seed) {
		const char *seen, *at;
		int32_t n;
		struct run r;

		TestLoad("events_base");
		TestScreensClear();
		RngSetSeed(TSeed() + 7000 + (uint32_t) seed);
		r.id = seed & 1 ? MSG_WILD_FRUIT : MSG_WILD_VEGETABLES;
		r.choice = 1;
		r.handled = -1;
		if (!prepare(&r) || !TGuarded(handle_one, &r))
			break;
		TestPump(50);
		seen = TestScreensSeen();
		at = strstr(seen, "You've added ");
		if (at && sscanf(at, "You've added %" SCNd32, &n) == 1) {
			++added;
			CHECK_MSG(strstr(at, n == 1 ? " pound of " : " pounds of ") != NULL, "%s", at);
		}
	}
	CHECK(added > 0);
	TestLoad("events_base");
}

TEST(scn_events, pounds_word)
{
	CHECK(!strcmp(PoundsWord(1), "pound"));
	CHECK(!strcmp(PoundsWord(0), "pounds"));
	CHECK(!strcmp(PoundsWord(2), "pounds"));
	CHECK(!strcmp(PoundsWord(25), "pounds"));
}

const struct test_reg TESTS_scn_events[] = {
	TEST_GAME_ENTRY(scn_events, every_choice),
	TEST_GAME_ENTRY(scn_events, animal_gone_before_answer),
	TEST_GAME_ENTRY(scn_events, impossible_choices),
	TEST_GAME_ENTRY(scn_events, impossible_events),
	TEST_GAME_ENTRY(scn_events, gathering_pounds),
	TEST_ENTRY(scn_events, pounds_word),
	TEST_END
};

#endif
