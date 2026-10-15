#pragma once

#ifdef OTII_TWITCH

#include <stdbool.h>
#include <stdint.h>

#define TWITCH_OPTIONS_MENU_ID 6096

void TwitchControlStart(void);
void TwitchControlStop(void);
void TwitchControlToggleWindow(void);

int32_t TwitchControlNewGameDifficulty(void);
void TwitchControlScoreReached(void);

int32_t TwitchControlRecentChat(char (*lines)[640], int32_t max);

int32_t TwitchControlLastPicks(char (*lines)[256], int32_t max);

#ifdef OTII_TEST
bool TwitchControlTestConnect(const char *channel, const char *party_reward_id, const char *server);
void TwitchControlTestDisconnect(void);
const char *TwitchControlTestOptionsText(void);
#endif

#endif
