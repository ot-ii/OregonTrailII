#include "twitchirc.h"

#if defined(OTII_TWITCH) || defined(TWITCH_PARTY)

#include <stdlib.h>
#include <string.h>

#include "../base/str.h"

#define TW_QUEUE 256
#define TW_OUT_QUEUE 32
#define TW_PARTY_QUEUE 256
#define TW_CHATTERS 16
#define TW_VIEWERS 512

struct viewer {
	char login[64];
	int32_t count;
};

static char g_channel[64];
static char g_party_reward[64];
static uint32_t g_received;
static uint32_t g_dropped;

static struct twitch_message g_queue[TW_QUEUE];
static int32_t g_q_head;
static int32_t g_q_count;
static char g_out[TW_OUT_QUEUE][512];
static int32_t g_out_head;
static int32_t g_out_count;
static struct twitch_message g_party[TW_PARTY_QUEUE];
static bool g_party_picked[TW_PARTY_QUEUE];
static int32_t g_party_count;
static uint32_t g_party_made_ids;
static struct viewer g_viewers[TW_VIEWERS];
static int32_t g_viewer_count;
static struct twitch_message g_chatters[TW_CHATTERS];
static int32_t g_chatter_count;

static void tag_unescape(char *dst, size_t size, const char *src, size_t len)
{
	size_t o = 0;

	for (size_t i = 0; i < len && o + 1 < size; ++i) {
		char ch = src[i];

		if (ch == '\\' && i + 1 < len) {
			char e = src[++i];
			ch = e == 's' ? ' ' : e == ':' ? ';' : e == 'r' ? '\r' : e == 'n' ? '\n' : e;
		}

		dst[o++] = ch;
	}

	dst[o] = 0;
}

static void copy_n(char *dst, size_t size, const char *src, size_t len)
{
	if (len >= size)
		len = size - 1;

	memcpy(dst, src, len);
	dst[len] = 0;
}

static bool starts_no_case(const char *s, const char *prefix)
{
	for (; *prefix; ++s, ++prefix) {
		char a = *s;
		char b = *prefix;

		if (a >= 'A' && a <= 'Z')
			a = (char) (a - 'A' + 'a');

		if (a != b)
			return false;
	}

	return true;
}

bool TwitchIrcParseLine(const char *line, struct twitch_message *m, char *first_param, size_t first_size)
{
	const char *p = line;
	const char *e;

	memset(m, 0, sizeof(*m));
	first_param[0] = 0;

	if (*p == '@') {
		const char *tags = p + 1;
		e = strchr(p, ' ');
		if (!e)
			return false;

		while (tags < e) {
			const char *semi = memchr(tags, ';', (size_t) (e - tags));
			const char *end = semi ? semi : e;
			const char *eq = memchr(tags, '=', (size_t) (end - tags));
			size_t klen = (size_t) ((eq ? eq : end) - tags);
			const char *val = eq ? eq + 1 : "true";
			size_t vlen = eq ? (size_t) (end - val) : 4;
			char *dst = NULL;
			size_t dsize = 0;

			if (klen == 12 && !strncmp(tags, "display-name", 12)) {
				dst = m->display_name;
				dsize = sizeof(m->display_name);

			} else if (klen == 7 && !strncmp(tags, "user-id", 7)) {
				dst = m->user_id;
				dsize = sizeof(m->user_id);

			} else if (klen == 2 && !strncmp(tags, "id", 2)) {
				dst = m->id;
				dsize = sizeof(m->id);

			} else if (klen == 6 && !strncmp(tags, "msg-id", 6)) {
				dst = m->msg_id;
				dsize = sizeof(m->msg_id);

			} else if (klen == 16 && !strncmp(tags, "custom-reward-id", 16)) {
				dst = m->reward_id;
				dsize = sizeof(m->reward_id);

			} else if (klen == 4 && !strncmp(tags, "bits", 4)) {
				char num[16];
				tag_unescape(num, sizeof(num), val, vlen);
				m->bits = atoi(num);
			}

			if (dst)
				tag_unescape(dst, dsize, val, vlen);

			tags = end + 1;
		}

		p = e + 1;
	}

	while (*p == ' ')
		++p;

	if (*p == ':') {
		e = strchr(p, ' ');
		if (!e)
			return false;

		const char *bang = memchr(p, '!', (size_t) (e - p));
		copy_n(m->login, sizeof(m->login), p + 1, (size_t) ((bang ? bang : e) - (p + 1)));
		p = e + 1;

		while (*p == ' ')
			++p;
	}

	e = strchr(p, ' ');
	copy_n(m->command, sizeof(m->command), p, e ? (size_t) (e - p) : strlen(p));
	if (!m->command[0])
		return false;

	if (!e)
		return true;

	p = e + 1;

	while (*p) {
		while (*p == ' ')
			++p;

		if (*p == ':') {
			copy_n(m->text, sizeof(m->text), p + 1, strlen(p + 1));
			if (!first_param[0])
				copy_n(first_param, first_size, p + 1, strlen(p + 1));
			break;
		}

		e = strchr(p, ' ');
		if (!first_param[0])
			copy_n(first_param, first_size, p, e ? (size_t) (e - p) : strlen(p));

		if (!e) {
			copy_n(m->text, sizeof(m->text), p, strlen(p));
			break;
		}

		p = e + 1;
	}

	if (first_param[0] == '#')
		copy_n(m->channel, sizeof(m->channel), first_param, strlen(first_param));

	return true;
}

bool TwitchIrcParseServer(const char *spec, char *host, size_t host_size, char *port, size_t port_size, bool *tls)
{
	char h[256];
	char pt[16];
	const char *p = spec;
	const char *colon;
	bool secure = true;

	if (!spec || !*spec) {
		StrCopyN(host, host_size, TWITCH_HOST);
		StrCopyN(port, port_size, TWITCH_PORT);
		*tls = true;
		return true;
	}

	if (starts_no_case(p, "irc://")) {
		secure = false;
		p += 6;

	} else if (starts_no_case(p, "ircs://")) {
		p += 7;
	}

	colon = strrchr(p, ':');
	if (colon && strchr(p, ':') != colon)
		colon = NULL;

	if (p[0] == '[') {
		const char *close = strchr(p, ']');
		if (!close)
			return false;

		copy_n(h, sizeof(h), p + 1, (size_t) (close - p - 1));
		colon = close[1] == ':' ? close + 1 : NULL;

	} else {
		copy_n(h, sizeof(h), p, colon ? (size_t) (colon - p) : strlen(p));
	}

	StrCopy(pt, secure ? TWITCH_PORT : "6667");

	if (colon) {
		const char *d;

		for (d = colon + 1; *d >= '0' && *d <= '9'; ++d)
			;

		if (*d || d == colon + 1 || d - (colon + 1) > 5 || atoi(colon + 1) < 1 || atoi(colon + 1) > 65535)
			return false;

		copy_n(pt, sizeof(pt), colon + 1, strlen(colon + 1));
	}

	if (!h[0] || strpbrk(h, " \t\r\n/"))
		return false;

	if (!secure && !StrEqualNoCase(h, "127.0.0.1") && !StrEqualNoCase(h, "localhost") && strcmp(h, "::1")) {
		TwitchIrcLog("! server %s refused: plain TCP (irc://) only to this computer", spec);
		return false;
	}

	StrCopyN(host, host_size, h);
	StrCopyN(port, port_size, pt);
	*tls = secure;
	return true;
}

bool TwitchIrcSetChannel(const char *channel, const char *party_reward_id)
{
	int32_t n = 0;

	g_channel[n++] = '#';

	for (; *channel && n < (int32_t) sizeof(g_channel) - 1; ++channel) {
		char c = *channel;

		if (c >= 'A' && c <= 'Z')
			c = (char) (c - 'A' + 'a');

		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
			g_channel[n++] = c;
	}

	g_channel[n] = 0;
	if (n == 1)
		return false;

	StrCopy(g_party_reward, party_reward_id ? party_reward_id : "");
	return true;
}

const char *TwitchIrcChannel(void)
{
	return g_channel;
}

static bool not_a_chatter(const char *login)
{
	static const char *const BOTS[] = {"nightbot", "streamelements", "streamlabs", "moobot", "fossabot", "wizebot",
		"soundalerts", "sery_bot", "botrixoficial", "kofistreambot", "commanderroot", NULL};

	for (int32_t i = 0; BOTS[i]; ++i)
		if (StrEqualNoCase(login, BOTS[i]))
			return true;

	return StrEqualNoCase(login, g_channel + 1);
}

static void note_chatter(const struct twitch_message *m)
{
	int32_t i;

	if (!m->login[0] || not_a_chatter(m->login))
		return;

	for (i = 0; i < g_chatter_count && !StrEqualNoCase(g_chatters[i].login, m->login); ++i)
		;

	if (i == g_chatter_count) {
		if (g_chatter_count < TW_CHATTERS)
			++g_chatter_count;
		else
			i = TW_CHATTERS - 1;
	}

	memmove(g_chatters + 1, g_chatters, sizeof(g_chatters[0]) * (size_t) i);
	g_chatters[0] = *m;
}

void TwitchIrcEnqueue(const struct twitch_message *m)
{
	if (!TwitchIrcLock())
		return;

	if (g_q_count == TW_QUEUE) {
		g_q_head = (g_q_head + 1) % TW_QUEUE;
		--g_q_count;
		++g_dropped;
	}

	g_queue[(g_q_head + g_q_count) % TW_QUEUE] = *m;
	++g_q_count;
	++g_received;

	if (!strcmp(m->command, "PRIVMSG"))
		note_chatter(m);

	if (!strcmp(m->command, "PRIVMSG") && g_party_reward[0] && !strcmp(m->reward_id, g_party_reward)) {
		if (g_party_count < TW_PARTY_QUEUE) {
			struct twitch_message *r = &g_party[g_party_count];
			*r = *m;
			if (!r->id[0])
				StrPrintf(r->id, "local-%u", ++g_party_made_ids);

			g_party_picked[g_party_count++] = false;
			TwitchIrcLog("  party redemption from %s: \"%s\" (%d waiting)",
				m->display_name[0] ? m->display_name : m->login, m->text, g_party_count);

		} else {
			TwitchIrcLog("  party waiting list full: %s not added", m->display_name[0] ? m->display_name : m->login);
		}
	}

	TwitchIrcUnlock();
}

bool TwitchIrcTakeOut(char *out, size_t size)
{
	bool have = false;

	if (!TwitchIrcLock())
		return false;

	if (g_out_count) {
		StrCopyN(out, size, g_out[g_out_head]);
		g_out_head = (g_out_head + 1) % TW_OUT_QUEUE;
		--g_out_count;
		have = true;
	}

	TwitchIrcUnlock();
	return have;
}

void TwitchIrcCounts(uint32_t *received, uint32_t *dropped)
{
	*received = g_received;
	*dropped = g_dropped;
}

bool TwitchPoll(struct twitch_message *out)
{
	bool got = false;

	if (!TwitchIrcLock())
		return false;

	if (g_q_count) {
		*out = g_queue[g_q_head];
		g_q_head = (g_q_head + 1) % TW_QUEUE;
		--g_q_count;
		got = true;
	}

	TwitchIrcUnlock();
	return got;
}

bool TwitchSendRaw(const char *line)
{
	if (!line || !TwitchIrcConnected())
		return false;

	size_t n = strlen(line);
	if (!n || n > 510 || strpbrk(line, "\r\n"))
		return false;

	if (!TwitchIrcLock())
		return false;

	if (g_out_count == TW_OUT_QUEUE) {
		TwitchIrcUnlock();
		return false;
	}

	memcpy(g_out[(g_out_head + g_out_count) % TW_OUT_QUEUE], line, n + 1);
	++g_out_count;
	TwitchIrcUnlock();
	return true;
}

bool TwitchSendMessage(const char *text)
{
	TwitchIrcLog("  not sent (read-only: anonymous login): %.200s", text ? text : "");
	return false;
}

int32_t TwitchPartyPending(void)
{
	if (!TwitchIrcLock())
		return 0;

	int32_t n = g_party_count;
	TwitchIrcUnlock();
	return n;
}

bool TwitchPartyPeek(int32_t index, struct twitch_message *out)
{
	bool got = false;

	if (index < 0 || !TwitchIrcLock())
		return false;

	if (index < g_party_count) {
		*out = g_party[index];
		got = true;
	}

	TwitchIrcUnlock();
	return got;
}

void TwitchPartyTake(int32_t count)
{
	if (count <= 0 || !TwitchIrcLock())
		return;

	if (count > g_party_count)
		count = g_party_count;

	memmove(g_party, g_party + count, sizeof(g_party[0]) * (size_t) (g_party_count - count));
	g_party_count -= count;
	int32_t left = g_party_count;
	TwitchIrcUnlock();

	memmove(g_party_picked, g_party_picked + count, sizeof(g_party_picked[0]) * (size_t) left);
	TwitchIrcLog("  %d redeemer(s) joined a party, %d still waiting", count, left);
}

bool TwitchPartyPicked(int32_t index)
{
	bool picked = false;

	if (index < 0 || !TwitchIrcLock())
		return false;

	if (index < g_party_count)
		picked = g_party_picked[index];

	TwitchIrcUnlock();
	return picked;
}

bool TwitchPartyMarkPicked(const char *id)
{
	bool found = false;

	if (!id || !id[0] || !TwitchIrcLock())
		return false;

	for (int32_t i = 0; i < g_party_count && !found; ++i) {
		if (strcmp(g_party[i].id, id))
			continue;

		found = true;

		if (!g_party_picked[i]) {
			int32_t v;

			g_party_picked[i] = true;

			for (v = 0; v < g_viewer_count && !StrEqualNoCase(g_viewers[v].login, g_party[i].login); ++v)
				;

			if (v == g_viewer_count && g_viewer_count < TW_VIEWERS) {
				StrCopy(g_viewers[v].login, g_party[i].login);
				g_viewers[v].count = 0;
				++g_viewer_count;
			}

			if (v < g_viewer_count)
				++g_viewers[v].count;
		}
	}

	TwitchIrcUnlock();
	return found;
}

int32_t TwitchPartyRemovePicked(void)
{
	int32_t kept = 0;

	if (!TwitchIrcLock())
		return 0;

	for (int32_t i = 0; i < g_party_count; ++i) {
		if (!g_party_picked[i]) {
			if (kept != i)
				g_party[kept] = g_party[i];

			g_party_picked[kept++] = false;
		}
	}

	int32_t removed = g_party_count - kept;
	g_party_count = kept;
	TwitchIrcUnlock();

	if (removed)
		TwitchIrcLog("  %d picked redemption(s) removed, %d still waiting", removed, kept);

	return removed;
}

int32_t TwitchPartyPlayCount(const char *login)
{
	int32_t n = 0;

	if (!login || !TwitchIrcLock())
		return 0;

	for (int32_t v = 0; v < g_viewer_count; ++v)
		if (StrEqualNoCase(g_viewers[v].login, login))
			n = g_viewers[v].count;

	TwitchIrcUnlock();
	return n;
}

bool TwitchChatterPeek(int32_t index, struct twitch_message *out)
{
	bool got = false;

	if (index < 0 || !TwitchIrcLock())
		return false;

	if (index < g_chatter_count) {
		*out = g_chatters[index];
		got = true;
	}

	TwitchIrcUnlock();
	return got;
}

#ifdef OTII_TEST
void TwitchTestReceive(const char *line, const char *party_reward_id)
{
	struct twitch_message m;
	char first[128];

	TwitchIrcInitLock();
	if (party_reward_id)
		StrCopy(g_party_reward, party_reward_id);
	if (TwitchIrcParseLine(line, &m, first, sizeof(first)))
		TwitchIrcEnqueue(&m);
}

void TwitchTestReset(void)
{
	TwitchIrcInitLock();
	TwitchIrcLock();
	g_party_count = g_viewer_count = g_q_count = g_q_head = g_chatter_count = 0;
	TwitchIrcUnlock();
}
#endif

#endif
