#pragma once

#include <stdbool.h>
#include <stdint.h>

struct river_situation {
	int32_t method;
	int32_t depth;
	int32_t width;
	int32_t swiftness;
	bool auto_pick;
	bool conditions_checked;
	bool raining;
	bool strained;
	bool conestoga;
	bool riverwork;
	int32_t ice_weight;
	bool ice_long_freeze;
	int32_t ice_high_temp;
};

int32_t RiverSuccessChance(const struct river_situation *s);
void RiverFailure(int32_t method, int32_t depth, int32_t swiftness, int32_t *outcome_b, int32_t *outcome_a,
	int32_t *loss_pct);
int32_t RiverGuideMethod(int32_t depth);
