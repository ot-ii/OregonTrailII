#include "testframework.h"
#include "testsupport.h"

#ifdef OTII_TEST

#include <stdlib.h>
#include <string.h>

#include "../src/data/assets.h"
#include "../src/base/cstr.h"
#include "../src/game/game.h"
#include "../src/data/gfxdev.h"
#include "../src/game/invitem.h"
#include "../src/game/invlist.h"
#include "../src/base/list.h"
#include "../src/data/mecclib.h"
#include "../src/game/party.h"
#include "../src/base/rng.h"
#include "../src/data/rsrc.h"
#include "../src/base/str.h"

#define STORE_MAX_LINES 400

struct line {
	char name[80];
	int32_t lo, hi, chance, pct[8];
	int32_t seen;
	int32_t fields;
};

static int32_t read_store(int32_t id, struct line *lines, int32_t max)
{
	struct rsrc *r = AssetsGetElement(g_data_lib, "Store", id, false);
	const char *p, *end, *line_end;
	int32_t n = 0;

	if (!r)
		return -1;
	p = (const char *) RsrcData(r);
	end = p + RsrcSize(r);
	while (p < end && *p != 13)
		++p;
	++p;
	while (p < end && *p != 13)
		++p;
	++p;
	while (p < end && *p != '-' && n < max) {
		char buf[256];
		int32_t len;
		for (line_end = p; line_end < end && *line_end != 13; ++line_end)
			;
		len = (int32_t) (line_end - p) < 255 ? (int32_t) (line_end - p) : 255;
		memcpy(buf, p, (size_t) len);
		buf[len] = 0;
		memset(&lines[n], 0, sizeof(lines[n]));
		{
			int32_t *const f[11] = {&lines[n].lo, &lines[n].hi, &lines[n].chance, &lines[n].pct[0], &lines[n].pct[1],
				&lines[n].pct[2], &lines[n].pct[3], &lines[n].pct[4], &lines[n].pct[5], &lines[n].pct[6],
				&lines[n].pct[7]};
			char *q = buf;
			int32_t name_len;
			for (name_len = 0; q[name_len] && q[name_len] != '\t'; ++name_len)
				;
			memcpy(lines[n].name, q, (size_t) (name_len < 79 ? name_len : 79));
			lines[n].name[name_len < 79 ? name_len : 79] = 0;
			q += name_len;
			lines[n].fields = 1;
			for (int32_t k = 0; k < 11; ++k) {
				if (*q == '\t')
					++q;
				if (*q == '\t' || *q == 0) {
					*f[k] = k == 1 ? lines[n].lo : 0;

				} else {
					*f[k] = atoi(q);
					++lines[n].fields;
				}
				while (*q && *q != '\t')
					++q;
			}
		}
		if (lines[n].name[0])
			++n;
		p = line_end + 1;
	}
	RsrcDestroy(&r);
	return n;
}

struct store_ids {
	int32_t *ids;
	int32_t n, max;
};

static void collect_store(const char *name, int32_t id, int32_t storage, int32_t size, int64_t offset, void *opaque)
{
	struct store_ids *s = opaque;
	int32_t j;

	(void) storage;
	(void) size;
	(void) offset;
	if (!StrEqualNoCase(name, "Store") || s->n >= s->max)
		return;
	for (j = 0; j < s->n && s->ids[j] != id; ++j)
		;
	if (j == s->n)
		s->ids[s->n++] = id;
}

static int32_t store_ids(int32_t *ids, int32_t max)
{
	struct store_ids s = {ids, 0, max};

	for (int32_t i = 0; i < AssetsSourceCount(g_data_lib); ++i) {
		struct mecclib *lib = AssetsSourceMeccLib(g_data_lib, i);
		if (lib)
			MeccLibEnumerate(lib, collect_store, &s);
	}
	return s.n;
}

static int32_t find_line(struct line *lines, int32_t n, const struct cstr *name)
{
	const char *s = CStrGet(name);
	int32_t found = -1;

	for (int32_t i = 0; i < n && found < 0; ++i)
		if (!strcmp(lines[i].name, s))
			found = i;
	return found;
}

TEST_GAME(scn_store, every_list_every_column)
{
	static int32_t ids[400];
	static struct line lines[STORE_MAX_LINES];
	int32_t n_ids = store_ids(ids, 400);
	int32_t rolls = TStatRuns() / 20 > 20 ? TStatRuns() / 20 : 20;
	int32_t alive = PartyMemberCount(g_game->party, true);

	CHECK(n_ids > 3);
	for (int32_t s = 0; s < n_ids; ++s) {
		int32_t n = read_store(ids[s], lines, STORE_MAX_LINES), i;
		TCase("store %d", ids[s]);
		CHECK(n > 0);
		if (n <= 0)
			continue;
		for (i = 0; i < n; ++i) {
			struct cstr *key = CStrCreateFrom(lines[i].name);
			TCase("store %d line \"%s\"", ids[s], lines[i].name);
			CHECK_MSG(LookupItemId(key) >= 0, "\"%s\" is not an item key", lines[i].name);
			CStrDestroy(&key);
			CHECK_MSG(lines[i].chance > 0 || n > 1, "the only line has a 0%% chance: the store would never finish");
			if (lines[i].fields < 12)
				TNote("store %d \"%s\" has %d of its 12 fields (a missing maximum is the minimum)", ids[s],
					lines[i].name, lines[i].fields);
		}
		for (int32_t col = 1; col <= 8; ++col) {
			RngSetSeed(TSeed() + (uint32_t) (ids[s] * 16 + col));
			for (i = 0; i < n; ++i)
				lines[i].seen = 0;
			for (int32_t k = 0; k < rolls; ++k) {
				struct inv_list *list = InvListCreateStore(ids[s], col, 0, -1);
				int32_t count = 0;
				for (struct list_node *node = list->base.head; node; node = node->next) {
					struct inv_item *it = InvItemFromNode(node);
					int32_t li = find_line(lines, n, it->name);
					++count;
					TCase("store %d column %d", ids[s], col);
					if (li < 0)
						CHECK_MSG(0, "\"%s\" is not on the store's lines", CStrGet(it->name));
					if (li < 0)
						continue;
					++lines[li].seen;
					{
						bool fits = false, any = false;
						for (int32_t j = li; j < n; ++j) {
							if (strcmp(lines[j].name, lines[li].name))
								continue;
							any = true;
							if ((lines[j].lo < 0 ? it->quantity == -lines[j].lo * alive :
									it->quantity >= (lines[j].lo > 800 ? 800 : lines[j].lo) &&
									it->quantity <= (lines[j].hi > 900 ? 900 : lines[j].hi)) &&
									it->price_factor > (100 + lines[j].pct[col - 1]) / 100.0 - 1e-9 &&
									it->price_factor < (100 + lines[j].pct[col - 1]) / 100.0 + 1e-9)
								fits = true;
						}
						if (any)
							CHECK_MSG(fits, "%d at x%.2f fits none of the store's lines for it", it->quantity,
								it->price_factor);
					}
				}
				CHECK(count > 0);
				InvListDestroy(&list);
			}
			if (col == 1) {
				double none = 1.0;
				for (i = 0; i < n; ++i)
					none *= 1.0 - (lines[i].chance > 100 ? 100 : lines[i].chance) / 100.0;
				for (i = 0; i < n; ++i) {
					TCase("store %d line \"%s\" (%d%%)", ids[s], lines[i].name, lines[i].chance);
					bool twice = false;
					for (int32_t j = 0; j < n; ++j)
						if (j != i && !strcmp(lines[j].name, lines[i].name))
							twice = true;
					if (twice)
						continue;
					if (lines[i].chance >= 100)
						CHECK_EQ(lines[i].seen, rolls);
					else if (lines[i].chance <= 0)
						CHECK_EQ(lines[i].seen, 0);
					else if (none < 0.01)
						CHECK_RATE(lines[i].seen, rolls, lines[i].chance / 100.0);
				}
			}
		}
	}
	TCase("");
	TNote("%d store lists", n_ids);
}

TEST_GAME(scn_store, budget)
{
	static int32_t ids[400];
	int32_t n_ids = store_ids(ids, 400);

	for (int32_t s = 0; s < n_ids; ++s) {
		if (ids[s] < 1000)
			continue;
		for (int32_t k = 0; k < 20; ++k) {
			struct inv_list *list = InvListCreateStore(ids[s], 1, 20, -1);
			int32_t count = 0;
			for (struct list_node *node = list->base.head; node; node = node->next)
				++count;
			TCase("trader %d", ids[s]);
			CHECK_RANGE(count, 1, 20);
			InvListDestroy(&list);
		}
	}
}

TEST_GAME(scn_store, impossible)
{
	struct inv_list *list;

	TCase("price column 0");
	list = InvListCreateStore(1, 0, 0, -1);
	InvListDestroy(&list);
	TCase("price column 9");
	list = InvListCreateStore(1, 9, 0, -1);
	InvListDestroy(&list);
	CHECK(1);
}

const struct test_reg TESTS_scn_store[] = {
	TEST_GAME_ENTRY(scn_store, every_list_every_column),
	TEST_GAME_ENTRY(scn_store, budget),
	TEST_GAME_ENTRY(scn_store, impossible),
	TEST_END
};

#endif
