#pragma once

#ifdef ONLINE_SCORES

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct os_score {
	char name[32];
	int32_t score;
	int32_t level;
	int32_t occupation;
	int32_t destination;
	int32_t year, month, day;
	int32_t days;
	int32_t survivors, party;
	uint32_t seed;
	char version[16];
};

void OsSubmitAsync(const struct os_score *score);

int32_t OsFetchTop(char names[10][32], int32_t scores[10], int32_t timeout_ms);

bool OsBuildSubmission(const struct os_score *s, char *body, size_t size);

int32_t OsParseTop(char *resp, char names[10][32], int32_t scores[10]);

void OsLog(const char *fmt, ...);
void OsCp1252ToUtf8(const char *in, char *out, int32_t size);
void OsUtf8ToCp1252(const char *in, char *out, int32_t size);
bool OsRandom16(uint8_t out[16]);
bool OsHmacSha256Hex(const char *key, const char *msg, char *out);

void OsToHex(const uint8_t *bytes, int32_t n, char *out);

#endif
