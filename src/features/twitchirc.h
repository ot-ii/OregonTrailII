#pragma once

#if defined(OTII_TWITCH) || defined(TWITCH_PARTY)

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct twitch_message {
	char command[32];
	char channel[64];
	char login[64];
	char display_name[64];
	char user_id[32];
	char id[64];
	char msg_id[64];
	char reward_id[64];
	int32_t bits;
	char text[512];
};

bool TwitchSetServer(const char *spec);

bool TwitchStart(const char *channel, const char *party_reward_id);

void TwitchStop(void);

bool TwitchPoll(struct twitch_message *out);

int32_t TwitchStatus(char *buf, int32_t size);

bool TwitchSendRaw(const char *line);

bool TwitchSendMessage(const char *text);

int32_t TwitchPartyPending(void);
bool TwitchPartyPeek(int32_t index, struct twitch_message *out);
void TwitchPartyTake(int32_t count);

bool TwitchPartyPicked(int32_t index);
bool TwitchPartyMarkPicked(const char *id);
int32_t TwitchPartyRemovePicked(void);
int32_t TwitchPartyPlayCount(const char *login);

bool TwitchChatterPeek(int32_t index, struct twitch_message *out);

#define TWITCH_HOST "irc.chat.twitch.tv"
#define TWITCH_PORT "6697"

bool TwitchIrcLock(void);
void TwitchIrcUnlock(void);

#ifdef OTII_TEST
void TwitchIrcInitLock(void);
void TwitchTestReceive(const char *line, const char *party_reward_id);
void TwitchTestReset(void);
#endif

void TwitchIrcLog(const char *fmt, ...);

bool TwitchIrcConnected(void);

bool TwitchIrcParseLine(const char *line, struct twitch_message *m, char *first_param, size_t first_size);

bool TwitchIrcParseServer(const char *spec, char *host, size_t host_size, char *port, size_t port_size, bool *tls);

bool TwitchIrcSetChannel(const char *channel, const char *party_reward_id);
const char *TwitchIrcChannel(void);

void TwitchIrcEnqueue(const struct twitch_message *m);

bool TwitchIrcTakeOut(char *out, size_t size);

void TwitchIrcCounts(uint32_t *received, uint32_t *dropped);

#endif
