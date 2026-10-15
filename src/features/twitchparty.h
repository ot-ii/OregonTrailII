#pragma once

#ifdef TWITCH_PARTY

#include <stdbool.h>

void TwitchPartyStart(void);
void TwitchPartyStop(void);

#define TWITCH_PARTY_MENU_ID 6092
void TwitchPartyShowWindow(void);
void TwitchPartyMenuUpdate(void *popup);

bool TwitchPartyConnect(const char *channel, const char *reward_id, const char *server);
void TwitchPartyDisconnect(void);
void *TwitchPartyWindow(void);
bool TwitchPartySettings(const char *channel, const char *reward_id, const char *server);

#define TWITCH_PARTY_LIST 201
#define TWITCH_PARTY_PICK 202
#define TWITCH_PARTY_REMOVE 203
#define TWITCH_PARTY_STATUS 204

#endif
