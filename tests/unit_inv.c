#include "testframework.h"

#ifdef OTII_TEST

#include <string.h>

#include "../src/base/cstr.h"
#include "../src/game/invitem.h"
#include "../src/game/invlist.h"
#include "../src/base/list.h"
#include "../src/base/str.h"
#include "../src/data/strrsrc.h"

extern int32_t g_item_anvil;
extern int32_t g_item_ax;
extern int32_t g_item_blanket;
extern int32_t g_item_boots;
extern int32_t g_item_bridle;
extern int32_t g_item_butter_churn;
extern int32_t g_item_canteen;
extern int32_t g_item_chain;
extern int32_t g_item_chicken;
extern int32_t g_item_conestoga;
extern int32_t g_item_egg;
extern int32_t g_item_grease;
extern int32_t g_item_hammer;
extern int32_t g_item_hatchet;
extern int32_t g_item_long_underwear;
extern int32_t g_item_matches;
extern int32_t g_item_milk;
extern int32_t g_item_milk_cow;
extern int32_t g_item_mittens;
extern int32_t g_item_nails;
extern int32_t g_item_pick_ax;
extern int32_t g_item_pistol;
extern int32_t g_item_rifle;
extern int32_t g_item_rifle_or_shotgun_sheath;
extern int32_t g_item_rope;
extern int32_t g_item_saw;
extern int32_t g_item_set_of_cooking_utensils;
extern int32_t g_item_shotgun;
extern int32_t g_item_shovel;
extern int32_t g_item_soap;
extern int32_t g_item_spare_horse_or_mule_harn;
extern int32_t g_item_turpentine;
extern int32_t g_item_water_keg_empty_holds_4_;
extern int32_t g_item_winter_coat;
extern int32_t g_item_winter_scarf;

static int32_t item_count(void)
{
	struct strrsrc *keys = GetItemKeyNames();

	return keys ? StrRsrcCount(keys) : 0;
}

static void key_at(int32_t i, char *out, int32_t size)
{
	StrRsrcCopyTo(GetItemKeyNames(), i, out, size);
}

static struct inv_item *only(struct inv_list *list)
{
	return InvItemFromNode(list->base.head);
}

static void add(struct inv_list *list, const char *key, int32_t n)
{
	struct cstr *s = CStrCreateFrom(key);
	InvListAdd(list, s, n, 0);
	CStrDestroy(&s);
}

static int32_t total_weight(struct inv_item *it)
{
	return INV_ITEM_OPS(it)->TotalWeight(it);
}

TEST_GAME(unit_inv, every_item_counts_weight_value)
{
	int32_t n = item_count();

	CHECK(n > 100);
	for (int32_t i = 0; i < n; ++i) {
		char key[80];
		struct inv_list list;
		struct inv_item *it;
		int32_t w1, v1;
		key_at(i, key, sizeof(key));
		if (!key[0])
			continue;
		TCase("\"%s\"", key);
		InvListInit(&list);
		{
			struct cstr *s = CStrCreateFrom(key);
			InvListAdd(&list, s, 1, 0);
			it = InvListFindByName(&list, s);
			CStrDestroy(&s);
		}
		CHECK(it != NULL);
		if (!it) {
			inv_list_destroy(&list);
			continue;
		}
		CHECK_EQ(InvItemGetCount(it, true), 1);
		w1 = total_weight(it);
		v1 = InvItemTotalValue(it);
		CHECK(w1 >= 0);
		CHECK(v1 >= 0);
		InvItemAddUnits(it, 4);
		CHECK_EQ(InvItemGetCount(it, true), 5);
		CHECK_NEAR(total_weight(it), 5.0 * w1, 5.0);
		CHECK_NEAR(InvItemTotalValue(it), 5.0 * v1, 5.0);
		CHECK(InvItemRemoveUnits(it, 2));
		CHECK_EQ(InvItemGetCount(it, true), 3);
		CHECK(!InvItemRemoveUnits(it, 10));
		CHECK_EQ(InvItemGetCount(it, true), 0);
		CHECK_EQ(total_weight(it), 0);
		InvListPurgeEmpty(&list);
		CHECK(list.base.head == NULL);
		inv_list_destroy(&list);
	}
}

TEST_GAME(unit_inv, portions)
{
	int32_t n = item_count();

	for (int32_t i = 0; i < n; ++i) {
		char key[80];
		struct inv_list list;
		struct inv_item *it;
		int32_t per, total, ate, asked;
		key_at(i, key, sizeof(key));
		if (!key[0])
			continue;
		InvListInit(&list);
		add(&list, key, 3);
		it = only(&list);
		if (!it) {
			inv_list_destroy(&list);
			continue;
		}
		per = InvItemGetPortionsPerUnit(it);
		if (per <= 0) {
			inv_list_destroy(&list);
			continue;
		}
		TCase("\"%s\" (%d portions a unit)", key, per);
		total = InvItemTotalPortions(it);
		CHECK_EQ(total, 3 * per);
		asked = per + 1;
		ate = InvItemConsumePortions(it, asked);
		CHECK_EQ(ate, asked);
		CHECK_EQ(InvItemTotalPortions(it), total - asked);
		ate = InvItemConsumePortions(it, 10 * per);
		CHECK_EQ(ate, total - asked);
		CHECK_EQ(InvItemTotalPortions(it), 0);
		CHECK_EQ(InvItemConsumePortions(it, 5), 0);
		CHECK(InvItemGetCount(it, true) >= 0);
		inv_list_destroy(&list);
	}
}

TEST_GAME(unit_inv, spoilage)
{
	int32_t n = item_count(), perishable = 0;

	for (int32_t i = 0; i < n; ++i) {
		char key[80];
		struct inv_list list;
		struct inv_item *it;
		int32_t days, d;
		key_at(i, key, sizeof(key));
		if (!key[0])
			continue;
		InvListInit(&list);
		add(&list, key, 2);
		it = only(&list);
		if (!it || !InvItemIsPerishable(it)) {
			inv_list_destroy(&list);
			continue;
		}
		++perishable;
		TCase("\"%s\"", key);
		days = it->days_to_spoil;
		CHECK(days > 0);
		for (d = 1; d < days && d < 2000; ++d)
			CHECK_MSG(!InvItemTickSpoilage(it), "spoiled after %d of %d days", d, days);
		if (days < 2000)
			CHECK_MSG(InvItemTickSpoilage(it), "not spoiled after its %d days", days);
		inv_list_destroy(&list);
	}
	TCase("");
	TNote("%d perishable items", perishable);
}

TEST_GAME(unit_inv, code_item_names)
{
	static const struct {
		const char *name;
		int32_t *index;
	} globals[] = {
		{"anvil", &g_item_anvil},
		{"ax", &g_item_ax},
		{"blanket", &g_item_blanket},
		{"boots", &g_item_boots},
		{"bridle", &g_item_bridle},
		{"butter_churn", &g_item_butter_churn},
		{"canteen", &g_item_canteen},
		{"chain", &g_item_chain},
		{"chicken", &g_item_chicken},
		{"conestoga", &g_item_conestoga},
		{"egg", &g_item_egg},
		{"grease", &g_item_grease},
		{"hammer", &g_item_hammer},
		{"hatchet", &g_item_hatchet},
		{"long_underwear", &g_item_long_underwear},
		{"matches", &g_item_matches},
		{"milk", &g_item_milk},
		{"milk_cow", &g_item_milk_cow},
		{"mittens", &g_item_mittens},
		{"nails", &g_item_nails},
		{"pick_ax", &g_item_pick_ax},
		{"pistol", &g_item_pistol},
		{"rifle", &g_item_rifle},
		{"rifle_or_shotgun_sheath", &g_item_rifle_or_shotgun_sheath},
		{"rope", &g_item_rope},
		{"saw", &g_item_saw},
		{"set_of_cooking_utensils", &g_item_set_of_cooking_utensils},
		{"shotgun", &g_item_shotgun},
		{"shovel", &g_item_shovel},
		{"soap", &g_item_soap},
		{"spare_horse_or_mule_harn", &g_item_spare_horse_or_mule_harn},
		{"turpentine", &g_item_turpentine},
		{"water_keg_empty_holds_4_", &g_item_water_keg_empty_holds_4_},
		{"winter_coat", &g_item_winter_coat},
		{"winter_scarf", &g_item_winter_scarf}
	};
	static const char *const literals[] = {
		"bullets", "chicken", "fish [from trail]", "fresh vegetables [from trail]", "fresh fruit [from trail]",
		"gunpowder", "hay", "meat [from hunting]", "ox", "pig", "salt", "salted fish [from trail]",
		"salted meat [from hunting]", "shot", "soap", "flour"
	};

	for (int32_t i = 0; i < (int32_t) (sizeof(globals) / sizeof(globals[0])); ++i) {
		TCase("g_item_%s", globals[i].name);
		CHECK(*globals[i].index >= 0);
	}
	for (int32_t i = 0; i < (int32_t) (sizeof(literals) / sizeof(literals[0])); ++i) {
		struct cstr *s = CStrCreateFrom(literals[i]);
		TCase("\"%s\"", literals[i]);
		CHECK(LookupItemId(s) >= 0);
		CStrDestroy(&s);
	}
}

TEST_GAME(unit_inv, format_contents_lists_every_item)
{
	int32_t n = item_count(), added = 0, pages = 0;
	struct inv_list list;
	struct inv_item *it, *cursor;
	struct cstr *all, *page, *orig;
	char joined[8192];

	InvListInit(&list);
	for (int32_t i = 0; i < n && added < 30; ++i) {
		char key[80];
		key_at(i, key, sizeof(key));
		if (!key[0])
			continue;
		add(&list, key, added + 2);
		++added;
	}
	CHECK_EQ(added, 30);
	all = InvListFormatContentsAll(&list, true);
	CHECK(strstr(CStrGet(all), "various") == NULL);
	for (it = only(&list); it; it = InvItemFromNode(it->node.next)) {
		struct cstr *name = InvItemGetName(it);
		TCase("\"%s\"", CStrGet(name));
		CHECK(strstr(CStrGet(all), CStrGet(name)) != NULL);
		CStrDestroy(&name);
	}
	TCase("pages");
	joined[0] = 0;
	cursor = only(&list);
	while (cursor) {
		const char *text;
		size_t len;
		page = InvListFormatContentsPage(&list, &cursor, true);
		text = CStrGet(page);
		len = strlen(text);
		++pages;
		CHECK(len < 400);
		if (cursor) {
			CHECK(len > 24 && strcmp(text + len - 24, " and various other items") == 0);
			size_t at = strlen(joined);
			if (len > 24 && at + len - 24 < sizeof(joined)) {
				memcpy(joined + at, text, len - 24);
				joined[at + len - 24] = 0;
			}

		} else {
			StrCat(joined, text);
		}
		CStrDestroy(&page);
	}
	CHECK(pages > 1);
	CHECK(strcmp(joined, CStrGet(all)) == 0);
	CStrDestroy(&all);
	inv_list_destroy(&list);

	TCase("short list");
	{
		struct inv_list short_list;
		InvListInit(&short_list);
		add(&short_list, "flour", 10);
		add(&short_list, "ax", 1);
		cursor = only(&short_list);
		page = InvListFormatContentsPage(&short_list, &cursor, true);
		orig = InvListFormatContents(&short_list, true);
		CHECK(cursor == NULL);
		CHECK(strcmp(CStrGet(page), CStrGet(orig)) == 0);
		CStrDestroy(&page);
		CStrDestroy(&orig);
		inv_list_destroy(&short_list);
	}
}

TEST_GAME(unit_inv, impossible)
{
	struct inv_list list;
	struct inv_item *it;

	InvListInit(&list);
	add(&list, "flour", 5);
	it = only(&list);
	CHECK(it != NULL);
	if (!it) {
		inv_list_destroy(&list);
		return;
	}
	TCase("RemoveUnits(-3)");
	InvItemRemoveUnits(it, -3);
	CHECK(InvItemGetCount(it, true) >= 0);
	TNote("RemoveUnits(-3) on 5 left %d", InvItemGetCount(it, true));
	TCase("ConsumePortions(-10)");
	InvItemConsumePortions(it, -10);
	CHECK(InvItemTotalPortions(it) >= 0);
	TCase("AddUnits(-100)");
	InvItemAddUnits(it, -100);
	TNote("AddUnits(-100) left %d", InvItemGetCount(it, true));
	CHECK(1);
	inv_list_destroy(&list);
}

const struct test_reg TESTS_unit_inv[] = {
	TEST_GAME_ENTRY(unit_inv, every_item_counts_weight_value),
	TEST_GAME_ENTRY(unit_inv, portions),
	TEST_GAME_ENTRY(unit_inv, spoilage),
	TEST_GAME_ENTRY(unit_inv, code_item_names),
	TEST_GAME_ENTRY(unit_inv, format_contents_lists_every_item),
	TEST_GAME_ENTRY(unit_inv, impossible),
	TEST_END
};

#endif
