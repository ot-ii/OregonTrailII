#include "endgame.h"

#include <string.h>

#include "../dialogs/convdlg.h"
#include "../base/cstr.h"
#include "../base/date.h"
#include "game.h"
#include "../data/gfxdev.h"
#include "invitem.h"
#include "invlist.h"
#include "../base/log.h"
#include "member.h"
#include "../base/memory.h"
#include "msg.h"
#include "../trail/node.h"
#include "party.h"
#include "../base/rng.h"
#include "../base/str.h"
#include "../data/strrsrc.h"

int32_t g_route_pts[16] = {814, 738, 767, 427, 804, 728, 757, 417, 720, 644, 673, 333, 817, 743, 772, 432};
int32_t g_route_days[16] = {202, 188, 194, 135, 200, 188, 192, 133, 185, 171, 177, 118, 203, 189, 195, 136};

const struct end_game_ops END_GAME_OPS_TABLE = {
	{end_game_destroy},
	end_game_get_result_mode,
};

void EndGameInit(struct end_game *self, struct game *game)
{
	ObjectInit(&self->base);
	self->base.ops = &END_GAME_OPS_TABLE.base;

	int32_t goods_sum = 0;
	char date_buf[52];
	char msg[100];

	struct strrsrc *month_names = StrRsrcLoad(g_text_lib, 50, true);

	self->game = game;
	self->pts_healthy = 0;
	self->pts_fair = 0;
	self->pts_poor = 0;
	self->pts_cash = 0;
	self->pts_wagons = 0;
	self->pts_handcarts = 0;
	self->pts_animals = 0;
	self->pts_goods = 0;
	self->pts_unused_skill_points = 0;
	self->pts_misc = 0;
	self->pts_year = 0;
	self->pts_route = 0;
	self->pts_speed = 0;
	self->multiplier = 1.0;
	self->raw_total = 0;
	self->final_total = 0;
	self->result_mode = 0;

	switch (self->game->cur_node->node_id) {
		case 265:
		case 266:
			self->destination = DEST_WILLAMETTE;
			break;
		case 506:
		case 507:
			self->destination = DEST_SACRAMENTO;
			break;
		case 619:
		case 620:
			self->destination = DEST_ROGUE_RIVER;
			break;
		case 753:
			self->destination = DEST_SALT_LAKE;
			break;
		default:
			StrPrintf(msg, "Destination node = %d.", self->game->cur_node->node_id);
			LogError(LOG_ERROR, msg);
			self->destination = DEST_WILLAMETTE;
			break;
	}

	self->pts_healthy = 0;
	self->pts_fair = 0;
	self->pts_poor = 0;
	self->n_healthy = 0;
	self->n_fair = 0;
	self->n_poor = 0;

	int32_t member_count = PartyMemberCount(self->game->party, false);

	for (int32_t index = 0; index < member_count; index++) {
		struct member *member = PartyGetMember(self->game->party, index);

		if (member->state == MEMBER_ALIVE) {
			switch (MemberGetHealthBand(member)) {
				case HEALTH_GOOD:
					self->pts_healthy += 500;
					++self->n_healthy;
					break;
				case HEALTH_OK:
					self->pts_fair += 450;
					++self->n_fair;
					break;
				case HEALTH_FAIR:
					self->pts_fair += 400;
					++self->n_fair;
					break;
				case HEALTH_POOR:
					self->pts_poor += 350;
					++self->n_poor;
					break;
				case HEALTH_BAD:
					self->pts_poor += 300;
					++self->n_poor;
					break;
			}
		}
	}

	int32_t cash_pts = 2 * (int32_t) self->game->party->cash / 5;
	self->pts_cash = cash_pts > 32767 ? 32767 : cash_pts < -32768 ? -32768 : cash_pts;

	self->pts_wagons = 0;
	self->pts_handcarts = 0;
	self->pts_animals = 0;

	int32_t loose_value = 0;
	int32_t food_weight = 0;
	struct inv_list *inventory = PartyBuildInventoryList(self->game->party);
	struct inv_item *item = InvItemFromNode(inventory->base.head);

	while (item) {
		struct cstr *name = InvItemGetNameCStr(item);

		if (InvItemInGroup(item, IGRP_WAGON)) {
			if (CStrEqualBuf(name, "handcart")) {
				self->pts_handcarts = (int16_t) (self->pts_handcarts + 25 * InvItemGetCount(item, false));

			} else {
				self->pts_wagons = (int16_t) (self->pts_wagons + 50 * InvItemGetCount(item, false));
			}

		} else if (InvItemInGroup(item, IGRP_DRAFT_ANIMAL)) {
			self->pts_animals = (int16_t) (self->pts_animals + 10 * InvItemGetCount(item, false));

		} else if (InvItemInGroup(item, IGRP_MILK_COW)) {
			goods_sum += 10 * InvItemGetCount(item, false);

		} else if (InvItemInGroup(item, IGRP_LIVESTOCK) || InvItemInGroup(item, IGRP_CHICKEN)) {
			goods_sum += 5 * InvItemGetCount(item, false);

		} else if (InvItemInGroup(item, IGRP_FIREARMS)) {
			if (CStrEqualBuf(name, "shotgun") || CStrEqualBuf(name, "rifle") || CStrEqualBuf(name, "pistol")) {
				goods_sum += 4 * InvItemGetCount(item, false);

			} else if (CStrEqualBuf(name, "bullets")) {
				goods_sum += (InvItemGetPortionsPerUnit(item) * InvItemGetCount(item, false) +
					InvItemGetPartialPortions(item)) / 10;

			} else if (CStrEqualBuf(name, "shot")) {
				goods_sum += INV_ITEM_OPS(item)->TotalWeight(item) / 10;

			} else {
				loose_value += InvItemTotalValue(item);
			}

		} else if (InvItemInGroup(item, IGRP_WAGON_PART)) {
			goods_sum += 3 * InvItemGetCount(item, false);

		} else if (InvItemInGroup(item, IGRP_CLOTHING)) {
			goods_sum += 2 * InvItemGetCount(item, false);

		} else if (InvItemInGroup(item, IGRP_FOOD)) {
			food_weight += INV_ITEM_OPS(item)->TotalWeight(item);

		} else {
			loose_value += InvItemTotalValue(item);
		}

		item = InvItemFromNode(item->node.next);
	}

	InvListDestroy(&inventory);

	int32_t goods_pts = goods_sum + 2 * food_weight / 16 / 25 + loose_value / 500;
	self->pts_goods = goods_pts > 32767 ? 32767 : goods_pts;

	self->pts_unused_skill_points = self->game->party->unused_skill_points;
	self->pts_misc = 0;

	for (int32_t index = 0; index < 1200; index++) {
		if (g_slot_flags[index])
			++self->pts_misc;
	}

	int32_t reputation = self->game->party->reputation;

	if (reputation > 80) {
		self->pts_misc += 100;

	} else if (reputation > 60) {
		self->pts_misc += 75;

	} else if (reputation > 40) {
		self->pts_misc += 50;

	} else if (reputation > 20) {
		self->pts_misc += 25;

	} else if (reputation < -10) {
		self->pts_misc -= 100;

	} else if (reputation < 0) {
		self->pts_misc -= 75;

	} else if (reputation < 10) {
		self->pts_misc -= 50;
	}

	bool arrived = self->game->player_level == LEVEL_GREENHORN;

	if (arrived && GameIsEventAllowed(self->game, NODEFEAT_NO_GREENHORN_ALLOWANCE))
		arrived = false;

	if (!arrived && self->game->destination == self->destination) {
		self->pts_misc += 500;

		if (self->game->player_level == LEVEL_TRAIL_GUIDE)
			self->pts_misc += 500;

		self->pts_misc = (int16_t) (5 * self->game->train_people + self->pts_misc);
		self->pts_misc = (int16_t) (self->pts_misc - 5 * (self->game->train_people - self->game->train_people_start));
		self->pts_misc = (int16_t) (self->pts_misc + 2 * self->game->train_wagons);
		self->pts_misc = (int16_t) (self->pts_misc - 2 * (self->game->train_wagons - self->game->train_wagons_start));
		self->pts_misc = (int16_t) (self->pts_misc +
			(self->pts_healthy + self->pts_fair + self->pts_poor) / self->game->party->member_count);
	}

	self->pts_year = (int16_t) (10 * (1861 - self->game->start_date->year));

	int32_t route = 4 * self->game->start_town + self->destination;

	self->pts_route = g_route_pts[route];

	if (g_route_days[route] < self->game->days_traveled) {
		self->pts_speed = (int16_t) -(self->game->days_traveled - g_route_days[route]);

	} else {
		self->pts_speed = 0;
	}

	self->multiplier = (double) self->game->score_multiplier / 10.0;
	self->raw_total = self->pts_cash + self->pts_speed + self->pts_wagons + self->pts_handcarts + self->pts_healthy +
		self->pts_animals + self->pts_goods + self->pts_fair + self->pts_unused_skill_points + self->pts_misc +
		self->pts_poor + self->pts_year + self->pts_route;
	self->final_total = (int32_t) ((double) self->raw_total * self->multiplier + 0.5);
	self->acres = (int16_t) (self->final_total / 20 + self->pts_year);
	self->score_per_day = (int16_t) (self->pts_year / 5 + self->final_total / 200);
	self->dollars = (int16_t) (20 * self->score_per_day);
	self->lucky_number = RandRangeAvg(100, 999, 1);

	self->arrival_date = MemDup(self->game->date, sizeof(struct date));
	DateSetDay(self->arrival_date, self->arrival_date->day + RandRangeAvg(30, 90, 1));
	self->arrival_text = StrRsrcGetCStr(month_names, self->arrival_date->month - 1);
	StrPrintf(date_buf, " %d, %d", self->arrival_date->day, self->arrival_date->year);
	CStrAppendBuf(self->arrival_text, date_buf);

	StrRsrcDestroy(&month_names);

	EndGameBuildEpilogue(self);
}

struct end_game *EndGameCreate(struct game *game)
{
	struct end_game *self = MemAlloc(1, sizeof(struct end_game));

	EndGameInit(self, game);

	return self;
}

void EndGameDestroy(struct end_game **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void end_game_destroy(struct object *self)
{
	struct end_game *e = (struct end_game *) self;

	MemFree(e->arrival_date);
	e->arrival_date = NULL;
	CStrDestroy(&e->arrival_text);
	StrRsrcDestroy(&e->epilogue);

	object_destroy(self);
}

void EndGameBuildEpilogue(struct end_game *self)
{
	struct cstr *tok_name = CStrCreateFrom("[Name]");
	struct cstr *tok_acres = CStrCreateFrom("[Acres]");
	struct cstr *tok_year = CStrCreateFrom("[Year]");
	struct cstr *tok_dollars = CStrCreateFrom("[Dollars]");

	struct member *leader_member = PartyGetMember(self->game->party, 0);
	struct cstr *leader = CStrCreateCopy(leader_member->name);
	struct cstr *s_year = CStrCreateInt(self->game->date->year, false);
	struct cstr *s_acres = CStrCreateInt(self->acres, true);
	struct cstr *s_dollars = CStrCreateInt(self->dollars, true);

	struct strrsrc *paragraphs = StrRsrcLoad(g_text_lib, 960, true);
	struct strrsrc *wordlists = StrRsrcLoad(g_text_lib, 961, true);

	self->epilogue = StrRsrcCreate();

	for (int32_t paragraph_index = 0; paragraph_index < 3; paragraph_index++) {
		struct cstr *paragraph = StrRsrcGetCStr(paragraphs,
			(int16_t) (RandRangeAvg(0, 4, 1) + 15 * self->destination + 5 * paragraph_index));

		if (!paragraph)
			paragraph = CStrCreate();

		CStrReplaceObjs(paragraph, tok_name, leader);
		CStrReplaceObjs(paragraph, tok_acres, s_acres);
		CStrReplaceObjs(paragraph, tok_year, s_year);
		CStrReplaceObjs(paragraph, tok_dollars, s_dollars);

		for (int32_t word_slot = 1; word_slot <= 8; word_slot++) {
			struct cstr *word = CStrCreate();
			struct cstr *key = CStrCreate();
			int32_t word_count = 0;
			char key_buf[12];

			struct cstr *word_list = StrRsrcGetCStr(wordlists, word_slot);

			if (!word_list)
				word_list = CStrCreate();

			CStrGetWord2(word_list, 2, word, 0);
			CStrToInt(word, &word_count);
			StrPrintf(key_buf, "[%d]", word_slot);
			CStrAssignBuf(key, key_buf);
			CStrGetWord2(word_list, RandRangeAvg(1, word_count, 1) + 2, word, 0);
			CStrReplaceObjs(paragraph, key, word);
			CStrDestroy(&word_list);
			CStrDestroy(&key);
			CStrDestroy(&word);
		}

		CStrMacRomanToAnsi(paragraph, 0);
		StrRsrcAppend(self->epilogue, CStrGet(paragraph));
		CStrDestroy(&paragraph);
	}

	StrRsrcDestroy(&wordlists);
	StrRsrcDestroy(&paragraphs);

	CStrDestroy(&s_dollars);
	CStrDestroy(&s_acres);
	CStrDestroy(&s_year);
	CStrDestroy(&leader);
	CStrDestroy(&tok_dollars);
	CStrDestroy(&tok_year);
	CStrDestroy(&tok_acres);
	CStrDestroy(&tok_name);
}

int32_t end_game_get_result_mode(struct end_game *self)
{
	return self->result_mode;
}
