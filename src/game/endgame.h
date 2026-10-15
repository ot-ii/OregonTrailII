#pragma once

#include <stdint.h>

#include "../base/object.h"

struct cstr;
struct date;
struct end_game;
struct game;
struct strrsrc;

struct end_game_ops {
	struct object_ops base;
	int32_t (*GetResultMode)(struct end_game *self);
};

struct end_game {
	struct object base;
	struct game *game;
	int32_t destination;
	int32_t result_mode;
	int32_t acres;
	int32_t score_per_day;
	int32_t dollars;
	int32_t lucky_number;
	int32_t pts_healthy;
	int32_t n_healthy;
	int32_t pts_fair;
	int32_t n_fair;
	int32_t pts_poor;
	int32_t n_poor;
	int32_t pts_cash;
	int32_t pts_wagons;
	int32_t pts_handcarts;
	int32_t pts_animals;
	int32_t pts_goods;
	int32_t pts_unused_skill_points;
	int32_t pts_misc;
	int32_t pts_year;
	int32_t pts_route;
	int32_t pts_speed;
	double multiplier;
	int32_t raw_total;
	int32_t final_total;
	struct date *arrival_date;
	struct cstr *arrival_text;
	struct strrsrc *epilogue;
};

#define END_GAME_OPS(p) ((const struct end_game_ops *) ((const struct object *) (p))->ops)

extern const struct end_game_ops END_GAME_OPS_TABLE;

extern int32_t g_route_pts[16];
extern int32_t g_route_days[16];

void EndGameInit(struct end_game *self, struct game *game);
struct end_game *EndGameCreate(struct game *game);
void EndGameDestroy(struct end_game **ctx_mod);
void end_game_destroy(struct object *self);

void EndGameBuildEpilogue(struct end_game *self);

int32_t end_game_get_result_mode(struct end_game *self);
