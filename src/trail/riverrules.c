#include "riverrules.h"

#include "../game/game.h"

static int32_t by_swiftness(int32_t swiftness, int32_t s1, int32_t s2, int32_t s3, int32_t s4, int32_t s5)
{
	switch (swiftness) {
		case 1:
			return s1;
		case 2:
			return s2;
		case 3:
			return s3;
		case 4:
			return s4;
		case 5:
			return s5;
		default:
			return 0;
	}
}

int32_t RiverGuideMethod(int32_t depth)
{
	return depth <= 24 ? NODE_CMD_FORD : NODE_CMD_CAULK_AND_FLOAT;
}

int32_t RiverSuccessChance(const struct river_situation *s)
{
	int32_t chance = 100;
	int32_t loss;

	switch (s->method) {
		case NODE_CMD_CAULK_AND_FLOAT:
			if (s->depth <= 24)
				chance = 0;
			else if (s->depth <= 30)
				chance = by_swiftness(s->swiftness, 99, 95, 85, 75, 50);
			else
				chance = by_swiftness(s->swiftness, 99, 97, 95, 85, 75);
			break;
		case NODE_CMD_FORD:
			if (s->depth <= 24)
				chance = by_swiftness(s->swiftness, 99, 97, 95, 85, 75);
			else if (s->depth <= 30)
				chance = by_swiftness(s->swiftness, 97, 95, 85, 75, 50);
			else
				chance = 0;
			break;
		case NODE_CMD_FERRY:
			chance = by_swiftness(s->swiftness, 99, 99, 95, 90, 85);
			break;
		case NODE_CMD_CROSS_ICE:
			loss = (int16_t) (s->ice_weight / 4000);
			loss = (int16_t) (loss + s->width / 300);
			if (s->ice_long_freeze)
				loss /= 2;
			if (s->ice_high_temp > 32)
				loss = (int16_t) (loss + s->ice_high_temp - 32);
			chance = (int16_t) (100 - loss);
			break;
	}
	if (s->method != NODE_CMD_FERRY && !s->conditions_checked)
		chance -= 10;
	if (s->auto_pick)
		chance += (100 - chance) / 2;
	if (s->method == NODE_CMD_FORD || s->method == NODE_CMD_CAULK_AND_FLOAT) {
		if (s->raining)
			chance -= 5;
		if (s->strained)
			chance -= 15;
		if (s->conestoga)
			chance -= 10;
	}
	if (s->riverwork)
		chance += (100 - chance) / 2;
	return (int16_t) chance;
}

void RiverFailure(int32_t method, int32_t depth, int32_t swiftness, int32_t *outcome_b, int32_t *outcome_a,
	int32_t *loss_pct)
{
	*outcome_b = *outcome_a = CROSS_OUTCOME_TIPPED;
	*loss_pct = 100;
	switch (method) {
		case NODE_CMD_FORD:
			if (depth <= 24) {
				*outcome_b = CROSS_OUTCOME_STUCK;
				*outcome_a = CROSS_OUTCOME_TIPPED;
				*loss_pct = by_swiftness(swiftness, 100, 100, 75, 50, 25);

			} else if (depth <= 30) {
				switch (swiftness) {
					case 1:
						*outcome_b = CROSS_OUTCOME_STUCK;
						*outcome_a = CROSS_OUTCOME_SWAMPED;
						*loss_pct = 75;
						break;
					case 2:
						*outcome_b = CROSS_OUTCOME_STUCK;
						*outcome_a = CROSS_OUTCOME_SWAMPED;
						*loss_pct = 50;
						break;
					case 3:
						*outcome_b = CROSS_OUTCOME_SWAMPED;
						*outcome_a = CROSS_OUTCOME_TIPPED;
						*loss_pct = 50;
						break;
					case 4:
						*outcome_b = CROSS_OUTCOME_SWAMPED;
						*outcome_a = CROSS_OUTCOME_TIPPED;
						*loss_pct = 25;
						break;
					default:
						*outcome_b = *outcome_a = CROSS_OUTCOME_TIPPED;
						*loss_pct = 0;
						break;
				}

			} else {
				*outcome_b = CROSS_OUTCOME_SWAMPED_DEEP;
				*outcome_a = CROSS_OUTCOME_TIPPED_DEEP;
				*loss_pct = by_swiftness(swiftness, 100, 100, 50, 25, 0);
			}
			break;
		case NODE_CMD_CAULK_AND_FLOAT:
		case NODE_CMD_FERRY:
			if (depth <= 24) {
				*outcome_b = CROSS_OUTCOME_STUCK;
				*outcome_a = CROSS_OUTCOME_TIPPED;
				*loss_pct = by_swiftness(swiftness, 100, 100, 75, 50, 25);

			} else if (depth <= 30) {
				switch (swiftness) {
					case 1:
						*outcome_b = *outcome_a = CROSS_OUTCOME_STUCK;
						*loss_pct = 100;
						break;
					case 2:
						*outcome_b = CROSS_OUTCOME_STUCK;
						*outcome_a = CROSS_OUTCOME_TIPPED;
						*loss_pct = 75;
						break;
					case 3:
						*outcome_b = CROSS_OUTCOME_STUCK;
						*outcome_a = CROSS_OUTCOME_TIPPED;
						*loss_pct = 50;
						break;
					case 4:
						*outcome_b = CROSS_OUTCOME_STUCK;
						*outcome_a = CROSS_OUTCOME_TIPPED;
						*loss_pct = 25;
						break;
					default:
						*outcome_b = *outcome_a = CROSS_OUTCOME_TIPPED;
						*loss_pct = 0;
						break;
				}

			} else {
				*outcome_b = *outcome_a = CROSS_OUTCOME_TIPPED_DEEP;
				*loss_pct = 100;
			}
			break;
		case NODE_CMD_CROSS_ICE:
			*loss_pct = 100;
			*outcome_b = *outcome_a = CROSS_OUTCOME_THROUGH_ICE;
			break;
	}
}
