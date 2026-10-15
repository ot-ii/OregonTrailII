#pragma once

#include <stdbool.h>
#include <stdint.h>

struct hill_situation {
	int32_t method;
	bool uphill;
	int32_t steepness;
	bool conditions_checked;
	bool strained;
	bool muddy_or_snow;
	bool conestoga;
};

int32_t HillSuccessChance(const struct hill_situation *s);
