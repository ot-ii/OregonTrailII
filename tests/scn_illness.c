#include "testframework.h"
#include "testsupport.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <string.h>

#include "../src/game/ailment.h"
#include "../src/base/cstr.h"
#include "../src/game/game.h"
#include "../src/base/list.h"
#include "../src/game/member.h"
#include "../src/game/party.h"
#include "../src/base/rng.h"
#include "../src/base/str.h"

static FILE *open_table(const char *suffix)
{
	char path[512];

	TestExeBase(path, sizeof(path));
	if (!path[0])
		return NULL;
	StrCat(path, suffix);
	return fopen(path, "w");
}

static struct member *patient(void)
{
	struct party *p = g_game->party;

	for (int32_t i = p->member_count > 1 ? 1 : 0; i < p->member_count; ++i)
		if (p->members[i] && p->members[i]->state == MEMBER_ALIVE)
			return p->members[i];
	return p->members[0];
}

static struct ailment *find(struct member *m, int32_t id)
{
	for (int32_t i = 0; i < 64; ++i) {
		struct ailment *a = OblistDataAt(m->nodes, i, false);
		if (!a)
			return NULL;
		if (a->def && a->def->id == id)
			return a;
	}
	return NULL;
}

static void name_of(struct ailment *a, char *out, size_t size)
{
	struct cstr *s = a && a->text ? AILMENT_OPS(a)->GetText(a, AILTEXT_NAME) : NULL;

	StrCopyN(out, size, "?");
	if (s) {
		StrCopyN(out, size, CStrGet(s));
		CStrDestroy(&s);
	}
}

struct life {
	int32_t id, start;
	int32_t days, died, ended, bad_stage, relapses;
	char name[48];
};

static void live(void *arg)
{
	struct life *l = arg;
	struct member *m = patient();
	struct ailment *a;
	int32_t prev_stage;

	l->days = l->died = l->ended = l->bad_stage = l->relapses = 0;
	if (!MemberAddAilment(m, l->id, l->start)) {
		StrCopy(l->name, "(not added)");
		return;
	}
	a = find(m, l->id);
	name_of(a, l->name, sizeof(l->name));
	prev_stage = a ? a->stage : 0;
	for (int32_t day = 0; day < 200 && a; ++day) {
		MemberHealthTick(m, 24, 0);
		++l->days;
		if (m->state != MEMBER_ALIVE) {
			l->died = 1;
			return;
		}
		a = find(m, l->id);
		if (!a) {
			l->ended = 1;
			return;
		}
		if (a->stage < prev_stage && a->stage != STAGE_NONE) {
			if ((a->def->flags & HPROB_FLAG_RELAPSE) && a->stage == STAGE_INCUBATION)
				++l->relapses;
			else
				++l->bad_stage;
		}
		prev_stage = a->stage;
		CHECK_RANGE(a->stage, STAGE_NONE, STAGE_RECOVERY);
		CHECK(a->days_left >= -1);
	}
}

struct treat {
	int32_t id, treatment;
	int32_t ok;
	int32_t severity, days_left, target, fc_inc, fc_acute, fid_inc, fid_acute;
};

static void apply_one(void *arg)
{
	struct treat *t = arg;
	struct member *m = patient();
	struct ailment *a;

	t->ok = 0;
	if (!MemberAddAilment(m, t->id, STAGE_ACUTE))
		return;
	a = find(m, t->id);
	if (!a)
		return;
	AILMENT_OPS(a)->ApplyEffects(a, t->treatment, false);
	t->severity = a->severity;
	t->days_left = a->days_left;
	t->target = a->target_severity;
	t->fc_inc = a->followup_chance_incubation;
	t->fc_acute = a->followup_chance_acute;
	t->fid_inc = a->followup_id_incubation;
	t->fid_acute = a->followup_id_acute;
	t->ok = 1;
}

TEST_GAME(scn_illness, every_ailment_lifecycle)
{
	FILE *table = open_table("_ailments.tsv");

	CHECK(TestSave("illness_base"));
	if (table)
		fprintf(table, "ailment\tname\tstart_stage\tdays\toutcome\n");
	for (int32_t id = 1; id <= AILMENT_WOUND; ++id)
		for (int32_t start = STAGE_INCUBATION; start <= STAGE_ACUTE; ++start) {
			struct life l;
			struct test_state s;
			memset(&l, 0, sizeof(l));
			l.id = id;
			l.start = start;
			StrCopy(l.name, "?");
			TCase("ailment %d, from stage %d", id, start);
			TestLoad("illness_base");
			RngSetSeed(TSeed() + 10 * (uint32_t) id + (uint32_t) start);
			if (!TGuarded(live, &l))
				continue;
			CHECK_EQ(l.bad_stage, 0);
			TestCapture(&s);
			TestCheckInvariants(&s, "after the illness");
			if (table)
				fprintf(table, "%d\t%s\t%d\t%d\t%s%s\n", id, l.name, start, l.days, l.relapses ? "relapsed, " : "",
					l.died ? "died" : l.ended ? "recovered" : !strcmp(l.name, "(not added)") ? "not added" : "still ill");
		}
	if (table)
		fclose(table);
	TestLoad("illness_base");
}

TEST_GAME(scn_illness, every_treatment)
{
	FILE *table = open_table("_treatments.tsv");

	TestSave("illness_base");
	if (table)
		fprintf(table, "ailment\ttreatment\tseverity\tdays_left\ttarget_severity\tfollowup_incubation\tchance\t"
			"followup_acute\tchance\n");
	for (int32_t id = 1; id <= AILMENT_WOUND; ++id)
		for (int32_t tr = TREAT_CONTINUE; tr <= TREAT_FLOUR_OR_GREASE; ++tr) {
			struct treat t;
			struct test_state s;
			memset(&t, 0, sizeof(t));
			t.id = id;
			t.treatment = tr;
			TCase("ailment %d, treatment %d", id, tr);
			TestLoad("illness_base");
			RngSetSeed(TSeed() + 100 * (uint32_t) id + (uint32_t) tr);
			if (!TGuarded(apply_one, &t))
				continue;
			TestCapture(&s);
			TestCheckInvariants(&s, "after a treatment");
			if (t.ok) {
				CHECK(t.severity >= 0);
				CHECK(t.fc_inc >= 0);
				CHECK(t.fc_acute >= 0);
				if (table)
					fprintf(table, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", id, tr, t.severity, t.days_left, t.target,
						t.fid_inc, t.fc_inc, t.fid_acute, t.fc_acute);
			}
		}
	if (table)
		fclose(table);
	TestLoad("illness_base");
}

TEST_GAME(scn_illness, impossible)
{
	struct member *m;
	struct test_state s;

	TestSave("illness_base");

	TCase("the same ailment twice");
	TestLoad("illness_base");
	m = patient();
	MemberAddAilment(m, AILMENT_CHOLERA, STAGE_ACUTE);
	MemberAddAilment(m, AILMENT_CHOLERA, STAGE_ACUTE);
	for (int32_t k = 0; k < 10; ++k)
		MemberHealthTick(m, 24, 0);
	TestCapture(&s);
	TestCheckInvariants(&s, "the same ailment twice");

	TCase("an ailment on a dead member");
	TestLoad("illness_base");
	m = patient();
	m->state = MEMBER_DEAD;
	m->health = 0;
	MemberAddAilment(m, AILMENT_DYSENTERY, STAGE_ACUTE);
	MemberHealthTick(m, 24, 0);
	TestCapture(&s);
	TestCheckInvariants(&s, "an ailment on a dead member");

	TCase("treatment codes out of range");
	TestLoad("illness_base");
	m = patient();
	MemberAddAilment(m, AILMENT_FROSTBITE, STAGE_ACUTE);
	{
		struct ailment *a = find(m, AILMENT_FROSTBITE);
		if (a) {
			AILMENT_OPS(a)->ApplyEffects(a, -1, false);
			AILMENT_OPS(a)->ApplyEffects(a, 30, false);
			AILMENT_OPS(a)->ApplyEffects(a, 255, false);
		}
	}
	TestCapture(&s);
	TestCheckInvariants(&s, "treatment codes out of range");

	TCase("ailment 0 with every treatment");
	TestLoad("illness_base");
	m = patient();
	if (MemberAddAilment(m, 0, STAGE_ACUTE)) {
		struct ailment *a = find(m, 0);
		for (int32_t k = TREAT_CONTINUE; a && k <= TREAT_FLOUR_OR_GREASE; ++k)
			AILMENT_OPS(a)->ApplyEffects(a, k, false);
		for (int32_t k = 0; k < 5; ++k)
			MemberHealthTick(m, 24, 0);
	}
	TestCapture(&s);
	TNote("ailment 0 after every treatment and 5 days: alive %d of %d", s.alive, s.members);

	TCase("an ailment number the game does not have");
	TestLoad("illness_base");
	m = patient();
	TExpectFatal(true);
	TNote("AddAilment(%d) returned %d", AILMENT_WOUND + 1, (int32_t) MemberAddAilment(m, AILMENT_WOUND + 1,
		STAGE_ACUTE));
	TExpectFatal(false);
	TestCapture(&s);
	TestCheckInvariants(&s, "an unknown ailment");

	TestLoad("illness_base");
}

const struct test_reg TESTS_scn_illness[] = {
	TEST_GAME_ENTRY(scn_illness, every_ailment_lifecycle),
	TEST_GAME_ENTRY(scn_illness, every_treatment),
	TEST_GAME_ENTRY(scn_illness, impossible),
	TEST_END
};

#endif
