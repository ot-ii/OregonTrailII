#pragma once

#ifdef ONLINE_SCORES

#include <stdint.h>

struct cstr;
struct game;

void OnlineScoresSubmitGame(struct game *game, const struct cstr *leader, int32_t score);

int32_t OnlineScoresTop10(char names[10][32], int32_t scores[10]);

#endif
