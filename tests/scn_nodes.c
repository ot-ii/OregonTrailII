#include "testframework.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/data/assets.h"
#include "../src/base/byteorder.h"
#include "../src/dialogs/convdlg.h"
#include "../src/base/date.h"
#include "../src/game/game.h"
#include "../src/data/gfxdev.h"
#include "../src/data/mecclib.h"
#include "../src/trail/node.h"
#include "../src/data/rsrc.h"
#include "../src/base/str.h"
#include "../src/data/strrsrc.h"

#define MAX_IDS 2000

struct id_list {
	const char *type;
	int32_t *ids;
	int32_t n, max;
};

static void collect_id(const char *name, int32_t id, int32_t storage, int32_t size, int64_t offset, void *opaque)
{
	struct id_list *l = opaque;
	int32_t j;

	(void) storage;
	(void) size;
	(void) offset;
	if (!StrEqualNoCase(name, l->type) || l->n >= l->max)
		return;
	for (j = 0; j < l->n && l->ids[j] != id; ++j)
		;
	if (j == l->n)
		l->ids[l->n++] = id;
}

static int32_t ids_of(const char *type, int32_t *ids, int32_t max)
{
	struct id_list l = {type, ids, 0, max};

	for (int32_t i = 0; i < AssetsSourceCount(g_data_lib); ++i) {
		struct mecclib *lib = AssetsSourceMeccLib(g_data_lib, i);
		if (lib && MeccLibIsAvailable(lib))
			MeccLibEnumerate(lib, collect_id, &l);
	}
	return l.n;
}

static bool has(const int32_t *ids, int32_t n, int32_t id)
{
	for (int32_t i = 0; i < n; ++i)
		if (ids[i] == id)
			return true;
	return false;
}

static int32_t g_tnodes[MAX_IDS], g_lnodes[MAX_IDS];
static int32_t g_tn, g_ln;

static void load_ids(void)
{
	if (!g_tn)
		g_tn = ids_of("TNode", g_tnodes, MAX_IDS);
	if (!g_ln)
		g_ln = ids_of("LNode", g_lnodes, MAX_IDS);
}

static bool valid_date(int32_t y, int32_t m, int32_t d)
{
	return (y == 0 && m == 0 && d == 0) || (y >= 1800 && y <= 1964 && m >= 1 && m <= 12 && d >= 1 && d <= 31);
}

static void check_common(struct node_base *n)
{
	CHECK_RANGE(n->link_count, 0, 8);
	for (int32_t i = 0; i < n->link_count && i < 8; ++i)
		if (!has(g_tnodes, g_tn, n->links[i]) && !has(g_lnodes, g_ln, n->links[i]))
			TNote("node %d: link %d leads to node %d, which the data does not have (never offered)", n->node_id, i,
				n->links[i]);
	CHECK_RANGE(n->event_count, 0, 20);
	for (int32_t i = 0; i < n->event_count && i < 20; ++i)
		CHECK_MSG((n->events[i] >= 0 && n->events[i] <= 102) || (n->events[i] >= 1000 && n->events[i] < 1100),
			"event or feature %d", n->events[i]);
	CHECK_RANGE(n->conv_count, 0, 70);
	CHECK_RANGE(n->inset_count, 0, 40);
	CHECK_RANGE(n->summer_inset_count, 0, 20);
	CHECK_RANGE(n->snow_inset_count, 0, 20);
	CHECK_RANGE(n->game_species_count, 0, 50);
	CHECK_MSG(valid_date(n->start_year, n->start_month, n->start_day), "starts %d-%d-%d", n->start_year,
		n->start_month, n->start_day);
	CHECK_MSG(valid_date(n->end_year, n->end_month, n->end_day), "ends %d-%d-%d", n->end_year, n->end_month,
		n->end_day);
}

static struct node_base *load_node(bool trail, int32_t id)
{
	return trail ? &NodeCreate(id, false)->base : &LNodeCreate(id, false)->base;
}

TEST(scn_nodes, trail_stretches)
{
	int32_t hills = 0, bad_zone = 0;

	load_ids();
	CHECK(g_tn > 100);
	for (int32_t i = 0; i < g_tn; ++i) {
		struct node *n;
		TCase("TNode %d", g_tnodes[i]);
		n = NodeCreate(g_tnodes[i], false);
		CHECK_EQ(n->base.node_id, g_tnodes[i]);
		check_common(&n->base);
		if (n->base.terrain == TERRAIN_UPHILL || n->base.terrain == TERRAIN_DOWNHILL) {
			struct rsrc *hill = AssetsGetElement(g_data_lib, "Hill", g_tnodes[i], false);
			++hills;
			CHECK_MSG(hill != NULL, "an %s stretch without a HILL record",
				n->base.terrain == TERRAIN_UPHILL ? "uphill" : "downhill");
			if (hill) {
				RsrcLoadAndUnpack(hill);
				const uint8_t *h = RsrcData(hill);
				int32_t h0 = (int16_t) ReadLe16(h), h1 = (int16_t) ReadLe16(h + 2);
				CHECK_RANGE(h1, 1, 5);
				CHECK_RANGE(h0, 0, 1);
				CHECK_MSG(h0 == (n->base.terrain == TERRAIN_UPHILL), "HILL says %s, the stretch %s", h0 ? "up" : "down",
					n->base.terrain == TERRAIN_UPHILL ? "uphill" : "downhill");
				RsrcDestroy(&hill);
			}
		}
		CHECK(n->base.miles >= 0);
		if (n->base.price_zone < 1 || n->base.price_zone > 8)
			++bad_zone;
		NodeDestroy(&n);
	}
	TCase("");
	TNote("%d trail stretches, %d hills, %d with a price zone outside 1..8 (stores are only on landmarks)", g_tn,
		hills, bad_zone);
}

TEST(scn_nodes, landmarks)
{
	int32_t rivers = 0;

	load_ids();
	CHECK(g_ln > 50);
	for (int32_t i = 0; i < g_ln; ++i) {
		struct lnode *n;
		TCase("LNode %d", g_lnodes[i]);
		n = LNodeCreate(g_lnodes[i], false);
		CHECK_EQ(n->base.node_id, g_lnodes[i]);
		check_common(&n->base);
		CHECK_MSG(n->base.price_zone >= 1 && n->base.price_zone <= 8, "price zone %d: a store here would read price "
			"column %d of 8", n->base.price_zone, n->base.price_zone);
		for (int32_t d = 0; d < 5; ++d)
			if (n->route_by_dest[d] > 0)
				CHECK_MSG(has(g_tnodes, g_tn, n->route_by_dest[d]) || has(g_lnodes, g_ln, n->route_by_dest[d]),
					"route %d leads to node %d, which does not exist", d, n->route_by_dest[d]);
		if (n->ferry_available)
			++rivers;
		LNodeDestroy(&n);
	}
	TCase("");
	TNote("%d landmarks, %d with a ferry (river depth, width and swiftness are set when the node is applied)", g_ln,
		rivers);
}

TEST(scn_nodes, unset_crossing_prices)
{
	load_ids();
	for (int32_t i = 0; i < g_ln; ++i) {
		struct lnode *n;

		TCase("LNode %d", g_lnodes[i]);
		n = malloc(sizeof(*n));
		if (!n)
			continue;
		memset(n, 0xA5, sizeof(*n));
		LNodeInit(n, g_lnodes[i], false);
		CHECK_EQ(n->ferry_available, 0);
		CHECK_EQ(n->ferry_price, 0);
		CHECK_EQ(n->toll_bridge_price, 0);
		CHECK_EQ(n->guide_conv, 0);
		for (int32_t k = 0; k < 9; ++k) {
			CHECK_EQ(n->crossing_pics_a[k], 0);
			CHECK_EQ(n->crossing_pics_b[k], 0);
		}
		free(n);
	}
	TCase("");
}

static void load_both(void *arg)
{
	int32_t id = (int32_t) (intptr_t) arg;
	struct node *t = NodeCreate(id, false);
	struct lnode *l;

	NodeDestroy(&t);
	l = LNodeCreate(id, false);
	LNodeDestroy(&l);
}

TEST(scn_nodes, missing_nodes)
{
	static const int32_t ids[] = {0, 29999, 32767};

	for (int32_t i = 0; i < 3; ++i) {
		TCase("node %d", ids[i]);
		CHECK(TGuarded(load_both, (void *) (intptr_t) ids[i]));
	}
	TCase("node -1");
	TExpectFatal(true);
	CHECK(!TGuarded(load_both, (void *) (intptr_t) -1));
	TExpectFatal(false);
}

TEST(scn_nodes, no_way_on)
{
	int32_t out[8];
	int32_t count;

	{
		struct node *hill = NodeCreate(411, false);
		struct date before = DateMake(7, 20, 1847), after = DateMake(7, 26, 1847);
		TCase("Steep Hill, July 20, 1847");
		count = NodeBaseListActiveLinks(&hill->base, &before, 409, out);
		CHECK_EQ(count, 1);
		CHECK_EQ(count ? out[0] : -1, 414);
		TCase("Steep Hill, July 26, 1847");
		count = NodeBaseListActiveLinks(&hill->base, &after, 409, out);
		CHECK_EQ(count, 1);
		CHECK_EQ(count ? out[0] : -1, 414);
		NodeDestroy(&hill);
	}
	{
		struct node *wall = NodeCreate(489, false);
		struct date d = DateMake(8, 1, 1850);
		TCase("Rock Wall Ascent, from Donner Pass");
		count = NodeBaseListActiveLinks(&wall->base, &d, 491, out);
		for (int32_t i = 0; i < count; ++i)
			CHECK(out[i] != 969);
		TCase("Rock Wall Ascent, from the phantom landmark");
		count = NodeBaseListActiveLinks(&wall->base, &d, 696, out);
		CHECK_EQ(count, 1);
		CHECK_EQ(count ? out[0] : -1, 491);
		NodeDestroy(&wall);
	}
	{
		struct lnode *missing = LNodeCreate(969, false);
		struct date d = DateMake(8, 1, 1850);
		TCase("node 969 (not in the data)");
		CHECK_EQ(missing->base.node_kind, NODE_KIND_NONE);
		CHECK_EQ(missing->base.weather_region, 0);
		CHECK(!NodeBaseActiveOnDate(&missing->base, &d));
		LNodeDestroy(&missing);
	}
	TCase("");
}

#define MAX_NODE 1024

struct walk_node {
	bool exists, trail;
	int32_t kind, miles;
	int32_t start, end;
	int32_t links[8], link_count;
};

static struct walk_node g_walk[MAX_NODE];

static int32_t day_key(int32_t y, int32_t m, int32_t d)
{
	return 372 * y + 31 * m + d;
}

static void load_walk(void)
{
	load_ids();
	memset(g_walk, 0, sizeof(g_walk));
	for (int32_t i = 0; i < g_tn + g_ln; ++i) {
		bool trail = i < g_tn;
		int32_t id = trail ? g_tnodes[i] : g_lnodes[i - g_tn];
		struct node_base *n;
		struct walk_node *w;
		if (id <= 0 || id >= MAX_NODE)
			continue;
		n = load_node(trail, id);
		w = &g_walk[id];
		w->exists = true;
		w->trail = trail;
		w->kind = n->node_kind;
		w->miles = trail ? n->miles : 0;
		w->start = day_key(n->start_year, n->start_month, n->start_day);
		w->end = day_key(n->end_year, n->end_month, n->end_day);
		w->link_count = n->link_count;
		for (int32_t k = 0; k < n->link_count && k < 8; ++k)
			w->links[k] = n->links[k];
		NodeBaseDestroy(&n);
	}
}

static bool usable(int32_t id)
{
	return id > 0 && id < MAX_NODE && g_walk[id].exists;
}

static void distances(const int32_t *from, int32_t n, double *dist)
{
	static bool done[MAX_NODE];

	for (int32_t i = 0; i < MAX_NODE; ++i)
		dist[i] = 1e18;
	memset(done, 0, sizeof(done));
	for (int32_t i = 0; i < n; ++i)
		dist[from[i]] = 0;
	for (;;) {
		int32_t u = -1;
		for (int32_t i = 1; i < MAX_NODE; ++i)
			if (g_walk[i].exists && !done[i] && dist[i] < 1e17 && (u < 0 || dist[i] < dist[u]))
				u = i;
		if (u < 0)
			break;
		done[u] = true;
		for (int32_t k = 0; k < g_walk[u].link_count; ++k) {
			int32_t v = g_walk[u].links[k];
			double d;
			if (!usable(v))
				continue;
			d = dist[u] + g_walk[u].miles / 2.0 + g_walk[v].miles / 2.0 + 0.001;
			if (d < dist[v])
				dist[v] = d;
		}
	}
}

static const int32_t STARTS[] = {1, 267, 289, 312, 313, 315};
#define STARTS_N ((int32_t) (sizeof(STARTS) / sizeof(STARTS[0])))
#define STAY_DAYS 60
#define FIRST_DAY (372 * 1840 + 31 + 1)
#define LAST_DAY (372 * 1870 + 31 * 12 + 31)

static double g_from_start[MAX_NODE], g_to_end[8][MAX_NODE];
static int32_t g_ends;

static bool forward(int32_t a, int32_t b)
{
	if (g_from_start[b] > g_from_start[a])
		return true;
	for (int32_t e = 0; e < g_ends; ++e)
		if (g_to_end[e][b] < g_to_end[e][a])
			return true;
	return false;
}

#define SLOTS 10
#define STARTED 8
#define ONE_WAY 9
static int32_t g_earliest[MAX_NODE][SLOTS];
static int32_t g_stack_cur[MAX_NODE * SLOTS], g_stack_slot[MAX_NODE * SLOTS];

static int32_t slot_of(int32_t cur, int32_t prev)
{
	for (int32_t k = 0; k < g_walk[cur].link_count; ++k)
		if (g_walk[cur].links[k] == prev)
			return k;
	return ONE_WAY;
}

static void walk(void)
{
	int32_t top = 0;

	for (int32_t i = 0; i < MAX_NODE; ++i)
		for (int32_t k = 0; k < SLOTS; ++k)
			g_earliest[i][k] = LAST_DAY + 1;
	for (int32_t i = 0; i < STARTS_N; ++i) {
		int32_t s = STARTS[i];
		if (!usable(s))
			continue;
		g_earliest[s][STARTED] = g_walk[s].start > FIRST_DAY ? g_walk[s].start : FIRST_DAY;
		g_stack_cur[top] = s;
		g_stack_slot[top++] = STARTED;
	}
	while (top > 0) {
		int32_t cur, slot, prev, lo;
		struct walk_node *c;
		--top;
		cur = g_stack_cur[top];
		slot = g_stack_slot[top];
		prev = slot < 8 ? g_walk[cur].links[slot] : slot == STARTED ? cur : -1;
		lo = g_earliest[cur][slot];
		c = &g_walk[cur];
		if (!c->trail && c->kind == 4)
			continue;
		for (int32_t k = 0; k < c->link_count; ++k) {
			int32_t next = c->links[k], back, latest, t;
			if (next == prev || !usable(next) || (slot != STARTED && !forward(cur, next)))
				continue;
			latest = g_walk[next].end;
			if (c->end + (c->trail ? STAY_DAYS : 0) < latest)
				latest = c->end + (c->trail ? STAY_DAYS : 0);
			if (latest > LAST_DAY)
				latest = LAST_DAY;
			t = lo > g_walk[next].start ? lo : g_walk[next].start;
			back = slot_of(next, cur);
			if (t > latest || t >= g_earliest[next][back])
				continue;
			g_earliest[next][back] = t;
			if (top < MAX_NODE * SLOTS) {
				g_stack_cur[top] = next;
				g_stack_slot[top++] = back;
			}
		}
	}
}

TEST(scn_nodes, trail_has_a_way_on)
{
	static const struct { int32_t cur, prev; const char *why; } KNOWN[] = {
		{321, 319, "Winter Quarters, 1846: the game's Wait for spring moves the party on in April 1847"},
		{411, 409, "Steep Hill after July 24, 1847: on to Great Salt Lake Valley"},
		{489, 490, "Rock Wall Ascent heading east (it lists 969 for 696): reached only by walking a loop backwards"},
		{489, 491, "Rock Wall Ascent heading east (it lists 969 for 696): reached only by walking a loop backwards"},
		{728, 727, "Segment ZZ after 1858 leads to Future Virginia City, not Virginia City: on to it"},
	};
	const int32_t known_n = (int32_t) (sizeof(KNOWN) / sizeof(KNOWN[0]));
	int32_t ends[8], reached = 0, found = 0;

	load_walk();
	distances(STARTS, STARTS_N, g_from_start);
	g_ends = 0;
	for (int32_t i = 1; i < MAX_NODE; ++i)
		if (g_walk[i].exists && !g_walk[i].trail && g_walk[i].kind == 4 && g_ends < 8)
			ends[g_ends++] = i;
	CHECK(g_ends >= 5);
	for (int32_t e = 0; e < g_ends; ++e)
		distances(&ends[e], 1, g_to_end[e]);
	for (int32_t i = 0; i < STARTS_N; ++i)
		CHECK(usable(STARTS[i]));
	walk();

	for (int32_t i = 1; i < MAX_NODE; ++i) {
		struct walk_node *c = &g_walk[i];
		bool any = false;
		for (int32_t k = 0; k < SLOTS; ++k)
			if (g_earliest[i][k] <= LAST_DAY)
				any = true;
		reached += any;
		if (c->exists && !any)
			TNote("node %d is not reached", i);
		if (!c->exists || (!c->trail && c->kind == 4))
			continue;
		for (int32_t k = 0; k < SLOTS; ++k) {
			int32_t prev = k < c->link_count ? c->links[k] : -1, j;
			int32_t hi, first = 0, last = 0, days = 0;
			if ((k >= c->link_count && k != ONE_WAY) || g_earliest[i][k] > LAST_DAY || (prev >= 0 && !usable(prev)))
				continue;
			hi = c->end;
			if (prev >= 0 && g_walk[prev].end + (c->trail ? STAY_DAYS : 0) < hi)
				hi = g_walk[prev].end + (c->trail ? STAY_DAYS : 0);
			if (hi > LAST_DAY)
				hi = LAST_DAY;
			for (int32_t d = g_earliest[i][k] > c->start ? g_earliest[i][k] : c->start; d <= hi; ++d) {
				int32_t m = (d % 372) / 31;
				bool open = false;
				if (m < 1 || m > 12 || d % 31 < 1)
					continue;
				for (j = 0; j < c->link_count && !open; ++j) {
					int32_t o = c->links[j];
					if (o != prev && usable(o) && g_walk[o].start <= d && d <= g_walk[o].end)
						open = true;
				}
				if (open)
					continue;
				if (!days++)
					first = d;
				last = d;
			}
			if (!days)
				continue;
			++found;
			for (j = 0; j < known_n && !(KNOWN[j].cur == i && KNOWN[j].prev == prev); ++j)
				;
			TCase("node %d from %d", i, prev);
			if (j < known_n)
				TNote("node %d from %d: no current way on %d/%d/%d .. %d/%d/%d (%d days) -- %s", i, prev,
					first / 372, (first % 372) / 31, first % 31, last / 372, (last % 372) / 31, last % 31, days,
					KNOWN[j].why);
			else
				CHECK_MSG(0, "node %d from %d: no current way on %d/%d/%d .. %d/%d/%d (%d days)", i, prev,
					first / 372, (first % 372) / 31, first % 31, last / 372, (last % 372) / 31, last % 31, days);
			if (i != 321) {
				struct node_base *n = load_node(c->trail, i);
				struct date date = DateMake((first % 372) / 31, first % 31, first / 372);
				int32_t out[8];
				int32_t count = NodeBaseListActiveLinks(n, &date, prev, out);
				CHECK_MSG(count > 0 && usable(out[0]), "ListActiveLinks gives no way on");
				NodeBaseDestroy(&n);
			}
		}
	}
	TCase("");
	TNote("%d of %d nodes reached from the starting towns; %d places with no current way on", reached, g_tn + g_ln,
		found);
	CHECK(reached >= g_tn + g_ln - 5);
	CHECK_EQ(found, known_n);
}

TEST(scn_nodes, menu_speaker)
{
	int32_t bad = 0;

	load_ids();
	for (int32_t i = 0; i < g_tn + g_ln; ++i) {
		bool trail = i < g_tn;
		int32_t id = trail ? g_tnodes[i] : g_lnodes[i - g_tn];
		struct node_base *n = load_node(trail, id);
		for (int32_t k = -1; k <= 70; ++k) {
			int32_t conv = GameTestNodeSpeakerConv(n, k);
			int32_t count = n->conv_count < 70 ? n->conv_count : 70;
			int32_t want = k < 0 || count <= 0 ? 0 : (n->conv_table[k % count] > 0 ? n->conv_table[k % count] : 0);
			if (conv != want)
				++bad;
		}
		for (int32_t k = n->conv_count; k < 70; ++k)
			if (k >= 0 && n->conv_table[k])
				++bad;
		NodeBaseDestroy(&n);
	}
	CHECK_EQ(bad, 0);
	TNote("%d nodes", g_tn + g_ln);
}

static int32_t check_conversation(int32_t id, char *why, size_t size)
{
	struct rsrc *tree = AssetsGetElement(g_text_lib, "CONVR", id, false);
	struct strrsrc *lines;
	const int8_t *rec;
	int32_t count, start[3], n, bad = 0, nodes_n;
	static bool seen[256];

	why[0] = 0;
	if (!tree) {
		StrPrintfN(why, size, "no CONVR %d", id);
		return 1;
	}
	if (RsrcStorage(tree) == RSRC_TEXT) {
		StrPrintfN(why, size, "CONVR %d is a text resource", id);
		RsrcDestroy(&tree);
		return 1;
	}
	lines = StrRsrcLoad(g_text_lib, id + CONV_TEXT_BASE, false);
	count = lines ? StrRsrcCount(lines) : 0;
	if (count <= 0) {
		StrPrintfN(why, size, "no lines (STRLST %d)", id + CONV_TEXT_BASE);
		StrRsrcDestroy(&lines);
		RsrcDestroy(&tree);
		return 1;
	}
	rec = (const int8_t *) RsrcData(tree);
	nodes_n = (RsrcSize(tree) - 4) / 6;
	if (nodes_n < count) {
		StrPrintfN(why, size, "the tree has %d nodes for %d lines", nodes_n, count);
		bad = 1;
	}
	start[0] = 0;
	start[1] = rec[1];
	start[2] = rec[2];
	memset(seen, 0, sizeof(seen));
	for (int32_t s = 0; s < 3 && !bad; ++s) {
		int32_t queue[256], head = 0, tail = 0;
		if (start[s] < 0 || start[s] >= count || start[s] >= nodes_n) {
			StrPrintfN(why, size, "start %d is node %d of %d", s, start[s], count);
			bad = 1;
			break;
		}
		if (!seen[start[s]]) {
			seen[start[s]] = true;
			queue[tail++] = start[s];
		}
		while (head < tail && !bad) {
			const int8_t *link;
			int32_t answers;
			n = queue[head++];
			link = rec + 4 + 6 * n;
			for (answers = 0; answers < 6 && link[answers]; ++answers)
				;
			if (answers == 1 && link[0] < 0)
				continue;
			if (!answers) {
				StrPrintfN(why, size, "node %d has no answer and no end", n);
				bad = 1;
				break;
			}
			for (int32_t k = 0; k < answers && !bad; ++k) {
				int32_t answer = link[k], next;
				if (answer <= 0 || answer >= count || answer >= nodes_n) {
					StrPrintfN(why, size, "node %d answer %d is line %d of %d", n, k, answer, count);
					bad = 1;
					break;
				}
				next = rec[4 + 6 * answer];
				if (next >= 0) {
					if (next >= count || next >= nodes_n) {
						StrPrintfN(why, size, "node %d answer %d leads to node %d of %d", n, k, next, count);
						bad = 1;

					} else if (!seen[next] && tail < 256) {
						seen[next] = true;
						queue[tail++] = next;
					}
				}
			}
		}
	}
	StrRsrcDestroy(&lines);
	RsrcDestroy(&tree);
	return bad;
}

static int32_t check_one_liner(int32_t id, char *why, size_t size)
{
	struct strrsrc *lines = StrRsrcLoad(g_text_lib, id / 100 + CONV_TEXT_BASE, false);
	int32_t row = id % 100, bad = 0;

	why[0] = 0;
	if (!lines || StrRsrcCount(lines) <= row) {
		StrPrintfN(why, size, "line %d of STRLST %d: it has %d", row, id / 100 + CONV_TEXT_BASE,
			lines ? StrRsrcCount(lines) : 0);
		bad = 1;

	} else {
		const char *text = StrRsrcGetAt(lines, row);
		if (!text || !strchr(text, '\t')) {
			StrPrintfN(why, size, "line %d of STRLST %d has no speaker<TAB>text", row, id / 100 + CONV_TEXT_BASE);
			bad = 1;
		}
	}
	StrRsrcDestroy(&lines);
	return bad;
}

TEST(scn_nodes, every_conversation)
{
	static int32_t checked[2048];
	int32_t n_checked = 0, bad = 0, refs = 0, one_liners = 0, other = 0;
	char why[200];

	load_ids();
	CHECK(g_text_lib != NULL);
	if (!g_text_lib)
		return;
	for (int32_t i = 0; i < g_tn + g_ln; ++i) {
		bool trail = i < g_tn;
		int32_t id = trail ? g_tnodes[i] : g_lnodes[i - g_tn];
		struct node_base *node = load_node(trail, id);
		for (int32_t k = 0; k < node->conv_count && k < 70; ++k) {
			int32_t conv = node->conv_table[k], j;
			++refs;
			if (conv <= 0)
				continue;
			if (conv >= 9999 && conv <= 100000) {
				++other;
				continue;
			}
			for (j = 0; j < n_checked && checked[j] != conv; ++j)
				;
			if (j < n_checked)
				continue;
			if (n_checked < 2048)
				checked[n_checked++] = conv;
			TCase("node %d conversation %d", id, conv);
			if (conv > 100000) {
				++one_liners;
				if (check_one_liner(conv, why, sizeof(why))) {
					++bad;
					CHECK_MSG(0, "%s", why);
				}

			} else if (check_conversation(conv, why, sizeof(why))) {
				++bad;
				CHECK_MSG(0, "%s", why);
			}
		}
		NodeBaseDestroy(&node);
	}
	TCase("");
	TNote("%d conversation references on %d nodes: %d different conversations (%d one-liners), %d special ids "
		"(9999-100000, not conversations); %d with a problem", refs, g_tn + g_ln, n_checked, one_liners, other, bad);
	CHECK(n_checked > 100);
}

const struct test_reg TESTS_scn_nodes[] = {
	TEST_ENTRY(scn_nodes, trail_stretches),
	TEST_ENTRY(scn_nodes, landmarks),
	TEST_ENTRY(scn_nodes, unset_crossing_prices),
	TEST_ENTRY(scn_nodes, missing_nodes),
	TEST_ENTRY(scn_nodes, no_way_on),
	TEST_ENTRY(scn_nodes, trail_has_a_way_on),
	TEST_ENTRY(scn_nodes, menu_speaker),
	TEST_ENTRY(scn_nodes, every_conversation),
	TEST_END
};

#endif
