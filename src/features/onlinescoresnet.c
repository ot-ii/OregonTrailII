#include "onlinescoresnet.h"

#ifdef ONLINE_SCORES

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "onlinescoresconfig.h"
#include "../base/str.h"

static void form_add(char *out, size_t size, const char *key, const char *value)
{
	static const char HEX[] = "0123456789ABCDEF";
	size_t len = strlen(out);

	StrPrintfN(out + len, size - len, "%s%s=", len ? "&" : "", key);
	len = strlen(out);

	for (const uint8_t *p = (const uint8_t *) value; *p && len + 4 < size; ++p) {
		if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-' ||
			*p == '_' || *p == '.' || *p == '~') {
			out[len++] = (char) *p;

		} else {
			out[len++] = '%';
			out[len++] = HEX[*p >> 4];
			out[len++] = HEX[*p & 15];
		}
	}

	out[len] = 0;
}

void OsToHex(const uint8_t *bytes, int32_t n, char *out)
{
	static const char HEX[] = "0123456789abcdef";

	for (int32_t i = 0; i < n; ++i) {
		out[2 * i] = HEX[bytes[i] >> 4];
		out[2 * i + 1] = HEX[bytes[i] & 15];
	}

	out[2 * n] = 0;
}

bool OsBuildSubmission(const struct os_score *s, char *body, size_t size)
{
	static const char *const KEYS[13] = {"name", "score", "level", "occupation", "destination", "arrived", "days",
		"survivors", "party", "seed", "version", "ts", "nonce"};
	char name[128];
	char fields[13][128];
	char message[2048];
	char sig[65];
	uint8_t random[16];

	OsCp1252ToUtf8(s->name, name, sizeof(name));
	StrCopy(fields[0], name);
	StrPrintf(fields[1], "%d", s->score);
	StrPrintf(fields[2], "%d", s->level);
	StrPrintf(fields[3], "%d", s->occupation);
	StrPrintf(fields[4], "%d", s->destination);
	StrPrintf(fields[5], "%04d-%02d-%02d", s->year, s->month, s->day);
	StrPrintf(fields[6], "%d", s->days);
	StrPrintf(fields[7], "%d", s->survivors);
	StrPrintf(fields[8], "%d", s->party);
	StrPrintf(fields[9], "%u", s->seed);
	StrCopy(fields[10], s->version);
	StrPrintf(fields[11], "%" PRId64, (int64_t) time(NULL));

	if (!OsRandom16(random)) {
		OsLog("submit: no random nonce");
		return false;
	}

	OsToHex(random, 16, fields[12]);

	message[0] = 0;
	body[0] = 0;
	form_add(body, size, "action", "submit");

	for (int32_t i = 0; i < 13; ++i) {
		size_t len = strlen(message);
		StrPrintfN(message + len, sizeof(message) - len, "%s%s", i ? "\n" : "", fields[i]);
		form_add(body, size, KEYS[i], fields[i]);
	}

	if (!OsHmacSha256Hex(ONLINE_SCORES_SECRET, message, sig)) {
		OsLog("submit: signing failed");
		return false;
	}

	form_add(body, size, "sig", sig);
	return true;
}

int32_t OsParseTop(char *resp, char names[10][32], int32_t scores[10])
{
	char *next;
	int32_t count = 0;

	for (char *line = resp; line && *line && count < 10; line = next) {
		char *end;

		next = strchr(line, '\n');
		if (next)
			*next++ = 0;

		char *tab = strchr(line, '\t');
		if (!tab)
			continue;

		*tab = 0;
		int32_t score = (int32_t) strtol(line, &end, 10);
		if (end == line || *end)
			continue;

		end = tab + 1 + strlen(tab + 1);
		while (end > tab + 1 && (end[-1] == '\r' || end[-1] == ' '))
			*--end = 0;

		OsUtf8ToCp1252(tab + 1, names[count], 32);
		scores[count] = score;
		++count;
	}

	return count;
}

#endif
