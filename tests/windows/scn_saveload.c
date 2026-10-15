#include "../testframework.h"
#include "../testsupport.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "../../src/game/ailment.h"
#include "../../src/base/byteorder.h"
#include "../../src/base/cstr.h"
#include "../../src/base/date.h"
#include "../../src/game/game.h"
#include "../../src/game/invitem.h"
#include "../../src/game/invlist.h"
#include "../../src/game/journal.h"
#include "../../src/base/list.h"
#include "../../src/game/member.h"
#include "../../src/base/memory.h"
#include "../../src/app/otiiapp.h"
#include "../../src/game/party.h"
#include "../../src/app/replay.h"
#include "../../src/base/rng.h"
#include "../../src/base/str.h"
#include "../../src/data/strrsrc.h"
#include "../../src/app/app.h"
#include "../../src/ui/framewin.h"
#include "../../src/media/mcivideo.h"
#include "../../src/ui/view.h"
#include "../../src/ui/window.h"

static void state_file(const char *name, char *path, size_t size)
{
	char *slash;

	GetModuleFileNameA(NULL, path, (DWORD) size);
	slash = strrchr(path, '\\');
	if (slash)
		slash[1] = 0;
	StrCatN(path, size, "test_state\\");
	StrCatN(path, size, name);
	StrCatN(path, size, ".ot2");
}

static long read_all(const char *name, unsigned char **data)
{
	char path[MAX_PATH];
	FILE *f;
	long n;

	*data = NULL;
	state_file(name, path, sizeof(path));
	if (fopen_s(&f, path, "rb") || !f)
		return -1;
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	*data = (unsigned char *) malloc(n > 0 ? (size_t) n : 1);
	if (*data && fread(*data, 1, (size_t) n, f) != (size_t) n)
		n = -1;
	fclose(f);
	return n;
}

static void write_all(const char *name, const unsigned char *data, long n)
{
	char path[MAX_PATH];
	FILE *f;

	state_file(name, path, sizeof(path));
	if (fopen_s(&f, path, "wb") || !f)
		return;
	if (n > 0)
		fwrite(data, 1, (size_t) n, f);
	fclose(f);
}

static void item_list(char *out, int size)
{
	int n = 0;

	out[0] = 0;
	for (struct list_node *node = g_game->party->inventory.base.head; node && n < size - 100; node = node->next) {
		struct inv_item *it = InvItemFromNode(node);
		const char *name = it->name ? CStrGet(it->name) : NULL;
		n += snprintf(out + n, (size_t) (size - n), "%s %d+%d/%d\n", name ? name : "?", it->quantity,
			it->partial_portions, it->days_to_spoil);
	}
}

static void list_diff(const char *a, const char *b, char *out, int size)
{
	int n = 0;

	out[0] = 0;
	for (int pass = 0; pass < 2; ++pass) {
		const char *x = pass ? b : a, *y = pass ? a : b, *line = x;
		while (*line && n < size - 100) {
			const char *end = strchr(line, '\n');
			int len = end ? (int) (end - line) : (int) strlen(line), found = 0;
			const char *p = y;
			while (*p) {
				const char *pe = strchr(p, '\n');
				int pl = pe ? (int) (pe - p) : (int) strlen(p);
				if (pl == len && !strncmp(p, line, (size_t) len)) {
					found = 1;
					break;
				}
				p = pe ? pe + 1 : p + pl;
			}
			if (!found)
				n += snprintf(out + n, (size_t) (size - n), "%s%.*s; ", pass ? "after: " : "before: ", len, line);
			line = end ? end + 1 : line + len;
		}
	}
}

static void round_trip(const char *label)
{
	struct test_state before, after;
	unsigned char *a, *b;
	long na, nb;
	uint32_t seed;
	uint64_t calls;
	char diff[512];

	static char items_before[16384], items_after[16384], items_diff[4096];

	TCase("%s", label);
	TestCapture(&before);
	item_list(items_before, sizeof(items_before));
	seed = RngSeed();
	calls = RngCalls();
	CHECK(TestSave("rt_a"));
	CHECK(TestLoad("rt_a"));
	TestCapture(&after);
	item_list(items_after, sizeof(items_after));
	TestDiff(&before, &after, diff, sizeof(diff));
	list_diff(items_before, items_after, items_diff, sizeof(items_diff));
	CHECK_MSG(diff[0] == 0, "changed by save and load: %s", diff);
	CHECK_MSG(items_diff[0] == 0, "items changed by save and load: %s", items_diff);
	CHECK_EQ(RngSeed(), seed);
	CHECK_EQ(RngCalls(), calls);
	CHECK(TestSave("rt_b"));
	na = read_all("rt_a", &a);
	nb = read_all("rt_b", &b);
	CHECK(na > 0);
	CHECK_EQ(na, nb);
	if (na > 0 && na == nb && a && b) {
		long i;
		for (i = 0; i < na && a[i] == b[i]; ++i)
			;
		if (i < na)
			TNote("%s: the second save differs from the first from byte %ld of %ld", label, i, na);
	}
	free(a);
	free(b);
}

TEST_GAME(scn_saveload, round_trips)
{
	CHECK(TestSave("saveload_base"));
	round_trip("the start game");

	TestLoad("saveload_base");
	g_game->party->cash = 12.34;
	g_game->party->reputation = 77;
	round_trip("money and morale changed");

	TestLoad("saveload_base");
	for (int32_t i = 0; i < g_game->party->member_count; ++i)
		if (g_game->party->members[i] && g_game->party->members[i]->state == MEMBER_ALIVE) {
			MemberAddAilment(g_game->party->members[i], AILMENT_DYSENTERY, STAGE_ACUTE);
			break;
		}
	round_trip("someone has dysentery");

	TestLoad("saveload_base");
	if (g_game->party->member_count > 1 && g_game->party->members[1]) {
		g_game->party->members[1]->state = MEMBER_DEAD;
		g_game->party->members[1]->health = 0;
	}
	round_trip("a party member is dead");

	TestLoad("saveload_base");
	TestSetItem("flour", 3);
	TestSetItem("spare wagon wheel", 0);
	round_trip("supplies changed");

	TestLoad("saveload_base");
	for (int32_t i = 0; i < 40; ++i)
		RandRangeAvg(1, 100, 1);
	round_trip("the random stream moved on");

	TestLoad("saveload_base");
}

TEST_GAME(scn_saveload, load_rolls)
{
	uint64_t before, after;

	TestSave("load_rolls");
	before = RngCalls();
	RpTraceRand(-1, -1, 0, 0);
	TestLoad("load_rolls");
	after = RngCalls();
	RpTraceRand(-2, -2, 0, 0);
	fflush(NULL);
	TNote("a load: rng_calls %llu -> %llu", (unsigned long long) before, (unsigned long long) after);
	CHECK(1);
}

static struct journal *the_journal(void)
{
	return &((struct otii_app *) g_app)->journal;
}

TEST_GAME(scn_saveload, journal_dates)
{
	static const struct { int32_t m, d, y; const char *text; } days[] = {
		{1, 31, 1850, "january 31"}, {2, 15, 1850, "february 15"}, {3, 31, 1850, "march 31"}, {4, 1, 1850, "april 1"},
		{5, 31, 1850, "may 31"}, {6, 2, 1850, "june 2"}, {12, 31, 1850, "december 31"}, {2, 28, 1851, "february 28"},
	};
	const int32_t count = (int32_t) (sizeof(days) / sizeof(days[0]));
	struct journal *j = the_journal();

	JournalClear(j);
	for (int32_t i = 0; i < count; ++i) {
		struct date d = DateMake(days[i].m, days[i].d, days[i].y);
		JournalSetDate(j, &d);
		JournalAddLine(j, CStrCreateFrom(days[i].text));
	}
	CHECK(TestSave("journal_dates"));
	CHECK(TestLoad("journal_dates"));
	for (int32_t i = 0; i < count; ++i) {
		struct date d = DateMake(days[i].m, days[i].d, days[i].y);
		struct strrsrc *lines;
		const char *line;

		TCase("%s", days[i].text);
		JournalSetDate(j, &d);
		lines = JournalGetEntryLines(j);
		CHECK_MSG(StrRsrcCount(lines) == 1, "the entry for %d/%d/%d has %d lines after loading", days[i].m, days[i].d,
			days[i].y, StrRsrcCount(lines));
		if (StrRsrcCount(lines) < 1)
			continue;
		line = StrRsrcGetAt(lines, 0);
		CHECK_MSG(line && !strcmp(line, days[i].text), "the entry for %d/%d/%d reads \"%s\"", days[i].m, days[i].d,
			days[i].y, line ? line : "(null)");
	}
	TCase("");
}

static struct inv_item *find_cornmeal(void)
{
	struct inv_item *it = NULL;

	for (struct list_node *node = g_game->party->inventory.base.head; node; node = node->next) {
		struct inv_item *x = InvItemFromNode(node);
		if (x->name && !strcmp(CStrGet(x->name), "cornmeal"))
			it = x;
	}
	return it;
}

TEST_GAME(scn_saveload, partial_units)
{
	struct inv_item *it;

	CHECK(TestSetItem("cornmeal", 4));
	it = find_cornmeal();
	CHECK(it != NULL);
	if (!it)
		return;
	it->partial_portions = 25;
	CHECK(TestSave("partial_units"));
	CHECK(TestLoad("partial_units"));
	it = find_cornmeal();
	CHECK(it != NULL);
	if (!it)
		return;
	CHECK_EQ(it->quantity, 4);
	CHECK_EQ(it->partial_portions, 25);
}

TEST_GAME(scn_saveload, spoil_days)
{
	struct list_node *node;
	struct inv_item *it = NULL;
	int32_t fresh, index;

	for (node = g_game->party->inventory.base.head; node && !it; node = node->next)
		if (InvItemFromNode(node)->days_to_spoil > 5)
			it = InvItemFromNode(node);
	if (!it) {
		CHECK(TestSetItem("bacon", 10));
		for (node = g_game->party->inventory.base.head; node && !it; node = node->next)
			if (InvItemFromNode(node)->days_to_spoil > 5)
				it = InvItemFromNode(node);
	}
	CHECK(it != NULL);
	if (!it)
		return;
	fresh = it->days_to_spoil;
	it->days_to_spoil = 3;
	index = it->table_index;
	TNote("item %d: %d days fresh, set to 3", index, fresh);
	CHECK(TestSave("spoil_days"));
	CHECK(TestLoad("spoil_days"));
	for (node = g_game->party->inventory.base.head; node; node = node->next)
		if (InvItemFromNode(node)->table_index == index)
			break;
	CHECK(node != NULL);
	if (!node)
		return;
	CHECK_EQ(InvItemFromNode(node)->days_to_spoil, 3);
}

static struct ailment *find_ailment(struct member *m, int32_t id)
{
	for (int32_t i = 0; m && i < 64; ++i) {
		struct ailment *a = OblistDataAt(m->nodes, i, false);
		if (!a)
			return NULL;
		if (a->def && a->def->id == id)
			return a;
	}
	return NULL;
}

static struct member *living_member(void)
{
	for (int32_t i = 0; i < g_game->party->member_count; ++i)
		if (g_game->party->members[i] && g_game->party->members[i]->state == MEMBER_ALIVE)
			return g_game->party->members[i];
	return NULL;
}

TEST_GAME(scn_saveload, followup_ids)
{
	struct member *m = living_member();
	struct ailment *a;
	int32_t who;

	CHECK(m != NULL);
	if (!m)
		return;
	for (who = 0; g_game->party->members[who] != m; ++who)
		;
	MemberAddAilment(m, AILMENT_DYSENTERY, STAGE_ACUTE);
	a = find_ailment(m, AILMENT_DYSENTERY);
	CHECK(a != NULL);
	if (!a)
		return;
	a->followup_chance_incubation = 40;
	a->followup_id_incubation = AILMENT_INFECTION;
	a->followup_chance_acute = 30;
	a->followup_id_acute = AILMENT_PNEUMONIA;
	CHECK(TestSave("followup_ids"));
	CHECK(TestLoad("followup_ids"));
	a = find_ailment(g_game->party->members[who], AILMENT_DYSENTERY);
	CHECK(a != NULL);
	if (!a)
		return;
	CHECK_EQ(a->followup_id_incubation, AILMENT_INFECTION);
	CHECK_EQ(a->followup_chance_incubation, 40);
	CHECK_EQ(a->followup_id_acute, AILMENT_PNEUMONIA);
	CHECK_EQ(a->followup_chance_acute, 30);
}

static void dirty_freed_memory(void)
{
	static void *h[512];
	int n = 0;

	for (int i = 1; i <= 1300 && n < 512; i += (i < 64 ? 1 : 7)) {
		h[n] = MemAlloc(1, (size_t) i);
		if (h[n]) {
			memset(h[n], 0xEE, (size_t) i);
			++n;
		}
	}
	while (n > 0)
		MemFree(h[--n]);
}

struct save_reader {
	const unsigned char *d;
	long n, p;
	int bad;
};

static const unsigned char *next_block(struct save_reader *r, long *len)
{
	long l;

	if (r->p + 4 > r->n) {
		r->bad = 1;
		return NULL;
	}
	l = ReadLe32(r->d + r->p);
	if (l < 0 || r->p + 4 + l > r->n) {
		r->bad = 1;
		return NULL;
	}
	*len = l;
	r->p += 4 + l;
	return r->d + r->p - l;
}

static void check_unused(const char *what, const unsigned char *b, long used, long len)
{
	long nonzero = 0, first = -1;

	for (long i = used; i < len; ++i)
		if (b[i]) {
			if (first < 0)
				first = i;
			++nonzero;
		}
	CHECK_MSG(nonzero == 0, "%s: %ld of its %ld unused bytes are not zero (from byte %ld of %ld: %02X)", what, nonzero,
		len - used, first, len, first >= 0 ? b[first] : 0);
}

static void check_text(const char *what, const unsigned char *b, long len)
{
	const unsigned char *end = (const unsigned char *) memchr(b, 0, (size_t) len);

	CHECK_MSG(end != NULL, "%s: the text has no end", what);
	if (end)
		check_unused(what, b, (long) (end - b) + 1, len);
}

TEST_GAME(scn_saveload, no_stale_bytes)
{
	struct journal *j = the_journal();
	unsigned char *data;
	struct save_reader r;
	const unsigned char *b;
	long len, n;
	int members, m, list, k;
	char what[64];

	JournalSetDate(j, g_game->date);
	JournalAddLine(j, CStrCreateFrom("A long line for the journal, so the block the next shorter lines are written into held it."));
	JournalAddLine(j, CStrCreateFrom("Short."));
	JournalAddLine(j, CStrCreateFrom(""));
	if (g_game->party->member_count > 0)
		MemberAddAilment(g_game->party->members[0], 1, 0);
	dirty_freed_memory();
	CHECK(TestSave("no_stale_bytes"));
	n = read_all("no_stale_bytes", &data);
	CHECK(n > 10);
	if (n <= 10 || !data) {
		free(data);
		return;
	}
	r.d = data;
	r.n = n;
	r.p = 10;
	r.bad = 0;

	b = next_block(&r, &len);
	if (b)
		check_unused("app state", b, 4, len);
	next_block(&r, &len);
	b = next_block(&r, &len);
	if (b && len >= 84) {
		int32_t path_len = ReadLe16(b + 82);
		CHECK_MSG(len == 84 + 2 * (path_len > 0 ? path_len : 0), "the game block is %ld bytes for a path of %d", len,
			path_len);
	}
	b = next_block(&r, &len);
	CHECK_MSG(b && len == 12, "the random position block is %ld bytes, not 12", b ? len : -1);
	b = next_block(&r, &len);
	if (b)
		check_unused("slot flags", b, 1200, len);
	next_block(&r, &len);
	b = next_block(&r, &len);
	members = 0;
	if (b && len >= 2) {
		members = b[1];
		check_unused("party", b, 39, len);
	}
	for (m = 0; m < members && !r.bad; ++m) {
		int ailments;
		b = next_block(&r, &len);
		if (!b || len < 1)
			break;
		ailments = b[0];
		StrPrintf(what, "member %d", m);
		{
			const unsigned char *name_end = (const unsigned char *) memchr(b + 1, 0, (size_t) (len - 1));
			if (name_end)
				check_unused(what, b, (long) (name_end - b) + 1 + 10 + 10, len);
		}
		for (k = 0; k < ailments && !r.bad; ++k) {
			b = next_block(&r, &len);
			StrPrintf(what, "member %d ailment %d", m, k);
			if (b)
				check_unused(what, b, 53, len);
		}
	}
	for (list = 0; list < 4 && !r.bad; ++list) {
		int32_t count = 0;
		b = next_block(&r, &len);
		if (b && len >= 2)
			count = ReadLe16(b);
		for (k = 0; k < count && !r.bad; ++k) {
			b = next_block(&r, &len);
			StrPrintf(what, "list %d item %d", list, k);
			if (b)
				check_unused(what, b, 12, len);
		}
	}
	next_block(&r, &len);
	for (k = 0; k < 2000 && !r.bad; ++k) {
		int32_t head[4] = {0, 0, 0, 0};
		b = next_block(&r, &len);
		if (!b || len < 2)
			break;
		for (int h = 0; h < (len >= 8 ? 4 : 1); ++h)
			head[h] = ReadLe16(b + 2 * h);
		if (!head[0])
			break;
		for (int line = 0; line < head[3] && !r.bad; ++line) {
			b = next_block(&r, &len);
			StrPrintf(what, "journal %d/%d/%d line %d", head[1], head[2], head[0], line);
			if (b)
				check_text(what, b, len);
		}
		b = next_block(&r, &len);
		StrPrintf(what, "journal %d/%d/%d text", head[1], head[2], head[0]);
		if (b)
			check_text(what, b, len);
	}
	CHECK_MSG(!r.bad && r.p == r.n, "the save did not parse to its end (%ld of %ld bytes)", r.p, r.n);
	free(data);
}

TEST_GAME(scn_saveload, format_2_0)
{
	unsigned char *data;
	struct save_reader r;
	const unsigned char *b;
	long n, len;
	uint32_t seed, saved_seed;
	uint64_t calls, saved_calls;

	seed = RngSeed();
	calls = RngCalls();
	CHECK(TestSave("format_2_0"));
	n = read_all("format_2_0", &data);
	CHECK(n > 10);
	if (n <= 10 || !data) {
		free(data);
		return;
	}
	CHECK_MSG(data[8] == 0x02 && data[9] == 0x00, "the save is format %d.%d", data[8], data[9]);
	r.d = data;
	r.n = n;
	r.p = 10;
	r.bad = 0;
	next_block(&r, &len);
	next_block(&r, &len);
	b = next_block(&r, &len);
	CHECK(b && len >= 84);
	if (b && len >= 84)
		CHECK_EQ(len, 84 + 2 * ReadLe16(b + 82));
	b = next_block(&r, &len);
	CHECK_EQ(b ? len : -1, 12);
	if (b && len == 12) {
		saved_seed = ReadLeU32(b);
		saved_calls = ReadLeU64(b + 4);
		CHECK_EQ(saved_seed, seed);
		CHECK_EQ((int64_t) saved_calls, (int64_t) calls);
	}
	b = next_block(&r, &len);
	CHECK_EQ(b ? len : -1, 1250);
	free(data);
}

TEST_GAME(scn_saveload, reads_format_1_2)
{
	uint32_t seed = RngSeed();
	struct ailment *a = NULL;

	TCase("the original game's save");
	CHECK(TestLoad("v12_original"));
	CHECK_EQ(g_game->date->year, 1840);
	CHECK_EQ(g_game->date->month, 10);
	CHECK_EQ(g_game->date->day, 31);
	CHECK_EQ(g_game->days_traveled, 91);
	CHECK_EQ(g_game->total_miles, 1131);
	CHECK_EQ(g_game->cur_node_id, 148);
	CHECK_EQ(g_game->party->member_count, 6);
	CHECK_NEAR(g_game->party->cash, 293.05, 0.001);
	CHECK(g_game->party->members[0] && !strcmp(CStrGet(g_game->party->members[0]->name), "Kelp"));
	CHECK_EQ(RngSeed(), seed);

	TCase("this build's format 1.2");
	CHECK(TestLoad("v12_packed_followup"));
	CHECK_EQ(g_game->date->year, 1857);
	CHECK_EQ(g_game->party->member_count, 5);
	CHECK_NEAR(g_game->party->cash, 1205.09, 0.001);
	CHECK_EQ(RngSeed(), 12345u);
	CHECK_EQ((int64_t) RngCalls(), 8);
	for (int32_t i = 0; i < g_game->party->member_count && !a; ++i)
		a = find_ailment(g_game->party->members[i], AILMENT_DYSENTERY);
	CHECK(a != NULL);
	if (a) {
		CHECK_EQ(a->followup_id_incubation, AILMENT_INFECTION);
		CHECK_EQ(a->followup_id_acute, AILMENT_PNEUMONIA);
		CHECK_EQ(a->followup_chance_incubation, 40);
		CHECK_EQ(a->followup_chance_acute, 30);
	}
	TCase("");
}

TEST_GAME(scn_saveload, load_forgets_ferry_wait)
{
	int32_t days, year, month, day;

	CHECK(TestSave("ferry_wait_base"));
	days = g_game->days_traveled;
	year = g_game->date->year;
	month = g_game->date->month;
	day = g_game->date->day;
	GameTestSetFerryWait(true, 3);
	CHECK(TestLoad("ferry_wait_base"));
	CHECK_EQ(GameTestFerryWaitPending(), 0);
	CHECK_EQ(g_game->days_traveled, days);
	CHECK_EQ(g_game->date->year, year);
	CHECK_EQ(g_game->date->month, month);
	CHECK_EQ(g_game->date->day, day);
	GameTestSetFerryWait(false, 0);
}

TEST_GAME(scn_saveload, reads_every_old_save)
{
	char pattern[MAX_PATH], name[MAX_PATH], *dot;
	WIN32_FIND_DATAA fd;
	HANDLE find;
	struct test_state st;
	int count = 0, node;

	state_file("v12_node_*", pattern, sizeof(pattern));
	find = FindFirstFileA(pattern, &fd);
	if (find == INVALID_HANDLE_VALUE) {
		CHECK_MSG(0, "no fixtures/v12_node_*.ot2 next to the test build (run_tests.py copies them)");
		return;
	}
	do {
		StrCopy(name, fd.cFileName);
		if ((dot = strrchr(name, '.')) != NULL)
			*dot = 0;
		node = atoi(name + 9);
		TCase("%s", name);
		if (!TestLoad(name)) {
			CHECK_MSG(0, "%s did not load", name);
			continue;
		}
		++count;
		CHECK_EQ(g_game->cur_node_id, node);
		CHECK(g_game->cur_node != NULL);
		CHECK_RANGE(g_game->date->year, 1840, 1870);
		CHECK_RANGE(g_game->party->member_count, 1, 6);
		TestCapture(&st);
		TestCheckInvariants(&st, name);
	} while (FindNextFileA(find, &fd));
	FindClose(find);
	TCase("");
	TNote("%d old saves loaded", count);
	CHECK(count >= 90);
}

static bool is_fixture(const char *name)
{
	return !strncmp(name, "v12_", 4) || !_stricmp(name, "raft_dalles.ot2") ||
		!_stricmp(name, "autopilot_mud_stuck.ot2");
}

TEST_GAME(scn_saveload, resave_fixtures)
{
	static char names[256][64];
	char pattern[MAX_PATH], name[80], *dot;
	WIN32_FIND_DATAA fd;
	HANDLE find;
	int count = 0;

	state_file("*", pattern, sizeof(pattern));
	find = FindFirstFileA(pattern, &fd);
	if (find == INVALID_HANDLE_VALUE) {
		CHECK_MSG(0, "no fixtures next to the test build (run_tests.py copies them)");
		return;
	}
	do {
		if (is_fixture(fd.cFileName) && count < 256 && strlen(fd.cFileName) < sizeof(names[0])) {
			StrCopy(names[count], fd.cFileName);
			if ((dot = strrchr(names[count], '.')) != NULL)
				*dot = 0;
			++count;
		}
	} while (FindNextFileA(find, &fd));
	FindClose(find);
	if (count > 0)
		TestLoad(names[0]);
	for (int i = 0; i < count; ++i) {
		unsigned char *a, *b;
		long na, nb;

		TCase("%s", names[i]);
		RngRestore(12345, 0);
		if (!TestLoad(names[i])) {
			CHECK_MSG(0, "%s did not load", names[i]);
			continue;
		}
		StrPrintf(name, "resave_%s", names[i]);
		CHECK(TestSave(name));
		RngRestore(12345, 0);
		CHECK(TestLoad(name));
		StrPrintf(name, "resave2_%s", names[i]);
		CHECK(TestSave(name));
		StrPrintf(name, "resave_%s", names[i]);
		na = read_all(name, &a);
		StrPrintf(name, "resave2_%s", names[i]);
		nb = read_all(name, &b);
		CHECK(na > 0);
		CHECK_MSG(na == nb && a && b && !memcmp(a, b, (size_t) na),
			"%s: the re-save saved again differs (%ld, %ld bytes)", names[i], na, nb);
		free(a);
		free(b);
	}
	TCase("");
	TNote("%d fixtures re-saved", count);
	CHECK(count >= 90);
}

TEST_GAME(scn_saveload, load_closes_the_movie)
{
	struct view *root = g_parent_win ? g_parent_win->base.root_view : NULL;
	struct rect r = RectMake(100, 100, 204, 208);
	struct mci_video *video;
	HWND hwnd;

	CHECK(TestSave("movie_base"));
	CHECK(root != NULL);
	if (!root || !g_game)
		return;

	video = MciVideoCreate(-2, &g_game->base.eh_event, &r, 2502, false, false);
	VIEW_OPS(root)->AddChild(root, (struct view *) video);
	MCI_VIDEO_OPS(video)->SetVolume(video, 0);
	MCI_VIDEO_OPS(video)->Play(video);
	TestPump(300);
	hwnd = (HWND) video->base.hwnd;
	CHECK_MSG(hwnd && IsWindowVisible(hwnd), "the movie did not open a window");
	CHECK(TestLoad("movie_base"));
	TestPump(300);
	CHECK_MSG(!hwnd || !IsWindow(hwnd), "the movie window outlived the load");
}

TEST_GAME(scn_saveload, damaged_files)
{
	unsigned char *good;
	long n;
	static const struct { const char *what; int kind; } cases[] = {
		{"empty file", 0}, {"cut to 10 bytes", 1}, {"cut in half", 2}, {"last byte missing", 3},
		{"body zeroed", 4}, {"random bytes after the header", 5}, {"header only", 6},
	};

	TestSave("saveload_base");
	n = read_all("saveload_base", &good);
	CHECK(n > 64);
	if (n <= 64 || !good)
		return;
	for (int c = 0; c < (int) (sizeof(cases) / sizeof(cases[0])); ++c) {
		unsigned char *bad = (unsigned char *) malloc((size_t) n);
		long len = n;
		bool loaded;
		struct test_state s;
		if (!bad)
			break;
		memcpy(bad, good, (size_t) n);
		switch (cases[c].kind) {
			case 0:
				len = 0;
				break;
			case 1:
				len = 10;
				break;
			case 2:
				len = n / 2;
				break;
			case 3:
				len = n - 1;
				break;
			case 4:
				memset(bad + 32, 0, (size_t) (n - 32));
				break;
			case 5:
				srand(1234);
				for (long i = 32; i < n; ++i)
					bad[i] = (unsigned char) rand();
				break;
			case 6:
				len = 32;
				break;
		}
		TCase("%s", cases[c].what);
		write_all("damaged", bad, len);
		free(bad);
		TestLoad("saveload_base");
		loaded = TestLoad("damaged");
		if (loaded) {
			TestCapture(&s);
			TestCheckInvariants(&s, cases[c].what);
		}
		TNote("%s: %s", cases[c].what, loaded ? "loaded" : "refused");
	}
	free(good);
	TestLoad("saveload_base");
}

const struct test_reg TESTS_scn_saveload[] = {
	TEST_GAME_ENTRY(scn_saveload, round_trips),
	TEST_GAME_ENTRY(scn_saveload, load_rolls),
	TEST_GAME_ENTRY(scn_saveload, journal_dates),
	TEST_GAME_ENTRY(scn_saveload, partial_units),
	TEST_GAME_ENTRY(scn_saveload, spoil_days),
	TEST_GAME_ENTRY(scn_saveload, followup_ids),
	TEST_GAME_ENTRY(scn_saveload, no_stale_bytes),
	TEST_GAME_ENTRY(scn_saveload, format_2_0),
	TEST_GAME_ENTRY(scn_saveload, reads_format_1_2),
	TEST_GAME_ENTRY(scn_saveload, load_forgets_ferry_wait),
	TEST_GAME_ENTRY(scn_saveload, reads_every_old_save),
	TEST_GAME_ENTRY(scn_saveload, resave_fixtures),
	TEST_GAME_ENTRY(scn_saveload, load_closes_the_movie),
	TEST_GAME_ENTRY(scn_saveload, damaged_files),
	TEST_END
};

#endif
