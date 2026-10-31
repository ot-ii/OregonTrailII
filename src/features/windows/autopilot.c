#include "../autopilot.h"

#ifdef OTII_AUTO_MODE

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "../autopilotcures.h"
#include "../../hunt/animalbase.h"
#include "../../game/ailment.h"
#include "../../data/assets.h"
#include "../../ui/button.h"
#include "../../base/byteorder.h"
#include "../../base/cstr.h"
#include "../../base/date.h"
#include "../../game/draftanimalitem.h"
#include "../../draw/drawable.h"
#include "../../ui/edit.h"
#include "../../game/endgame.h"
#include "../../app/event.h"
#include "../../app/evthandler.h"
#include "../../trail/fordwagon.h"
#include "../../game/game.h"
#include "../../game/gamecmd.h"
#include "../../data/gfxdev.h"
#include "../../hunt/hunting.h"
#include "../../dialogs/huntweapondlg.h"
#include "../../base/ini.h"
#include "../../screens/invgrid.h"
#include "../../game/invitem.h"
#include "../../game/invlist.h"
#include "../../base/list.h"
#include "../../screens/mainui.h"
#include "../../game/member.h"
#include "../../base/memory.h"
#include "../../dialogs/modaldialog.h"
#include "../../game/msgcontroller.h"
#include "../../trail/node.h"
#include "../../app/otiiapp.h"
#include "../../game/party.h"
#include "../../dialogs/pickerdlg.h"
#include "../../trail/raftframe.h"
#include "../../app/replay.h"
#include "../../data/rsrc.h"
#include "../../ui/scaler.h"
#include "../../screens/sceneframe.h"
#include "../../draw/sprite.h"
#include "../../base/str.h"
#include "../../data/strrsrc.h"
#include "../../trail/tilemap.h"
#include "../../dialogs/tradedlg.h"
#include "../../ui/view.h"
#include "../../game/weather.h"
#include "../../ui/window.h"

#define AUTO_TICK_MS 700
#define GOOD_CROSSING 80
#define MAX_WAITS 4
#define RESERVE_CENTS 6000
#define TRAIL_RESERVE_CENTS 2500

typedef struct control_option cand;

static UINT_PTR g_timer;
static int32_t g_busy;

static int32_t g_wait_node = -1, g_waits;
static int32_t g_rest_node = -1, g_rests;
static int32_t g_town_node = -1, g_town_steps;
static int32_t g_shop_node = -1;
static int32_t g_shop_visits;
static int32_t g_shop_had_store;
static int32_t g_package_bought, g_oxen_tries;
static int32_t g_hunt_day = -1, g_hunt_pending, g_hunt_idle;
static char g_town_seen[64][96];
static int32_t g_store_opened;
static int32_t g_town_seen_n;
static int32_t g_scene_turns;
static int32_t g_dump_rounds;
static int32_t g_dump_stuck;

static struct game *g_seen_game;
static int32_t g_game_over;
static int32_t g_score_seen;
static int32_t g_no_hunt_items = -1;
static int32_t g_talk_node = -1, g_talks;
static int32_t g_last_talk_code = -1;
static int32_t g_hay_node = -1, g_ruins_node = -1;
static int32_t g_settings_checked;
static int32_t g_trail_settings_day = -1;
static int32_t g_morale_node = -1, g_morale_rests;
static int32_t g_mud_node = -1, g_mud_waits;
static int32_t g_lighten_node = -1, g_lighten_count;
static int32_t g_need_rope;
static int32_t g_rope_trade;

static int32_t g_rest_days = 1;

static void *g_rolled_dlg, *g_firearm_dlg, *g_trade_dlg;
static int32_t g_trade_picks;

static char g_last_pick[2200];
static int32_t g_same_picks;

static int32_t g_opt_next_game = -1, g_opt_score_goods = -1;

static int32_t g_hill_node = -1, g_hill_tried = -1;
static int32_t g_hill_fails[64];

static int32_t g_guide_refused_node = -1;
static int32_t g_guide_asked_node = -1;
static int32_t g_trade_refused_day = -10;
static int32_t g_breakdown_refusals;
static int32_t g_trade_stuck;
static int32_t g_trade_haggles;

static int32_t restock_needed(void);
static int32_t score_shopping(void);
static int32_t food_for_morale_rest(int32_t rest_days);
static int32_t food_target_days(void);
static int32_t wanted_pace(void);
static int32_t wanted_rations(void);
static int32_t days_to_go(void);
static int32_t need_produce(void);
static int32_t category(const char *name);
static void item_name(struct inv_item *item, char *out, int32_t size);
static int32_t critical(struct inv_item *item, int32_t qty);
static void reset_game_state(void);

static struct party *party(void)
{
	return g_game->party;
}

static struct inv_item *item_of(struct list_node *node)
{
	return InvItemFromNode(node);
}

static const char *item_key(struct inv_item *item)
{
	return CStrGet(InvItemGetNameCStr(item));
}

static int32_t unit_weight_x16(struct inv_item *item)
{
	return INV_ITEM_OPS(item)->GetUnitWeightX16(item);
}

static struct inv_grid *as_inv_grid(struct view *v)
{
	if (!v)
		return NULL;

	const void *ops = ((struct object *) v)->ops;
	if (ops == &INV_GRID_OPS_TABLE || ops == &STORE_GRID_OPS_TABLE || ops == &DROP_GRID_OPS_TABLE)
		return (struct inv_grid *) v;

	return NULL;
}

static int32_t may_lighten(void)
{
	if (g_lighten_node != g_game->cur_node_id) {
		g_lighten_node = g_game->cur_node_id;
		g_lighten_count = 0;
	}

	return g_lighten_count++ < 2;
}

static int32_t option_next_game(void)
{
	if (g_opt_next_game < 0)
		g_opt_next_game = IniGetInt("autopilot", "new_game_after_end", 0) ? 1 : 0;

	return g_opt_next_game;
}

static int32_t option_score_goods(void)
{
	if (g_opt_score_goods < 0)
		g_opt_score_goods = IniGetInt("autopilot", "buy_goods_for_score", 0) ? 1 : 0;

	return g_opt_score_goods;
}

static uint32_t auto_rand(void)
{
	static bool seeded;
	static uint32_t state;

	if (!seeded) {
		state = RpGetTickCount() | 1;
		seeded = true;
	}

	state = state * 1103515245u + 12345u;
	return state >> 16;
}

static void ap_log(const char *fmt, ...)
{
	char path[MAX_PATH];
	char *dot;
	FILE *f;
	va_list a;

	if (!GetModuleFileNameA(NULL, path, MAX_PATH))
		return;

	dot = strrchr(path, '.');
	if (dot)
		*dot = 0;
	StrCat(path, "_autopilot.txt");
	f = fopen(path, "a");
	if (!f)
		return;

	va_start(a, fmt);
	vfprintf(f, fmt, a);
	va_end(a);
	fputc('\n', f);
	fclose(f);
}

static void describe_screen(const struct control_state *st)
{
	static char last[2048];
	char now[2048];
	int32_t i, len;

	len = StrPrintfN(now, sizeof(now), "%s:", st->title);
	for (i = 0; i < st->option_count && len >= 0 && len < 1900; ++i)
		len += StrPrintfN(now + len, sizeof(now) - (size_t) len, " %s[cmd %d code %d]", st->options[i].label,
			st->options[i].command, st->options[i].code);

	if (strcmp(now, last)) {
		StrCopy(last, now);
		ap_log("  screen %s", now);
	}
}

static int32_t has(const char *text, const char *word)
{
	char a[1024], b[128];
	int32_t i;

	for (i = 0; text[i] && i < 1023; ++i)
		a[i] = (char) tolower((unsigned char) text[i]);
	a[i] = 0;
	for (i = 0; word[i] && i < 127; ++i)
		b[i] = (char) tolower((unsigned char) word[i]);
	b[i] = 0;

	return strstr(a, b) != NULL;
}

static const cand *find(const struct control_state *st, const char *word)
{
	int32_t i;
	size_t n = strlen(word);

	for (i = 0; i < st->side_option_first; ++i)
		if (!_strnicmp(st->options[i].label, word, n))
			return &st->options[i];

	for (i = 0; i < st->side_option_first; ++i)
		if (has(st->options[i].label, word))
			return &st->options[i];

	return NULL;
}

static const cand *find_prefix(const struct control_state *st, const char *word)
{
	int32_t i;
	size_t n = strlen(word);

	for (i = 0; i < st->side_option_first; ++i)
		if (!_strnicmp(st->options[i].label, word, n))
			return &st->options[i];

	return NULL;
}

static const cand *by_code(const struct control_state *st, int32_t code)
{
	int32_t i;

	for (i = 0; i < st->side_option_first; ++i)
		if (st->options[i].command == GCMD_CHOICE && st->options[i].code == code)
			return &st->options[i];

	return NULL;
}

static int32_t text_has(const struct control_state *st, const char *word)
{
	int32_t i;

	for (i = 0; i < st->side_text_first; ++i)
		if (has(st->text[i], word))
			return 1;

	return 0;
}

static int32_t alive_members(void)
{
	int32_t i, n = 0;

	for (i = 0; i < party()->member_count && i < 6; ++i)
		if (party()->members[i] && party()->members[i]->state == MEMBER_ALIVE)
			++n;

	return n;
}

static int32_t plenty_of_food(void)
{
	return PartyHasPlentyOfFood(party()) != 0;
}

static const int32_t MEAL_GROUPS[5] = {IGRP_SEASONING, IGRP_FOOD_OTHER, IGRP_FOOD_PRODUCE, IGRP_FOOD_MEAT,
	IGRP_FOOD_GRAIN};

static int32_t meal_helping(int32_t level)
{
	struct party *p = party();
	int32_t a = alive_members() * (level == RATIONS_BARE_BONES ? 1 : level == RATIONS_MEAGER ? 2 : 3);

	if (!InvListHasGroup(&p->inventory, IGRP_COOKWARE)) {
		a = a * 7 / 6;
		if (!a)
			a = 1;
	}

	if (p->skills[SKILL_COOKING]) {
		a = a * 5 / 6;
		if (!a)
			a = 1;
	}

	return a;
}

static int32_t group_units(int32_t g)
{
	struct list_node *node;
	int32_t units = 0;

	GetItemTable();
	for (node = party()->inventory.base.head; node; node = node->next)
		if (InvItemInGroup(item_of(node), (int16_t) MEAL_GROUPS[g]))
			units += InvItemTotalPortions(item_of(node));

	return units;
}

static int32_t food_days_at(int32_t level)
{
	struct stack {
		int32_t group, units, spoil;
	} stacks[96];
	static uint32_t cached_at[3];
	static int32_t cached_days[3] = {-1, -1, -1};
	struct list_node *node;
	int32_t n = 0, i, k, g, day, helping;

	if (!alive_members())
		return 999;

	if (level < 0 || level > 2)
		level = RATIONS_FILLING;

	if (cached_days[level] >= 0 && RpClockNow() - cached_at[level] < 200)
		return cached_days[level];

	helping = meal_helping(level);
	GetItemTable();
	for (node = party()->inventory.base.head; node && n < 96; node = node->next) {
		struct inv_item *item = item_of(node);
		for (g = 0; g < 5 && !InvItemInGroup(item, (int16_t) MEAL_GROUPS[g]); ++g)
			;
		if (g < 5 && InvItemTotalPortions(item) > 0) {
			stacks[n].group = g;
			stacks[n].units = InvItemTotalPortions(item);
			stacks[n++].spoil = item->days_to_spoil > 0 ? item->days_to_spoil : 0x7fff;
		}
	}

	for (i = 1; i < n; ++i)
		for (k = i; k > 0 && stacks[k].spoil < stacks[k - 1].spoil; --k) {
			struct stack swap = stacks[k];
			stacks[k] = stacks[k - 1];
			stacks[k - 1] = swap;
		}

	for (day = 0; day < 400; ++day) {
		int32_t a = helping, ate = 0;
		for (g = 0; g < 5; ++g) {
			int32_t want = a, had = 0;
			for (i = 0; i < n && want > 0; ++i)
				if (stacks[i].group == g && stacks[i].units > 0 && stacks[i].spoil > day) {
					int32_t take = want < stacks[i].units ? want : stacks[i].units;
					stacks[i].units -= take;
					want -= take;
					had = 1;
				}

			if (had) {
				if (g >= 2)
					ate = 1;

			} else {
				a = g == 0 ? (a + 3) / 4 + a : g == 1 ? (a + 2) / 3 + a : g == 2 ? (a + 1) / 2 + a : a + a;
			}
		}

		if (!ate)
			break;
	}

	cached_at[level] = RpClockNow();
	cached_days[level] = day;
	return day;
}

static int32_t food_days(void)
{
	return food_days_at(RATIONS_FILLING);
}

static int32_t morale(void)
{
	return party()->reputation;
}

static int32_t key_is(const char *name, const char *key)
{
	const char *bracket = strstr(name, " [");
	size_t n = bracket ? (size_t) (bracket - name) : strlen(name), k = strlen(key);

	if (n == k)
		return !_strnicmp(name, key, k);

	if (n == k + 1 && name[n - 1] == 's')
		return !_strnicmp(name, key, k);

	return n == k + 2 && !_strnicmp(name + k, "es", 2) && !_strnicmp(name, key, k);
}

static int32_t count_key(struct inv_list *list, const char *key)
{
	struct list_node *node;
	int32_t n = 0;

	for (node = list ? list->base.head : NULL; node; node = node->next) {
		struct inv_item *item = item_of(node);
		const char *name = item_key(item);
		int32_t hit = name && key_is(name, key);
		if (hit)
			n += InvItemGetCount(item, false);
	}

	return n;
}

static int32_t have(const char *key)
{
	return count_key(&party()->inventory, key);
}

static int32_t count_all(struct inv_list *list)
{
	struct list_node *node;
	int32_t n = 0;

	for (node = list ? list->base.head : NULL; node; node = node->next)
		n += InvItemGetCount(item_of(node), false);

	return n;
}

static int32_t strained(void)
{
	int32_t p = party()->pull_mode;

	return p == PULL_SINGLE_STRAINED || p == PULL_TEAM_STRAINED;
}

static int32_t conestoga(void)
{
	return InvListFindByType(&party()->wagons, g_item_conestoga) != NULL;
}

static int32_t contagious(void)
{
	return !GameEvalChoiceCondition(COND_NO_CONTAGION, party());
}

static int32_t keep_waiting(int32_t waits)
{
	return waits < 7 || contagious();
}

static int32_t needs_rest(void)
{
	int32_t i;

	for (i = 0; i < party()->member_count && i < 6; ++i) {
		struct member *m = party()->members[i];
		if (m && m->state == MEMBER_ALIVE && m->max_health > 0 && m->health * 2 < m->max_health)
			return 1;
	}

	return 0;
}

static int32_t hunting_gun(void)
{
	return ((have("rifle") || have("pistol")) && have("bullets")) || (have("shotgun") && have("shot"));
}

static int32_t can_hunt(void)
{
	if (!have("gunpowder") || !hunting_gun())
		return 0;

	return g_no_hunt_items != count_all(&party()->inventory);
}

static const char *hunting_lacks(void)
{
	if (!have("rifle") && !have("pistol") && !have("shotgun"))
		return "a gun";

	if (!hunting_gun())
		return have("shotgun") && !have("rifle") && !have("pistol") ? "shot" : "bullets";

	if (!have("gunpowder"))
		return "gunpowder";

	return NULL;
}

static int32_t load(void)
{
	return InvListTotalWeight(&party()->wagons) / 16 + InvListTotalWeight(&party()->inventory) / 16;
}

static int32_t easy_cap(const char *kind, int32_t less)
{
	struct party *p = party();
	struct list_node *node;
	int32_t wagons = count_all(&p->wagons), cap = 0, kinds = 0, work = 0, min_easy = 0;

	for (node = p->animals.base.head; node; node = node->next) {
		struct draft_animal_item *a = (struct draft_animal_item *) item_of(node);
		int32_t have_n = InvItemGetCount(&a->base, false), units = wagons * DraftAnimalItemGetPerWagon(a);
		int32_t easy = DraftAnimalItemGetEasyLoad(a);
		if (kind) {
			char name[96];
			item_name(&a->base, name, sizeof(name));
			if (!strcmp(name, kind))
				have_n -= less;
		}

		if (have_n <= 0)
			continue;

		if (units > have_n)
			units = have_n;
		if (easy * units > cap)
			cap = easy * units;
		if (!kinds++ || easy < min_easy)
			min_easy = easy;
		work += units;
	}

	if (kinds > 1 && min_easy * work - 1 > cap)
		cap = min_easy * work - 1;

	return cap;
}

static int32_t spare_easy_load(void)
{
	return easy_cap(NULL, 0) - load();
}

static int32_t river_chance(int32_t method)
{
	static const int32_t FORD_SHALLOW[6] = {0, 99, 97, 95, 85, 75}, FORD_MID[6] = {0, 97, 95, 85, 75, 50};
	static const int32_t CAULK_MID[6] = {0, 99, 95, 85, 75, 50}, CAULK_DEEP[6] = {0, 99, 97, 95, 85, 75};
	static const int32_t FERRY[6] = {0, 99, 99, 95, 90, 85};
	struct node_base *node = g_game->cur_node;
	int32_t depth = node->depth, scale = node->swiftness, chance = 0, guide = 0;

	if (scale < 1 || scale > 5)
		return 0;

	if (method == NODE_CMD_HIRE_INDIANS) {
		guide = 1;
		method = depth <= 24 ? NODE_CMD_FORD : NODE_CMD_CAULK_AND_FLOAT;
	}

	switch (method) {
	case NODE_CMD_FORD:
		chance = depth <= 24 ? FORD_SHALLOW[scale] : depth <= 30 ? FORD_MID[scale] : 0;
		break;
	case NODE_CMD_CAULK_AND_FLOAT:
		chance = depth <= 24 ? 0 : depth <= 30 ? CAULK_MID[scale] : CAULK_DEEP[scale];
		break;
	case NODE_CMD_FERRY:
		chance = FERRY[scale];
		break;
	default:
		return 0;
	}

	if (method != NODE_CMD_FERRY && !g_conditions_checked)
		chance -= 10;
	if (guide)
		chance += (100 - chance) / 2;
	if (method == NODE_CMD_FORD || method == NODE_CMD_CAULK_AND_FLOAT) {
		if (WeatherSumPrecip(g_game->weather, 0))
			chance -= 5;
		if (strained())
			chance -= 15;
		if (conestoga())
			chance -= 10;
	}

	if (party()->skills[SKILL_RIVERWORK])
		chance += (100 - chance) / 2;

	return chance;
}

static const cand *pick_river(const struct control_state *st)
{
	static const int32_t METHODS[] = {NODE_CMD_FORD, NODE_CMD_CAULK_AND_FLOAT, NODE_CMD_FERRY, NODE_CMD_HIRE_INDIANS, 0};
	const cand *pick = NULL, *c;
	int32_t i, best = -1;

	if ((c = by_code(st, NODE_CMD_TOLL_BRIDGE)) != NULL || (c = by_code(st, NODE_CMD_PAY_TOLL)) != NULL) {
		ap_log("    river: toll/bridge -- certain");
		return c;
	}

	for (i = 0; METHODS[i]; ++i) {
		int32_t chance;
		if ((c = by_code(st, METHODS[i])) == NULL)
			continue;
		if (METHODS[i] == NODE_CMD_HIRE_INDIANS && g_guide_refused_node == g_game->cur_node_id)
			continue;

		chance = river_chance(METHODS[i]);
		if (METHODS[i] == NODE_CMD_FERRY && NodeBaseGetId(g_game->cur_node) != NODE_KIND_TRAIL &&
			party()->cash < ((struct lnode *) g_game->cur_node)->ferry_price / 100.0)
			chance = -1;

		ap_log("    river '%s': %d%% (depth %d, width %d, current %d)", c->label, chance, g_game->cur_node->depth,
			g_game->cur_node->width, g_game->cur_node->swiftness);
		if (chance > best) {
			best = chance;
			pick = c;
		}
	}

	if (!pick && (c = by_code(st, NODE_CMD_CROSS_ICE)) != NULL)
		return c;

	if (best < GOOD_CROSSING && strained() && (c = by_code(st, NODE_CMD_LIGHTEN_LOAD)) != NULL && may_lighten()) {
		ap_log("    river: best only %d%% and the team is straining -- lightening the load", best);
		return c;
	}

	if (best < GOOD_CROSSING && (c = by_code(st, NODE_CMD_REST)) != NULL) {
		int32_t left = days_to_go();
		if (g_wait_node != g_game->cur_node_id) {
			g_wait_node = g_game->cur_node_id;
			g_waits = 0;
		}
		if (g_waits < MAX_WAITS && (g_waits < 2 || food_days() > (left < 0 ? 30 : left) + 5)) {
			++g_waits;
			g_rest_days = 1;
			ap_log("    river: best only %d%% -- wait a day (%d)", best, g_waits);
			return c;
		}
	}

	if (pick && pick->code == NODE_CMD_HIRE_INDIANS)
		g_guide_asked_node = g_game->cur_node_id;

	return pick;
}

static int32_t hill_chance(int32_t method)
{
	static const int32_t WALK[6] = {0, 97, 75, 50, 0, 0};
	static const int32_t LOCK[6] = {0, 97, 92, 75, 50, 0};
	static const int32_t ANCHOR[6] = {0, 97, 92, 87, 75, 50};
	static const int32_t DOUBLE_TEAM[6] = {0, 97, 92, 75, 50, 0};
	static const int32_t ROPES_UP[6] = {0, 75, 75, 97, 92, 85}, ROPES_DOWN[6] = {0, 75, 75, 75, 95, 90};
	int32_t steep = g_game->cur_node->steepness, chance;
	int32_t uphill = NodeBaseGetId(g_game->cur_node) == NODE_KIND_TRAIL &&
		NodeGetTerrain((struct node *) g_game->cur_node) == TERRAIN_UPHILL;

	if (steep < 1 || steep > 5)
		return 0;

	switch (method) {
	case NODE_CMD_GO_DOWN:
	case NODE_CMD_GO_UP:
		chance = WALK[steep];
		break;
	case NODE_CMD_LOCK_WHEELS:
		chance = LOCK[steep];
		break;
	case NODE_CMD_ANCHOR_WAGON:
		chance = ANCHOR[steep];
		break;
	case NODE_CMD_DOUBLE_TEAM:
		chance = DOUBLE_TEAM[steep];
		break;
	case NODE_CMD_ROPES_AND_CHAINS:
		chance = uphill ? ROPES_UP[steep] : ROPES_DOWN[steep];
		break;
	default:
		return 0;
	}

	if (!g_conditions_checked)
		chance -= 10;
	if (strained())
		chance -= 25;
	if (MainUiGetCatFlag(CAT_MUDDY_TRAIL) || g_game->weather->snow_depth)
		chance -= 25;
	if (conestoga())
		chance -= 15;

	return chance;
}

static const cand *pick_hill(const struct control_state *st)
{
	static const int32_t METHODS[] = {NODE_CMD_GO_DOWN, NODE_CMD_GO_UP, NODE_CMD_LOCK_WHEELS, NODE_CMD_ANCHOR_WAGON,
		NODE_CMD_DOUBLE_TEAM, NODE_CMD_ROPES_AND_CHAINS, 0};
	static const int32_t STURDY[] = {NODE_CMD_DOUBLE_TEAM, NODE_CMD_ROPES_AND_CHAINS, NODE_CMD_ANCHOR_WAGON,
		NODE_CMD_LOCK_WHEELS, NODE_CMD_GO_UP, NODE_CMD_GO_DOWN, 0};
	const int32_t *order = g_game->cur_node->steepness >= 1 ? METHODS : STURDY;
	const cand *pick = NULL, *c;
	int32_t i, best = -1000;

	if (g_hill_node != g_game->cur_node_id) {
		g_hill_node = g_game->cur_node_id;
		g_hill_tried = -1;
		memset(g_hill_fails, 0, sizeof(g_hill_fails));
	}

	for (i = 0; order[i]; ++i) {
		int32_t chance, fails;
		if ((c = by_code(st, order[i])) == NULL)
			continue;

		fails = order[i] < 64 ? g_hill_fails[order[i]] : 0;
		chance = hill_chance(order[i]) - (g_game->cur_node->steepness >= 1 ? 0 : 40 * fails);
		ap_log("    hill '%s': %d%% (steepness %d, failed here %d time%s)", c->label, chance,
			g_game->cur_node->steepness, fails, fails == 1 ? "" : "s");
		if (chance > best) {
			best = chance;
			pick = c;
		}
	}

	if (pick)
		g_hill_tried = pick->code;

	return pick;
}

static int32_t is_target(int32_t id)
{
	switch (g_game->destination) {
	case DEST_WILLAMETTE:
		return id == 265 || id == 266;
	case DEST_SACRAMENTO:
		return id == 506 || id == 507;
	case DEST_SALT_LAKE:
		return id == 415;
	case DEST_ROGUE_RIVER:
		return id == 619 || id == 620;
	}

	return 0;
}

static struct node_base *load_node(int32_t id, int32_t is_trail)
{
	return is_trail ? (struct node_base *) NodeCreate((int16_t) id, true) :
		(struct node_base *) LNodeCreate((int16_t) id, false);
}

static int32_t hops_to_destination(int32_t start, int32_t start_is_trail, int32_t avoid)
{
	static int32_t queue[2048];
	static uint8_t trail_of[2048];
	static uint8_t seen[4096];
	int32_t head = 0, tail = 0, level_end, level = 0;

	memset(seen, 0, sizeof(seen));
	queue[tail] = start;
	trail_of[tail++] = (uint8_t) start_is_trail;
	seen[start & 4095] = 1;
	seen[avoid & 4095] = 1;
	while (head < tail && level < 400) {
		level_end = tail;
		while (head < level_end) {
			int32_t id = queue[head];
			int32_t is_trail = trail_of[head++];
			struct node_base *node;
			int32_t k;

			if (!is_trail && is_target(id))
				return level;

			node = load_node(id, is_trail);
			if (!node)
				continue;

			if (NodeBaseActiveOnDate(node, g_game->date))
				for (k = 0; k < node->link_count && tail < 2048; ++k) {
					int32_t next = node->links[k];
					if (next > 0 && !seen[next & 4095]) {
						seen[next & 4095] = 1;
						queue[tail] = next;
						trail_of[tail++] = (uint8_t) !is_trail;
					}
				}

			NodeBaseDestroy(&node);
		}
		++level;
	}

	return 100000;
}

static int32_t miles_to_node(int32_t start, int32_t start_is_trail, int32_t avoid, int32_t (*want)(int32_t id))
{
	static int32_t ids[2048];
	static uint8_t trail_of[2048], done[2048];
	static int32_t dist[2048];
	int32_t n = 0, i, k;

	ids[n] = start;
	trail_of[n] = (uint8_t) start_is_trail;
	done[n] = 0;
	dist[n++] = 0;
	for (;;) {
		int32_t at = -1, id, is_trail;
		struct node_base *node;
		for (i = 0; i < n; ++i)
			if (!done[i] && (at < 0 || dist[i] < dist[at]))
				at = i;

		if (at < 0)
			return -1;

		done[at] = 1;
		id = ids[at];
		is_trail = trail_of[at];
		if (!is_trail && want(id))
			return dist[at];

		node = load_node(id, is_trail);
		if (!node)
			continue;

		if (NodeBaseActiveOnDate(node, g_game->date))
			for (k = 0; k < node->link_count; ++k) {
				int32_t next = node->links[k], cost = 0, j;
				if (next <= 0 || next == avoid)
					continue;

				if (!is_trail) {
					struct node seg;
					NodeInit(&seg, (int16_t) next, true);
					cost = NodeBaseGetSegCount(&seg.base) > 0 ? NodeBaseGetSegCount(&seg.base) : 0;
				}

				for (j = 0; j < n && !(ids[j] == next && trail_of[j] == (uint8_t) !is_trail); ++j)
					;
				if (j == n) {
					if (n == 2048)
						continue;
					ids[n] = next;
					trail_of[n] = (uint8_t) !is_trail;
					done[n] = 0;
					dist[n++] = dist[at] + cost;

				} else if (!done[j] && dist[at] + cost < dist[j]) {
					dist[j] = dist[at] + cost;
				}
			}

		NodeBaseDestroy(&node);
	}
}

static int32_t miles_from_here(int32_t (*want)(int32_t id))
{
	int32_t miles;

	if (NodeBaseGetId(g_game->cur_node) == NODE_KIND_TRAIL) {
		int32_t best = -1, k;
		for (k = 0; k < g_game->cur_node->link_count; ++k) {
			int32_t next = g_game->cur_node->links[k], m;
			if (next <= 0 || next == g_game->prev_node_id)
				continue;
			m = miles_to_node(next, 0, g_game->cur_node_id, want);
			if (m >= 0 && (best < 0 || m < best))
				best = m;
		}
		miles = best < 0 ? -1 : best + (g_game->miles_to_go > 0 ? g_game->miles_to_go : 0);

	} else {
		miles = miles_to_node(g_game->cur_node_id, 0, g_game->prev_node_id, want);
	}

	return miles;
}

static int32_t pace_tenths(void)
{
	int32_t pace = g_game->days_traveled > 10 && g_game->total_miles > 0 ?
		g_game->total_miles * 10 / g_game->days_traveled : 140;

	return pace < 50 ? 50 : pace;
}

static int32_t days_to_go(void)
{
	static int32_t node = -1, day = -1, days = -1;
	int32_t miles, pace;

	if (g_game->cur_node_id == node && g_game->days_traveled == day)
		return days;

	node = g_game->cur_node_id;
	day = g_game->days_traveled;
	miles = miles_from_here(is_target);
	pace = pace_tenths();
	days = miles < 0 ? -1 : (miles * 10 + pace - 1) / pace;
	ap_log("    %d miles to go, about %d days at %d.%d miles a day", miles, days, pace / 10, pace % 10);

	return days;
}

static int32_t food_target_days(void)
{
	int32_t cap = party()->cash > 200.0 && spare_easy_load() > 3 * alive_members() * 25 ? 90 : 65;
	int32_t left = days_to_go(), want, m = alive_members();

	if (left < 0)
		return cap;

	if (left + 15 > cap && g_game->days_traveled > 0 && m > 0) {
		int32_t longer = left + 15 < 130 ? left + 15 : 130, fits = food_days() + spare_easy_load() / (3 * m);
		if (longer > fits)
			longer = fits;
		if (longer > cap)
			cap = longer;
	}

	want = left * 3 / 2 + 10;
	if (want < 60 && left > 25)
		want = 60;

	return want < cap ? want : cap;
}

static const cand *pick_route(const struct control_state *st)
{
	int32_t ids[8];
	int32_t found, i, best_hops = 100001;
	const cand *best = NULL;

	found = NodeBaseListActiveLinks(g_game->cur_node, g_game->date, g_game->prev_node_id, ids);
	if (!found)
		found = NodeBaseListActiveLinks(g_game->cur_node, g_game->date, g_game->cur_node_id, ids);

	for (i = 0; i < st->side_option_first; ++i) {
		const cand *c = &st->options[i];
		int32_t hops;

		if (c->command != GCMD_CHOICE || (c->code != 0 && c->code != 1) || c->code >= found)
			continue;

		hops = hops_to_destination(ids[c->code], NodeBaseGetId(g_game->cur_node) != NODE_KIND_TRAIL,
			g_game->cur_node_id);
		if (has(c->label, "raft"))
			hops += 1000;

		ap_log("    route '%s' -> %d hops", c->label, hops);
		if (hops < best_hops) {
			best_hops = hops;
			best = c;
		}
	}

	return best;
}

static const cand *pick_landmark(const struct control_state *st)
{
	static const int32_t RIVER[] = {NODE_CMD_FORD, NODE_CMD_CAULK_AND_FLOAT, NODE_CMD_FERRY, NODE_CMD_HIRE_INDIANS,
		NODE_CMD_TOLL_BRIDGE, NODE_CMD_CROSS_ICE, 0};
	static const int32_t HILL[] = {NODE_CMD_ROPES_AND_CHAINS, NODE_CMD_LOCK_WHEELS, NODE_CMD_ANCHOR_WAGON,
		NODE_CMD_DOUBLE_TEAM, NODE_CMD_GO_DOWN, NODE_CMD_GO_UP, 0};
	static const int32_t OTHER[] = {NODE_CMD_WAIT_FOR_SPRING, NODE_CMD_PASS_QUARANTINE, NODE_CMD_WAIT_FOR_SUNSET,
		NODE_CMD_TRAVEL_BY_DAY, NODE_CMD_PORTAGE, NODE_CMD_HIRE_RAFTERS, NODE_CMD_CONTINUE_RAFTING, 0};
	const cand *c;
	int32_t i, river = 0, hill = 0, node = g_game->cur_node_id;

	{
		const cand *picture = NULL;
		int32_t choices = 0;
		for (i = 0; i < st->side_option_first; ++i) {
			if (st->options[i].command == GCMD_CHOICE)
				++choices;
			else if (!strncmp(st->options[i].label, "(image button", 13))
				picture = &st->options[i];
		}

		if (picture && !choices)
			return picture;
	}

	if (g_game->destination == DEST_SALT_LAKE && (c = by_code(st, NODE_CMD_SETTLE_HERE)) != NULL)
		return c;

	if (by_code(st, NODE_CMD_END_GAME)) {
		if (!g_score_seen && (c = by_code(st, NODE_CMD_FINAL_SCORE)) != NULL) {
			g_score_seen = 1;
			ap_log("    the end of the trail: %d of %d alive, $%.2f, morale %d", alive_members(),
				party()->member_count, party()->cash, morale());
			return c;
		}

		if (g_game->end_score)
			ap_log("    final score %d", g_game->end_score->final_total);
		g_game_over = 1;
		return by_code(st, NODE_CMD_END_GAME);
	}

	for (i = 0; RIVER[i]; ++i)
		if (by_code(st, RIVER[i]))
			river = 1;
	for (i = 0; HILL[i]; ++i)
		if (by_code(st, HILL[i]))
			hill = 1;

	if ((river || hill) && !g_conditions_checked && (c = by_code(st, NODE_CMD_CHECK_CONDITIONS)) != NULL) {
		ap_log("    checking the conditions first");
		return c;
	}

	if (needs_rest() && (c = by_code(st, NODE_CMD_REST)) != NULL) {
		if (g_rest_node != node) {
			g_rest_node = node;
			g_rests = 0;
		}
		if (g_rests < 2) {
			++g_rests;
			g_rest_days = 3;
			ap_log("    resting: someone is below half health");
			return c;
		}
	}

	if ((g_game->travel_pace != wanted_pace() || party()->ration_level != wanted_rations()) &&
		g_settings_checked != g_game->days_traveled * 10000 + node + 1 && (c = find_prefix(st, "Supplies")) != NULL) {
		g_settings_checked = g_game->days_traveled * 10000 + node + 1;
		ap_log("    travel pace %d, rations %d -- changing them", g_game->travel_pace, party()->ration_level);
		return c;
	}

	if (g_talk_node != node) {
		g_talk_node = node;
		g_talks = 0;
		g_last_talk_code = -1;
	}

	if (morale() <= 95 && g_talks < (morale() < 50 ? 4 : 2) && by_code(st, NODE_CMD_TALK)) {
		if (g_last_talk_code == NODE_CMD_TALK && (c = by_code(st, NODE_CMD_LOOK_FOR_SOMEONE_ELSE)) != NULL) {
			g_last_talk_code = NODE_CMD_LOOK_FOR_SOMEONE_ELSE;
			return c;
		}
		if (g_last_talk_code != NODE_CMD_TALK) {
			++g_talks;
			g_last_talk_code = NODE_CMD_TALK;
			ap_log("    talking (morale %d)", morale());
			return by_code(st, NODE_CMD_TALK);
		}
	}

	if ((c = by_code(st, NODE_CMD_DONE_LOOKING)) != NULL)
		return c;

	if (morale() < 40 && food_for_morale_rest(5) && !need_produce() && (c = by_code(st, NODE_CMD_REST)) != NULL) {
		if (g_morale_node != node) {
			g_morale_node = node;
			g_morale_rests = 0;
		}
		if (g_morale_rests < 1) {
			++g_morale_rests;
			g_rest_days = (50 - morale()) / 2;
			if (g_rest_days < 2)
				g_rest_days = 2;
			if (g_rest_days > 10)
				g_rest_days = 10;
			if (g_rest_days > food_days() / 2)
				g_rest_days = food_days() / 2 > 1 ? food_days() / 2 : 1;
			ap_log("    resting %d days: morale %d", g_rest_days, morale());
			return c;
		}
	}

	if (g_shop_node != node)
		g_shop_visits = 0;

	if ((g_shop_node != node ? food_days() < food_target_days() - 5 || restock_needed() || score_shopping() :
		food_days() < food_target_days() - 5 && g_shop_visits < 4 && g_shop_had_store &&
		party()->cash * 100.0 > TRAIL_RESERVE_CENTS + 500) &&
		(c = by_code(st, NODE_CMD_GO_INTO_TOWN)) != NULL) {
		g_shop_node = node;
		g_town_node = -1;
		ap_log("    going into town%s: %d days of food left, %d wanted%s%s", g_shop_visits ? " again (new stock)" : "",
			food_days(), food_target_days(), restock_needed() ? ", supplies to replace" : "",
			score_shopping() ? ", goods for the score" : "");
		++g_shop_visits;
		g_shop_had_store = 0;
		return c;
	}

	if ((food_days() < 20 || (group_units(3) == 0 && food_days() < food_target_days())) &&
		(days_to_go() < 0 || food_days() < days_to_go() + 5) && can_hunt() && g_game->days_traveled > g_hunt_day &&
		(c = find_prefix(st, "Supplies")) != NULL) {
		g_hunt_day = g_game->days_traveled;
		g_hunt_pending = 1;
		ap_log("    food for %d days%s -- going hunting", food_days(), group_units(3) == 0 ? ", no meat" : "");
		return c;
	}

	if (g_ruins_node != node && (c = by_code(st, NODE_CMD_CHECK_RUINS)) != NULL) {
		g_ruins_node = node;
		return c;
	}

	if (g_hay_node != node && spare_easy_load() > 200 && (c = by_code(st, NODE_CMD_GATHER_HAY)) != NULL) {
		g_hay_node = node;
		return c;
	}

	if (river && (c = pick_river(st)) != NULL)
		return c;

	if (hill) {
		if (strained() && (c = by_code(st, NODE_CMD_LIGHTEN_LOAD)) != NULL && may_lighten()) {
			ap_log("    hill: the team is straining -- lightening the load");
			return c;
		}

		if (MainUiGetCatFlag(CAT_MUDDY_TRAIL) && (c = by_code(st, NODE_CMD_REST)) != NULL) {
			if (g_mud_node != node) {
				g_mud_node = node;
				g_mud_waits = 0;
			}
			if (g_mud_waits < 2) {
				++g_mud_waits;
				g_rest_days = 1;
				ap_log("    hill: the trail is muddy -- wait a day (%d)", g_mud_waits);
				return c;
			}
		}

		if ((c = pick_hill(st)) != NULL)
			return c;
	}

	if (!hill && !river && find_prefix(st, "Check the hill")) {
		static int32_t stuck_node = -1, tries, trade_day = -1, trades;
		int32_t no_rope = !have("rope") && !have("chain");
		if (stuck_node != node) {
			stuck_node = node;
			tries = 0;
		}
		if (trade_day != g_game->days_traveled) {
			trade_day = g_game->days_traveled;
			trades = 0;
		}

		g_need_rope = no_rope;
		if (no_rope && trades < 15 && (c = find_prefix(st, "Supplies")) != NULL) {
			++trades;
			g_rope_trade = 1;
			ap_log("    hill: no way over and no rope or chain -- looking for a trader who has one (%d)", trades);
			return c;
		}

		if (tries == 0 && (c = by_code(st, NODE_CMD_LIGHTEN_LOAD)) != NULL) {
			++tries;
			ap_log("    hill: no way over offered -- lightening the load");
			return c;
		}

		if ((tries < 3 || no_rope) && (c = by_code(st, NODE_CMD_REST)) != NULL) {
			tries = tries ? tries + 1 : 2;
			g_rest_days = 1;
			ap_log("    hill: no way over offered -- wait a day");
			return c;
		}
	}

	for (i = 0; OTHER[i]; ++i)
		if ((c = by_code(st, OTHER[i])) != NULL)
			return c;

	{
		static int32_t wait_node = -1, waits;
		int32_t onward = 0;
		for (i = 0; i < st->side_option_first; ++i) {
			const cand *o = &st->options[i];
			if (o->command == GCMD_CHOICE && o->code != NODE_CMD_REST && o->code != NODE_CMD_TALK &&
				o->code != NODE_CMD_SAVE_DIARY && o->code != NODE_CMD_LOOK_AROUND && strncmp(o->label, "Get advice", 10))
				onward = 1;
		}

		if (wait_node != node) {
			wait_node = node;
			waits = 0;
		}

		if (!onward && keep_waiting(waits) && (c = by_code(st, NODE_CMD_REST)) != NULL) {
			++waits;
			g_rest_days = 1;
			ap_log("    no way on offered here -- wait a day (%d)%s", waits,
				contagious() ? ", someone is contagious" : "");
			return c;
		}
	}

	if ((c = by_code(st, NODE_CMD_READY_TO_GO)) != NULL)
		return c;

	if ((c = pick_route(st)) != NULL)
		return c;

	if ((c = by_code(st, NODE_CMD_CONTINUE)) != NULL)
		return c;

	for (i = 0; i < st->side_option_first; ++i) {
		const cand *o = &st->options[i];
		if (o->command == GCMD_CHOICE && o->code != NODE_CMD_REST && o->code != NODE_CMD_LOOK_AROUND &&
			o->code != NODE_CMD_TALK && o->code != NODE_CMD_CHECK_CONDITIONS && o->code != NODE_CMD_SAVE_DIARY &&
			o->code != NODE_CMD_END_GAME && o->code != NODE_CMD_LIGHTEN_LOAD) {
			ap_log("    no rule for '%s' (code %d) -- taking it", o->label, o->code);
			return o;
		}
	}

	return NULL;
}

struct known_illness {
	int32_t id;
	int32_t identify;
	int32_t symptoms[14][2];
	char name[48];
};

static struct known_illness g_illness[64];
static int32_t g_illness_count = -1;

static void load_illnesses(void)
{
	struct strrsrc *names;
	int32_t id;

	g_illness_count = 0;
	for (id = 1; id < 100 && g_illness_count < 64; ++id) {
		struct rsrc *r = g_data_lib ? AssetsGetElement(g_data_lib, "HPROB", (int16_t) (15000 + id), false) : NULL;
		const uint8_t *def;
		struct known_illness *k;
		int32_t i;
		if (!r)
			continue;

		RsrcLoadAndUnpack(r);
		def = RsrcData(r);
		k = &g_illness[g_illness_count];
		memset(k, 0, sizeof(*k));
		if (def) {
			k->id = ReadLe16(def + 2 * HPROB_ID);
			k->identify = ReadLe16(def + 2 * HPROB_IDENTIFY_CHANCE);
			for (i = 0; i < 14; ++i) {
				k->symptoms[i][0] = ReadLe16(def + 2 * (HPROB_SYMPTOMS + 2 * i));
				k->symptoms[i][1] = ReadLe16(def + 2 * (HPROB_SYMPTOMS + 2 * i + 1));
			}
		}
		RsrcDestroy(&r);

		names = StrRsrcLoad(g_text_lib, 15000 + id, true);
		if (StrRsrcCount(names) > 0)
			StrRsrcCopyTo(names, 0, k->name, sizeof(k->name));
		StrRsrcDestroy(&names);
		if (def && k->identify < 100 && k->symptoms[0][0])
			++g_illness_count;
	}
}

static struct member *named_member(const struct control_state *st)
{
	int32_t i;

	for (i = 0; i < party()->member_count && i < 6; ++i) {
		struct member *m = party()->members[i];
		const char *name;
		int32_t hit;
		if (!m || m->state != MEMBER_ALIVE || !m->name)
			continue;

		name = CStrGet(m->name);
		hit = name && name[0] && text_has(st, name);
		if (hit)
			return m;
	}

	return NULL;
}

static const char *guess_illness(const struct control_state *st)
{
	static char best_name[48];
	uint8_t shown[128];
	char status[512], symptom[96];
	struct strrsrc *names;
	struct member *m;
	struct cstr *text;
	const char *p;
	double best = 0, second = 0;
	int32_t i, k, count, best_i = -1;

	if (g_illness_count < 0)
		load_illnesses();

	if ((m = named_member(st)) == NULL)
		return NULL;

	text = MemberGetStatusText(m);
	status[0] = 0;
	if (text) {
		p = CStrGet(text);
		StrCopy(status, p ? p : "");
		CStrDestroy(&text);
	}

	memset(shown, 0, sizeof(shown));
	names = StrRsrcLoad(g_text_lib, 16000, true);
	count = StrRsrcCount(names) < 128 ? StrRsrcCount(names) : 128;
	for (p = status; *p; ) {
		const char *end = strstr(p, "; ");
		int32_t n = end ? (int32_t) (end - p) : (int32_t) strlen(p);
		if (n > 0 && n < (int32_t) sizeof(symptom))
			for (k = 0; k < count - 2; ++k) {
				char name[96];
				int32_t len;
				StrRsrcCopyTo(names, (int16_t) k, name, sizeof(name));
				len = (int32_t) strlen(name);
				while (len > 0 && name[len - 1] == ' ')
					name[--len] = 0;
				if (len == n && !_strnicmp(p, name, (size_t) n))
					shown[k + 1] = 1;
			}

		p += n;
		if (end)
			p += 2;
	}
	StrRsrcDestroy(&names);

	for (i = 0; i < g_illness_count; ++i) {
		const struct known_illness *ill = &g_illness[i];
		uint8_t explained[128];
		double like = (100 - ill->identify) / 100.0;
		int32_t first_neg = -1, neg_shown_other = 0;
		memset(explained, 0, sizeof(explained));
		for (k = 0; k < 14 && ill->symptoms[k][0]; ++k) {
			int32_t s = ill->symptoms[k][0], ch = ill->symptoms[k][1];
			if (s <= 0 || s >= 128)
				continue;

			explained[s] = 1;
			if (ch < 0) {
				if (first_neg < 0)
					first_neg = k;
				else if (shown[s])
					neg_shown_other = 1;
				continue;
			}

			if (ch >= 100)
				like *= shown[s] ? 1.0 : 0.02;
			else
				like *= shown[s] ? ch / 100.0 : 1.0 - ch / 100.0;
		}

		if (first_neg >= 0) {
			double pneg = -ill->symptoms[first_neg][1] / 100.0;
			like *= shown[ill->symptoms[first_neg][0]] ? pneg : neg_shown_other ? 1.0 - pneg : 0.02;
		}

		for (k = 1; k < 128; ++k)
			if (shown[k] && !explained[k])
				like *= 0.05;

		if (like > best) {
			second = best;
			best = like;
			best_i = i;

		} else if (like > second) {
			second = like;
		}
	}

	if (best_i < 0 || best <= 0)
		return NULL;

	ap_log("    unknown ailment: symptoms '%s' -- best guess %s (%.0f%% of the weight of the top two)", status,
		g_illness[best_i].name, 100.0 * best / (best + second));
	StrCopy(best_name, g_illness[best_i].name);
	return best_name;
}

struct event_rule {
	const char *title;
	const char *choices[6];
};

static const struct event_rule EVENTS[] = {
	{"Blizzard", {"Slow down", NULL}},
	{"Snowbound", {"Try to continue as usual", NULL}},
	{"Severe Thunderstorm", {"Slow down", NULL}},
	{"Hailstorm", {"Continue as usual", NULL}},
	{"Heavy Fog", {"Continue as usual", NULL}},
	{"Flooded Trail", {"Try to ford through the water", NULL}},
	{"Quicksand Ahead", {"Put weakest animal up front", NULL}},
	{"Prairie Fire", {"Wait for conditions to improve", NULL}},
	{"Duststorm", {"Slow down", NULL}},
	{"Locusts", {"Slow down", NULL}},
	{"Thick Dust", {"Slow down", NULL}},
	{"Bad Mosquitoes", {"Continue", NULL}},
	{"Caught some fish", {"Clean them for food", NULL}},
	{"Missing Livestock", {"Organize a search party", NULL}},
	{"Injured Livestock", {"Slow down", NULL}},
	{"Animals Exhausted", {"Stop and rest awhile", NULL}},
	{"Abandoned Wagon", {"Search it for something useful", NULL}},
	{"Caught in Quicksand", {"Try to rescue the animal", NULL}},
	{"Fire in a Wagon", {"Try to put out the fire", NULL}},
	{"Theft", {"Continue", NULL}},
	{"Missing Person", {"Organize a search party", NULL}},
	{"Strangers Ahead", {"Continue at a distance", NULL}},
	{"Indian Party Ahead", {"Continue at a distance", NULL}},
	{"Independence Day", {"Celebrate the Fourth", "Continue", NULL}},
	{"elected captain", {"Accept position of captain", NULL}},
	{"re-elected", {"Accept position of captain", NULL}},
	{"has died", {"Provide a proper burial", NULL}},
	{"Thirst", {"Increase rations", "Go faster", NULL}},
	{"Delay at Ferry", {"Wait your turn", NULL}},
	{"Too Many People", {"Wait your turn", NULL}},
	{NULL, {NULL}}
};

static const cand *first(const struct control_state *st, const char *const *choices)
{
	const cand *c;
	int32_t i;

	for (i = 0; choices[i]; ++i)
		if ((c = find(st, choices[i])) != NULL)
			return c;

	return NULL;
}

static const cand *pick_event(const struct control_state *st)
{
	const cand *c;
	int32_t i;

	if ((c = by_code(st, NODE_CMD_EVENT_END_GAME)) != NULL) {
		g_game_over = 1;
		return c;
	}

	if (text_has(st, "Tipped Wagon") && g_hill_node == g_game->cur_node_id && g_hill_tried >= 0 && g_hill_tried < 64) {
		++g_hill_fails[g_hill_tried];
		g_hill_tried = -1;
	}

	if ((c = find(st, "Provide a proper burial")) != NULL)
		return c;

	if (text_has(st, "Fallen Timbers") && (have("ax") || have("hatchet") || have("saw") || have("pick ax")) &&
		(c = find(st, "Try to clear the path")))
		return c;

	if (text_has(st, "Fallen Rocks") && (have("pick ax") || have("shovel")) && (c = find(st, "Try to clear the path")))
		return c;

	if (text_has(st, "Fallen Timbers") || text_has(st, "Fallen Rocks"))
		if ((party()->skills[SKILL_TRACKING] && (c = find(st, "Try to find another path"))) ||
			(c = find(st, "Continue over the obstruction")) || (c = find(st, "Try to find another path")) ||
			(c = find(st, "Wait for conditions")))
			return c;

	if (text_has(st, "Buffalo Stampede"))
		if ((food_days() < 45 && can_hunt() && (c = find(st, "Go hunting"))) || (c = find(st, "Wait for stampede")))
			return c;

	if (text_has(st, "Strangers Ahead") || text_has(st, "Indian Party Ahead"))
		if ((plenty_of_food() && (c = find(st, "Continue at a distance"))) ||
			(c = find(st, "Wait to see what they do")) || (c = find(st, "Continue at a distance")))
			return c;

	if (text_has(st, "Unknown Ailment")) {
		const char *guess = guess_illness(st);
		for (i = 0; guess && AUTO_CURES[i].ailment; ++i)
			if (!_stricmp(AUTO_CURES[i].ailment, guess) && (c = first(st, AUTO_CURES[i].choices)) != NULL) {
				ap_log("    treating it as %s", guess);
				return c;
			}

		if ((c = find(st, "Rest here awhile")) != NULL)
			return c;
	}

	if (text_has(st, "Extreme Heat") || text_has(st, "Extreme Cold"))
		if ((c = find(st, (needs_rest() || MainUiGetCatFlag(CAT_SOMEONE_SICK) || MainUiGetCatFlag(CAT_POOR_HEALTH)) ?
			"Wait for conditions to improve" : "Continue as usual")) != NULL)
			return c;

	if (text_has(st, "Wild Fruit") || text_has(st, "Wild Vegetables")) {
		const char *how = (!plenty_of_food() || party()->scurvy_risk > 20) ? "Stop and gather" :
			food_days() < 40 ? "Gather while continuing" : "Continue";
		if ((c = find_prefix(st, how)) || (c = find(st, "Gather while continuing")) || (c = find(st, "Stop and gather")))
			return c;
	}

	if (text_has(st, "Broken Wagon") || text_has(st, "Broken Yoke")) {
		struct party *p = party();
		int32_t repair = 10 + (p->skills[SKILL_BLACKSMITHING] ? 40 : 0) + (p->skills[SKILL_CARPENTRY] ? 40 : 0) +
			(have("anvil") ? 5 : 0) + (have("hammer") ? 5 : 0) + (have("nails") ? 5 : 0) + (have("saw") ? 5 : 0);
		int32_t trade_ok = g_game->days_traveled - g_trade_refused_day > 1;
		g_trade_stuck = !find(st, "Replace it from supplies") && !find(st, "Try to repair it");
		if (!g_trade_stuck)
			g_breakdown_refusals = 0;

		if (g_trade_stuck && (c = find(st, "Trade for a replacement")) != NULL) {
			ap_log("    breakdown: no repair left to try -- a trade is the only way on (%d turned down)",
				g_breakdown_refusals);
			return c;
		}

		if ((c = find(st, "Replace it from supplies")) || (repair >= 50 && (c = find(st, "Try to repair it"))) ||
			(trade_ok && (c = find(st, "Trade for a replacement"))) || (c = find(st, "Try to repair it"))) {
			ap_log("    breakdown: repair chance %d%%%s", repair, trade_ok ? "" : ", trade turned down already");
			return c;
		}
	}

	if (text_has(st, "Stepping in a Hole") || text_has(st, " Died"))
		if ((!plenty_of_food() && (c = find(st, "Butcher the animal"))) || (c = find(st, "Slow down")) ||
			(c = find(st, "Butcher the animal")))
			return c;

	if (text_has(st, "Stuck in the")) {
		static int32_t tries;
		static int32_t stuck_day = -1;
		int32_t today = g_game->date->year * 400 + g_game->date->month * 32 + g_game->date->day;
		if (today - stuck_day > 3)
			tries = 0;
		stuck_day = today;
		if ((c = find(st, (strained() || tries++ % 2) ? "Lighten the load" : "Double-team")) != NULL ||
			(c = find(st, "Double-team")) != NULL) {
			g_dump_stuck = strained();
			return c;
		}
	}

	if (text_has(st, "morale is low") && (c = find(st, food_for_morale_rest(5) ? "Rest here awhile" : "Continue")) != NULL)
		return c;

	if (text_has(st, "Starvation"))
		if ((count_all(&party()->spare_animals) && (c = find(st, "Kill a draft animal"))) ||
			(can_hunt() && (c = find(st, "Go hunting"))) || (c = find(st, "Continue")))
			return c;

	for (i = 0; EVENTS[i].title; ++i)
		if (text_has(st, EVENTS[i].title) && (c = first(st, EVENTS[i].choices)) != NULL)
			return c;

	for (int32_t pass = 0; pass < 2; ++pass)
		for (i = 0; AUTO_CURES[i].ailment; ++i) {
			int32_t hit = 0, k;
			for (k = 0; k < st->side_text_first && !hit; ++k)
				hit = pass ? has(st->text[k], AUTO_CURES[i].ailment) :
					!_strnicmp(st->text[k], AUTO_CURES[i].ailment, strlen(AUTO_CURES[i].ailment));
			if (hit && (c = first(st, AUTO_CURES[i].choices)) != NULL) {
				ap_log("    treating %s", AUTO_CURES[i].ailment);
				return c;
			}
		}

	{
		static const char *const CARRY_ON[] = {"Continue as usual", "Continue", "Wait your turn",
			"Get the wagon upright", "Clean up", "Good!", NULL};
		if ((c = first(st, CARRY_ON)) != NULL)
			return c;
	}

	for (i = 0; i < st->side_option_first; ++i)
		if (st->options[i].command == GCMD_CHOICE && strncmp(st->options[i].label, "Get advice", 10) &&
			st->options[i].code != NODE_CMD_SAVE_DIARY) {
			ap_log("    no rule for this event -- taking '%s'", st->options[i].label);
			return &st->options[i];
		}

	return NULL;
}

static int32_t seen_count(const char *label)
{
	int32_t i;

	for (i = 0; i < g_town_seen_n; ++i)
		if (!strcmp(g_town_seen[i], label))
			return 1;

	return 0;
}

static void mark_seen(const char *label)
{
	if (!seen_count(label) && g_town_seen_n < 64)
		StrCopy(g_town_seen[g_town_seen_n++], label);
}

static int32_t need_oxen(void)
{
	return count_all(&party()->animals) + count_all(&party()->spare_animals) < 4;
}

static const cand *new_store(const struct control_state *st, const char *word)
{
	int32_t i;

	for (i = 0; i < st->side_option_first; ++i)
		if (!_strnicmp(st->options[i].label, "Go to the ", 10) && has(st->options[i].label, word) &&
			!seen_count(st->options[i].label))
			return &st->options[i];

	return NULL;
}

static const cand *enter(const cand *c)
{
	mark_seen(c->label);
	return c;
}

struct town_scene {
	int32_t id;
	int32_t exits[4];
	int32_t walks[16];
	int32_t walk_count;
	int32_t leaves;
	char store[64];
	int32_t store_id;
};

static struct town_scene g_scenes[512];
static int32_t g_scene_count;
static int32_t g_route_scene = -1, g_route_stuck;

static void store_title(int32_t store, char *out, int32_t size)
{
	struct rsrc *r = g_data_lib ? AssetsGetElement(g_data_lib, "STORE", (int16_t) store, false) : NULL;
	const char *p;
	int32_t i;

	out[0] = 0;
	if (!r)
		return;

	p = (const char *) RsrcData(r);
	for (i = 0; p && i < size - 1 && i < RsrcSize(r) && p[i] && p[i] != '\r' && p[i] != '\n'; ++i)
		out[i] = p[i];
	out[i] = 0;
	RsrcDestroy(&r);
}

static int32_t parse_scene(int32_t id, struct town_scene *sc)
{
	struct rsrc *r;
	const uint8_t *s;
	int32_t i, n;

	if (id <= 0 || !g_data_lib || (r = AssetsGetElement(g_data_lib, "SCENE", (int16_t) id, false)) == NULL)
		return 0;

	if (RsrcStorage(r) == RSRC_TEXT) {
		RsrcDestroy(&r);
		return 0;
	}

	RsrcLoadAndUnpack(r);
	s = RsrcData(r);
	memset(sc, 0, sizeof(*sc));
	sc->id = id;
	if (s) {
		struct scene_record rec;
		SceneRecordRead(s, &rec);
		sc->exits[SCENE_EDGE_LEFT] = rec.exit_left;
		sc->exits[SCENE_EDGE_RIGHT] = rec.exit_right;
		sc->exits[SCENE_EDGE_FORWARD] = rec.exit_forward;
		sc->exits[SCENE_EDGE_BACK] = rec.exit_back;
		for (i = 0; i < 4; ++i) {
			if (sc->exits[i] < 0 && sc->exits[i] != SCENE_EXIT_BACK)
				sc->leaves = 1;
		}

		if (rec.store_list > 0) {
			sc->store_id = rec.store_list;
			store_title(sc->store_id, sc->store, sizeof(sc->store));
		}

		n = rec.hotspot_count > 0 && rec.hotspot_count < 64 ? rec.hotspot_count : 0;
		for (i = 0; i < n; ++i) {
			struct scene_hotspot hot;
			SceneHotspotRead(s, i, &hot);
			if (hot.kind != GCMD_SCENE_WALK)
				continue;

			if (hot.argument > 0 && sc->walk_count < 16)
				sc->walks[sc->walk_count++] = hot.argument;
			else if (hot.argument < 0 && hot.argument != SCENE_EXIT_BACK)
				sc->leaves = 1;
		}
	}

	RsrcDestroy(&r);
	return 1;
}

static const struct town_scene *scene_info(int32_t id)
{
	int32_t i;

	if (id <= 0)
		return NULL;

	for (i = 0; i < g_scene_count; ++i)
		if (g_scenes[i].id == id)
			return &g_scenes[i];

	if (g_scene_count == 512 || !parse_scene(id, &g_scenes[g_scene_count]))
		return NULL;

	return &g_scenes[g_scene_count++];
}

static int32_t store_sells_gun(int32_t store)
{
	struct rsrc *r = g_data_lib ? AssetsGetElement(g_data_lib, "STORE", (int16_t) store, false) : NULL;
	const char *p, *end;
	int32_t found = 0;

	if (!r)
		return 0;

	p = (const char *) RsrcData(r);
	end = p ? p + RsrcSize(r) : p;
	while (p && p < end && !found) {
		const char *eol = p, *col = p;
		int32_t tabs = 0;
		while (eol < end && *eol != '\n' && *eol != '\r')
			++eol;

		if ((!strncmp(p, "rifle\t", 6) || !strncmp(p, "shotgun\t", 8) || !strncmp(p, "pistol\t", 7))) {
			for (col = p; col < eol && tabs < 3; ++col)
				if (*col == '\t')
					++tabs;
			if (tabs == 3 && col < eol && atoi(col) > 0)
				found = 1;
		}

		p = eol + 1;
	}

	RsrcDestroy(&r);
	return found;
}

static int32_t town_sells_gun(int32_t id)
{
	static int8_t known[2048];
	static int32_t queue[256];
	static struct town_scene sc;
	int32_t head = 0, tail = 0, i, k, found = 0, first_scene;

	if (id <= 0 || id >= 2048)
		return 0;

	if (known[id])
		return known[id] > 0;

	{
		struct lnode node;
		LNodeInit(&node, (int16_t) id, false);
		first_scene = node.landmark_conv;
	}

	if (first_scene > 0)
		queue[tail++] = first_scene;

	while (head < tail && !found) {
		int32_t at = queue[head++];
		if (!parse_scene(at, &sc))
			continue;

		if (sc.store_id > 0 && store_sells_gun(sc.store_id))
			found = 1;

		for (k = 0; k < 4 + sc.walk_count && tail < 256; ++k) {
			int32_t next = k < 4 ? sc.exits[k] : sc.walks[k - 4];
			if (next <= 0)
				continue;
			for (i = 0; i < tail && queue[i] != next; ++i)
				;
			if (i == tail)
				queue[tail++] = next;
		}
	}

	known[id] = found ? 1 : -1;
	return found;
}

struct town_goal {
	const char *const *words;
};

static int32_t goal_met(const struct town_scene *sc, const struct town_goal *g)
{
	char seen[32];
	int32_t i;

	if (!g->words)
		return sc->leaves;

	if (!sc->store[0])
		return 0;

	StrPrintf(seen, "scene %d", sc->id);
	if (seen_count(seen))
		return 0;

	for (i = 0; g->words[i]; ++i)
		if (has(sc->store, g->words[i]))
			return 1;

	return 0;
}

static int32_t town_route(int32_t from, const struct town_goal *goal, int32_t *dir, int32_t *target,
	int32_t *goal_scene)
{
	static int32_t queue[512], first_dir[512], first_target[512], depth[512];
	int32_t head = 0, tail = 0, i, k;
	const struct town_scene *sc;

	if ((sc = scene_info(from)) == NULL)
		return -1;

	queue[tail] = from;
	first_dir[tail] = -1;
	first_target[tail] = 0;
	depth[tail++] = 0;
	while (head < tail) {
		int32_t at = head++;
		int32_t next[20];
		int32_t next_dir[20];
		int32_t n = 0;
		if ((sc = scene_info(queue[at])) == NULL)
			continue;

		if (goal_met(sc, goal)) {
			*dir = first_dir[at];
			*target = first_target[at];
			*goal_scene = sc->id;
			return depth[at];
		}

		for (k = 0; k < 4; ++k)
			if (sc->exits[k] > 0) {
				next_dir[n] = k;
				next[n++] = sc->exits[k];
			}

		for (k = 0; k < sc->walk_count && n < 20; ++k) {
			next_dir[n] = -1;
			next[n++] = sc->walks[k];
		}

		for (k = 0; k < n && tail < 512; ++k) {
			for (i = 0; i < tail && queue[i] != next[k]; ++i)
				;
			if (i < tail)
				continue;

			queue[tail] = next[k];
			depth[tail] = depth[at] + 1;
			if (at == 0) {
				first_dir[tail] = next_dir[k];
				first_target[tail] = next_dir[k] < 0 ? next[k] : 0;

			} else {
				first_dir[tail] = first_dir[at];
				first_target[tail] = first_target[at];
			}
			++tail;
		}
	}

	return -1;
}

static const cand *step_option(const struct control_state *st, int32_t dir, int32_t target)
{
	int32_t i;

	for (i = 0; i < st->side_option_first; ++i) {
		const cand *c = &st->options[i];
		struct control *b = c->button ? &c->button->base : NULL;
		if (!b || c->command != GCMD_SCENE_WALK)
			continue;

		if (dir >= 0 ? (b->command_p2 == 0 && b->command_p1 == dir) : b->command_p2 == target)
			return c;
	}

	return NULL;
}

static const cand *route_to(const struct control_state *st, const struct town_goal *goal, const char *what)
{
	int32_t cur = g_scene_cur, dir = -1, target = 0, goal_scene = 0, steps;
	const cand *c;

	if (cur <= 0)
		return NULL;

	if (cur == g_route_scene) {
		if (++g_route_stuck > 3)
			return NULL;

	} else {
		g_route_scene = cur;
		g_route_stuck = 0;
	}

	steps = town_route(cur, goal, &dir, &target, &goal_scene);
	if (steps < 0)
		return NULL;

	if (steps == 0) {
		const struct town_scene *sc = scene_info(cur);
		int32_t k, i;
		if (goal->words || !sc)
			return NULL;

		for (k = 0; k < 4; ++k)
			if (sc->exits[k] < 0 && sc->exits[k] != SCENE_EXIT_BACK && (c = step_option(st, k, 0)) != NULL) {
				ap_log("    town map: leaving town from scene %d -- '%s'", cur, c->label);
				return c;
			}

		for (i = 0; i < st->side_option_first; ++i) {
			struct control *b = st->options[i].button ? &st->options[i].button->base : NULL;
			if (b && st->options[i].command == GCMD_SCENE_WALK && b->command_p2 < 0 &&
				b->command_p2 != SCENE_EXIT_BACK) {
				ap_log("    town map: leaving town from scene %d -- '%s'", cur, st->options[i].label);
				return &st->options[i];
			}
		}

		return find_prefix(st, "Leave");
	}

	if ((c = step_option(st, dir, target)) != NULL)
		ap_log("    town map: %s is scene %d, %d step%s from scene %d -- '%s'", what, goal_scene, steps,
			steps == 1 ? "" : "s", cur, c->label);

	return c;
}

static const char *const OXEN_STORES[] = {"livery", "stable", NULL};
static const char *const FOOD_STORES[] = {"General Store", "Trading Post", NULL};
static const char *const ALL_STORES[] = {"Store", "Trading Post", "gunsmith", "pharmacy", "Blacksmith", "waggoner",
	"wagonner", "clothier", "livery", NULL};

static const cand *pick_town(const struct control_state *st)
{
	const cand *c;
	int32_t i, in_store = 0, want_oxen, want_food;

	if (g_town_node != g_game->cur_node_id) {
		g_town_node = g_game->cur_node_id;
		g_town_steps = 0;
		g_town_seen_n = 0;
	}

	++g_town_steps;
	for (i = 0; i < st->side_option_first; ++i)
		if (!strncmp(st->options[i].label, "Buy:", 4))
			in_store = g_shop_had_store = 1;

	want_oxen = need_oxen() && g_oxen_tries < 3;
	want_food = !want_oxen && g_game->days_traveled > 0 &&
		(food_days() < food_target_days() - 5 || restock_needed() || score_shopping());

	if (in_store && g_game->days_traveled == 0) {
		char bought[32];
		StrPrintf(bought, "bought %d", g_scene_cur);
		if (!seen_count(bought) && (c = find_prefix(st, "Buy: everything")) != NULL) {
			mark_seen(bought);
			return c;
		}
	}

	if (in_store) {
		{
			char seen[32];
			StrPrintf(seen, "scene %d", g_scene_cur);
			mark_seen(seen);
		}

		if (want_oxen) {
			for (i = 0; i < st->side_option_first; ++i)
				if (!strncmp(st->options[i].label, "Buy:", 4) && (has(st->options[i].label, " ox") ||
					has(st->options[i].label, "(horse") || has(st->options[i].label, "livestock")))
					return &st->options[i];

			if (seen_count("livery") && (c = find_prefix(st, "Buy: everything")) != NULL)
				return c;

		} else if (want_food) {
			char bought[32];
			StrPrintf(bought, "bought %d", g_scene_cur);
			if (!seen_count(bought) && (c = find_prefix(st, "Buy: everything")) != NULL) {
				mark_seen(bought);
				g_store_opened = 0;
				return c;
			}

			{
				static const char *const FIRST[] = {"Buy: food", "Buy: spices", "Buy:", NULL};
				char failed[32];
				int32_t pass;
				StrPrintf(failed, "no window %d", g_scene_cur);
				if (!g_store_opened)
					mark_seen(failed);

				for (pass = 0; seen_count(failed) && FIRST[pass]; ++pass)
					for (i = 0; i < st->side_option_first; ++i) {
						char kind[96];
						const char *l = st->options[i].label;
						if (strncmp(l, FIRST[pass], strlen(FIRST[pass])) || !strncmp(l, "Buy: everything", 15))
							continue;

						StrPrintf(kind, "%d %s", g_scene_cur, l);
						if (!seen_count(kind)) {
							mark_seen(kind);
							ap_log("    store: no window for everything -- trying '%s'", l);
							return &st->options[i];
						}
					}
			}
		}

		if ((c = find_prefix(st, "Go back")) != NULL || (c = find_prefix(st, "Turn around")) != NULL ||
			(c = find_prefix(st, "Leave")) != NULL)
			return c;

		return NULL;
	}

	{
		struct town_goal oxen = {OXEN_STORES}, food = {FOOD_STORES}, shops = {ALL_STORES}, out = {NULL};
		if (want_oxen) {
			if ((c = route_to(st, &oxen, "the livery stable")) != NULL) {
				mark_seen("livery");
				return c;
			}

		} else if (g_game->days_traveled == 0 && (c = route_to(st, &shops, "the next store")) != NULL) {
			return c;

		} else if (want_food && (c = route_to(st, restock_needed() ? &shops : &food, "the store")) != NULL) {
			return c;
		}

		if ((!want_oxen && !want_food) || g_town_steps > 80 || scene_info(g_scene_cur)) {
			if ((c = find_prefix(st, "Leave")) != NULL)
				return c;
			if ((c = route_to(st, &out, "the way out")) != NULL)
				return c;
		}
	}

	if (want_oxen && ((c = new_store(st, "livery")) != NULL || (c = new_store(st, "stable")) != NULL)) {
		mark_seen("livery");
		return enter(c);
	}

	if (want_food && ((c = new_store(st, "General Store")) != NULL || (c = new_store(st, "Trading Post")) != NULL))
		return enter(c);

	if ((!want_oxen && !want_food) || g_town_steps > 80)
		if ((c = find_prefix(st, "Leave")) != NULL)
			return c;

	for (i = 0; i < st->side_option_first; ++i) {
		const char *l = st->options[i].label;
		if ((!strncmp(l, "Walk on", 7) || !strncmp(l, "Go over to", 10) || !strncmp(l, "Walk forward", 12)) &&
			!seen_count(l)) {
			mark_seen(l);
			g_scene_turns = 0;
			return &st->options[i];
		}
	}

	if (++g_scene_turns > 4 && (c = find_prefix(st, "Go back")) != NULL) {
		g_scene_turns = 0;
		return c;
	}

	if ((c = find_prefix(st, "Walk forward")) != NULL && auto_rand() % 4)
		return c;

	switch (auto_rand() % 3) {
	case 0:
		if ((c = find_prefix(st, "Turn right")) != NULL)
			return c;
	case 1:
		if ((c = find_prefix(st, "Turn left")) != NULL)
			return c;
	default:
		if ((c = find_prefix(st, "Turn around")) != NULL)
			return c;
	}

	if ((c = find_prefix(st, "Walk forward")) != NULL)
		return c;

	return find_prefix(st, "Leave");
}

struct start_item {
	const char *key;
	int32_t base, per_person;
	const char *avoid;
};

static const struct start_item START_ITEMS[] = {
	{"of flour", 0, 3, NULL}, {"of bacon", 0, 35, NULL}, {"of dried vegetables", 0, 6, NULL},
	{"of dried fruit", 0, 5, NULL}, {"of dried beans", 0, 5, NULL}, {"of crackers", 0, 3, NULL},
	{"of salt pork", 0, 10, NULL}, {"of cornmeal", 0, 1, NULL}, {"of salt", 3, 0, NULL}, {"of coffee beans", 0, 3, NULL},
	{"of molasses", 0, 3, NULL}, {"of pepper", 2, 0, NULL}, {"of pickles", 2, 0, NULL},
	{"rifle", 1, 0, NULL}, {"of gunpowder", 1, 0, NULL}, {"bullets", 4, 0, NULL}, {"sheath", 1, 0, NULL},
	{"wagon wheel", 1, 0, NULL}, {"wagon axle", 1, 0, NULL}, {"wagon tongue", 1, 0, NULL}, {"ox yoke", 1, 0, NULL},
	{"of boots", 0, 1, NULL}, {"blanket", 0, 1, "saddle"}, {"of long underwear", 0, 1, NULL},
	{"isopropyl alcohol", 2, 0, NULL}, {"of sulfur", 2, 0, NULL}, {"of laudanum", 2, 0, NULL},
	{"of peppermint", 2, 0, NULL}, {"of quinine", 1, 0, NULL}, {"of camphor", 1, 0, NULL},
	{"of epsom salts", 1, 0, NULL}, {"dover's powder", 1, 0, NULL}, {"of soap", 1, 0, NULL},
	{"of matches", 2, 0, NULL}, {"cooking utensils", 1, 0, NULL}, {"frying pan", 1, 0, NULL}, {"of rope", 2, 0, NULL},
	{"pick ax", 1, 0, NULL}, {"hammer", 1, 0, NULL}, {"saw", 1, 0, NULL}, {"of nails", 5, 0, NULL},
	{"water keg", 2, 0, NULL}, {"canteen", 0, 1, NULL}, {"bridle", 1, 0, NULL}, {"mending yarn", 1, 0, NULL},
	{"of grease", 1, 0, NULL},
	{NULL, 0, 0, NULL}
};

static int32_t name_is(const char *name, const struct start_item *it)
{
	static const char *const plural[] = {"", "s", "es", "en", NULL};
	size_t n = strlen(name), k = strlen(it->key);
	int32_t i;

	if (it->avoid && has(name, it->avoid))
		return 0;

	for (i = 0; plural[i]; ++i) {
		size_t p = strlen(plural[i]);
		if (n >= k + p && !_strnicmp(name + n - k - p, it->key, k) && !_stricmp(name + n - p, plural[i]) &&
			(n == k + p || name[n - k - p - 1] == ' ' || it->key[0] == ' '))
			return 1;
	}

	return 0;
}

static int32_t short_name_is(const char *name, const struct start_item *it)
{
	char base[96];
	const char *key = !_strnicmp(it->key, "of ", 3) ? it->key + 3 : it->key;
	const char *bracket = strstr(name, " [");
	size_t n = bracket ? (size_t) (bracket - name) : strlen(name), k = strlen(key);

	if (n >= sizeof(base))
		return 0;

	memcpy(base, name, n);
	base[n] = 0;
	if (it->avoid && has(base, it->avoid))
		return 0;

	return n >= k && !_stricmp(base + n - k, key) && (n == k || base[n - k - 1] == ' ');
}

static int32_t owned(const struct start_item *it)
{
	struct list_node *node;
	int32_t n = 0;

	for (node = party()->inventory.base.head; node; node = node->next) {
		struct inv_item *item = item_of(node);
		const char *name = item_key(item);
		int32_t hit = name && short_name_is(name, it);
		int32_t gun = name && (has(name, "rifle") || has(name, "shotgun") || has(name, "pistol")) && !has(name, "sheath");
		if (hit || (gun && !strcmp(it->key, "rifle")))
			n += InvItemGetCount(item, false);
	}

	return n;
}

static int32_t g_plan[64];
static int32_t g_plan_price[64], g_plan_lb[64];
static int32_t g_plan_oxen;
static int32_t g_plan_made;
static int32_t g_visit_qty[64];
static int32_t g_visit_cents, g_visit_lb;
static int32_t g_visit_food_lb;
static void *g_visit_dlg;

static void table_item(const struct start_item *it, int32_t *cents, int32_t *lb)
{
	struct strrsrc *keys = GetItemKeyNames();
	int32_t i;

	*cents = *lb = 0;
	for (i = 0; keys && i < StrRsrcCount(keys); ++i) {
		struct cstr *s = StrRsrcGetCStr(keys, (int16_t) i);
		const char *p = CStrGet(s);
		int32_t hit = p && short_name_is(p, it);
		if (hit) {
			struct inv_item tmp;
			InvItemInit(&tmp, s, 1, 1.0);
			*cents = InvItemGetUnitPrice(&tmp);
			*lb = (inv_item_get_unit_weight_x16(&tmp) + 15) / 16;
			CStrDestroy(&s);
			inv_item_destroy(&tmp.base);
			return;
		}
		CStrDestroy(&s);
	}
}

static void make_plan(void)
{
	struct start_item ox = {"ox", 0, 0, NULL};
	int32_t m = alive_members(), i, budget, animals, price, lb;

	budget = (int32_t) (party()->cash * 100.0) - RESERVE_CENTS;
	animals = count_all(&party()->animals) + count_all(&party()->spare_animals);
	table_item(&ox, &price, &lb);
	price = (price > 0 ? price : 1000) * 12 / 10;
	g_plan_oxen = budget >= 60 * price ? 10 : budget >= 30 * price ? 8 : 6;
	if (g_plan_oxen < animals)
		g_plan_oxen = animals;
	budget -= (g_plan_oxen - animals) * price;
	for (i = 0; START_ITEMS[i].key && i < 64; ++i) {
		int32_t own = owned(&START_ITEMS[i]);
		int32_t want = START_ITEMS[i].base + START_ITEMS[i].per_person * m - own;
		table_item(&START_ITEMS[i], &g_plan_price[i], &g_plan_lb[i]);
		g_plan_price[i] = g_plan_price[i] * 12 / 10;
		if (g_plan_price[i] < 1)
			g_plan_price[i] = 1;
		if (want < 0)
			want = 0;
		if (want * g_plan_price[i] > budget)
			want = budget > 0 ? budget / g_plan_price[i] : 0;
		g_plan[i] = own + want;
		budget -= want * g_plan_price[i];
	}

	g_plan_made = 1;
	ap_log("    shopping plan: $%.2f, %d oxen, $%.2f left over the reserve", party()->cash, g_plan_oxen,
		budget / 100.0);
}

static void plan_remaining(int32_t *cents, int32_t *lb)
{
	int32_t i;

	*cents = *lb = 0;
	for (i = 0; g_plan_made && START_ITEMS[i].key && i < 64; ++i) {
		int32_t left = g_plan[i] - owned(&START_ITEMS[i]) - g_visit_qty[i];
		if (left > 0) {
			*cents += left * g_plan_price[i];
			*lb += left * g_plan_lb[i];
		}
	}
}

static int32_t start_want(const char *label, int32_t *entry)
{
	int32_t i, want;

	for (i = 0; START_ITEMS[i].key && i < 64; ++i)
		if (name_is(label, &START_ITEMS[i])) {
			*entry = i;
			want = (g_plan_made ? g_plan[i] : START_ITEMS[i].base + START_ITEMS[i].per_person * alive_members()) -
				owned(&START_ITEMS[i]) - g_visit_qty[i];
			return want > 0 ? want : 0;
		}

	return 0;
}

static const struct start_item RESTOCK_ITEMS[] = {
	{"wagon wheel", 1, 0, NULL}, {"wagon axle", 1, 0, NULL}, {"wagon tongue", 1, 0, NULL}, {"of boots", 0, 1, NULL},
	{"isopropyl alcohol", 1, 0, NULL}, {"of laudanum", 1, 0, NULL}, {"of soap", 1, 0, NULL},
	{"of matches", 1, 0, NULL}, {"water keg", 1, 0, NULL}, {"ox yoke", 1, 0, NULL}, {"of salt", 2, 0, NULL},
	{"of rope", 2, 0, NULL},
	{NULL, 0, 0, NULL}
};

static int32_t need_produce(void)
{
	return (MainUiGetCatFlag(CAT_NO_FRUIT) && MainUiGetCatFlag(CAT_NO_VEGETABLES)) || party()->scurvy_risk > 30;
}

static int32_t restock_needed(void)
{
	if (party()->cash < 5.0)
		return 0;

	return hunting_lacks() != NULL || !owned(&RESTOCK_ITEMS[0]) || !owned(&RESTOCK_ITEMS[1]) ||
		!owned(&RESTOCK_ITEMS[2]) || !InvListHasGroup(&party()->inventory, IGRP_CLOTHING) || need_produce();
}

static int32_t restock_want(const char *label)
{
	static const struct start_item RIFLE = {"rifle", 0, 0, NULL}, POWDER = {"of gunpowder", 0, 0, NULL};
	static const struct start_item BULLETS = {"bullets", 0, 0, NULL}, SHOT = {"of shot", 0, 0, NULL};
	int32_t i, want, rifle_or_pistol = have("rifle") || have("pistol");

	if (name_is(label, &RIFLE))
		return rifle_or_pistol || have("shotgun") ? 0 : 1;

	if (name_is(label, &POWDER))
		return have("gunpowder") ? 0 : 1;

	if (name_is(label, &BULLETS))
		return have("bullets") < 2 ? 3 - have("bullets") : 0;

	if (name_is(label, &SHOT))
		return have("shotgun") && !rifle_or_pistol && have("shot") < 2 ? 3 - have("shot") : 0;

	for (i = 0; RESTOCK_ITEMS[i].key; ++i)
		if (name_is(label, &RESTOCK_ITEMS[i])) {
			want = RESTOCK_ITEMS[i].base + RESTOCK_ITEMS[i].per_person * alive_members() - owned(&RESTOCK_ITEMS[i]);
			return want > 0 ? want : 0;
		}

	return 0;
}

static int32_t plan_done(void)
{
	int32_t cents, lb;

	plan_remaining(&cents, &lb);
	return g_plan_made && cents < 500;
}

static int32_t is_food_item(struct inv_item *item)
{
	static int32_t food = -2;

	if (food == -2)
		food = category("food");

	return food >= 0 && InvItemInCategory(item, (int16_t) food);
}

static int32_t side_dish_group(struct inv_item *item)
{
	if (!item || InvItemInGroup(item, IGRP_FOOD_PRODUCE) || InvItemInGroup(item, IGRP_FOOD_MEAT) ||
		InvItemInGroup(item, IGRP_FOOD_GRAIN))
		return -1;

	return InvItemInGroup(item, IGRP_SEASONING) ? 0 : InvItemInGroup(item, IGRP_FOOD_OTHER) ? 1 : -1;
}

static int32_t g_visit_side[2];

static int32_t side_dish_want(struct inv_item *item)
{
	int32_t g = side_dish_group(item), portions, cents, need, left = days_to_go();

	if (g < 0)
		return 0;

	portions = InvItemGetPortionsPerUnit(item);
	cents = InvItemGetUnitPrice(item);
	if (portions <= 0 || unit_weight_x16(item) > portions || cents * 100 > 50 * portions)
		return 0;

	if (item->days_to_spoil > 0 && item->days_to_spoil < (left > 0 ? left : 60))
		return 0;

	need = meal_helping(RATIONS_FILLING) * food_target_days() - group_units(g) - g_visit_side[g];
	return need > 0 ? (need + portions - 1) / portions : 0;
}

static int32_t food_want(const char *label, struct inv_item *item)
{
	static const struct start_item PRODUCE[] = {{"of dried vegetables", 0, 0, NULL}, {"of dried fruit", 0, 0, NULL},
		{"of pickles", 0, 0, NULL}, {"of onions", 0, 0, NULL}, {NULL, 0, 0, NULL}};
	static const struct start_item STAPLES[] = {{"of bacon", 0, 0, NULL}, {"of flour", 0, 0, NULL},
		{"of salt pork", 0, 0, NULL}, {"of cornmeal", 0, 0, NULL}, {"of dried beans", 0, 0, NULL},
		{"of rice", 0, 0, NULL}, {"of crackers", 0, 0, NULL}, {"of biscuits", 0, 0, NULL},
		{"of pemmican", 0, 0, NULL}, {"of dried bread", 0, 0, NULL}, {NULL, 0, 0, NULL}};
	int32_t days = food_target_days();
	int32_t i, lb, need = 3 * alive_members() * (days - food_days()), left = days_to_go();
	int32_t unit_lb = item ? (unit_weight_x16(item) + 15) / 16 : 1;

	if (unit_lb < 1)
		unit_lb = 1;

	if (item && (lb = side_dish_want(item)) > 0)
		return lb;

	for (i = 0; PRODUCE[i].key; ++i)
		if (name_is(label, &PRODUCE[i])) {
			if (need_produce())
				lb = 3 * alive_members() * days / 4;
			else if (need > 0)
				lb = need * 15 / 100;
			else
				return 0;
			return lb / unit_lb > 0 ? lb / unit_lb : 1;
		}

	if (need <= 0)
		return 0;

	for (i = 0; STAPLES[i].key; ++i)
		if (name_is(label, &STAPLES[i])) {
			if (item && item->days_to_spoil > 0 && left > 0 && item->days_to_spoil < left)
				return 0;
			lb = need - g_visit_food_lb;
			if (lb > need * 60 / 100)
				lb = need * 60 / 100;
			return lb > 0 ? (lb + unit_lb - 1) / unit_lb : 0;
		}

	return 0;
}

static int32_t score_shopping(void)
{
	int32_t left = days_to_go();

	return option_score_goods() && left >= 0 && left <= 20 &&
		party()->cash * 100.0 > RESERVE_CENTS + (left + 7) * 3 * alive_members() * 10 + 2000;
}

static int32_t score_want(struct inv_item *item, const char *name, int32_t unit_cents, int32_t unit_lb)
{
	static int32_t clothing = -2;
	int32_t money, room, n;

	if (clothing == -2)
		clothing = category("clothing");

	if (!key_is(name, "shot") && !(clothing >= 0 && InvItemInCategory(item, (int16_t) clothing) && unit_cents <= 60))
		return 0;

	if (days_to_go() < 0 || days_to_go() > 20)
		return 0;

	money = (int32_t) (party()->cash * 100.0) - RESERVE_CENTS - g_visit_cents -
		(days_to_go() + 7) * 3 * alive_members() * 10;
	room = spare_easy_load() - g_visit_lb - 150;
	if (unit_cents < 1)
		unit_cents = 1;
	if (unit_lb < 1)
		unit_lb = 1;
	n = money / unit_cents;
	if (n > room / unit_lb)
		n = room / unit_lb;

	return n > 0 ? n : 0;
}

static int32_t shop_step(struct control_state *st)
{
	static char last_page_key[128];
	static char filled[48][64];
	static int32_t filled_qty[48], filled_cents[48], filled_lb[48], filled_food[48], filled_entry[48];
	static int32_t filled_side[48], filled_units[48];
	static int32_t pages, filled_n;
	int32_t i, k, acted = 0, start = g_game->days_traveled == 0;
	const cand *c;

	g_store_opened = 1;
	if (st->hwnd != g_visit_dlg) {
		g_visit_dlg = st->hwnd;
		memset(g_visit_qty, 0, sizeof(g_visit_qty));
		g_visit_cents = g_visit_lb = g_visit_food_lb = 0;
		g_visit_side[0] = g_visit_side[1] = 0;
	}

	if (start && !g_plan_made)
		make_plan();

	for (i = 0; i < st->field_count; ++i) {
		struct control_field *f = &st->fields[i];
		struct inv_grid *grid = f->edit ? as_inv_grid(f->edit->base.base.parent) : NULL;
		struct inv_item *item = grid ? InvGridGetItemAt(grid, (int16_t) (f->id - GRID_ROW_QTY)) : NULL;
		const char *label = f->label + 10, *why = NULL;
		char name[96];
		int32_t want = 0, entry = -1, cents = 0, lb = 1, money, food, side;
		char qty[16];

		if (f->is_choice || strncmp(f->label, "Quantity: ", 10))
			continue;

		for (k = 0; k < filled_n && strcmp(filled[k], f->label); ++k)
			;
		if (k < filled_n) {
			int32_t now = atoi(f->text), cut = filled_qty[k] - now;
			if (now >= 0 && cut > 0) {
				ap_log("    store: %s lowered to %d", label, now);
				if (filled_entry[k] >= 0)
					g_visit_qty[filled_entry[k]] -= cut;
				g_visit_cents -= cut * filled_cents[k];
				g_visit_lb -= cut * filled_lb[k];
				if (filled_food[k])
					g_visit_food_lb -= cut * filled_lb[k];
				if (filled_side[k] >= 0)
					g_visit_side[filled_side[k]] -= cut * filled_units[k];
				filled_qty[k] = now;
			}
			continue;
		}

		name[0] = 0;
		if (item) {
			const char *p = item_key(item);
			StrCopy(name, p ? p : "");
			cents = InvItemGetUnitPrice(item);
			lb = (unit_weight_x16(item) + 15) / 16;
		}

		if (!_strnicmp(label, "ox", 2) || has(f->label, " oxen")) {
			int32_t animals = count_all(&party()->animals) + count_all(&party()->spare_animals);
			want = (start && g_plan_made ? g_plan_oxen : need_oxen() ? 6 : 0) - animals;
			why = "draft animals";

		} else if (start) {
			if ((want = start_want(label, &entry)) > 0)
				why = "starting supplies";
			else if (plan_done() && (want = food_want(label, item)) > 0)
				why = "food";

		} else {
			if ((want = food_want(label, item)) > 0)
				why = "food";
			else if ((want = restock_want(label)) > 0)
				why = "restock";
			else if (option_score_goods() && item && (want = score_want(item, name, cents, lb)) > 0)
				why = "for the score";
		}

		if (want <= 0)
			continue;

		money = (int32_t) (party()->cash * 100.0) - g_visit_cents - (start ? 0 : TRAIL_RESERVE_CENTS);
		if (cents > 0 && want > money / cents)
			want = money / cents;
		if (want <= 0)
			continue;

		food = !strcmp(why, "food") || (entry >= 0 && item && is_food_item(item));
		side = food ? side_dish_group(item) : -1;
		if (filled_n < 48) {
			StrCopy(filled[filled_n], f->label);
			filled_qty[filled_n] = want;
			filled_cents[filled_n] = cents;
			filled_lb[filled_n] = lb;
			filled_food[filled_n] = food;
			filled_side[filled_n] = side;
			filled_units[filled_n] = item ? InvItemGetPortionsPerUnit(item) : 0;
			filled_entry[filled_n++] = entry;
		}

		if (side >= 0 && item)
			g_visit_side[side] += want * InvItemGetPortionsPerUnit(item);
		if (entry >= 0)
			g_visit_qty[entry] += want;
		g_visit_cents += want * cents;
		g_visit_lb += want * lb;
		if (food)
			g_visit_food_lb += want * lb;

		StrPrintf(qty, "%d", want);
		if (strcmp(f->text, qty)) {
			ap_log("    store: %s = %s (%s, $%.2f)", label, qty, why, want * cents / 100.0);
			ControlSetField(st, i, qty);
			acted = 1;
			if (!strcmp(why, "food"))
				break;
		}
	}

	if (acted)
		return 1;

	if (st->field_count && strcmp(last_page_key, st->fields[st->field_count - 1].label)) {
		StrCopy(last_page_key, st->fields[st->field_count - 1].label);
		if ((c = find(st, "Next page of items")) != NULL && pages++ < 25) {
			ControlClick(st, c->index);
			return 1;
		}
	}

	pages = 0;
	filled_n = 0;
	last_page_key[0] = 0;
	g_visit_dlg = NULL;
	if (need_oxen())
		++g_oxen_tries;

	if ((c = find(st, "Purchase")) != NULL) {
		ap_log("    store: purchase ($%.2f)", g_visit_cents / 100.0);
		ControlClick(st, c->index);
		return 1;
	}

	return 0;
}

static int32_t category(const char *name)
{
	struct strrsrc *cats = GetItemCategoryNames();
	int32_t i, found = -1;

	for (i = 0; cats && i < StrRsrcCount(cats) && found < 0; ++i) {
		struct cstr *s = StrRsrcGetCStr(cats, (int16_t) i);
		const char *p = CStrGet(s);
		if (p && !_stricmp(p, name))
			found = i;
		CStrDestroy(&s);
	}

	return found;
}

struct dump_plan_item {
	char name[96];
	int32_t count, unit_lb;
};

static struct dump_plan_item g_dump_plan[96];
static int32_t g_dump_plan_n;

enum {
	DUMP_FURNITURE,
	DUMP_FOOD_SURPLUS,
	DUMP_SPARE,
	DUMP_FOOD_15,
	DUMP_ALL,
};

static int32_t plan_dump(int32_t target, int32_t keep_days, int32_t last_tier)
{
	static const int32_t FOOD_DAYS[DUMP_ALL + 1] = {0, 0, 0, 15, 5};
	int32_t furniture = category("furniture"), food = category("food"), tier, per_day, food_days_n, food_dumped = 0;
	int32_t start, remaining;
	struct list_node *node;

	start = remaining = 16 * target + 32;
	g_dump_plan_n = 0;
	per_day = 3 * 16 * alive_members();
	food_days_n = food_days();
	for (tier = DUMP_FURNITURE; tier <= last_tier && remaining > 0; ++tier)
		for (node = party()->inventory.base.head; node && remaining > 0; node = node->next) {
			struct inv_item *item = item_of(node);
			int32_t unit = unit_weight_x16(item), is_food = food >= 0 && InvItemInCategory(item, (int16_t) food);
			int32_t i, planned = 0, more = 0, have_n = InvItemGetCount(item, false);
			char name[96];
			if (unit < 1 || have_n <= 0)
				continue;

			item_name(item, name, sizeof(name));
			if (key_is(name, "cash"))
				continue;

			for (i = 0; i < g_dump_plan_n && strcmp(g_dump_plan[i].name, name); ++i)
				;
			if (i < g_dump_plan_n)
				planned = g_dump_plan[i].count;
			if (planned >= have_n)
				continue;

			switch (tier) {
			case DUMP_FURNITURE:
				if (furniture >= 0 && InvItemInCategory(item, (int16_t) furniture))
					more = have_n;
				break;
			case DUMP_FOOD_SURPLUS:
			case DUMP_FOOD_15:
				if (is_food)
					more = ((food_days_n - (tier == DUMP_FOOD_SURPLUS ? keep_days : FOOD_DAYS[tier])) * per_day -
						food_dumped) / unit;
				break;
			case DUMP_SPARE:
				if (!is_food)
					for (more = have_n - planned; more > 0 && critical(item, planned + more); --more)
						;
				break;
			case DUMP_ALL:
				more = is_food ? ((food_days_n - FOOD_DAYS[tier]) * per_day - food_dumped) / unit : have_n;
				break;
			}

			if (more > have_n - planned)
				more = have_n - planned;
			if (more > (remaining + unit - 1) / unit)
				more = (remaining + unit - 1) / unit;
			if (more <= 0)
				continue;

			if (i == g_dump_plan_n) {
				if (g_dump_plan_n == (int32_t) (sizeof(g_dump_plan) / sizeof(g_dump_plan[0])))
					continue;
				StrCopy(g_dump_plan[i].name, name);
				g_dump_plan[i].count = 0;
				g_dump_plan[i].unit_lb = (unit + 15) / 16;
				++g_dump_plan_n;
			}

			g_dump_plan[i].count += more;
			remaining -= more * unit;
			if (is_food)
				food_dumped += more * unit;
		}

	return (start - remaining) / 16;
}

static int32_t dump_step(struct control_state *st)
{
	static struct inv_grid *grid_seen;
	struct inv_grid *grid = NULL;
	int32_t i;
	const cand *c;

	for (i = 0; i < st->field_count && !grid; ++i)
		if (st->fields[i].edit)
			grid = as_inv_grid(st->fields[i].edit->base.base.parent);

	if (!grid)
		return 0;

	if (grid != grid_seen) {
		int32_t tenth = load() / 10, excess = -spare_easy_load(), planned;
		grid_seen = grid;
		if (tenth < 100)
			tenth = 100;
		planned = excess > 0 ? plan_dump(excess, g_dump_rounds < 2 ? 90 : 45, g_dump_stuck ? DUMP_ALL : DUMP_SPARE) :
			plan_dump(tenth, g_dump_rounds < 2 ? 90 : 45, DUMP_FOOD_SURPLUS);
		ap_log("    dump: %d lb planned (%d over the easy load)", planned, excess > 0 ? excess : 0);
	}

	for (i = 0; i < st->field_count; ++i) {
		struct control_field *f = &st->fields[i];
		struct inv_item *item;
		char name[96], qty[16];
		int32_t k;
		if (!f->edit || f->text[0] || strncmp(f->label, "Quantity: ", 10))
			continue;

		item = InvGridGetItemAt(grid, (int16_t) (f->id - GRID_ROW_QTY));
		if (!item)
			continue;

		item_name(item, name, sizeof(name));
		for (k = 0; k < g_dump_plan_n; ++k)
			if (g_dump_plan[k].count > 0 && !strcmp(g_dump_plan[k].name, name)) {
				int32_t count = InvItemGetCount(item, false);
				int32_t dump = g_dump_plan[k].count < count ? g_dump_plan[k].count : count;
				g_dump_plan[k].count = 0;
				StrPrintf(qty, "%d", dump);
				ap_log("    dump: %s x%d (%d lb)", f->label + 10, dump, dump * g_dump_plan[k].unit_lb);
				ControlSetField(st, i, qty);
				return 1;
			}
	}

	for (i = 0; i < g_dump_plan_n && !g_dump_plan[i].count; ++i)
		;
	if (i < g_dump_plan_n && grid->page < grid->last_page && (c = find_prefix(st, "Next page of items")) != NULL) {
		ControlClick(st, c->index);
		return 1;
	}

	grid_seen = NULL;
	g_dump_plan_n = 0;
	g_dump_stuck = 0;
	++g_dump_rounds;
	if ((c = find_prefix(st, "Dump")) != NULL) {
		ap_log("    dump: done (round %d)", g_dump_rounds);
		ControlClick(st, c->index);
		return 1;
	}

	return 0;
}

static uint32_t g_raft_steered;

struct raft_route {
	const void *grid;
	int32_t w, h, reach_c, reach_r;
	int32_t *dist;
};
static struct raft_route g_raft_route;

static int32_t raft_grid_w(struct ford_wagon *raft)
{
	return ReadLe16(raft->grid);
}

static int32_t raft_grid_h(struct ford_wagon *raft)
{
	return ReadLe16(raft->grid + 2);
}

static uint32_t raft_cell(struct ford_wagon *raft, int32_t col, int32_t row)
{
	return ReadLeU16(raft->grid + 4 + 2 * ((intptr_t) row * (intptr_t) raft->grid_w + col));
}

static int32_t raft_cell_blocked(struct ford_wagon *raft, int32_t col, int32_t row)
{
	uint32_t tile;

	if (!raft->grid || col < 0 || row < 0 || col >= raft_grid_w(raft) || row >= raft_grid_h(raft))
		return 1;

	tile = raft_cell(raft, col, row);
	return tile == RAFT_LAND || tile >= RAFT_SHORE_FIRST || (tile > RAFT_CURRENT_LAST && tile < RAFT_START);
}

static int32_t raft_cost(const int32_t *wall, int32_t cell)
{
	return 1 + (wall[cell] < 7 ? (7 - wall[cell]) * 4 : 0);
}

static int32_t raft_plan(struct ford_wagon *raft, int32_t reach)
{
	int32_t w = raft_grid_w(raft), h = raft_grid_h(raft), n = w * h, c, r, k, i, heap_n = 0;
	int32_t *wall = malloc(sizeof(int32_t) * (size_t) n), *queue = malloc(sizeof(int32_t) * (size_t) n);
	int32_t *heap = malloc(sizeof(int32_t) * (size_t) n * 8 + 64), head = 0, tail = 0;
	static const int32_t DC[8] = {-1, 0, 1, -1, 1, -1, 0, 1}, DR[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

	free(g_raft_route.dist);
	g_raft_route.dist = malloc(sizeof(int32_t) * (size_t) n);
	if (!wall || !queue || !heap || !g_raft_route.dist) {
		free(wall);
		free(queue);
		free(heap);
		free(g_raft_route.dist);
		g_raft_route.dist = NULL;
		return 0;
	}

	for (i = 0; i < n; ++i) {
		c = i % w;
		r = i / w;
		wall[i] = raft_cell_blocked(raft, c, r) ? 0 : -1;
		if (wall[i] < 0 && (c == 0 || c == w - 1))
			wall[i] = 1;
		if (wall[i] >= 0)
			queue[tail++] = i;
		g_raft_route.dist[i] = -1;
	}

	while (head < tail) {
		int32_t at = queue[head++];
		for (k = 0; k < 8; ++k) {
			int32_t nc = at % w + DC[k], nr = at / w + DR[k], m;
			if (nc < 0 || nr < 0 || nc >= w || nr >= h)
				continue;
			m = nr * w + nc;
			if (wall[m] < 0) {
				wall[m] = wall[at] + 1;
				queue[tail++] = m;
			}
		}
	}

	for (c = 0; c < w; ++c)
		for (r = 0; r <= 1; ++r)
			if (wall[r * w + c] > reach) {
				g_raft_route.dist[r * w + c] = 0;
				heap[heap_n++] = r * w + c;
			}

	while (heap_n > 0) {
		int32_t top = heap[0], at = top % n, d = top / n, pos = 0;
		heap[0] = heap[--heap_n];
		for (;;) {
			int32_t a = 2 * pos + 1, b = a + 1, s = pos;
			if (a < heap_n && heap[a] < heap[s])
				s = a;
			if (b < heap_n && heap[b] < heap[s])
				s = b;
			if (s == pos)
				break;
			k = heap[s];
			heap[s] = heap[pos];
			heap[pos] = k;
			pos = s;
		}

		if (d > g_raft_route.dist[at])
			continue;

		for (k = 0; k < 8; ++k) {
			int32_t nc = at % w + DC[k], nr = at / w + DR[k], m, nd;
			if (nc < 0 || nr < 0 || nc >= w || nr >= h)
				continue;
			m = nr * w + nc;
			if (wall[m] <= reach)
				continue;
			nd = d + raft_cost(wall, m) + (DC[k] && DR[k] ? 1 : 0);
			if ((g_raft_route.dist[m] < 0 || nd < g_raft_route.dist[m]) && heap_n < n * 8 && nd < 40000) {
				g_raft_route.dist[m] = nd;
				heap[heap_n] = nd * n + m;
				for (pos = heap_n++; pos > 0 && heap[(pos - 1) / 2] > heap[pos]; pos = (pos - 1) / 2) {
					k = heap[pos];
					heap[pos] = heap[(pos - 1) / 2];
					heap[(pos - 1) / 2] = k;
				}
			}
		}
	}

	free(wall);
	free(queue);
	free(heap);
	g_raft_route.grid = raft->grid;
	g_raft_route.w = w;
	g_raft_route.h = h;
	g_raft_route.reach_c = g_raft_route.reach_r = reach;
	return 1;
}

static int32_t raft_dist(int32_t c, int32_t r)
{
	if (!g_raft_route.dist || c < 0 || r < 0 || c >= g_raft_route.w || r >= g_raft_route.h)
		return -1;

	return g_raft_route.dist[r * g_raft_route.w + c];
}

struct raft_frames {
	const void *grid;
	int32_t count, pictures;
	int32_t off_h[128], off_v[128], w[128], h[128];
};
static struct raft_frames g_raft_frames;

struct raft_state {
	int32_t ph, pv;
	int32_t heading, frame;
};

static int32_t raft_frame_for(int32_t heading)
{
	int32_t n = g_raft_frames.pictures;
	int32_t offset = (int16_t) ((90 - heading) * n / 360);

	return (int16_t) ((offset + n + n / 2) % n);
}

static void raft_rect(const struct raft_state *s, struct rect *rc)
{
	int32_t f = s->frame < g_raft_frames.count ? s->frame : 0;

	rc->left = s->ph - g_raft_frames.off_h[f];
	rc->top = s->pv - g_raft_frames.off_v[f];
	rc->right = rc->left + g_raft_frames.w[f];
	rc->bottom = rc->top + g_raft_frames.h[f];
}

static uint32_t raft_tile(struct ford_wagon *raft, int32_t col, int32_t row)
{
	if (col < 0 || row < 0 || col >= raft_grid_w(raft) || row >= raft_grid_h(raft))
		return RAFT_LAND;

	return raft_cell(raft, col, row);
}

static int32_t raft_sim_can_move(struct ford_wagon *raft, const struct raft_state *s, int32_t dh, int32_t dv)
{
	struct rect rc;
	int32_t c, r, top, left, bottom, right;

	raft_rect(s, &rc);
	top = (rc.top + dv) / 16;
	left = (rc.left + dh) / 16;
	bottom = rc.bottom + dv;
	right = rc.right + dh;
	if (rc.top + dv < 0 || rc.left + dh < 0)
		return 0;

	bottom = (bottom & 15) ? bottom / 16 + 1 : bottom / 16;
	right = (right & 15) ? right / 16 + 1 : right / 16;
	for (r = top; r < bottom; ++r)
		for (c = left; c < right; ++c) {
			uint32_t tile = raft_tile(raft, c, r);
			if (tile == RAFT_LAND || tile >= RAFT_SHORE_FIRST)
				return 0;
		}

	return 1;
}

static int32_t raft_sim_step(struct ford_wagon *raft, struct raft_state *s, double target, int32_t speed,
	double drift, int32_t progress)
{
	struct rect rc;
	double drift_h = 0, drift_v = 0, velocity = 0, a;
	int32_t phys = 0, turn = 0, diff, c, r, top, left, bottom, right, step_h, step_v, attempt, cent_h, cent_v;
	int32_t heading = s->heading;
	struct sprite *image = raft->sprite[FORD_FRAMES_HEADINGS];

	diff = (int16_t) (target - heading);
	diff = diff < 0 ? -diff : diff;
	if (diff > 180)
		turn = heading < target ? -5 : heading > target ? 5 : 0;
	else if (diff > 10)
		turn = heading < target ? 5 : heading > target ? -5 : 0;

	a = heading / 360.0 * g_pi;
	step_h = (int16_t) (cos(a) * speed);
	step_v = (int16_t) (sin(a) * speed);

	raft_rect(s, &rc);
	cent_h = rc.left + (rc.right - rc.left) / 2;
	cent_v = rc.top + (rc.bottom - rc.top) / 2;
	top = rc.top / 16;
	left = rc.left / 16;
	bottom = (rc.bottom & 15) ? rc.bottom / 16 + 1 : rc.bottom / 16;
	right = (rc.right & 15) ? rc.right / 16 + 1 : rc.right / 16;
	for (r = top; r < bottom; ++r)
		for (c = left; c < right; ++c) {
			uint32_t tile = raft_tile(raft, c, r);
			int32_t ph = c * 16 + 8, pv = r * 16 + 8, dir = 0, strength = 1;
			double ang;
			if (!SPRITE_OPS(image)->HitTest(image, (uint16_t) (ph - rc.left), (uint16_t) (pv - rc.top),
				(uint16_t) s->frame))
				continue;

			if (tile > RAFT_CURRENT_LAST && tile < RAFT_START)
				return 0;

			if (tile >= RAFT_CURRENT_FIRST && tile <= RAFT_CURRENT_LAST) {
				dir = (int32_t) (tile - 2) % 8;
				strength = (int32_t) (tile - 2) / 8 + 1;
			}

			if (progress == 2 || progress == 3) {
				int32_t against = heading - g_current_angle[dir];
				against = against < 0 ? -against : against;
				if (against > 180)
					against = 360 - against;
				if (against >= 100)
					return 0;
			}

			ang = g_current_angle[dir] / 360.0 * 6.283185307179586;
			drift_h += cos(ang) * drift * strength;
			drift_v += sin(ang) * drift * strength;
			++phys;
			if (dir)
				velocity -= strength * (ph - cent_h) + strength * (pv - cent_v);
		}

	if (phys) {
		step_h += (int16_t) (drift_h / phys * raft->drift_gain);
		step_v += -(int16_t) (drift_v / phys * raft->drift_gain);
	}

	velocity /= 20.0;
	if (velocity < 10.0 && velocity > -10.0)
		velocity = 0.0;

	heading = (int16_t) (heading + (int16_t) velocity + turn);
	heading = (int16_t) ((heading + 360) % 360);
	s->heading = heading;
	s->frame = raft_frame_for(heading);
	raft_rect(s, &rc);
	if (rc.top <= 16)
		return 2;

	if (raft_sim_can_move(raft, s, step_h, step_v)) {
		s->ph += step_h;
		s->pv += step_v;
		return 1;
	}

	for (attempt = 0; attempt < 30; ++attempt)
		if (raft_sim_can_move(raft, s, step_h - attempt, step_v)) {
			s->ph += step_h - attempt;
			s->pv += step_v;
			return 1;
		}

	for (attempt = 0; attempt < 30; ++attempt)
		if (raft_sim_can_move(raft, s, step_h + attempt, step_v)) {
			s->ph += step_h + attempt;
			s->pv += step_v;
			return 1;
		}

	for (attempt = 0; attempt < 30; ++attempt)
		if (raft_sim_can_move(raft, s, step_h - attempt, step_v + attempt)) {
			s->ph += step_h - attempt;
			s->pv += step_v + attempt;
			return 1;
		}

	for (attempt = 0; attempt < 30; ++attempt)
		if (raft_sim_can_move(raft, s, step_h + attempt, step_v + attempt)) {
			s->ph += step_h + attempt;
			s->pv += step_v + attempt;
			return 1;
		}

	return 1;
}

static int32_t raft_sim_score(const struct raft_state *s)
{
	struct rect rc;
	int32_t c, r, d, best = -1, k;

	raft_rect(s, &rc);
	c = (rc.left + rc.right) / 32;
	r = (rc.top + rc.bottom) / 32;
	for (k = 0; k < 9 && best < 0; ++k) {
		d = raft_dist(c + k % 3 - 1, r + k / 3 - 1);
		if (k == 0)
			d = raft_dist(c, r);
		if (d >= 0)
			best = d;
	}

	return best < 0 ? -100000 : -best;
}

#define RAFT_POINTER 4000.0

static double raft_target_for(double degrees)
{
	double a = degrees * 3.14159265358979 / 180.0, dh = (int32_t) (cos(a) * RAFT_POINTER),
		dv = (int32_t) (sin(a) * RAFT_POINTER), angle, target;

	if (dh == 0.0)
		dh = 9.999999999999999e-35;

	angle = atan(dv / dh);
	if (dh < 0.0)
		angle = angle + 3.14159827;
	if ((target = angle / g_pi * 360.0) < 0.0)
		target = (target + 90.0) + 270.0;

	return target;
}

static struct raft_state g_raft_predicted = {0, 0, -1, 0};
static int32_t g_raft_runs;
static const void *g_raft_seen;
static int32_t g_raft_step;

#define RAFT_HORIZON 24
#define RAFT_TURNS 36
#define RAFT_DIRECTION(t) ((t) * 360.0 / RAFT_TURNS + 2.5)

static void raft_pilot(struct ford_wagon *raft)
{
	static const int32_t SPEED[4] = {8, 6, 5, 3};
	static int32_t last_target = -1;
	struct tile_map *map = (struct tile_map *) FORD_WAGON_OPS(raft)->GetRootView(raft);
	struct raft_frame *frame = raft->parent;
	struct window *win = frame ? frame->base.owner_window : NULL;
	struct spr_obj *obj = &raft->base.base;
	int32_t progress, speed, t, k, best_t = 0, best_score = 0, best_end = 0, found = 0;
	double drift;
	struct raft_state now;

	if (!map || !win || !raft->grid || raft->anim_count != 0 || !raft->sprite[FORD_FRAMES_HEADINGS])
		return;

	progress = RAFT_FRAME_OPS(frame)->GetProgress(frame);
	speed = progress >= 0 && progress < 4 ? SPEED[progress] : 8;
	drift = RAFT_FRAME_OPS(frame)->GetDrift(frame);
	if (g_raft_route.grid != raft->grid) {
		raft_plan(raft, 0);
		last_target = -1;
	}

	if (g_raft_frames.grid != raft->grid) {
		struct rsrc *acel = raft->acel[FORD_FRAMES_HEADINGS];
		const uint8_t *anchors = acel ? RsrcData(acel) : NULL;
		struct drawable *pictures = (struct drawable *) raft->sprite[FORD_FRAMES_HEADINGS];
		int32_t n = pictures->item_count;
		g_raft_frames.pictures = n;
		if (anchors && AnimSpriteAcelCount(anchors) < n)
			n = AnimSpriteAcelCount(anchors);
		g_raft_frames.count = !anchors ? 0 : n > 128 ? 128 : n;
		for (k = 0; k < g_raft_frames.count; ++k) {
			struct point size;
			DRAWABLE_OPS(pictures)->GetSizeAt(pictures, &size, (int16_t) k);
			struct point anchor = AnimSpriteAcelAnchor(anchors, k);
			g_raft_frames.off_h[k] = anchor.h;
			g_raft_frames.off_v[k] = anchor.v;
			g_raft_frames.w[k] = size.h;
			g_raft_frames.h[k] = size.v;
		}
		g_raft_frames.grid = raft->grid;
	}

	if (g_raft_frames.count <= 0)
		return;

	if (g_raft_seen != raft) {
		g_raft_seen = raft;
		++g_raft_runs;
		g_raft_step = 0;
		ap_log("    raft: steering a raft down river map %dx%d", g_raft_route.w, g_raft_route.h);
	}

	now.ph = obj->region1.p2.h + map->scroll_pos.h;
	now.pv = obj->region1.p2.v + map->scroll_pos.v;
	now.heading = raft->heading;
	now.frame = obj->frame;

	for (t = 0; t < RAFT_TURNS; ++t) {
		struct raft_state s = now;
		int32_t target = t, end = 0, result = 1, score;
		for (k = 0; k < RAFT_HORIZON && result == 1; ++k)
			result = raft_sim_step(raft, &s, raft_target_for(RAFT_DIRECTION(target)), speed, drift, progress);

		if (result == 0)
			score = -1000000 + k * 1000;
		else if (result == 2)
			score = 1000000 - k;
		else
			score = raft_sim_score(&s);

		end = result;
		if (target == last_target)
			score += 2;

		if (!found || score > best_score) {
			best_score = score;
			best_t = target;
			best_end = end;
			found = 1;
		}
	}

	if (g_raft_predicted.heading >= 0 && (g_raft_predicted.ph != now.ph || g_raft_predicted.pv != now.pv ||
		g_raft_predicted.heading != now.heading))
		ap_log("    raft: predicted %d,%d heading %d, got %d,%d heading %d", g_raft_predicted.ph, g_raft_predicted.pv,
			g_raft_predicted.heading, now.ph, now.pv, now.heading);

	g_raft_predicted = now;
	if (raft_sim_step(raft, &g_raft_predicted, raft_target_for(RAFT_DIRECTION(best_t)), speed, drift, progress) != 1)
		g_raft_predicted.heading = -1;

	last_target = best_t;
	{
		double a = RAFT_DIRECTION(best_t) * 3.14159265358979 / 180.0;
		struct point origin = ViewGetOrigin(&frame->base);
		int32_t k2 = ScalerScale();
		POINT p;
		p.x = (obj->region1.p2.h + (int32_t) (cos(a) * RAFT_POINTER) + origin.h) * k2 + k2 / 2;
		p.y = (obj->region1.p2.v + (int32_t) (sin(a) * RAFT_POINTER) + origin.v) * k2 + k2 / 2;
		ClientToScreen((HWND) win->hwnd, &p);
		RpSetVirtualCursor(p.x, p.y, 500);
	}

	if (g_raft_step++ % 25 == 0 || best_end == 0)
		ap_log("    raft: at %d,%d heading %d, steering %d (score %d%s), damage %d", now.ph, now.pv, raft->heading,
			(int32_t) RAFT_DIRECTION(best_t), best_score, best_end == 0 ? ", rocks ahead" : "", raft->damage);

	g_raft_steered = RpClockNow();
}

static const cand *pick_hunt(const struct control_state *st)
{
	struct hunting *h = g_hunt_state;
	const cand *c, *best = NULL;
	int32_t i, best_score = -1, best_meat = 0;

	int32_t carry = alive_members() > 1 ? 160 : 80, wanted;

	if (!h)
		return NULL;

	wanted = carry - h->aim[HUNT_MEAT];
	if (wanted <= 0 || h->aim[HUNT_SHOTS_LEFT] <= 0 || h->aim[HUNT_SHOTS_FIRED] >= (food_days() < 20 ? 20 : 8) ||
		g_hunt_idle > 60) {
		g_hunt_idle = 0;
		ap_log("    hunt over: %d lb of meat, %d shots", h->aim[HUNT_MEAT], h->aim[HUNT_SHOTS_FIRED]);
		if (h->aim[HUNT_MEAT] < 50)
			g_hunt_day = g_game->days_traveled + 2;
		return find(st, "Back to the party");
	}

	if ((c = find(st, "Reload")) != NULL)
		return c;

	for (i = 0; i < st->side_option_first; ++i) {
		const char *l = st->options[i].label, *p = strstr(l, "about ");
		int32_t chance = p ? atoi(p + 6) : 0, meat = 0;
		const char *m = strstr(l, " lbs of meat");
		if (strncmp(l, "Shoot:", 6))
			continue;

		if (m) {
			while (m > l && m[-1] != '(')
				--m;
			meat = atoi(m);
		}

		if (chance >= 60 && chance * (meat + 5) > best_score) {
			best_score = chance * (meat + 5);
			best = &st->options[i];
			best_meat = meat;
		}
	}

	if (best && best_meat < 50 && best_meat < wanted && g_hunt_idle < 20)
		best = NULL;

	if (best) {
		g_hunt_idle = 0;
		return best;
	}

	if (++g_hunt_idle % 12 == 0 && (c = find(st, "Rotate")) != NULL)
		return c;

	return NULL;
}

static void item_name(struct inv_item *item, char *out, int32_t size)
{
	const char *p = item ? item_key(item) : NULL;
	int32_t i;

	for (i = 0; p && p[i] && i < size - 1; ++i)
		out[i] = p[i];
	out[i] = 0;
}

static int32_t is_food_name(const char *name)
{
	static const char *const FOOD[] = {"flour", "bacon", "salt pork", "beans", "dried", "crackers", "cornmeal",
		"biscuits", "rice", "meat", "jerky", "ham", "potatoes", "cheese", "sugar", NULL};
	int32_t i;

	for (i = 0; FOOD[i]; ++i)
		if (has(name, FOOD[i]))
			return 1;

	return 0;
}

static int32_t needed(const char *name)
{
	int32_t rifle_or_pistol = have("rifle") || have("pistol"), gun = rifle_or_pistol || have("shotgun");

	if (has(name, "rope") || key_is(name, "chain") || has(name, " chain"))
		return g_need_rope && !have("rope") && !have("chain");
	if (has(name, "shotgun"))
		return !gun;
	if (has(name, "sheath"))
		return 0;
	if (has(name, "rifle") || has(name, "pistol"))
		return !gun;
	if (has(name, "gunpowder"))
		return gun && !have("gunpowder");
	if (has(name, "bullets"))
		return rifle_or_pistol && have("bullets") < 2;
	if (has(name, "shot"))
		return have("shotgun") && !rifle_or_pistol && have("shot") < 2;
	if (has(name, "ox yoke"))
		return !have("spare ox yoke");
	if (has(name, "wagon wheel") || has(name, "wagon axle") || has(name, "wagon tongue"))
		return !owned(&RESTOCK_ITEMS[has(name, "wheel") ? 0 : has(name, "axle") ? 1 : 2]);
	if (has(name, "oxen") || key_is(name, "ox") || has(name, "mule"))
		return need_oxen();
	if (is_food_name(name))
		return food_days() < 30;

	return 0;
}

static int32_t critical(struct inv_item *item, int32_t qty)
{
	char name[96];
	int32_t left, rifle_or_pistol = have("rifle") || have("pistol");

	if (!item)
		return 1;

	item_name(item, name, sizeof(name));
	left = InvItemGetCount(item, false) - qty;
	if (key_is(name, "cash"))
		return party()->cash - qty < 30.0;
	if (key_is(name, "rifle") || key_is(name, "pistol") || key_is(name, "shotgun"))
		return left < 1 && (rifle_or_pistol + (have("shotgun") != 0)) < 2;
	if (key_is(name, "gunpowder") || key_is(name, "bullets") || key_is(name, "shot"))
		return left < 1;
	if (has(name, "wagon wheel") || has(name, "wagon axle") || has(name, "wagon tongue") || has(name, "yoke"))
		return left < 1;
	if (key_is(name, "ox") || has(name, "mule") || has(name, "horse"))
		return count_all(&party()->animals) + count_all(&party()->spare_animals) - qty < 6;
	if (is_food_name(name))
		return PartyGetFoodPounds(party()) - qty * ((unit_weight_x16(item) + 15) / 16) <= 21 * alive_members();

	{
		static int32_t clothing = -2, medicine = -2;
		if (clothing == -2) {
			clothing = category("clothing");
			medicine = category("medicines");
		}
		if (clothing >= 0 && InvItemInCategory(item, (int16_t) clothing))
			return left < alive_members();
		if (medicine >= 0 && InvItemInCategory(item, (int16_t) medicine))
			return left < 1;
	}

	return 0;
}

static int32_t stuck_gun_ok(int32_t food_days_n, int32_t days_left, int32_t refusals, int32_t days_to_gun_town,
	int32_t can_afford)
{
	if (days_to_gun_town >= 0 && can_afford && food_days_n >= days_to_gun_town + 5)
		return 1;

	if (refusals >= 20)
		return food_days_n > 0;

	return food_days_n >= (days_left < 0 ? 30 : days_left) + 10;
}

static int32_t days_to_gun_town(void)
{
	int32_t miles = miles_from_here(town_sells_gun), pace = pace_tenths();

	if (pace < 120)
		pace = 120;

	return miles < 0 ? -1 : (miles * 10 + pace - 1) / pace;
}

static int32_t stuck_trade_ok(struct inv_item *item, int32_t qty)
{
	char name[96];
	int32_t left = days_to_go();

	item_name(item, name, sizeof(name));
	if (key_is(name, "ox") || has(name, "mule") || has(name, "horse"))
		return count_all(&party()->animals) + count_all(&party()->spare_animals) - qty >=
			(g_breakdown_refusals >= 3 ? 2 : 4) &&
			(g_breakdown_refusals >= 6 || easy_cap(name, qty) >= load() - plan_dump(load(), 45, DUMP_FOOD_15));

	if (key_is(name, "rifle") || key_is(name, "pistol") || key_is(name, "shotgun"))
		return stuck_gun_ok(food_days(), left, g_breakdown_refusals, days_to_gun_town(),
			party()->cash * 100.0 >= 2.0 * InvItemGetUnitPrice(item));

	return !critical(item, qty);
}

static int32_t cheap_and_plenty(struct inv_item *item, int32_t qty)
{
	return item && !critical(item, qty) && InvItemGetCount(item, false) >= 3 * qty &&
		qty * InvItemGetUnitPrice(item) <= 500;
}

static const cand *pick_trade(struct control_state *st)
{
	struct modal_dialog *running = g_running_dialog;
	struct trade_dlg *dlg = running && ((struct object *) running)->ops == (const void *) &TRADE_DLG_OPS_TABLE ?
		(struct trade_dlg *) running : NULL;
	char give[96], want[96];
	const cand *c;
	int32_t i;

	if (!dlg) {
		ap_log("    trade: the offer cannot be read -- no");
		return find_prefix(st, "No");
	}

	ap_log("    trade: dialog %p, give %p x%d, wanted %p x%d, pick %d, offer line %d: '%.80s'", (void *) dlg,
		(void *) dlg->give_item, dlg->give_qty, (void *) dlg->matched, dlg->offered_qty, g_trade_picks,
		(int32_t) dlg->forced_zero, st->side_text_first ? st->text[0] : "");
	if (st->hwnd != g_trade_dlg) {
		g_trade_dlg = st->hwnd;
		g_trade_picks = 0;
		g_trade_haggles = 0;
	}

	give[0] = 0;
	if (!dlg->forced_zero)
		item_name(dlg->give_item, give, sizeof(give));
	item_name(dlg->matched, want, sizeof(want));

	if (dlg->forced_zero && (g_guide_asked_node == g_game->cur_node_id || text_has(st, "guide"))) {
		if (!critical(dlg->matched, dlg->offered_qty) && (c = find_prefix(st, "Yes")) != NULL) {
			ap_log("    trade: the guide wants %d %s -- yes", dlg->offered_qty, want);
			return c;
		}
		if (g_trade_picks++ < 2 && (c = find_prefix(st, "Haggle")) != NULL)
			return c;

		g_guide_refused_node = g_game->cur_node_id;
		ap_log("    trade: the guide wants %d %s, which the party needs -- no", dlg->offered_qty, want);
		return find_prefix(st, "No");
	}

	if (dlg->forced_zero || !dlg->give_item) {
		int32_t yes = morale() < 60 && cheap_and_plenty(dlg->matched, dlg->offered_qty);
		ap_log("    trade: asked for %d %s, morale %d -- %s", dlg->offered_qty, want, morale(), yes ? "yes" : "no");
		return find_prefix(st, yes ? "Yes" : "No");
	}

	if (!needed(give) && g_trade_picks < 3) {
		for (i = 0; i < st->field_count; ++i)
			if (st->fields[i].is_choice && st->fields[i].id == 9002) {
				static char names[64][64];
				int32_t n = ControlFieldChoices(st, i, names, 64), k;
				for (k = 0; k < n; ++k)
					if (needed(names[k]) && strcmp(names[k], st->fields[i].text)) {
						char pick[8];
						++g_trade_picks;
						StrPrintf(pick, "#%d", k);
						ap_log("    trade: asking for %s instead of %s", names[k], give);
						ControlSetField(st, i, pick);
						return NULL;
					}
			}

		g_trade_picks = 3;
	}

	if (needed(give) && dlg->matched && !critical(dlg->matched, dlg->offered_qty)) {
		ap_log("    trade: %d %s for %d %s -- yes", dlg->give_qty, give, dlg->offered_qty, want);
		g_trade_stuck = 0;
		return find_prefix(st, "Yes");
	}

	if (needed(give) && g_trade_stuck && dlg->matched && stuck_trade_ok(dlg->matched, dlg->offered_qty)) {
		ap_log("    trade: %d %s for %d %s -- yes, the wagon cannot move otherwise", dlg->give_qty, give,
			dlg->offered_qty, want);
		g_trade_stuck = 0;
		g_breakdown_refusals = 0;
		return find_prefix(st, "Yes");
	}

	if (needed(give) && g_trade_haggles < 5 && (c = find_prefix(st, "Haggle")) != NULL) {
		++g_trade_haggles;
		ap_log("    trade: %d %s for %d %s, which the party needs -- haggle (%d)", dlg->give_qty, give,
			dlg->offered_qty, want[0] ? want : "nothing it has", g_trade_haggles);
		return c;
	}

	ap_log("    trade: %d %s for %d %s -- no", dlg->give_qty, give, dlg->offered_qty, want);
	g_trade_refused_day = g_game->days_traveled;
	if (g_trade_stuck)
		++g_breakdown_refusals;
	g_trade_stuck = 0;
	return find_prefix(st, "No");
}

static int32_t pick_choice(struct control_state *st, int32_t id, const char *word)
{
	static char names[64][64];
	int32_t i, k, n;

	for (i = 0; i < st->field_count; ++i)
		if (st->fields[i].is_choice && st->fields[i].id == id) {
			n = ControlFieldChoices(st, i, names, 64);
			for (k = 0; k < n; ++k)
				if (has(names[k], word)) {
					char pick[8];
					StrPrintf(pick, "#%d", k);
					ControlSetField(st, i, pick);
					return 1;
				}
		}

	return 0;
}

static const cand *pick_dialog(struct control_state *st)
{
	static const char *const DIALOG[] = {"OK", "Continue", "Done", "Ready", "Yes", "Close", "Leave", "Travel",
		"So long", "No", NULL};
	static void *rest_dlg;
	const cand *c;
	int32_t i, months, best = 0;

	for (i = 0, c = NULL; i < st->side_option_first; ++i)
		if (sscanf(st->options[i].label, "For %d month", &months) == 1 && months > best) {
			best = months;
			c = &st->options[i];
		}

	if (c && (c = find_prefix(st, "No")) != NULL) {
		g_package_bought = 1;
		ap_log("    package: declined, buying our own supplies");
		return c;
	}

	if (!strcmp(st->title, "Purchase Inventory")) {
		shop_step(st);
		return NULL;
	}

	if (!strcmp(st->title, "Dump Inventory")) {
		if (!dump_step(st) && (c = find_prefix(st, "Cancel")) != NULL)
			return c;
		return NULL;
	}

	if (!strcmp(st->title, "Trading"))
		return pick_trade(st);

	if (text_has(st, "want to quit") && (c = find_prefix(st, "Cancel")) != NULL)
		return c;

	if (text_has(st, "do not have a firearm") && g_game && g_game->party) {
		g_no_hunt_items = count_all(&party()->inventory);
		ap_log("    cannot hunt: %s missing", hunting_lacks() ? hunting_lacks() : "something");
	}

	if (!strcmp(st->title, "Wagon Train")) {
		struct modal_dialog *running = g_running_dialog;
		struct picker_dlg *dlg = running && ((struct object *) running)->ops == (const void *) &PICKER_DLG_OPS_TABLE ?
			(struct picker_dlg *) running : NULL;
		int32_t pick = 0, k;
		if (dlg)
			for (k = 1; k < 3; ++k) {
				int32_t dk = dlg->people[k] < 50 ? 50 - dlg->people[k] : dlg->people[k] > 100 ? dlg->people[k] - 100 : 0;
				int32_t dp = dlg->people[pick] < 50 ? 50 - dlg->people[pick] :
					dlg->people[pick] > 100 ? dlg->people[pick] - 100 : 0;
				if (dk < dp)
					pick = k;
			}

		{
			char label[16];
			StrPrintf(label, "Train %d", pick + 1);
			if (dlg)
				ap_log("    wagon train %d: %d wagons, %d people", pick + 1, dlg->wagons[pick], dlg->people[pick]);
			if ((c = find_prefix(st, label)) != NULL)
				return c;
		}
	}

	if (!strcmp(st->title, "Resting") && st->hwnd != rest_dlg) {
		char pick[8];
		rest_dlg = st->hwnd;
		for (i = 0; i < st->field_count; ++i)
			if (st->fields[i].is_choice && st->fields[i].id == 9001) {
				int32_t days = g_rest_days < 1 ? 1 : g_rest_days > 14 ? 14 : g_rest_days;
				StrPrintf(pick, "#%d", days - 1);
				ap_log("    resting %d day%s", days, days == 1 ? "" : "s");
				ControlSetField(st, i, pick);
				g_rest_days = 1;
				return NULL;
			}
	}

	if (!strcmp(st->title, "Firearm") && st->hwnd != g_firearm_dlg) {
		g_firearm_dlg = st->hwnd;
		if ((have("rifle") && have("bullets") && pick_choice(st, 9001, "rifle")) ||
			(have("pistol") && have("bullets") && pick_choice(st, 9001, "pistol")) ||
			(have("shotgun") && have("shot") && pick_choice(st, 9001, "shotgun")))
			return NULL;
	}

	if (!strcmp(st->title, "Character Creation")) {
		if (AutoBatchNewGame(st)) {
			g_rolled_dlg = st->hwnd;
			reset_game_state();
			return NULL;
		}

		if (st->hwnd != g_rolled_dlg) {
			g_rolled_dlg = st->hwnd;
			reset_game_state();
			for (i = 0; i < st->field_count; ++i)
				if (st->fields[i].is_choice && st->fields[i].id == 9020) {
					char pick[8];
					int32_t level = (int32_t) (auto_rand() % 10);
					StrPrintf(pick, "#%d", level);
					ap_log("---- new game: difficulty %d ----", level + 1);
					ControlSetField(st, i, pick);
					return NULL;
				}
		}

		for (i = 0; i < st->field_count; ++i)
			if (!strcmp(st->fields[i].label, "Leader's name") && !st->fields[i].text[0]) {
				ControlSetField(st, i, "Auto");
				return NULL;
			}

		if ((c = find_prefix(st, "Travel")) != NULL)
			return c;
	}

	for (i = 0; DIALOG[i]; ++i)
		if ((c = find_prefix(st, DIALOG[i])) != NULL)
			return c;

	return st->side_option_first ? &st->options[0] : NULL;
}

static void reset_game_state(void)
{
	g_seen_game = NULL;
	g_wait_node = g_rest_node = g_town_node = g_shop_node = -1;
	g_waits = g_rests = g_town_steps = 0;
	g_package_bought = g_oxen_tries = 0;
	g_hunt_day = -1;
	g_hunt_pending = g_hunt_idle = 0;
	g_town_seen_n = 0;
	g_scene_turns = 0;
	g_dump_rounds = g_dump_stuck = 0;
	g_game_over = g_score_seen = 0;
	g_no_hunt_items = -1;
	g_talk_node = g_hay_node = g_ruins_node = g_morale_node = g_mud_node = g_guide_refused_node = -1;
	g_trade_refused_day = -10;
	g_hill_node = -1;
	g_guide_asked_node = -1;
	g_talks = g_morale_rests = g_mud_waits = 0;
	g_last_talk_code = -1;
	g_settings_checked = 0;
	g_trail_settings_day = -1;
	g_need_rope = g_rope_trade = 0;
	g_rest_days = 1;
	g_plan_made = 0;
	g_visit_dlg = NULL;
	g_same_picks = 0;
}

static int32_t fit_for_strain(void)
{
	struct weather *w = g_game->weather;
	int32_t i;

	for (i = 0; i < party()->member_count && i < 6; ++i) {
		struct member *m = party()->members[i];
		if (m && m->state == MEMBER_ALIVE && m->max_health > 0 && m->health * 5 < m->max_health * 4)
			return 0;
	}

	return !MainUiGetCatFlag(CAT_SOMEONE_SICK) && !MainUiGetCatFlag(CAT_POOR_HEALTH) &&
		!strained() && WeatherGetLowTemp(w, 0) >= 10 && WeatherGetHighTemp(w, 0) <= 95;
}

static int32_t arriving_late(int32_t left)
{
	struct date *d = g_game->date;

	return left >= 0 && d->month * 31 + d->day + left > MONTH_OCTOBER * 31 + 15;
}

static int32_t food_short_endgame(int32_t left)
{
	int32_t food = food_days();

	return food < 20 || (left >= 0 && left <= 60 && food < left + 5);
}

static int32_t wanted_pace(void)
{
	int32_t left = days_to_go();

	if (left < 8 || !fit_for_strain() || party()->ration_level != RATIONS_FILLING)
		return PACE_8_HOURS;

	return arriving_late(left) || (food_short_endgame(left)) ? PACE_10_HOURS : PACE_8_HOURS;
}

static int32_t wanted_rations(void)
{
	int32_t left = days_to_go(), need, cur = party()->ration_level;

	if (left < 0 || !food_short_endgame(left))
		return RATIONS_FILLING;

	need = wanted_pace() == PACE_10_HOURS ? left * 14 / 19 : left;
	if (food_days_at(RATIONS_FILLING) >= need + (cur == RATIONS_FILLING ? 5 : 8))
		return RATIONS_FILLING;

	if (food_days_at(RATIONS_MEAGER) >= need + 3 || needs_rest())
		return RATIONS_MEAGER;

	return RATIONS_BARE_BONES;
}

static int32_t settings_step(struct control_state *st)
{
	static const char *const PACES[] = {"8 hours", "10 hours", "until exhausted"};
	static const char *const RATIONS[] = {"bare bones", "meager", "filling"};
	int32_t i, pace = wanted_pace(), rations = wanted_rations();
	char pick[8];

	for (i = 0; i < st->field_count; ++i) {
		const struct control_field *f = &st->fields[i];
		if (!f->is_choice)
			continue;

		if (!strcmp(f->label, "Travel pace") && g_game->travel_pace != pace) {
			ap_log("    travel pace: %s a day (%d days to go, food for %d)", PACES[pace], days_to_go(), food_days());
			StrPrintf(pick, "#%d", pace);
			ControlSetField(st, i, pick);
			return 1;
		}

		if (!strcmp(f->label, "Rations") && party()->ration_level != rations) {
			ap_log("    rations: %s (%d days to go, food for %d filling, %d meager)", RATIONS[rations], days_to_go(),
				food_days_at(RATIONS_FILLING), food_days_at(RATIONS_MEAGER));
			StrPrintf(pick, "#%d", rations);
			ControlSetField(st, i, pick);
			return 1;
		}
	}

	return 0;
}

static int32_t event_rest_days(const struct control_state *st)
{
	int32_t days = 3, most = food_days() / 2;

	if (text_has(st, "morale")) {
		days = (50 - morale()) / 2;
		days = days < 2 ? 2 : days > 10 ? 10 : days;

	} else if (text_has(st, "Exhausted")) {
		days = 2;
	}

	return days < most ? days : most > 1 ? most : 1;
}

static int32_t food_for_morale_rest(int32_t rest_days)
{
	int32_t left = days_to_go();

	return plenty_of_food() && food_days() >= (left < 0 ? 30 : left) + rest_days + 5;
}

static const cand *watchdog(struct control_state *st, const cand *pick)
{
	char sig[2200];
	int32_t i, len, n;

	len = StrPrintfN(sig, sizeof(sig), "%d %s:", st->screen, st->title);
	for (i = 0; i < st->option_count && len >= 0 && len < 2000; ++i)
		len += StrPrintfN(sig + len, sizeof(sig) - (size_t) len, "|%s", st->options[i].label);
	if (len >= 0 && len < 2100)
		StrPrintfN(sig + len, sizeof(sig) - (size_t) len, " -> %s", pick->label);

	if (strcmp(sig, g_last_pick)) {
		StrCopy(g_last_pick, sig);
		g_same_picks = 0;
		return pick;
	}

	if (++g_same_picks < 20)
		return pick;

	if (g_same_picks >= 60 && st->screen == CONTROL_DIALOG) {
		ap_log("    watchdog: stuck on '%s' -- closing the window", st->title);
		g_same_picks = 0;
		ControlClose(st);
		return NULL;
	}

	n = st->side_option_first;
	for (i = 0; i < n; ++i) {
		const cand *o = &st->options[(g_same_picks + i) % n];
		if (o != pick && o->code != NODE_CMD_SAVE_DIARY && strncmp(o->label, "Get advice", 10) &&
			strcmp(o->label, "Map") && strcmp(o->label, "Journal") && strcmp(o->label, "Party health") &&
			strcmp(o->label, "Turn")) {
			ap_log("    watchdog: '%s' changed nothing %d times -- trying '%s'", pick->label, g_same_picks, o->label);
			return o;
		}
	}

	return pick;
}

static int32_t travel_key(void)
{
	uint32_t key = ((uint32_t) g_game->date->year * 400u + (uint32_t) g_game->date->month * 32u +
		(uint32_t) g_game->date->day) * 4096u + (uint32_t) g_game->total_miles * 16u + (uint32_t) g_game->miles_to_go;

	return (int32_t) key;
}

static void travel_stall(struct control_state *st)
{
	static int32_t last_key = -1;
	static uint32_t since;
	int32_t key;

	if (!g_game || !g_game->date || RpClockNow() - g_raft_steered < 3000) {
		last_key = -1;
		return;
	}

	key = travel_key();
	if (key != last_key) {
		last_key = key;
		since = RpClockNow();
		return;
	}

	if (RpClockNow() - since < 20000)
		return;

	since = RpClockNow();
	ap_log("    travel stalled (%d/%d/%d, %d miles to go) -- stopping the wagon to start it again",
		g_game->date->year, g_game->date->month, g_game->date->day, g_game->miles_to_go);
	if (g_game->cur_node && NodeBaseGetId(g_game->cur_node) == NODE_KIND_TRAIL) {
		EvtHandlerRemoveIdleHandler(&g_game->base, false);
		GameSetTravelState(g_game, 3);
		DispatchGlobalEvent(CmdEventCreate(GCMD_NODE_CONTINUE));

	} else {
		ControlClickAt(st, 300, 200);
	}
}

static const cand *stalled(struct control_state *st)
{
	static char last[2200];
	static int32_t ticks;
	char sig[2200];
	const cand *c;
	int32_t i, len, other = 0;

	for (i = 0; i < st->side_option_first; ++i)
		if (strcmp(st->options[i].label, "Map") && strcmp(st->options[i].label, "Journal") &&
			strcmp(st->options[i].label, "Party health") && strcmp(st->options[i].label, "Supplies"))
			other = 1;

	len = StrPrintfN(sig, sizeof(sig), "%d %s:", st->screen, st->title);
	for (i = 0; i < st->option_count && len >= 0 && len < 2000; ++i)
		len += StrPrintfN(sig + len, sizeof(sig) - (size_t) len, "|%s", st->options[i].label);

	if (!other) {
		travel_stall(st);
		ticks = 0;
		return NULL;
	}

	if (strcmp(sig, last)) {
		StrCopy(last, sig);
		ticks = 0;
		return NULL;
	}

	if (++ticks < 40)
		return NULL;

	ticks = 0;
	ap_log("    stalled on '%s' with no rule -- leaving it", st->title);
	if ((c = find_prefix(st, "Back to travel")) != NULL || (c = find_prefix(st, "OK")) != NULL ||
		(c = find_prefix(st, "Continue")) != NULL || (c = find_prefix(st, "Done")) != NULL ||
		(c = find_prefix(st, "Supplies")) != NULL)
		return c;

	if (st->screen == CONTROL_DIALOG)
		ControlClose(st);

	return NULL;
}

static char g_click_key[2200];
static uint32_t g_click_time;

static void screen_key(const struct control_state *st, char *out, int32_t size)
{
	int32_t i, len = StrPrintfN(out, (size_t) size, "%d %s:", st->screen, st->title);

	for (i = 0; i < st->option_count && len >= 0 && len < size - 200; ++i)
		len += StrPrintfN(out + len, (size_t) (size - len), "|%s", st->options[i].label);
}

static void CALLBACK tick(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
	(void) hwnd;
	(void) msg;
	(void) id;
	(void) time;

	static struct control_state st;
	static char key[2200];
	const cand *pick = NULL, *hunt;
	struct otii_app *app = (struct otii_app *) g_app;

	static uint32_t busy_since;
	static int32_t busy_logged;

	if (g_busy) {
		if (!busy_logged && RpClockNow() - busy_since > 5000) {
			busy_logged = 1;
			ap_log("    (a decision has been running for %lu s -- a window opened inside it?)",
				(unsigned long) ((RpClockNow() - busy_since) / 1000));
		}
		if (RpClockNow() - busy_since > 30000) {
			ap_log("    (the decision never returned -- it failed; carrying on)");
			g_busy = 0;
		}
		return;
	}

	g_busy = 1;
	busy_since = RpClockNow();
	busy_logged = 0;
	if (!ControlGetState(&st)) {
		g_busy = 0;
		return;
	}

	describe_screen(&st);
	screen_key(&st, key, sizeof(key));
	if (!strcmp(key, g_click_key) && RpClockNow() - g_click_time < 1500) {
		g_busy = 0;
		return;
	}

	if (g_game && g_game->party && g_game != g_seen_game) {
		reset_game_state();
		g_seen_game = g_game;
	}

	if (!g_game || !g_game->party || (st.screen != CONTROL_DIALOG && find_prefix(&st, "New Game"))) {
		if (st.screen != CONTROL_DIALOG && (pick = find_prefix(&st, "New Game")) != NULL && g_game_over) {
			if (option_next_game()) {
				ap_log("---- the game is over: starting the next one ----");
				g_game_over = 0;

			} else {
				ap_log("---- the game is over: autopilot stops ----");
				g_game_over = 0;
				g_busy = 0;
				AutoPilotStop();
				return;
			}

		} else if (st.screen == CONTROL_DIALOG) {
			pick = pick_dialog(&st);
		}

	} else if (app && app->app_state == APP_STATE_HUNTING && st.screen != CONTROL_DIALOG) {
		pick = pick_hunt(&st);

	} else if (st.screen != CONTROL_DIALOG && (hunt = find_prefix(&st, "Hunt") ? find_prefix(&st, "Hunt") :
		find(&st, "Go hunting")) != NULL && find_prefix(&st, "Turn")) {
		if (!settings_step(&st)) {
			if (g_rope_trade && (pick = find_prefix(&st, "Trade")) != NULL)
				;
			else if (g_hunt_pending && can_hunt())
				pick = hunt;
			else if ((pick = find_prefix(&st, "Back to travel")) == NULL)
				pick = find_prefix(&st, "Supplies");
			g_hunt_pending = 0;
			g_rope_trade = 0;
		}

	} else {
		switch (st.screen) {
		case CONTROL_EVENT:
			pick = pick_event(&st);
			if (pick && has(pick->label, "rest"))
				g_rest_days = event_rest_days(&st);
			break;
		case CONTROL_LANDMARK:
			pick = pick_landmark(&st);
			break;
		case CONTROL_TOWN:
			pick = pick_town(&st);
			break;
		case CONTROL_DIALOG:
			pick = pick_dialog(&st);
			break;
		}
	}

	if (!pick && st.screen != CONTROL_DIALOG && (!app || app->app_state != APP_STATE_HUNTING) && g_game &&
		g_game->party && g_trail_settings_day != g_game->days_traveled &&
		(g_game->travel_pace != wanted_pace() || party()->ration_level != wanted_rations()) &&
		(pick = find_prefix(&st, "Supplies")) != NULL) {
		g_trail_settings_day = g_game->days_traveled;
		ap_log("    on the trail: travel pace %d, rations %d -- changing them", g_game->travel_pace,
			party()->ration_level);
	}

	if (pick)
		pick = watchdog(&st, pick);
	else if (!app || app->app_state != APP_STATE_HUNTING)
		pick = stalled(&st);

	if (pick) {
		const char *about = st.title;
		if (st.screen == CONTROL_EVENT) {
			int32_t k;
			for (k = st.side_text_first - 1; k >= 0; --k)
				if (st.text[k][0] && !strchr(st.text[k], ',') && strlen(st.text[k]) < 60) {
					about = st.text[k];
					break;
				}
		}

		ap_log("%d/%02d/%02d  %s  -> '%s' (cmd %d, code %d)", st.year, st.month, st.day, about, pick->label,
			pick->command, pick->code);
		if (st.screen == CONTROL_DIALOG && st.side_text_first && strcmp(st.title, "Resting"))
			ap_log("    (it said: %.200s)", st.text[0]);
		else if (st.screen == CONTROL_EVENT && st.side_text_first)
			ap_log("    (it said: %.200s)", st.text[0]);

		ControlClick(&st, pick->index);
		StrCopy(g_click_key, key);
		g_click_time = RpClockNow();
	}

	g_busy = 0;
}

#ifdef OTII_TEST
int32_t AutoPilotTestMealHelping(int32_t level)
{
	return meal_helping(level);
}

int32_t AutoPilotTestFoodDaysAt(int32_t level)
{
	return food_days_at(level);
}

int32_t AutoPilotTestFoodShortEndgame(int32_t left)
{
	return food_short_endgame(left);
}

int32_t AutoPilotTestArrivingLate(int32_t left)
{
	return arriving_late(left);
}

int32_t AutoPilotTestWantedPace(void)
{
	return wanted_pace();
}

int32_t AutoPilotTestWantedRations(void)
{
	return wanted_rations();
}

int32_t AutoPilotTestDaysToGo(void)
{
	return days_to_go();
}

int32_t AutoPilotTestStuckGunOk(int32_t food_days_n, int32_t days_left, int32_t refusals, int32_t days_to_gun_town_n,
	int32_t can_afford)
{
	return stuck_gun_ok(food_days_n, days_left, refusals, days_to_gun_town_n, can_afford);
}

static int32_t g_test_gun_towns, g_test_landmarks;

static int32_t count_gun_town(int32_t id)
{
	++g_test_landmarks;
	if (town_sells_gun(id))
		++g_test_gun_towns;
	return 0;
}

int32_t AutoPilotTestGunTowns(int32_t *landmarks, int32_t *days)
{
	g_test_gun_towns = g_test_landmarks = 0;
	miles_from_here(count_gun_town);
	*landmarks = g_test_landmarks;
	*days = days_to_gun_town();
	return g_test_gun_towns;
}

int32_t AutoPilotTestStuckTradeOk(struct inv_item *item, int32_t qty, int32_t refusals)
{
	g_breakdown_refusals = refusals;
	return stuck_trade_ok(item, qty);
}

int32_t AutoPilotTestCanPullWithout(struct inv_item *item, int32_t qty)
{
	char name[96];

	item_name(item, name, sizeof(name));
	return easy_cap(name, qty) >= load() - plan_dump(load(), 45, DUMP_FOOD_15);
}

void AutoPilotTestSteerRafts(bool on)
{
	g_raft_pilot = on ? raft_pilot : NULL;
}

int32_t AutoPilotTestRaftRuns(void)
{
	return g_raft_runs;
}

int32_t AutoPilotTestRaftSteering(void)
{
	return g_raft_steered && RpClockNow() - g_raft_steered < 2000;
}

int32_t AutoPilotTestKeepWaiting(int32_t waits)
{
	return keep_waiting(waits);
}

int32_t AutoPilotTestStrained(void)
{
	return strained();
}

int32_t AutoPilotTestLoadExcess(void)
{
	return -spare_easy_load();
}

int32_t AutoPilotTestDump(int32_t lb)
{
	struct inv_list *inv = &g_game->party->inventory;
	int32_t planned;

	Sleep(210);
	RpGetTickCount();
	planned = plan_dump(lb, 45, DUMP_ALL);
	for (int32_t i = 0; i < g_dump_plan_n; ++i) {
		struct cstr *name = CStrCreateFrom(g_dump_plan[i].name);
		struct inv_item *item = InvListFindByName(inv, name);
		CStrDestroy(&name);
		if (item && !InvItemRemoveUnits(item, g_dump_plan[i].count)) {
			ListRemove(&inv->base, &item->node);
			ObjectDelete(item);
		}
	}
	g_dump_plan_n = 0;
	PartyCalcPullingMode(g_game->party);
	return planned;
}
#endif

void AutoPilotAppendMenu(void *popup)
{
	AppendMenuA((HMENU) popup, MF_STRING | (option_next_game() ? MF_CHECKED : 0), AUTOPILOT_NEXT_GAME_MENU_ID,
		"Autopilot: New Game When One Ends");
	AppendMenuA((HMENU) popup, MF_STRING | (option_score_goods() ? MF_CHECKED : 0), AUTOPILOT_SCORE_GOODS_MENU_ID,
		"Autopilot: Buy Goods for Score");
}

void AutoPilotMenuUpdate(void *popup)
{
	HMENU menu = (HMENU) popup;

	if (!menu || GetMenuState(menu, AUTOPILOT_MENU_ID, MF_BYCOMMAND) == (UINT) -1)
		return;

	CheckMenuItem(menu, AUTOPILOT_MENU_ID, MF_BYCOMMAND | (AutoPilotRunning() ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(menu, AUTOPILOT_NEXT_GAME_MENU_ID, MF_BYCOMMAND | (option_next_game() ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(menu, AUTOPILOT_SCORE_GOODS_MENU_ID,
		MF_BYCOMMAND | (option_score_goods() ? MF_CHECKED : MF_UNCHECKED));
}

int32_t AutoPilotMenuCommand(void *hwnd, int32_t id)
{
	HMENU menu = GetMenu((HWND) hwnd);

	if (id == AUTOPILOT_MENU_ID) {
		if (AutoPilotRunning())
			AutoPilotStop();
		else
			AutoPilotStart();
		CheckMenuItem(menu, AUTOPILOT_MENU_ID, MF_BYCOMMAND | (AutoPilotRunning() ? MF_CHECKED : MF_UNCHECKED));
		return 1;
	}

	if (id == AUTOPILOT_NEXT_GAME_MENU_ID) {
		g_opt_next_game = !option_next_game();
		IniSetString("autopilot", "new_game_after_end", g_opt_next_game ? "1" : "0");
		CheckMenuItem(menu, (UINT) id, MF_BYCOMMAND | (g_opt_next_game ? MF_CHECKED : MF_UNCHECKED));
		return 1;
	}

	if (id == AUTOPILOT_SCORE_GOODS_MENU_ID) {
		g_opt_score_goods = !option_score_goods();
		IniSetString("autopilot", "buy_goods_for_score", g_opt_score_goods ? "1" : "0");
		CheckMenuItem(menu, (UINT) id, MF_BYCOMMAND | (g_opt_score_goods ? MF_CHECKED : MF_UNCHECKED));
		return 1;
	}

	return 0;
}

static LONG CALLBACK fault_logger(EXCEPTION_POINTERS *info)
{
	static DWORD_PTR seen[32];
	static int32_t seen_n;
	DWORD code = info->ExceptionRecord->ExceptionCode;
	DWORD_PTR at = (DWORD_PTR) info->ExceptionRecord->ExceptionAddress, base = (DWORD_PTR) GetModuleHandleA(NULL);
	int32_t i;

	if (!g_timer || (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_INT_DIVIDE_BY_ZERO &&
		code != EXCEPTION_ARRAY_BOUNDS_EXCEEDED && code != EXCEPTION_STACK_OVERFLOW && code != 0xE06D7363))
		return EXCEPTION_CONTINUE_SEARCH;

	for (i = 0; i < seen_n && seen[i] != at; ++i)
		;
	if (i < seen_n || seen_n >= 32)
		return EXCEPTION_CONTINUE_SEARCH;

	seen[seen_n++] = at;
	ap_log("    FAULT %08lX at exe+%lX (%p)%s", (unsigned long) code, (unsigned long) (at - base), (void *) at,
		code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2 ? " -- memory access" : "");
	return EXCEPTION_CONTINUE_SEARCH;
}

void AutoPilotStart(void)
{
	static int32_t fault_logger_added;

	if (!fault_logger_added) {
		AddVectoredExceptionHandler(0, fault_logger);
		fault_logger_added = 1;
	}

	if (!g_timer) {
		g_timer = RpSetTimer(NULL, 0, AutoBatchTickMs(AUTO_TICK_MS), (void *) tick);
		ap_log("---- auto mode on ----");
	}

	g_raft_pilot = raft_pilot;
}

void AutoPilotStop(void)
{
	if (g_timer) {
		KillTimer(NULL, g_timer);
		g_timer = 0;
		ap_log("---- auto mode off ----");
	}

	g_raft_pilot = NULL;
}

int32_t AutoPilotRunning(void)
{
	return g_timer != 0;
}

#endif
