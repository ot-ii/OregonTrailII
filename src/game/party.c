#include "party.h"

#include <string.h>

#include "../base/byteorder.h"
#include "../base/cstr.h"
#include "../data/docfile.h"
#include "draftanimalitem.h"
#include "game.h"
#include "../data/gfxdev.h"
#include "../screens/healthwin.h"
#include "invitem.h"
#include "journal.h"
#include "../screens/mainui.h"
#include "member.h"
#include "../base/memory.h"
#include "msg.h"
#include "newgamescenario.h"
#include "occupmgr.h"
#include "../app/otiiapp.h"
#include "../base/rng.h"
#include "../data/strrsrc.h"
#include "wagonitem.h"
#include "weather.h"

#define PARTY_BLOCK_SIZE 50

static struct party *party_alloc(void)
{
	struct party *self = MemAlloc(1, sizeof(struct party));

	InvListInit(&self->animals);
	InvListInit(&self->spare_animals);
	InvListInit(&self->wagons);
	InvListInit(&self->inventory);

	return self;
}

struct party *PartyCreateScenario(struct new_game_scenario *scenario)
{
	struct party *self = party_alloc();

	self->member_count = 0;

	for (int32_t index = 0; index < 6; index++) {
		if (StrRsrcCount(scenario->names) > index) {
			int32_t age = scenario->ages[index];

			self->members[self->member_count] = MemberCreate(self->member_count,
				StrRsrcGetCStr(scenario->names, index), age, 0, 0);
			++self->member_count;

		} else {
			self->members[index] = NULL;
		}
	}

	self->occupation = scenario->occupation;
	self->cash = (double) scenario->cash;
	self->score_multiplier = scenario->score_multiplier;
	self->unused_skill_points = scenario->unused_skill_points;

	struct cstr *wagon_name = CStrCreate();

	switch (scenario->wagon_type) {
		case 1:
			CStrAssignBuf(wagon_name, "large farmwagon");
			break;
		case 2:
			CStrAssignBuf(wagon_name, "Conestoga wagon");
			break;
		case 3:
			CStrAssignBuf(wagon_name, "handcart");
			break;
		default:
			CStrAssignBuf(wagon_name, "small farmwagon");
			break;
	}

	struct inv_list *loot = InvListCreate();
	InvListAdd(loot, wagon_name, 1, 0.0);
	PartyDistributeLoot(self, loot);
	InvListDestroy(&loot);

	for (int32_t index = 0; index < 15; index++)
		self->skills[index] = scenario->skills[index] != 0;

	self->reputation = 90;
	StrRsrcDestroy(&scenario->names);
	self->days_starving = 0;
	self->scurvy_risk = 0;
	self->beriberi_risk = 0;
	self->daily_health_drain = 0;
	self->hitched = NULL;
	self->ration_level = RATIONS_FILLING;

	CStrDestroy(&wagon_name);

	return self;
}

struct party *PartyCreateQuickStart(int32_t command)
{
	(void) command;

	struct party *self = party_alloc();
	int32_t picks[6] = {0};
	struct cstr *member_names[6] = {0};

	self->member_count = RandRangeAvg(4, 6, 1);

	struct occup_mgr *occupations = OccupMgrCreate();
	struct strrsrc *names = StrRsrcLoad(g_text_lib, 2208, true);
	int32_t pick = RandRangeAvg(0, StrRsrcCount(names) - 1, 1);

	member_names[0] = StrRsrcGetCStr(names, pick);
	StrRsrcDestroy(&names);
	names = StrRsrcLoad(g_text_lib, 2203, true);

	for (int32_t index = 1; index < self->member_count; index++) {
		bool unique = false;

		while (!unique) {
			picks[index] = RandRangeAvg(0, StrRsrcCount(names) - 1, 1);
			unique = true;

			for (int32_t other = 1; other < index; other++) {
				if (picks[index] == picks[other])
					unique = false;
			}
		}

		member_names[index] = StrRsrcGetCStr(names, picks[index]);
	}

	StrRsrcDestroy(&names);

	for (int32_t index = 0; index < 6; index++) {
		if (index < self->member_count) {
			int32_t age;

			if (index < 2) {
				age = RandRangeAvg(15, 20, 1);

			} else if (index < 4) {
				age = RandRangeAvg(4, 17, 1);

			} else {
				age = RandRangeAvg(18, 24, 1);
			}

			int32_t birth_month = RandRangeAvg(1, 12, 1);
			int32_t birth_day = RandRangeAvg(1, 28, 1);

			self->members[index] = MemberCreate(index, member_names[index], age, birth_month, birth_day);

		} else {
			self->members[index] = NULL;
		}
	}

	self->occupation = RandRangeAvg(0, 8, 1);
	self->cash = (double) OccupMgrGetStartingCash(occupations, self->occupation);
	self->cash = self->cash - 75.0;
	self->score_multiplier = OccupMgrGetScoreMultX10(occupations, self->occupation);
	self->unused_skill_points = 120;

	struct cstr *wagon_name = CStrCreateFrom("large farmwagon");
	struct inv_list *loot = InvListCreate();
	InvListAdd(loot, wagon_name, 1, 0.0);
	PartyDistributeLoot(self, loot);
	InvListDestroy(&loot);

	for (int32_t index = 0; index < 15; index++)
		self->skills[index] = OccupMgrHasSkill(occupations, self->occupation, index);

	self->reputation = 90;
	self->days_starving = 0;
	self->scurvy_risk = 0;
	self->beriberi_risk = 0;
	self->daily_health_drain = 0;
	self->hitched = NULL;
	self->ration_level = RATIONS_FILLING;
	OccupMgrDestroy(&occupations);

	CStrDestroy(&wagon_name);

	return self;
}

struct party *PartyCreate(void)
{
	return party_alloc();
}

void PartyDestroy(struct party **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct party *ctx = *ctx_mod;
	*ctx_mod = NULL;

	inv_list_destroy(&ctx->animals);
	inv_list_destroy(&ctx->spare_animals);
	inv_list_destroy(&ctx->wagons);
	inv_list_destroy(&ctx->inventory);

	for (int32_t i = 0; ctx->member_count > i; i++)
		ObjectDelete(ctx->members[i]);

	MemFree(ctx);
}

void PartyLoadState(struct party *self, struct doc_file *file)
{
	void *mem = NULL;
	int32_t size = 0;

	DocFileReadObject2(file, &mem, &size);

	uint8_t blk[PARTY_BLOCK_SIZE] = {0};

	if (mem && size > 0)
		memcpy(blk, mem, (size_t) (size < PARTY_BLOCK_SIZE ? size : PARTY_BLOCK_SIZE));

	MemFree(mem);

	const uint8_t *cursor = blk;

	self->pull_mode = *cursor++;
	self->member_count = *cursor++;
	self->occupation = *cursor++;
	self->meat_spoiled = (int8_t) *cursor++;
	self->hitched = NULL;

	const uint8_t *words = cursor;

	self->cash = (double) ReadLe16(words);
	self->cash = (double) ReadLe16(words + 2) / 100.0 + self->cash;
	self->score_multiplier = ReadLe16(words + 4);
	self->unused_skill_points = ReadLe16(words + 6);
	self->reputation = ReadLe16(words + 8);
	self->days_starving = ReadLe16(words + 10);
	self->ration_level = ReadLe16(words + 12);
	self->scurvy_risk = ReadLe16(words + 14);
	self->beriberi_risk = ReadLe16(words + 16);
	self->daily_health_drain = ReadLe16(words + 18);
	cursor = words + 20;

	for (int32_t index = 0; index < 15; index++)
		self->skills[index] = *cursor++ != 0;

	if (self->member_count > 6)
		self->member_count = 6;

	for (int32_t index = 0; self->member_count > index; index++) {
		self->members[index] = MemberCreateEmpty();
		MemberLoadState(self->members[index], file);
	}

	struct inv_list *loot = InvListCreate();
	InvListLoadState(loot, file);
	InvListLoadState(loot, file);
	InvListLoadState(loot, file);
	InvListLoadState(loot, file);
	PartyDistributeLoot(self, loot);
	InvListDestroy(&loot);
}

void PartySaveState(struct party *self, struct doc_file *file)
{
	uint8_t blk[PARTY_BLOCK_SIZE] = {0};
	uint8_t *cursor = blk;

	*cursor++ = (uint8_t) self->pull_mode;
	*cursor++ = (uint8_t) self->member_count;
	*cursor++ = (uint8_t) self->occupation;
	*cursor++ = (uint8_t) self->meat_spoiled;

	uint8_t *words = cursor;

	double total_cents = self->cash * 100.0 + 0.5;

	if (total_cents < 0)
		total_cents = 0;

	if (total_cents > 32767.0 * 100.0 + 99.0)
		total_cents = 32767.0 * 100.0 + 99.0;

	int32_t all_cents = (int32_t) total_cents;

	WriteLe16(words, all_cents / 100);
	WriteLe16(words + 2, (int32_t) (double) (all_cents % 100));
	WriteLe16(words + 4, self->score_multiplier);
	WriteLe16(words + 6, self->unused_skill_points);
	WriteLe16(words + 8, self->reputation);
	WriteLe16(words + 10, self->days_starving);
	WriteLe16(words + 12, self->ration_level);
	WriteLe16(words + 14, self->scurvy_risk);
	WriteLe16(words + 16, self->beriberi_risk);
	WriteLe16(words + 18, self->daily_health_drain);
	cursor = words + 20;

	for (int32_t index = 0; index < 15; index++)
		*cursor++ = self->skills[index];

	DocFileWriteObject(file, blk, PARTY_BLOCK_SIZE);

	for (int32_t index = 0; self->member_count > index; index++)
		MemberSaveState(self->members[index], file);

	InvListSaveState(&self->animals, file);
	InvListSaveState(&self->spare_animals, file);
	InvListSaveState(&self->wagons, file);
	InvListSaveState(&self->inventory, file);
}

void PartyDailyHealthUpdate(struct party *self, int32_t hours)
{
	int32_t item_count = 0;

	if (!InvListHasGroup(&self->inventory, IGRP_CLOTHING))
		self->daily_health_drain += 30;

	if (self->days_starving) {
		self->daily_health_drain += 80;

	} else {
		if (self->ration_level == RATIONS_MEAGER)
			self->daily_health_drain += 45;

		if (!self->ration_level)
			self->daily_health_drain += 60;
	}

	if (WeatherGetLowTemp(GetGameObj()->weather, 0) < 10)
		self->daily_health_drain += 25;

	if (GetGameObj()->travel_state != TRAVEL_RESTING) {
		if (GetGameObj()->travel_pace == PACE_10_HOURS)
			self->daily_health_drain += 45;

		if (GetGameObj()->travel_pace == PACE_EXHAUSTING)
			self->daily_health_drain += 60;

		if (WeatherSumPrecip(GetGameObj()->weather, 0))
			self->daily_health_drain += 20;

		if (WeatherGetHighTemp(GetGameObj()->weather, 0) > 95)
			self->daily_health_drain += 25;

	} else {
		self->daily_health_drain -= 50;

		if (PartyHasPlentyOfFood(self)) {
			self->reputation += 2;

			if (self->reputation > 100)
				self->reputation = 100;

			if (self->reputation < -50)
				self->reputation = -50;
		}
	}

	for (int32_t index = 0; self->member_count > index; ++index) {
		if (self->members[index]->state == MEMBER_ALIVE) {
			int32_t sickness = RandRangeAvg(self->daily_health_drain - 15, self->daily_health_drain + 15, 2);
			int32_t starve_penalty;

			if (self->days_starving) {
				starve_penalty = self->members[index]->health / 10 + 10 * (self->days_starving % 10) -
					self->daily_health_drain;

			} else {
				starve_penalty = 0;
			}

			MemberHealthTick(self->members[index], hours, (int16_t) (sickness + starve_penalty));
		}
	}

	if (g_health_window)
		HealthWinUpdate(g_health_window, false);

	self->meat_spoiled = 0;

	struct inv_item **meat_items = InvListCollectGroup(&self->inventory, IGRP_FOOD, &item_count, 0);

	for (int32_t index = 0; index < item_count; ++index) {
		if (WeatherGetHighTemp(GetGameObj()->weather, 0) > 45) {
			if (InvItemTickSpoilage(meat_items[index])) {
				ListRemove(&self->inventory.base, &meat_items[index]->node);
				self->meat_spoiled = 1;
				ObjectDelete(meat_items[index]);

			} else if (WeatherGetHighTemp(GetGameObj()->weather, 0) > 80 && InvItemTickSpoilage(meat_items[index])) {
				ListRemove(&self->inventory.base, &meat_items[index]->node);
				self->meat_spoiled = 1;
				ObjectDelete(meat_items[index]);
			}
		}
	}

	MemFree(meat_items);

	if (PartyConsumeDailyFood(self)) {
		self->days_starving = 0;

	} else {
		++self->days_starving;
	}

	if (GameIsEventAllowed(GetGameObj(), NODEFEAT_NO_GRASS)) {
		int32_t feed_weight = 2 * PartyCountFood(self);
		struct inv_item **feed_items = InvListCollectGroup(&self->inventory, IGRP_ANIMAL_FEED, &item_count, 0);

		for (int32_t index = 0; index < item_count && feed_weight; ++index)
			feed_weight -= InvItemConsumePortions(feed_items[index], feed_weight);

		MemFree(feed_items);
	}

	InvListPurgeEmpty(&self->inventory);
	InvListPurgeEmpty(&self->wagons);
	PartyPurgeEmptyAnimals(self);
	InvListPurgeEmpty(&self->spare_animals);
	self->daily_health_drain = 0;
}

void PartyDistributeLoot(struct party *self, struct inv_list *loot)
{
	while (loot->base.head) {
		struct inv_item *item = InvItemFromNode(loot->base.head);

		ListRemove(&loot->base, &item->node);

		if (InvItemInGroup(item, IGRP_WAGON)) {
			PartyAddWagon(self, &self->wagons, InvItemGetNameCStr(item), InvItemGetCount(item, false));

		} else if (InvItemInGroup(item, IGRP_DRAFT_ANIMAL)) {
			PartyAddDraftAnimal(self, &self->animals, InvItemGetNameCStr(item), InvItemGetCount(item, false));

		} else if (InvItemInGroup(item, IGRP_CASH)) {
			self->cash = (double) InvItemGetCount(item, false) + self->cash;

		} else {
			PartyLoadItem(self, item);
			continue;
		}

		ObjectDelete(item);
	}
}

void PartyLoadItem(struct party *self, struct inv_item *item)
{
	struct inv_item *found = InvListFindByName(&self->inventory, InvItemGetNameCStr(item));

	item->selected_qty = 0;

	if (!found || InvItemIsPerishable(found) || InvItemIsPerishable(item)) {
		ListInsertTail(&self->inventory.base, &item->node);
		return;
	}

	InvItemAddUnits(found, item->quantity);

	if (item->partial_portions) {
		int32_t per_unit = InvItemGetPortionsPerUnit(found);

		if (!found->partial_portions) {
			found->partial_portions = item->partial_portions;

		} else {
			found->partial_portions += item->partial_portions;

			if (per_unit > 0 && found->partial_portions > per_unit) {
				found->partial_portions -= per_unit;
				InvItemAddUnits(found, 1);
			}
		}
	}

	ObjectDelete(item);
}

static struct draft_animal_item *animal_from_node(struct list_node *node)
{
	return (struct draft_animal_item *) InvItemFromNode(node);
}

int32_t PartyCalcPullingMode(struct party *self)
{
	int32_t team_work = 0;
	int32_t wagon_count = 0;
	int32_t min_speed = 2147483647;
	int32_t min_easy = 2147483647;
	int32_t min_strained = 2147483647;
	double scale = 0.0;
	double rating[PULL_CANNOT_MOVE + 1];
	struct draft_animal_item *best_animal[PULL_CANNOT_MOVE + 1];

	int32_t load = InvListTotalWeight(&self->wagons) / 16 + InvListTotalWeight(&self->inventory) / 16;

	for (int32_t mode = PULL_SINGLE_EASY; mode <= PULL_CANNOT_MOVE; mode++) {
		rating[mode] = 0.0;
		best_animal[mode] = NULL;
	}

	for (struct inv_item *wagon = InvItemFromNode(self->wagons.base.head); wagon;
		wagon = InvItemFromNode(wagon->node.next))
		wagon_count += InvItemGetCount(wagon, false);

	if (!wagon_count) {
		self->pull_mode = PULL_CANNOT_MOVE;
		return 0;
	}

	struct draft_animal_item *animal = animal_from_node(self->animals.base.head);

	while (animal) {
		if (DraftAnimalItemEasyTeamLoad(animal, wagon_count) >= load) {
			if ((double) DraftAnimalItemGetSpeed(animal) > rating[PULL_SINGLE_EASY]) {
				rating[PULL_SINGLE_EASY] = (double) DraftAnimalItemGetSpeed(animal);
				best_animal[PULL_SINGLE_EASY] = animal;
			}
		}

		animal = animal_from_node(animal->base.node.next);
	}

	animal = animal_from_node(self->animals.base.head);

	while (animal) {
		if (DraftAnimalItemStrainedTeamLoad(animal, wagon_count) >= load) {
			if ((double) DraftAnimalItemGetSpeed(animal) > rating[PULL_SINGLE_STRAINED]) {
				if (DraftAnimalItemEasyTeamLoad(animal, wagon_count) < load) {
					rating[PULL_SINGLE_STRAINED] = (double) DraftAnimalItemGetSpeed(animal);
					best_animal[PULL_SINGLE_STRAINED] = animal;
				}
			}
		}

		animal = animal_from_node(animal->base.node.next);
	}

	if (rating[PULL_SINGLE_STRAINED] != 0.0) {
		scale = 1.0 - (double) (load - DraftAnimalItemEasyTeamLoad(best_animal[PULL_SINGLE_STRAINED], wagon_count)) /
			(double) (DraftAnimalItemStrainedTeamLoad(best_animal[PULL_SINGLE_STRAINED], wagon_count) -
			DraftAnimalItemEasyTeamLoad(best_animal[PULL_SINGLE_STRAINED], wagon_count));
		rating[PULL_SINGLE_STRAINED] = scale * rating[PULL_SINGLE_STRAINED];
	}

	if (self->animals.base.count > 1) {
		animal = animal_from_node(self->animals.base.head);

		while (animal) {
			team_work += DraftAnimalItemWorkUnits(animal, wagon_count);

			if (DraftAnimalItemGetSpeed(animal) < min_speed)
				min_speed = DraftAnimalItemGetSpeed(animal);

			if (DraftAnimalItemGetEasyLoad(animal) < min_easy)
				min_easy = DraftAnimalItemGetEasyLoad(animal);

			if (DraftAnimalItemGetStrainedLoad(animal) < min_strained)
				min_strained = DraftAnimalItemGetStrainedLoad(animal);

			animal = animal_from_node(animal->base.node.next);
		}

		scale = 0.81;
		min_easy = min_easy * team_work;
		min_strained = min_strained * team_work;

		if (load < min_easy)
			rating[PULL_TEAM_EASY] = (double) min_speed * scale;

		if (load < min_strained && load > min_easy) {
			rating[PULL_TEAM_STRAINED] = (double) min_speed * scale;

			double over = (double) (load - min_easy);

			if ((scale = (double) (min_strained - min_easy)) == 0.0)
				scale = 1.0;

			scale = 1.0 - over / scale;
			rating[PULL_TEAM_STRAINED] = scale * rating[PULL_TEAM_STRAINED];
		}
	}

	self->pull_mode = PULL_CANNOT_MOVE;

	if (rating[PULL_SINGLE_EASY] != 0.0) {
		self->pull_mode = PULL_SINGLE_EASY;

	} else if (rating[PULL_TEAM_EASY] != 0.0) {
		self->pull_mode = PULL_TEAM_EASY;

	} else if (rating[PULL_SINGLE_STRAINED] != 0.0 || rating[PULL_TEAM_STRAINED] != 0.0) {
		self->pull_mode = rating[PULL_TEAM_STRAINED] < rating[PULL_SINGLE_STRAINED] ? PULL_SINGLE_STRAINED :
			PULL_TEAM_STRAINED;
	}

	self->hitched = (struct inv_item *) best_animal[self->pull_mode];

	return (int32_t) rating[self->pull_mode];
}

bool PartyUnhitchDraft(struct party *self)
{
	struct inv_item *animal = self->hitched;

	if (!animal)
		animal = InvItemFromNode(self->animals.base.head);

	if (!animal)
		return false;

	if (InvItemGetCount(animal, false) == 1) {
		ListRemove(&self->animals.base, &animal->node);
		PartyAddDraftAnimal(self, &self->spare_animals, InvItemGetNameCStr(animal), 1);

		if (self->hitched == animal)
			self->hitched = NULL;

		ObjectDelete(animal);

	} else {
		InvItemRemoveUnits(animal, 1);
		PartyAddDraftAnimal(self, &self->spare_animals, InvItemGetNameCStr(animal), 1);
	}

	return true;
}

bool PartyUnhitchSpare(struct party *self)
{
	struct inv_item *animal = NULL;

	if (self->hitched)
		animal = InvListFindByName(&self->spare_animals, InvItemGetNameCStr(self->hitched));

	if (!animal)
		animal = InvItemFromNode(self->spare_animals.base.head);

	if (!animal)
		return false;

	if (InvItemGetCount(animal, false) == 1) {
		ListRemove(&self->spare_animals.base, &animal->node);
		PartyAddDraftAnimal(self, &self->animals, InvItemGetNameCStr(animal), 1);

		if (self->hitched == animal)
			self->hitched = NULL;

		ObjectDelete(animal);

	} else {
		InvItemRemoveUnits(animal, 1);
		PartyAddDraftAnimal(self, &self->animals, InvItemGetNameCStr(animal), 1);
	}

	return true;
}

void PartyPurgeEmptyAnimals(struct party *self)
{
	if (self->hitched && !InvItemGetCount(self->hitched, false))
		self->hitched = NULL;

	InvListPurgeEmpty(&self->animals);
}

void PartyAddDraftAnimal(struct party *self, struct inv_list *list, const struct cstr *name, int32_t n)
{
	(void) self;

	struct inv_item *found = InvListFindByName(list, name);

	if (found) {
		InvItemAddUnits(found, n);

	} else {
		found = (struct inv_item *) DraftAnimalItemCreate(name, n);
		InvListAppend(list, found);
	}
}

void PartyAddWagon(struct party *self, struct inv_list *list, const struct cstr *name, int32_t n)
{
	(void) self;

	struct inv_item *found = InvListFindByName(list, name);

	if (found) {
		InvItemAddUnits(found, n);

	} else {
		found = (struct inv_item *) WagonItemCreate(name, n);
		InvListAppend(list, found);
	}
}

int32_t LevelStringToInt(struct cstr *s)
{
	int32_t cmp = strcmp(CStrGet(s), "low");

	if (cmp == 0)
		return 1;

	if (cmp > 0)
		return 2;

	return 3;
}

int32_t PartyTotalWagonCapacity(struct party *self)
{
	int32_t total = 1;

	for (struct inv_item *wagon = InvItemFromNode(self->wagons.base.head); wagon;
		wagon = InvItemFromNode(wagon->node.next))
		total += WagonItemGetCapacity((struct wagon_item *) wagon) * InvItemGetCount(wagon, false);

	return (int16_t) total;
}

int32_t PartyGetLoadUnits(struct party *self)
{
	return (int16_t) (InvListTotalWeight(&self->inventory) / 16 + 1);
}

int32_t PartySumSpares(struct party *self)
{
	int32_t sum = 0;

	for (struct inv_item *animal = InvItemFromNode(self->spare_animals.base.head); animal;
		animal = InvItemFromNode(animal->node.next))
		sum += InvItemGetCount(animal, false);

	return (int16_t) sum;
}

int32_t PartyCountFood(struct party *self)
{
	int32_t sum = 0;

	for (struct inv_item *animal = InvItemFromNode(self->animals.base.head); animal;
		animal = InvItemFromNode(animal->node.next))
		sum += InvItemGetCount(animal, false);

	return (int16_t) sum;
}

int32_t PartyMemberCount(struct party *self, bool alive_only)
{
	if (!alive_only)
		return self->member_count;

	int32_t alive_count = 0;

	for (int32_t index = 0; self->member_count > index; index++) {
		if (self->members[index]->state == MEMBER_ALIVE)
			++alive_count;
	}

	return alive_count;
}

int32_t PartyGetFoodPounds(struct party *self)
{
	int32_t count = 0;
	int32_t total = 0;
	struct inv_item **items = InvListCollectGroup(&self->inventory, IGRP_FOOD, &count, 0);

	for (int32_t index = 0; index < count; index++)
		total += INV_ITEM_OPS(items[index])->TotalWeight(items[index]);

	MemFree(items);

	total /= 16;

	if (total > 32000)
		total = 32000;

	return (int16_t) total;
}

bool PartyHasPlentyOfFood(struct party *self)
{
	int32_t alive = PartyMemberCount(self, true);

	if (!alive)
		return true;

	return PartyGetFoodPounds(self) / alive > 20;
}

bool PartyConsumeDailyFood(struct party *self)
{
	bool ate_antiscurvy = false;
	bool ate_antiberiberi = false;
	int32_t ration;

	if (!self->ration_level) {
		ration = 1;

	} else if (self->ration_level == RATIONS_MEAGER) {
		ration = 2;

	} else {
		ration = 3;
	}

	int32_t amount = PartyMemberCount(self, true) * ration;

	if (!InvListHasGroup(&self->inventory, IGRP_COOKWARE)) {
		amount *= 7;
		amount /= 6;

		if (!amount)
			amount = 1;
	}

	if (self->skills[SKILL_COOKING]) {
		amount *= 5;
		amount /= 6;

		if (!amount)
			amount = 1;
	}

	bool ate_seasoning = PartyConsumeRandomFood(self, IGRP_SEASONING, amount, &ate_antiscurvy, &ate_antiberiberi);

	if (!ate_seasoning)
		amount = (amount + 3) / 4 + amount;

	bool ate_other = PartyConsumeRandomFood(self, IGRP_FOOD_OTHER, amount, &ate_antiscurvy, &ate_antiberiberi);

	if (!ate_other)
		amount = (amount + 2) / 3 + amount;

	bool ate_produce = PartyConsumeRandomFood(self, IGRP_FOOD_PRODUCE, amount, &ate_antiscurvy, &ate_antiberiberi);

	if (!ate_produce)
		amount = (amount + 1) / 2 + amount;

	bool ate_meat = PartyConsumeRandomFood(self, IGRP_FOOD_MEAT, amount, &ate_antiscurvy, &ate_antiberiberi);

	if (!ate_meat)
		amount = amount + amount;

	bool ate_grain = PartyConsumeRandomFood(self, IGRP_FOOD_GRAIN, amount, &ate_antiscurvy, &ate_antiberiberi);

	if (ate_antiscurvy) {
		if (self->scurvy_risk)
			--self->scurvy_risk;

	} else {
		++self->scurvy_risk;
	}

	if (ate_antiberiberi) {
		if (self->beriberi_risk)
			--self->beriberi_risk;

	} else {
		++self->beriberi_risk;
	}

	return ate_seasoning || ate_other || ate_grain || ate_meat || ate_produce;
}

static bool consume_pass(struct inv_item **items, int32_t item_count, int32_t *amount, bool *hit_group81,
	bool *hit_group82)
{
	bool ate = false;
	uint8_t *used = MemAlloc((size_t) item_count, 1);

	if (item_count) {
		ate = true;

		for (int32_t index = 0; index < item_count && *amount > 0; index++) {
			int32_t pick;

			do
				pick = RandRangeAvg(0, item_count - 1, 1);
			while (used[pick]);

			used[pick] = 1;

			if (InvItemInGroup(items[pick], IGRP_ANTISCURVY))
				*hit_group81 = true;

			if (InvItemInGroup(items[pick], IGRP_ANTIBERIBERI))
				*hit_group82 = true;

			*amount -= InvItemConsumePortions(items[pick], *amount);
		}
	}

	MemFree(used);

	return ate;
}

bool PartyConsumeRandomFood(struct party *self, int32_t group, int32_t amount, bool *hit_group81, bool *hit_group82)
{
	bool ate = false;
	int32_t item_count = 0;

	struct inv_item **items = InvListCollectGroup(&self->inventory, group, &item_count, 15);

	if (consume_pass(items, item_count, &amount, hit_group81, hit_group82))
		ate = true;

	MemFree(items);

	if (!amount)
		return ate;

	items = InvListCollectGroup(&self->inventory, group, &item_count, 0);

	if (consume_pass(items, item_count, &amount, hit_group81, hit_group82))
		ate = true;

	MemFree(items);

	return ate;
}

bool PartyHasDraftAnimals(struct party *self)
{
	return PartyCalcPullingMode(self) > 0;
}

void PartyRefreshCatFlags(struct party *self)
{
	bool any_hurt = false;
	int32_t alive_count = 0;
	int32_t avg_health = 0;

	bool has_food = InvListHasGroup(&self->inventory, IGRP_FOOD);

	if (has_food) {
		MainUiClearCatFlag(CAT_NO_FOOD);

	} else {
		MainUiSetCatFlag(CAT_NO_FOOD);
	}

	if ((int16_t) self->wagons.base.count) {
		MainUiClearCatFlag(CAT_NO_WAGON);

	} else {
		MainUiSetCatFlag(CAT_NO_WAGON);
	}

	if ((int16_t) self->animals.base.count) {
		MainUiClearCatFlag(CAT_NO_DRAFT_ANIMALS);

	} else {
		MainUiSetCatFlag(CAT_NO_DRAFT_ANIMALS);
	}

	for (int32_t index = 0; index < self->member_count; index++) {
		if (self->members[index]->state == MEMBER_ALIVE) {
			if (self->members[index]->health < 100)
				any_hurt = true;

			avg_health += self->members[index]->health;
			alive_count++;
		}
	}

	if (alive_count) {
		avg_health /= alive_count;

	} else {
		avg_health = 0;
	}

	if (any_hurt) {
		MainUiSetCatFlag(CAT_PERSON_NEAR_DEATH);

	} else {
		MainUiClearCatFlag(CAT_PERSON_NEAR_DEATH);
	}

	if (has_food && !PartyHasPlentyOfFood(self)) {
		MainUiSetCatFlag(CAT_LOW_FOOD);

	} else {
		MainUiClearCatFlag(CAT_LOW_FOOD);
	}

	if (has_food) {
		bool has_meat = InvListHasGroup(&self->inventory, IGRP_FOOD_MEAT);

		if (!has_meat) {
			struct cstr *pig_name = CStrCreateFrom("pig");
			struct inv_item *pig = InvListFindByName(&self->inventory, pig_name);

			if (pig) {
				struct cstr *journal_line = NULL;

				InvItemRemoveUnits(pig, 1);
				InventoryAddMeat3(RandRangeAvg(20, 40, 2));
				has_meat = true;

				switch (RandRangeAvg(0, 1, 1)) {
					case 0:
						journal_line = CStrCreateFrom("Butchered a pig for some meat today.");
						break;
					case 1:
						journal_line = CStrCreateFrom("Getting low on meat, so we butchered a pig.");
						break;
				}

				struct otii_app *app = (struct otii_app *) g_app;

				JournalSetDate(&app->journal, GetGameObj()->date);
				JournalAddLine(&app->journal, journal_line);
			}

			CStrDestroy(&pig_name);
		}

		if (!has_meat) {
			MainUiSetCatFlag(CAT_NO_MEAT);

		} else {
			MainUiClearCatFlag(CAT_NO_MEAT);
		}

	} else {
		MainUiClearCatFlag(CAT_NO_MEAT);
	}

	if (has_food && !InvListHasGroup(&self->inventory, IGRP_FOOD_PRODUCE)) {
		MainUiSetCatFlag(CAT_NO_FRUIT);
		MainUiSetCatFlag(CAT_NO_VEGETABLES);

	} else {
		MainUiClearCatFlag(CAT_NO_FRUIT);
		MainUiClearCatFlag(CAT_NO_VEGETABLES);
	}

	if (avg_health < 400) {
		MainUiSetCatFlag(CAT_POOR_HEALTH);

	} else {
		MainUiClearCatFlag(CAT_POOR_HEALTH);
	}

	MainUiClearCatFlag(CAT_MORALE_VERY_LOW);
	MainUiClearCatFlag(CAT_MORALE_LOW);
	MainUiClearCatFlag(CAT_MORALE_FAIR);
	MainUiClearCatFlag(CAT_MORALE_HIGH);

	if (self->reputation <= 25) {
		MainUiSetCatFlag(CAT_MORALE_VERY_LOW);

	} else if (self->reputation <= 40 && self->reputation > 25) {
		MainUiSetCatFlag(CAT_MORALE_LOW);

	} else if (self->reputation <= 50 && self->reputation > 40) {
		MainUiSetCatFlag(CAT_MORALE_FAIR);

	} else if (self->reputation > 80) {
		MainUiSetCatFlag(CAT_MORALE_HIGH);
	}
}

struct inv_list *PartyBuildInventoryList(struct party *self)
{
	struct inv_list *merged = InvListCreate();

	for (int32_t list_index = 0; list_index < 4; list_index++) {
		struct inv_list *list = NULL;

		switch (list_index) {
			case 0:
				list = &self->inventory;
				break;
			case 1:
				list = &self->animals;
				break;
			case 2:
				list = &self->spare_animals;
				break;
			case 3:
				list = &self->wagons;
				break;
		}

		for (struct inv_item *item = InvItemFromNode(list->base.head); item; item = InvItemFromNode(item->node.next)) {
			struct inv_item *found = InvListFindByName(merged, InvItemGetNameCStr(item));

			if (found) {
				InvItemAddUnits(found, InvItemGetCount(item, false));

			} else {
				InvListAdd(merged, InvItemGetNameCStr(item), InvItemGetCount(item, false), 0.0);
			}
		}
	}

	return merged;
}

int32_t PartyCountDraftAnimals(struct party *self)
{
	int32_t wagon_count = 0;

	for (struct inv_item *wagon = InvItemFromNode(self->wagons.base.head); wagon;
		wagon = InvItemFromNode(wagon->node.next))
		wagon_count += InvItemGetCount(wagon, false);

	return (int16_t) wagon_count;
}

void PartyRemoveOneAnimal(struct party *self)
{
	struct inv_item *head = InvItemFromNode(self->wagons.base.head);

	if (head) {
		InvItemRemoveUnits(head, 1);
		InvListPurgeEmpty(&self->wagons);
	}
}

struct member *PartyGetMember(struct party *self, int32_t index)
{
	if (self->member_count <= index || index < 0)
		return NULL;

	return self->members[index];
}

void InventoryAddMeat3(int32_t pounds)
{
	struct cstr *meat_name = CStrCreateFrom("meat [from hunting]");
	struct cstr *salted_name = CStrCreateFrom("salted meat [from hunting]");
	struct inv_list *inventory = &GetGameObj()->party->inventory;
	struct cstr *salt_name = CStrCreateFrom("salt");
	int32_t salt_units = (pounds + 7) / 8;

	struct inv_item *salt_item = InvListFindByName(inventory, salt_name);

	if (salt_item) {
		int32_t salted_lbs;
		int32_t leftover_lbs;
		int32_t salt_have = (int16_t) InvItemTotalPortions(salt_item);

		if (salt_have >= salt_units) {
			salted_lbs = pounds;
			leftover_lbs = 0;

		} else {
			salted_lbs = (int16_t) (8 * salt_have);
			salt_units = salt_have;
			leftover_lbs = (int16_t) (pounds - salted_lbs);
		}

		if (salted_lbs) {
			InvListAdd(&GetGameObj()->party->inventory, salted_name, salted_lbs, 0.0);
			InvItemConsumePortions(salt_item, salt_units);
		}

		if (leftover_lbs)
			InvListAdd(&GetGameObj()->party->inventory, meat_name, leftover_lbs, 0.0);

	} else {
		InvListAdd(&GetGameObj()->party->inventory, meat_name, pounds, 0.0);
	}

	CStrDestroy(&salt_name);
	CStrDestroy(&salted_name);
	CStrDestroy(&meat_name);
}
