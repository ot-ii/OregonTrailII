#pragma once

#include <stdint.h>

struct strrsrc;

struct new_game_scenario {
	int32_t occupation;
	int32_t score_multiplier;
	int32_t cash;
	int32_t start_month;
	int32_t start_year;
	int32_t start_node;
	int32_t destination;
	int32_t player_level;
	struct strrsrc *names;
	int32_t skills[15];
	int32_t unused_skill_points;
	int32_t wagon_type;
	int32_t ages[6];
};
