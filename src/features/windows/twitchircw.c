//

#include "../twitchirc.h"

#if defined(OTII_TWITCH) || defined(TWITCH_PARTY)

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define SECURITY_WIN32
#include <security.h>
#include <schannel.h>

#include "../../base/str.h"

#define TW_WATCHDOG_MS (6 * 60 * 1000)
#define TW_MAX_LINE 8192

enum {
	TW_STOPPED,
	TW_CONNECTING,
	TW_CONNECTED,
	TW_WAITING,
};

static CRITICAL_SECTION g_cs;
static volatile LONG g_cs_ready;
static HANDLE g_thread;
static volatile LONG g_stop;
static volatile LONG g_state = TW_STOPPED;
static char g_nick[32];
static uint32_t g_reconnects;
static int32_t g_wait_seconds;
static char g_host[256] = TWITCH_HOST;
static char g_port[16] = TWITCH_PORT;
static bool g_tls = true;

bool TwitchIrcLock(void)
{
	if (!g_cs_ready)
		return false;

	EnterCriticalSection(&g_cs);
	return true;
}

void TwitchIrcUnlock(void)
{
	LeaveCriticalSection(&g_cs);
}

#ifdef OTII_TEST
void TwitchIrcInitLock(void)
{
	if (!g_cs_ready) {
		InitializeCriticalSection(&g_cs);
		g_cs_ready = 1;
	}
}
#endif

bool TwitchIrcConnected(void)
{
	return g_state == TW_CONNECTED;
}

void TwitchIrcLog(const char *fmt, ...)
{
	char path[MAX_PATH];
	FILE *f = NULL;
	va_list a;
	SYSTEMTIME now;

	if (!GetModuleFileNameA(NULL, path, MAX_PATH))
		return;

	path[MAX_PATH - 1] = 0;
	char *dot = strrchr(path, '.');
	if (dot)
		*dot = 0;

	StrCat(path, "_twitch.txt");
	if (fopen_s(&f, path, "a") || !f)
		return;

	GetLocalTime(&now);
	fprintf(f, "%02d:%02d:%02d ", now.wHour, now.wMinute, now.wSecond);
	va_start(a, fmt);
	vfprintf(f, fmt, a);
	va_end(a);
	fputc('\n', f);
	fclose(f);
}

static uint32_t tw_random(void)
{
	static uint32_t state;

	if (!state) {
		LARGE_INTEGER t;
		QueryPerformanceCounter(&t);
		state = (uint32_t) t.LowPart ^ (uint32_t) t.HighPart ^ GetCurrentProcessId() ^ 0x9E3779B9u;
		if (!state)
			state = 1;
	}

	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return state;
}

struct tls_conn {
	SOCKET s;
	CredHandle cred;
	CtxtHandle ctx;
	bool have_cred;
	bool have_ctx;
	SecPkgContext_StreamSizes sizes;
	char in[16384 + 2048];
	int32_t in_len;
	char plain[32768];
	int32_t plain_len;
};

static bool send_all(SOCKET s, const char *p, int32_t n)
{
	while (n > 0) {
		int32_t k = send(s, p, n, 0);
		if (k <= 0)
			return false;

		p += k;
		n -= k;
	}

	return true;
}

static void tls_close(struct tls_conn *c)
{
	if (c->have_ctx)
		DeleteSecurityContext(&c->ctx);

	if (c->have_cred)
		FreeCredentialsHandle(&c->cred);

	if (c->s != INVALID_SOCKET)
		closesocket(c->s);

	c->have_ctx = false;
	c->have_cred = false;
	c->s = INVALID_SOCKET;
}

static int32_t wait_readable(SOCKET s, int32_t ms)
{
	fd_set r;
	struct timeval tv;

	FD_ZERO(&r);
	FD_SET(s, &r);
	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * 1000;
	return select(0, &r, NULL, NULL, &tv);
}

static bool tls_handshake(struct tls_conn *c)
{
	SCHANNEL_CRED sc;
	SECURITY_STATUS st;
	TimeStamp expiry;
	DWORD flags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY |
		ISC_REQ_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;
	DWORD out_flags;
	SecBuffer out_buf[1];
	SecBuffer in_buf[2];
	SecBufferDesc out_desc;
	SecBufferDesc in_desc;
	bool first = true;

	ZeroMemory(&sc, sizeof(sc));
	sc.dwVersion = SCHANNEL_CRED_VERSION;
	sc.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT;
	sc.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_USE_STRONG_CRYPTO | SCH_CRED_AUTO_CRED_VALIDATION;
	st = AcquireCredentialsHandleA(NULL, UNISP_NAME_A, SECPKG_CRED_OUTBOUND, NULL, &sc, NULL, NULL, &c->cred,
		&expiry);
	if (st != SEC_E_OK) {
		TwitchIrcLog("! TLS: AcquireCredentialsHandle failed 0x%08lX", (unsigned long) st);
		return false;
	}

	c->have_cred = true;
	c->in_len = 0;

	for (;;) {
		out_buf[0].BufferType = SECBUFFER_TOKEN;
		out_buf[0].pvBuffer = NULL;
		out_buf[0].cbBuffer = 0;
		out_desc.ulVersion = SECBUFFER_VERSION;
		out_desc.cBuffers = 1;
		out_desc.pBuffers = out_buf;
		in_buf[0].BufferType = SECBUFFER_TOKEN;
		in_buf[0].pvBuffer = c->in;
		in_buf[0].cbBuffer = (unsigned long) c->in_len;
		in_buf[1].BufferType = SECBUFFER_EMPTY;
		in_buf[1].pvBuffer = NULL;
		in_buf[1].cbBuffer = 0;
		in_desc.ulVersion = SECBUFFER_VERSION;
		in_desc.cBuffers = 2;
		in_desc.pBuffers = in_buf;
		st = InitializeSecurityContextA(&c->cred, first ? NULL : &c->ctx, g_host, flags, 0, 0,
			first ? NULL : &in_desc, 0, first ? &c->ctx : NULL, &out_desc, &out_flags, &expiry);
		if (first)
			c->have_ctx = true;

		first = false;

		if (out_buf[0].cbBuffer && out_buf[0].pvBuffer) {
			bool ok = send_all(c->s, (const char *) out_buf[0].pvBuffer, (int32_t) out_buf[0].cbBuffer);
			FreeContextBuffer(out_buf[0].pvBuffer);
			if (!ok)
				return false;
		}

		if (st == SEC_E_INCOMPLETE_MESSAGE) {
			if (c->in_len >= (int32_t) sizeof(c->in) || wait_readable(c->s, 15000) <= 0)
				return false;

			int32_t k = recv(c->s, c->in + c->in_len, (int32_t) sizeof(c->in) - c->in_len, 0);
			if (k <= 0)
				return false;

			c->in_len += k;
			continue;
		}

		if (in_buf[1].BufferType == SECBUFFER_EXTRA && in_buf[1].cbBuffer > 0 &&
			(st == SEC_E_OK || st == SEC_I_CONTINUE_NEEDED)) {
			memmove(c->in, c->in + c->in_len - in_buf[1].cbBuffer, in_buf[1].cbBuffer);
			c->in_len = (int32_t) in_buf[1].cbBuffer;

		} else if (st == SEC_E_OK || st == SEC_I_CONTINUE_NEEDED) {
			c->in_len = 0;
		}

		if (st == SEC_E_OK)
			break;

		if (st != SEC_I_CONTINUE_NEEDED) {
			TwitchIrcLog("! TLS: handshake failed 0x%08lX", (unsigned long) st);
			return false;
		}

		if (c->in_len == 0) {
			if (wait_readable(c->s, 15000) <= 0)
				return false;

			int32_t k = recv(c->s, c->in, (int32_t) sizeof(c->in), 0);
			if (k <= 0)
				return false;

			c->in_len = k;
		}
	}

	if (QueryContextAttributesA(&c->ctx, SECPKG_ATTR_STREAM_SIZES, &c->sizes) != SEC_E_OK)
		return false;

	c->plain_len = 0;
	return true;
}

static bool tls_send(struct tls_conn *c, const char *data, int32_t len)
{
	char buf[16384 + 512];
	SecBuffer b[4];
	SecBufferDesc d;

	if (len <= 0 || (unsigned long) len > c->sizes.cbMaximumMessage ||
		c->sizes.cbHeader + (unsigned long) len + c->sizes.cbTrailer > sizeof(buf))
		return false;

	memcpy(buf + c->sizes.cbHeader, data, (size_t) len);
	b[0].BufferType = SECBUFFER_STREAM_HEADER;
	b[0].pvBuffer = buf;
	b[0].cbBuffer = c->sizes.cbHeader;
	b[1].BufferType = SECBUFFER_DATA;
	b[1].pvBuffer = buf + c->sizes.cbHeader;
	b[1].cbBuffer = (unsigned long) len;
	b[2].BufferType = SECBUFFER_STREAM_TRAILER;
	b[2].pvBuffer = buf + c->sizes.cbHeader + len;
	b[2].cbBuffer = c->sizes.cbTrailer;
	b[3].BufferType = SECBUFFER_EMPTY;
	b[3].pvBuffer = NULL;
	b[3].cbBuffer = 0;
	d.ulVersion = SECBUFFER_VERSION;
	d.cBuffers = 4;
	d.pBuffers = b;

	if (EncryptMessage(&c->ctx, 0, &d, 0) != SEC_E_OK)
		return false;

	return send_all(c->s, buf, (int32_t) (b[0].cbBuffer + b[1].cbBuffer + b[2].cbBuffer));
}

static bool tls_pump(struct tls_conn *c)
{
	if ((int32_t) sizeof(c->in) - c->in_len <= 0)
		return false;

	int32_t k = recv(c->s, c->in + c->in_len, (int32_t) sizeof(c->in) - c->in_len, 0);
	if (k <= 0)
		return false;

	c->in_len += k;

	while (c->in_len > 0) {
		SecBuffer b[4];
		SecBufferDesc d;
		SECURITY_STATUS st;
		int32_t extra = 0;

		b[0].BufferType = SECBUFFER_DATA;
		b[0].pvBuffer = c->in;
		b[0].cbBuffer = (unsigned long) c->in_len;

		for (int32_t i = 1; i < 4; ++i) {
			b[i].BufferType = SECBUFFER_EMPTY;
			b[i].pvBuffer = NULL;
			b[i].cbBuffer = 0;
		}

		d.ulVersion = SECBUFFER_VERSION;
		d.cBuffers = 4;
		d.pBuffers = b;
		st = DecryptMessage(&c->ctx, &d, 0, NULL);
		if (st == SEC_E_INCOMPLETE_MESSAGE)
			return true;

		if (st == SEC_I_CONTEXT_EXPIRED)
			return false;

		if (st != SEC_E_OK && st != SEC_I_RENEGOTIATE) {
			TwitchIrcLog("! TLS: DecryptMessage failed 0x%08lX", (unsigned long) st);
			return false;
		}

		for (int32_t i = 1; i < 4; ++i) {
			if (b[i].BufferType == SECBUFFER_DATA && b[i].cbBuffer) {
				if (c->plain_len + (int32_t) b[i].cbBuffer > (int32_t) sizeof(c->plain))
					return false;

				memcpy(c->plain + c->plain_len, b[i].pvBuffer, b[i].cbBuffer);
				c->plain_len += (int32_t) b[i].cbBuffer;
			}

			if (b[i].BufferType == SECBUFFER_EXTRA && b[i].cbBuffer)
				extra = (int32_t) b[i].cbBuffer;
		}

		if (st == SEC_I_RENEGOTIATE) {
			TwitchIrcLog("! TLS: renegotiation requested -- reconnecting");
			return false;
		}

		if (extra) {
			memmove(c->in, c->in + c->in_len - extra, (size_t) extra);
			c->in_len = extra;

		} else {
			c->in_len = 0;
		}
	}

	return true;
}

static bool send_line(struct tls_conn *c, const char *line)
{
	char buf[520];

	if (!strncmp(line, "PASS ", 5))
		TwitchIrcLog("> PASS ***");
	else
		TwitchIrcLog("> %s", line);

	int32_t n = StrPrintf(buf, "%s\r\n", line);
	if (n >= (int32_t) sizeof(buf))
		n = (int32_t) sizeof(buf) - 1;

	return n > 0 && (g_tls ? tls_send(c, buf, n) : send_all(c->s, buf, n));
}

static bool plain_pump(struct tls_conn *c)
{
	if ((int32_t) sizeof(c->plain) - c->plain_len <= 0)
		return false;

	int32_t k = recv(c->s, c->plain + c->plain_len, (int32_t) sizeof(c->plain) - c->plain_len, 0);
	if (k <= 0)
		return false;

	c->plain_len += k;
	return true;
}

static void copy_line(char *dst, size_t size, const char *src, size_t len)
{
	if (len >= size)
		len = size - 1;

	memcpy(dst, src, len);
	dst[len] = 0;
}

static bool run_session(void)
{
	static struct tls_conn conn;
	static char raw[TW_MAX_LINE];
	struct tls_conn *c = &conn;
	struct addrinfo hints;
	struct addrinfo *res = NULL;
	char line[600];
	bool welcomed = false;
	uint32_t id = 1000 + tw_random() % 80000;

	memset(c, 0, sizeof(*c));
	c->s = INVALID_SOCKET;
	StrPrintf(g_nick, "justinfan%u", id);
	g_state = TW_CONNECTING;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (getaddrinfo(g_host, g_port, &hints, &res) || !res) {
		TwitchIrcLog("! cannot resolve %s", g_host);
		return false;
	}

	for (struct addrinfo *ai = res; ai && c->s == INVALID_SOCKET; ai = ai->ai_next) {
		c->s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (c->s != INVALID_SOCKET && connect(c->s, ai->ai_addr, (int) ai->ai_addrlen)) {
			closesocket(c->s);
			c->s = INVALID_SOCKET;
		}
	}

	freeaddrinfo(res);

	if (c->s == INVALID_SOCKET) {
		TwitchIrcLog("! cannot connect to %s:%s", g_host, g_port);
		return false;
	}

	if (!g_tls) {
		TwitchIrcLog("  connected to %s:%s (plain TCP: the local test server)", g_host, g_port);

	} else if (!tls_handshake(c)) {
		tls_close(c);
		return false;

	} else {
		TwitchIrcLog("  TLS established with %s:%s", g_host, g_port);
	}

	send_line(c, "CAP REQ :twitch.tv/tags twitch.tv/commands");
	send_line(c, "PASS SCHMOOPIE");
	StrPrintf(line, "NICK %s", g_nick);
	send_line(c, line);
	StrPrintf(line, "USER %s 8 * :%s", g_nick, g_nick);
	send_line(c, line);
	StrPrintf(line, "JOIN %s", TwitchIrcChannel());
	send_line(c, line);
	DWORD last_data = GetTickCount();

	while (!g_stop) {
		int32_t r = wait_readable(c->s, 250);
		char *eol;

		for (;;) {
			char out[512];

			if (!TwitchIrcTakeOut(out, sizeof(out)) || !send_line(c, out))
				break;
		}

		if (r < 0)
			break;

		if (r == 0) {
			if (GetTickCount() - last_data > TW_WATCHDOG_MS) {
				TwitchIrcLog("! nothing received for 6 minutes -- reconnecting");
				break;
			}

			continue;
		}

		if (!(g_tls ? tls_pump(c) : plain_pump(c))) {
			TwitchIrcLog("! connection closed");
			break;
		}

		last_data = GetTickCount();

		while ((eol = memchr(c->plain, '\n', (size_t) c->plain_len)) != NULL) {
			int32_t n = (int32_t) (eol - c->plain);
			struct twitch_message m;
			char first[128];

			copy_line(raw, sizeof(raw), c->plain, (size_t) (n > 0 && c->plain[n - 1] == '\r' ? n - 1 : n));
			memmove(c->plain, eol + 1, (size_t) (c->plain_len - n - 1));
			c->plain_len -= n + 1;
			if (!raw[0])
				continue;

			TwitchIrcLog("< %s", raw);
			if (!TwitchIrcParseLine(raw, &m, first, sizeof(first)))
				continue;

			if (!strcmp(m.command, "PING")) {
				StrPrintf(line, "PONG :%s", m.text);
				send_line(c, line);
				continue;
			}

			if (!strcmp(m.command, "001")) {
				welcomed = true;
				g_state = TW_CONNECTED;
			}

			if (!strcmp(m.command, "RECONNECT")) {
				TwitchIrcLog("  the server asked for a new connection");
				tls_close(c);
				g_state = TW_WAITING;
				return welcomed;
			}

			TwitchIrcEnqueue(&m);
		}
	}

	tls_close(c);
	g_state = TW_WAITING;
	return welcomed;
}

static DWORD WINAPI tw_thread(LPVOID unused)
{
	WSADATA wsa;
	int32_t backoff = 1;

	(void) unused;

	if (WSAStartup(MAKEWORD(2, 2), &wsa)) {
		TwitchIrcLog("! WSAStartup failed");
		g_state = TW_STOPPED;
		return 0;
	}

	TwitchIrcLog("---- twitch chat: %s ----", TwitchIrcChannel());

	while (!g_stop) {
		if (run_session())
			backoff = 1;

		if (g_stop)
			break;

		++g_reconnects;
		g_state = TW_WAITING;
		g_wait_seconds = backoff;
		TwitchIrcLog("  reconnecting in %d s", backoff);

		for (int32_t i = 0; i < backoff * 4 && !g_stop; ++i)
			Sleep(250);

		backoff = backoff * 2 > 60 ? 60 : backoff * 2;
	}

	WSACleanup();
	g_state = TW_STOPPED;
	TwitchIrcLog("---- twitch chat stopped ----");
	return 0;
}

bool TwitchSetServer(const char *spec)
{
	char host[256];
	char port[16];
	bool tls;

	if (g_thread)
		return false;

	if (!TwitchIrcParseServer(spec, host, sizeof(host), port, sizeof(port), &tls))
		return false;

	StrCopy(g_host, host);
	StrCopy(g_port, port);
	g_tls = tls;
	return true;
}

bool TwitchStart(const char *channel, const char *party_reward_id)
{
	if (!g_cs_ready) {
		InitializeCriticalSection(&g_cs);
		g_cs_ready = 1;
	}

	if (g_thread)
		return false;

	if (!TwitchIrcSetChannel(channel, party_reward_id))
		return false;

	g_stop = 0;
	g_thread = CreateThread(NULL, 0, tw_thread, NULL, 0, NULL);
	return g_thread != NULL;
}

void TwitchStop(void)
{
	if (!g_thread)
		return;

	InterlockedExchange(&g_stop, 1);
	WaitForSingleObject(g_thread, 5000);
	CloseHandle(g_thread);
	g_thread = NULL;
}

int32_t TwitchStatus(char *buf, int32_t size)
{
	uint32_t received;
	uint32_t dropped;
	int32_t n;

	TwitchIrcCounts(&received, &dropped);

	switch (g_state) {
		case TW_CONNECTING:
			n = StrPrintfN(buf, (size_t) size, "connecting to %s", TwitchIrcChannel());
			break;

		case TW_CONNECTED:
			n = StrPrintfN(buf, (size_t) size, "connected to %s as %s, %u messages (%u dropped), %u reconnects",
				TwitchIrcChannel(), g_nick, received, dropped, g_reconnects);
			break;

		case TW_WAITING:
			n = StrPrintfN(buf, (size_t) size, "disconnected, retrying in %d s (%u reconnects)", g_wait_seconds,
				g_reconnects);
			break;

		default:
			n = StrPrintfN(buf, (size_t) size, "not running");
			break;
	}

	return n < 0 || n >= size ? size - 1 : n;
}

#endif
