#include "testsupport.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <string.h>

#include "../src/base/cstr.h"
#include "../src/base/file.h"
#include "../src/game/game.h"
#include "../src/game/gamedoc.h"
#include "../src/game/invitem.h"
#include "../src/game/invlist.h"
#include "../src/base/list.h"
#include "../src/game/member.h"
#include "../src/game/party.h"
#include "../src/base/path.h"
#include "../src/base/str.h"
#include "../src/base/date.h"
#include "testframework.h"

void TestStatePath(const char *name, char *path, size_t size)
{
	FileGetExeDir(path, size);
	StrCatN(path, size, "test_state");
	TestMakeDir(path);
	StrCatN(path, size, "\\");
	StrCatN(path, size, name);
	StrCatN(path, size, ".ot2");
}

static int32_t count_units(struct inv_list *list)
{
	int32_t n = 0;

	for (struct list_node *p = list->base.head; p; p = p->next)
		n += InvItemGetCount(InvItemFromNode(p), true);
	return n;
}

static int32_t ailments(struct member *m)
{
	int32_t n = 0;

	if (!m || !m->nodes)
		return 0;
	while (n < 64 && OblistDataAt(m->nodes, n, false))
		++n;
	return n;
}

static struct inv_item *find_item(const char *name)
{
	if (!g_game || !g_game->party)
		return NULL;
	struct cstr *s = CStrCreateFrom(name);
	struct inv_item *it = InvListFindByName(&g_game->party->inventory, s);
	CStrDestroy(&s);
	return it;
}

bool TestSave(const char *name)
{
	char path[512];

	TestStatePath(name, path, sizeof(path));
	FileDelete(path);
	struct cstr *text = CStrCreateFrom(path);
	bool ok = GameDocWriteCopy(PathCreateStr(text)) == 0;
	CStrDestroy(&text);
	return ok;
}

bool TestLoad(const char *name)
{
	char path[512];

	TestStatePath(name, path, sizeof(path));
	struct cstr *text = CStrCreateFrom(path);
	bool ok = GameDocReadIntoGame(PathCreateStr(text)) == 0;
	CStrDestroy(&text);
	TestPump(50);
	return ok;
}

void TestCapture(struct test_state *s)
{
	struct party *p;

	memset(s, 0, sizeof(*s));
	if (!g_game || !g_game->party)
		return;
	p = g_game->party;
	s->cash = p->cash;
	s->food = PartyGetFoodPounds(p);
	s->members = p->member_count;
	for (int32_t i = 0; i < 6; ++i) {
		struct member *m = i < p->member_count ? p->members[i] : NULL;
		if (!m)
			continue;
		s->health[i] = m->health;
		s->max_health[i] = m->max_health;
		s->state[i] = m->state;
		s->ailments[i] = ailments(m);
		if (m->state == MEMBER_ALIVE)
			++s->alive;
	}
	s->animals = count_units(&p->animals);
	s->spare_animals = count_units(&p->spare_animals);
	s->wagons = count_units(&p->wagons);
	s->inventory_items = count_units(&p->inventory);
	s->weight = InvListTotalWeight(&p->inventory);
	if (g_game->date)
		s->date_key = 372 * g_game->date->year + 31 * g_game->date->month + g_game->date->day;
	s->days_traveled = g_game->days_traveled;
	s->travel_state = g_game->travel_state;
	s->pace = g_game->travel_pace;
	s->rations = p->ration_level;
	s->morale = p->reputation;
	s->node = g_game->cur_node_id;
}

#define DIFF(field, label) if (a->field != b->field && n < size - 64) \
	n += snprintf(out + n, (size_t) (size - n), "%s" label " %+d", n ? ", " : "", b->field - a->field)

void TestDiff(const struct test_state *a, const struct test_state *b, char *out, int32_t size)
{
	int32_t n = 0;

	out[0] = 0;
	if ((a->cash - b->cash >= 0.005 || b->cash - a->cash >= 0.005) && n < size - 64)
		n += snprintf(out + n, (size_t) (size - n), "%scash %+.2f", n ? ", " : "", b->cash - a->cash);
	DIFF(food, "food");
	DIFF(alive, "alive");
	DIFF(animals, "animals");
	DIFF(spare_animals, "spare animals");
	DIFF(wagons, "wagons");
	DIFF(inventory_items, "items");
	DIFF(weight, "weight");
	DIFF(date_key, "days");
	DIFF(morale, "morale");
	DIFF(pace, "pace");
	DIFF(rations, "rations");
	DIFF(travel_state, "travel state");
	for (int32_t i = 0; i < 6; ++i) {
		if (a->health[i] != b->health[i] && n < size - 64)
			n += snprintf(out + n, (size_t) (size - n), "%shealth[%d] %+d", n ? ", " : "", i,
				b->health[i] - a->health[i]);
		if (a->ailments[i] != b->ailments[i] && n < size - 64)
			n += snprintf(out + n, (size_t) (size - n), "%sailments[%d] %+d", n ? ", " : "", i,
				b->ailments[i] - a->ailments[i]);
		if (a->state[i] != b->state[i] && n < size - 64)
			n += snprintf(out + n, (size_t) (size - n), "%sstate[%d] %d->%d", n ? ", " : "", i, a->state[i],
				b->state[i]);
	}
}

#undef DIFF

#define INV(cond) do { bool ok_ = !!(cond); bad += !ok_; TCheck(ok_, #cond, __FILE__, __LINE__, "%s", when); } while (0)

int32_t TestCheckInvariants(const struct test_state *s, const char *when)
{
	int32_t bad = 0;

	INV(s->cash >= 0);
	INV(s->food >= 0);
	INV(s->members >= 1 && s->members <= 6);
	INV(s->alive >= 0 && s->alive <= s->members);
	INV(s->animals >= 0 && s->spare_animals >= 0 && s->wagons >= 0);
	INV(s->inventory_items >= 0);
	INV(s->weight >= 0);
	for (int32_t i = 0; i < s->members && i < 6; ++i) {
		bool ok_ = s->health[i] >= 0 && (s->health[i] <= s->max_health[i] || s->state[i] != MEMBER_ALIVE);
		bad += !ok_;
		TCheck(ok_, "0 <= health <= max_health", __FILE__, __LINE__, "%s: member %d health %d of %d, state %d", when,
			i, s->health[i], s->max_health[i], s->state[i]);
	}
	return bad;
}

#undef INV

int32_t TestItemCount(const char *name)
{
	struct inv_item *it = find_item(name);

	return it ? InvItemGetCount(it, true) : 0;
}

bool TestSetItem(const char *name, int32_t units)
{
	struct inv_item *it = find_item(name);

	if (!g_game || !g_game->party)
		return false;
	struct cstr *s = CStrCreateFrom(name);
	if (LookupItemId(s) < 0) {
		TCheck(false, "a catalog item", __FILE__, __LINE__, "\"%s\" is not an item key (see INVIT 2100)", name);
		CStrDestroy(&s);
		return false;
	}
	if (!it) {
		if (units > 0)
			InvListAdd(&g_game->party->inventory, s, units, 0);
		CStrDestroy(&s);
		return true;
	}
	CStrDestroy(&s);
	if (units > InvItemGetCount(it, true))
		InvItemAddUnits(it, units - InvItemGetCount(it, true));
	else if (units < InvItemGetCount(it, true))
		InvItemRemoveUnits(it, InvItemGetCount(it, true) - units);
	return true;
}

#endif
