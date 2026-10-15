#include "hillrules.h"

#include "../game/game.h"

static int32_t by_steepness(int32_t steepness, int32_t s1, int32_t s2, int32_t s3, int32_t s4, int32_t s5)
{
	switch (steepness) {
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

int32_t HillSuccessChance(const struct hill_situation *s)
{
	int32_t chance = 0;

	if (s->uphill) {
		switch (s->method) {
			case NODE_CMD_GO_UP:
				chance = by_steepness(s->steepness, 97, 75, 50, 0, 0);
				break;
			case NODE_CMD_DOUBLE_TEAM:
				chance = by_steepness(s->steepness, 97, 92, 75, 50, 0);
				break;
			case NODE_CMD_ROPES_AND_CHAINS:
				chance = by_steepness(s->steepness, 75, 75, 97, 92, 85);
				break;
		}

	} else {
		switch (s->method) {
			case NODE_CMD_GO_DOWN:
				chance = by_steepness(s->steepness, 97, 75, 50, 0, 0);
				break;
			case NODE_CMD_LOCK_WHEELS:
				chance = by_steepness(s->steepness, 97, 92, 75, 50, 0);
				break;
			case NODE_CMD_ANCHOR_WAGON:
				chance = by_steepness(s->steepness, 97, 92, 87, 75, 50);
				break;
			case NODE_CMD_ROPES_AND_CHAINS:
				chance = by_steepness(s->steepness, 75, 75, 75, 95, 90);
				break;
		}
	}
	if (!s->conditions_checked)
		chance -= 10;
	if (s->strained)
		chance -= 25;
	if (s->muddy_or_snow)
		chance -= 25;
	if (s->conestoga)
		chance -= 15;
	return chance;
}
