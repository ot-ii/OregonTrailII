#pragma once

#include <stdbool.h>
#include <stdint.h>

struct mov_player;

struct mov_player *MovPlayerCreate(const char *path, void *parent, void *instance);
void MovPlayerDestroy(struct mov_player **ctx_mod);
void *MovPlayerHwnd(const struct mov_player *ctx);
void MovPlayerPlay(struct mov_player *ctx);
void MovPlayerPause(struct mov_player *ctx);
int32_t MovPlayerPosition(const struct mov_player *ctx);
bool MovPlayerPlaying(const struct mov_player *ctx);
void MovPlayerTick(struct mov_player *ctx);
bool MovPlayerSound(struct mov_player *ctx, uint8_t **pcm, int32_t *bytes, int32_t *rate, int32_t *channels,
	int32_t *bits);
