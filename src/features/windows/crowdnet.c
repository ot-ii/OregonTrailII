#include "../crowdnet.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <shellapi.h>

#include "../../base/memory.h"
#include "../../base/str.h"
#include "../../third_party/mty_json.h"

#define QUEUE_SIZE 256
#define PING_IDLE_MS (9 * 60 * 1000)
#define PONG_WAIT_MS (15 * 1000)
#define RENEW_MS (110 * 60 * 1000)
#define CODE_LIFE_MS (15 * 60 * 1000)
#define EXTEND_BEFORE_S (2 * 60 * 60)
#define EXTEND_RETRY_MS (10 * 60 * 1000)
#define MESSAGE_SIZE (256 * 1024)
#define STOP_GRACE_MS 2000
#define STOP_ABORT_MS 5000
#define DEFAULT_SCOPES "profile:read session:write session:control"

struct crowd_net {
	CRITICAL_SECTION cs;
	HANDLE thread;
	HANDLE receiver;
	HANDLE wake;
	volatile LONG stop;
	struct crowd_config cfg;
	HINTERNET session;
	char *message;

	HINTERNET connect;
	HINTERNET socket;
	HINTERNET api_connect;
	HINTERNET request;
	bool aborted;

	int32_t state;
	char status[256];
	char note[160];
	char token[4096];
	int64_t token_exp;
	char topic[160];
	char game_session[128];
	char name[128];
	char *in[QUEUE_SIZE];
	int32_t in_head;
	int32_t in_count;
	char *out[QUEUE_SIZE];
	int32_t out_head;
	int32_t out_count;
	int32_t session_wanted;
	bool sign_in_wanted;
	bool sign_out_wanted;
	bool subscribed;
	int32_t connections;
	DWORD last_message;
	DWORD pong_at;

	bool code_asked;
	char code[64];
	char code_url[512];
	char verifier[64];
	DWORD code_at;
	bool code_redeemed;

	uint32_t rng;
};


static void cc_log(const char *fmt, ...)
{
	char path[MAX_PATH];
	char line[1024];

	if (!GetModuleFileNameA(NULL, path, MAX_PATH))
		return;

	char *dot = strrchr(path, '.');

	if (dot)
		*dot = 0;

	StrCat(path, "_crowdcontrol.txt");

	SYSTEMTIME now;
	GetLocalTime(&now);

	va_list args;
	va_start(args, fmt);
	vsnprintf(line, sizeof(line), fmt, args);
	va_end(args);
	line[sizeof(line) - 1] = 0;

	FILE *f = fopen(path, "a");

	if (f) {
		fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d  %s\n", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
			now.wSecond, line);
		fclose(f);
	}
}

static void set_state(struct crowd_net *ctx, int32_t state, const char *fmt, ...)
{
	char text[256];

	va_list args;
	va_start(args, fmt);
	vsnprintf(text, sizeof(text), fmt, args);
	va_end(args);
	text[sizeof(text) - 1] = 0;

	EnterCriticalSection(&ctx->cs);
	if (state >= 0)
		ctx->state = state;
	StrCopy(ctx->status, text);
	LeaveCriticalSection(&ctx->cs);
	cc_log("%s", text);
}

static uint32_t next_random(struct crowd_net *ctx)
{
	ctx->rng ^= ctx->rng << 13;
	ctx->rng ^= ctx->rng >> 17;
	ctx->rng ^= ctx->rng << 5;

	return ctx->rng;
}

static void random_id(struct crowd_net *ctx, char *out, int32_t size)
{
	uint8_t b[16];

	for (int32_t i = 0; i < 16; i += 4) {
		uint32_t r = next_random(ctx);

		memcpy(b + i, &r, 4);
	}
	b[6] = (uint8_t) ((b[6] & 0x0F) | 0x40);
	b[8] = (uint8_t) ((b[8] & 0x3F) | 0x80);
	StrPrintfN(out, (size_t) size, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", b[0], b[1],
		b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}

static void push(char **queue, int32_t *head, int32_t *count, char *text)
{
	if (*count == QUEUE_SIZE) {
		MemFree(queue[*head]);
		*head = (*head + 1) % QUEUE_SIZE;
		--*count;
	}
	queue[(*head + *count) % QUEUE_SIZE] = text;
	++*count;
}

static char *pop(char **queue, int32_t *head, int32_t *count)
{
	if (!*count)
		return NULL;

	char *text = queue[*head];
	*head = (*head + 1) % QUEUE_SIZE;
	--*count;

	return text;
}

static void queue_send(struct crowd_net *ctx, const char *json)
{
	char *copy = MemStrDup(json);

	EnterCriticalSection(&ctx->cs);
	push(ctx->out, &ctx->out_head, &ctx->out_count, copy);
	LeaveCriticalSection(&ctx->cs);
	if (ctx->wake)
		SetEvent(ctx->wake);
}

static void free_secret_text(char *text)
{
	if (text) {
		SecureZeroMemory(text, strlen(text));
		MTY_Free(text);
	}
}

static void send_action(struct crowd_net *ctx, const char *action, MTY_JSON *data)
{
	MTY_JSON *root = MTY_JSONObjCreate();

	MTY_JSONObjSetString(root, "action", action);
	if (data)
		MTY_JSONObjSetItem(root, "data", data);

	char *text = MTY_JSONSerialize(root);

	if (text)
		queue_send(ctx, text);
	free_secret_text(text);
	MTY_JSONDestroy(&root);
}

static bool hold(struct crowd_net *ctx, HINTERNET *field, HINTERNET h)
{
	EnterCriticalSection(&ctx->cs);
	bool aborted = ctx->aborted;

	if (!aborted)
		*field = h;
	LeaveCriticalSection(&ctx->cs);
	if (aborted)
		WinHttpCloseHandle(h);

	return !aborted;
}

static HINTERNET held(struct crowd_net *ctx, HINTERNET *field)
{
	EnterCriticalSection(&ctx->cs);
	HINTERNET h = *field;
	LeaveCriticalSection(&ctx->cs);

	return h;
}

static void release(struct crowd_net *ctx, HINTERNET *field)
{
	EnterCriticalSection(&ctx->cs);
	HINTERNET h = *field;
	*field = NULL;
	LeaveCriticalSection(&ctx->cs);
	if (h)
		WinHttpCloseHandle(h);
}

static void cut_calls(struct crowd_net *ctx)
{
	HINTERNET h[4];

	EnterCriticalSection(&ctx->cs);
	ctx->aborted = true;
	h[0] = ctx->request;
	h[1] = ctx->api_connect;
	h[2] = ctx->socket;
	h[3] = ctx->connect;
	ctx->request = ctx->api_connect = ctx->socket = ctx->connect = NULL;
	LeaveCriticalSection(&ctx->cs);
	for (int32_t i = 0; i < 4; ++i)
		if (h[i])
			WinHttpCloseHandle(h[i]);
}


static int32_t base64url_decode(const char *in, int32_t in_len, uint8_t *out, int32_t out_size)
{
	int32_t bits = 0;
	int32_t value = 0;
	int32_t n = 0;

	for (int32_t i = 0; i < in_len; ++i) {
		char c = in[i];
		int32_t d;

		if (c >= 'A' && c <= 'Z') {
			d = c - 'A';

		} else if (c >= 'a' && c <= 'z') {
			d = c - 'a' + 26;

		} else if (c >= '0' && c <= '9') {
			d = c - '0' + 52;

		} else if (c == '-' || c == '+') {
			d = 62;

		} else if (c == '_' || c == '/') {
			d = 63;

		} else if (c == '=') {
			break;

		} else {
			return -1;
		}

		value = (value << 6) | d;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			if (n >= out_size - 1)
				return -1;
			out[n++] = (uint8_t) ((value >> bits) & 0xFF);
		}
	}
	out[n] = 0;

	return n;
}

static bool base64url_encode(const uint8_t *in, int32_t in_len, char *out, int32_t out_size)
{
	static const char digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	uint32_t value = 0;
	int32_t bits = 0;
	int32_t n = 0;

	for (int32_t i = 0; i < in_len; ++i) {
		value = (value << 8) | in[i];
		bits += 8;
		while (bits >= 6) {
			bits -= 6;
			if (n >= out_size - 1)
				return false;
			out[n++] = digits[(value >> bits) & 0x3F];
		}
	}
	if (bits > 0) {
		if (n >= out_size - 1)
			return false;
		out[n++] = digits[(value << (6 - bits)) & 0x3F];
	}
	out[n] = 0;

	return true;
}

static MTY_JSON *token_claims(const char *token)
{
	uint8_t claims[4096];
	const char *first = token ? strchr(token, '.') : NULL;
	const char *second = first ? strchr(first + 1, '.') : NULL;

	if (!second)
		return NULL;

	int32_t n = base64url_decode(first + 1, (int32_t) (second - first - 1), claims, sizeof(claims));
	MTY_JSON *root = n > 0 ? MTY_JSONParse((const char *) claims) : NULL;

	SecureZeroMemory(claims, sizeof(claims));
	if (root && MTY_JSONGetType(root) != MTY_JSON_OBJECT)
		MTY_JSONDestroy(&root);

	return root;
}

bool CrowdNetTokenClaims(const char *token, int64_t *exp, char *name, int32_t name_size)
{
	*exp = 0;
	if (name && name_size)
		name[0] = 0;

	MTY_JSON *root = token_claims(token);

	if (!root)
		return false;

	double value;

	if (MTY_JSONNumber(MTY_JSONObjGetItem(root, "exp"), &value))
		*exp = (int64_t) value;

	if (name && name_size) {
		const char *s = MTY_JSONObjGetStringPtr(root, "name");

		if (!s)
			s = MTY_JSONObjGetStringPtr(root, "originName");
		if (s)
			StrCopyN(name, (size_t) name_size, s);
	}
	MTY_JSONDestroy(&root);

	return true;
}

bool CrowdNetTokenString(const char *token, const char *claim, char *out, int32_t size)
{
	MTY_JSON *root = token_claims(token);
	const char *s = MTY_JSONObjGetStringPtr(root, claim);

	if (size > 0)
		StrCopyN(out, (size_t) size, s ? s : "");
	MTY_JSONDestroy(&root);

	return s && size > 0 && out[0];
}

bool CrowdNetCodeChallenge(const char *verifier, char *out, int32_t size)
{
	BCRYPT_ALG_HANDLE alg = NULL;
	BCRYPT_HASH_HANDLE hash = NULL;
	uint8_t digest[32];
	bool ok = false;

	if (size > 0)
		out[0] = 0;
	if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0)
		return false;
	if (BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0) == 0 &&
			BCryptHashData(hash, (PUCHAR) verifier, (ULONG) strlen(verifier), 0) == 0 &&
			BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0)
		ok = base64url_encode(digest, sizeof(digest), out, size);
	if (hash)
		BCryptDestroyHash(hash);
	BCryptCloseAlgorithmProvider(alg, 0);

	return ok;
}

static bool make_verifier(char *out, int32_t size)
{
	uint8_t raw[32];

	if (BCryptGenRandom(NULL, raw, sizeof(raw), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
		return false;

	bool ok = base64url_encode(raw, sizeof(raw), out, size);

	SecureZeroMemory(raw, sizeof(raw));

	return ok;
}

static bool token_usable(const char *token)
{
	int64_t exp;

	if (!token[0] || !CrowdNetTokenClaims(token, &exp, NULL, 0))
		return false;

	return exp == 0 || exp > (int64_t) time(NULL) + 60;
}

static void save_token(struct crowd_net *ctx, const char *token)
{
	DATA_BLOB in;
	DATA_BLOB out;

	if (!token[0]) {
		DeleteFileA(ctx->cfg.token_file);
		return;
	}
	in.pbData = (BYTE *) token;
	in.cbData = (DWORD) strlen(token);
	if (!CryptProtectData(&in, L"Crowd Control", NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &out))
		return;

	FILE *f = fopen(ctx->cfg.token_file, "wb");

	if (f) {
		fwrite(out.pbData, 1, out.cbData, f);
		fclose(f);
	}
	LocalFree(out.pbData);
}

static void load_token(struct crowd_net *ctx, char *token, int32_t size)
{
	uint8_t raw[8192];
	DATA_BLOB in;
	DATA_BLOB out;

	token[0] = 0;

	FILE *f = fopen(ctx->cfg.token_file, "rb");

	if (!f)
		return;

	size_t n = fread(raw, 1, sizeof(raw), f);
	fclose(f);
	in.pbData = raw;
	in.cbData = (DWORD) n;
	if (!n || !CryptUnprotectData(&in, NULL, NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &out))
		return;
	if ((int32_t) out.cbData < size) {
		memcpy(token, out.pbData, out.cbData);
		token[out.cbData] = 0;
	}
	SecureZeroMemory(out.pbData, out.cbData);
	LocalFree(out.pbData);
}

static void set_token_locked(struct crowd_net *ctx, const char *token)
{
	int64_t exp = 0;
	char uid[128];

	SecureZeroMemory(ctx->token, sizeof(ctx->token));
	StrCopy(ctx->token, token);
	ctx->topic[0] = 0;
	ctx->token_exp = 0;
	if (token[0]) {
		CrowdNetTokenClaims(token, &exp, NULL, 0);
		ctx->token_exp = exp;
		if (CrowdNetTokenString(token, "ccUID", uid, sizeof(uid)))
			StrPrintf(ctx->topic, "pub/%s", uid);
	}
}

static void set_token(struct crowd_net *ctx, const char *token)
{
	EnterCriticalSection(&ctx->cs);
	set_token_locked(ctx, token);
	LeaveCriticalSection(&ctx->cs);
	save_token(ctx, token);
}


static bool crack(const char *url_text, WCHAR *host, DWORD host_size, WCHAR *path, DWORD path_size,
	INTERNET_PORT *port, bool *secure)
{
	WCHAR url[512];
	URL_COMPONENTS parts;
	char fixed[512];

	if (!_strnicmp(url_text, "wss://", 6)) {
		StrPrintf(fixed, "https://%s", url_text + 6);

	} else if (!_strnicmp(url_text, "ws://", 5)) {
		StrPrintf(fixed, "http://%s", url_text + 5);

	} else {
		StrCopy(fixed, url_text);
	}

	if (!MultiByteToWideChar(CP_UTF8, 0, fixed, -1, url, 512))
		return false;

	memset(&parts, 0, sizeof(parts));
	parts.dwStructSize = sizeof(parts);
	parts.lpszHostName = host;
	parts.dwHostNameLength = host_size;
	parts.lpszUrlPath = path;
	parts.dwUrlPathLength = path_size;
	if (!WinHttpCrackUrl(url, 0, 0, &parts))
		return false;
	if (!path[0])
		wcscpy_s(path, path_size, L"/");
	*port = parts.nPort;
	*secure = parts.nScheme == INTERNET_SCHEME_HTTPS;

	return true;
}

static int32_t api_call(struct crowd_net *ctx, const WCHAR *verb, const char *call, const char *body, bool auth,
	char *resp, int32_t resp_size)
{
	char url[512];
	char header_text[4400];
	WCHAR header[4400];
	WCHAR host[256];
	WCHAR path[512];
	INTERNET_PORT port;
	bool secure;
	int32_t len = 0;
	int32_t result = -1;
	HINTERNET connect = NULL;
	HINTERNET request = NULL;
	DWORD status = 0;
	DWORD status_size = sizeof(status);
	DWORD got = 0;
	DWORD body_len = body ? (DWORD) strlen(body) : 0;

	resp[0] = 0;
	StrPrintf(url, "%s%s", ctx->cfg.api, call);
	if (!crack(url, host, 256, path, 512, &port, &secure))
		return -1;

	StrCopy(header_text, "Content-Type: application/json\r\n");
	if (auth) {
		EnterCriticalSection(&ctx->cs);
		StrCat(header_text, "Authorization: cc-auth-token ");
		StrCat(header_text, ctx->token);
		LeaveCriticalSection(&ctx->cs);
		StrCat(header_text, "\r\n");
	}
	MultiByteToWideChar(CP_UTF8, 0, header_text, -1, header, 4400);
	SecureZeroMemory(header_text, sizeof(header_text));
	connect = WinHttpConnect(ctx->session, host, port, 0);
	if (connect && hold(ctx, &ctx->api_connect, connect)) {
		request = WinHttpOpenRequest(connect, verb, path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
			secure ? WINHTTP_FLAG_SECURE : 0);
		if (request && !hold(ctx, &ctx->request, request))
			request = NULL;
	}
	if (request &&
			WinHttpSendRequest(request, header, (DWORD) -1L, body_len ? (LPVOID) body : WINHTTP_NO_REQUEST_DATA,
				body_len, body_len, 0) &&
			WinHttpReceiveResponse(request, NULL) &&
			WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX)) {
		while (len < resp_size - 1 && WinHttpReadData(request, resp + len, (DWORD) (resp_size - 1 - len), &got) && got)
			len += (int32_t) got;
		resp[len] = 0;
		result = (int32_t) status;
	}
	SecureZeroMemory(header, sizeof(header));
	release(ctx, &ctx->request);
	release(ctx, &ctx->api_connect);

	return result;
}

static bool post_for_token(struct crowd_net *ctx, const char *call, MTY_JSON *body, int32_t *status, char *token,
	int32_t token_size, char *message, int32_t message_size)
{
	char *text = MTY_JSONSerialize(body);
	char *resp = MemAlloc(8192, 1);

	message[0] = 0;
	*status = text ? api_call(ctx, L"POST", call, text, false, resp, 8192) : -1;
	free_secret_text(text);
	MTY_JSONDestroy(&body);

	MTY_JSON *root = *status > 0 ? MTY_JSONParse(resp) : NULL;
	const char *got = *status == 200 ? MTY_JSONObjGetStringPtr(root, "token") : NULL;
	bool ok = got && (int32_t) strlen(got) < token_size && got[0];

	if (ok) {
		StrCopyN(token, (size_t) token_size, got);

	} else {
		const char *why = MTY_JSONObjGetStringPtr(root, "message");

		if (why)
			StrCopyN(message, (size_t) message_size, why);
	}
	MTY_JSONDestroy(&root);
	SecureZeroMemory(resp, 8192);
	MemFree(resp);

	return ok;
}

static void start_session(struct crowd_net *ctx)
{
	char resp[4096];

	if (!ctx->cfg.pack_id[0]) {
		set_state(ctx, CROWD_SIGNED_IN, "No game pack ID: set [crowdcontrol] pack_id in OREGONII.INI");
		return;
	}

	MTY_JSON *body = MTY_JSONObjCreate();

	MTY_JSONObjSetString(body, "gamePackID", ctx->cfg.pack_id);

	char *text = MTY_JSONSerialize(body);
	int32_t status = text ? api_call(ctx, L"POST", "game-session/start", text, true, resp, sizeof(resp)) : -1;

	MTY_Free(text);
	MTY_JSONDestroy(&body);

	MTY_JSON *root = status == 200 ? MTY_JSONParse(resp) : NULL;
	const char *id = MTY_JSONObjGetStringPtr(root, "gameSessionID");

	if (id) {
		EnterCriticalSection(&ctx->cs);
		StrCopy(ctx->game_session, id);
		LeaveCriticalSection(&ctx->cs);
		set_state(ctx, CROWD_SESSION, "Game session running: viewers can buy effects");

	} else {
		set_state(ctx, CROWD_SIGNED_IN, "The game session did not start (HTTP %d)", status);
	}
	MTY_JSONDestroy(&root);
}

static void stop_session(struct crowd_net *ctx)
{
	char resp[1024];
	char id[128];

	EnterCriticalSection(&ctx->cs);
	StrCopy(id, ctx->game_session);
	ctx->game_session[0] = 0;
	LeaveCriticalSection(&ctx->cs);
	if (!id[0])
		return;

	MTY_JSON *body = MTY_JSONObjCreate();

	MTY_JSONObjSetString(body, "gameSessionID", id);

	char *text = MTY_JSONSerialize(body);
	int32_t status = text ? api_call(ctx, L"POST", "game-session/stop", text, true, resp, sizeof(resp)) : -1;

	MTY_Free(text);
	MTY_JSONDestroy(&body);
	set_state(ctx, CROWD_SIGNED_IN, "Game session stopped (HTTP %d)", status);
}


static void subscribe(struct crowd_net *ctx)
{
	MTY_JSON *data = MTY_JSONObjCreate();
	MTY_JSON *topics = MTY_JSONArrayCreate(1);

	EnterCriticalSection(&ctx->cs);
	MTY_JSONObjSetString(data, "token", ctx->token);
	MTY_JSONArraySetString(topics, 0, ctx->topic);
	LeaveCriticalSection(&ctx->cs);
	MTY_JSONObjSetItem(data, "topics", topics);
	send_action(ctx, "subscribe", data);
	cc_log("subscribing");
}

static void ask_code(struct crowd_net *ctx)
{
	char verifier[64];
	char challenge[64];
	char scopes[256];
	char *names[32];
	char *next;
	uint32_t n = 0;

	if (!make_verifier(verifier, sizeof(verifier)) || !CrowdNetCodeChallenge(verifier, challenge, sizeof(challenge))) {
		set_state(ctx, CROWD_SIGN_IN, "Sign-in failed: no secure random numbers");
		return;
	}

	StrCopy(scopes, ctx->cfg.scopes[0] ? ctx->cfg.scopes : DEFAULT_SCOPES);
	for (char *scope = strtok_s(scopes, " ,", &next); scope && n < 32; scope = strtok_s(NULL, " ,", &next))
		names[n++] = scope;

	MTY_JSON *data = MTY_JSONObjCreate();
	MTY_JSON *list = MTY_JSONArrayCreate(n);

	for (uint32_t i = 0; i < n; ++i)
		MTY_JSONArraySetString(list, i, names[i]);
	MTY_JSONObjSetString(data, "appID", ctx->cfg.app_id);
	MTY_JSONObjSetItem(data, "scopes", list);
	MTY_JSONObjSetString(data, "codeChallenge", challenge);
	MTY_JSONObjSetString(data, "codeChallengeMethod", "S256");

	EnterCriticalSection(&ctx->cs);
	ctx->code_asked = true;
	ctx->code[0] = 0;
	ctx->code_url[0] = 0;
	ctx->code_redeemed = false;
	ctx->code_at = GetTickCount();
	SecureZeroMemory(ctx->verifier, sizeof(ctx->verifier));
	StrCopy(ctx->verifier, verifier);
	LeaveCriticalSection(&ctx->cs);
	SecureZeroMemory(verifier, sizeof(verifier));
	send_action(ctx, "generate-auth-code", data);
	set_state(ctx, CROWD_SIGN_IN, "Asking Crowd Control for a sign-in code");
}

static void forget_code_locked(struct crowd_net *ctx)
{
	ctx->code_asked = false;
	ctx->code_redeemed = false;
	ctx->code[0] = 0;
	ctx->code_url[0] = 0;
	SecureZeroMemory(ctx->verifier, sizeof(ctx->verifier));
}

static MTY_JSON *token_request(struct crowd_net *ctx, const char *code, const char *verifier, bool use_secret)
{
	MTY_JSON *body = MTY_JSONObjCreate();

	MTY_JSONObjSetString(body, "appID", ctx->cfg.app_id);
	MTY_JSONObjSetString(body, "code", code);
	if (use_secret) {
		MTY_JSONObjSetString(body, "secret", ctx->cfg.secret);

	} else {
		MTY_JSONObjSetString(body, "codeVerifier", verifier);
	}

	return body;
}

static void redeem_code(struct crowd_net *ctx)
{
	char code[64];
	char verifier[64];
	char message[256];
	char *token = MemAlloc(4096, 1);
	int32_t status;

	EnterCriticalSection(&ctx->cs);
	StrCopy(code, ctx->code);
	StrCopy(verifier, ctx->verifier);
	forget_code_locked(ctx);
	LeaveCriticalSection(&ctx->cs);

	bool ok = post_for_token(ctx, "auth/application/token", token_request(ctx, code, verifier, false), &status, token,
		4096, message, sizeof(message));

	if (!ok && status > 0 && ctx->cfg.secret[0]) {
		cc_log("the code verifier was refused (HTTP %d%s%s): trying the secret", status, message[0] ? ", " : "",
			message);
		ok = post_for_token(ctx, "auth/application/token", token_request(ctx, code, verifier, true), &status, token,
			4096, message, sizeof(message));
	}
	SecureZeroMemory(verifier, sizeof(verifier));
	if (ok) {
		set_token(ctx, token);
		cc_log("signed in");
		subscribe(ctx);

	} else if (message[0]) {
		set_state(ctx, CROWD_SIGN_IN, "Sign-in failed (HTTP %d): %s", status, message);

	} else {
		set_state(ctx, CROWD_SIGN_IN, "Sign-in failed (HTTP %d): connect again", status);
	}
	SecureZeroMemory(token, 4096);
	MemFree(token);
}

static bool extend_token(struct crowd_net *ctx)
{
	char jti[128];
	char message[256];
	char *token = MemAlloc(4096, 1);
	int32_t status;

	EnterCriticalSection(&ctx->cs);
	CrowdNetTokenString(ctx->token, "jti", jti, sizeof(jti));
	LeaveCriticalSection(&ctx->cs);
	if (!jti[0]) {
		MemFree(token);
		return false;
	}

	MTY_JSON *body = MTY_JSONObjCreate();

	MTY_JSONObjSetString(body, "jti", jti);
	SecureZeroMemory(jti, sizeof(jti));

	bool ok = post_for_token(ctx, "auth/token/extend", body, &status, token, 4096, message, sizeof(message));

	if (ok) {
		set_token(ctx, token);
		cc_log("sign-in extended");

	} else {
		cc_log("the sign-in could not be extended (HTTP %d%s%s)", status, message[0] ? ", " : "", message);
	}
	SecureZeroMemory(token, 4096);
	MemFree(token);

	return ok;
}

static void sign_out(struct crowd_net *ctx)
{
	char resp[1024];
	bool have;

	stop_session(ctx);
	EnterCriticalSection(&ctx->cs);
	have = ctx->token[0] != 0;
	LeaveCriticalSection(&ctx->cs);

	int32_t status = have ? api_call(ctx, L"DELETE", "auth/token", NULL, true, resp, sizeof(resp)) : 0;

	EnterCriticalSection(&ctx->cs);
	set_token_locked(ctx, "");
	forget_code_locked(ctx);
	ctx->session_wanted = -1;
	ctx->sign_in_wanted = false;
	LeaveCriticalSection(&ctx->cs);
	save_token(ctx, "");
	EnterCriticalSection(&ctx->cs);
	StrPrintf(ctx->note, have ? "Signed out (HTTP %d)" : "Signed out", status);
	LeaveCriticalSection(&ctx->cs);
}

static bool is_pong(const char *text, const MTY_JSON *root)
{
	const char *kind = MTY_JSONObjGetStringPtr(root, "type");

	return !strcmp(text, "pong") || (kind && !strcmp(kind, "pong"));
}

static bool has_string(const MTY_JSON *list, const char *text)
{
	uint32_t n = MTY_JSONGetType(list) == MTY_JSON_ARRAY ? MTY_JSONArrayGetLength(list) : 0;

	for (uint32_t i = 0; i < n; ++i) {
		const char *s = MTY_JSONStringPtr(MTY_JSONArrayGetItem(list, i));

		if (s && !strcmp(s, text))
			return true;
	}

	return false;
}

static bool web_url(const char *url)
{
	if (_strnicmp(url, "https://", 8) && _strnicmp(url, "http://", 7))
		return false;
	for (const char *c = url; *c; ++c)
		if ((uint8_t) *c <= ' ' || *c == '"')
			return false;

	return true;
}

static void handle_message(struct crowd_net *ctx, const char *text)
{
	MTY_JSON *root = MTY_JSONParse(text);

	EnterCriticalSection(&ctx->cs);
	ctx->last_message = GetTickCount();
	if (is_pong(text, root))
		ctx->pong_at = ctx->last_message | 1;
	LeaveCriticalSection(&ctx->cs);
	if (!root) {
		if (strcmp(text, "pong"))
			cc_log("not JSON: %.200s", text);
		return;
	}

	const MTY_JSON *payload = MTY_JSONObjGetItem(root, "payload");
	const char *kind = MTY_JSONObjGetStringPtr(root, "type");
	const char *code = MTY_JSONObjGetStringPtr(payload, "code");
	const char *message = MTY_JSONObjGetStringPtr(payload, "message");

	if (!kind)
		kind = "";
	if (!strcmp(kind, "application-auth-code")) {
		const char *url = MTY_JSONObjGetStringPtr(payload, "url");

		EnterCriticalSection(&ctx->cs);

		bool mine = ctx->code_asked && code && code[0] && strlen(code) < sizeof(ctx->code);

		if (mine) {
			ctx->code_asked = false;
			StrCopy(ctx->code, code);
			ctx->code_at = GetTickCount();
			if (url && strlen(url) < sizeof(ctx->code_url) && web_url(url))
				StrCopy(ctx->code_url, url);
		}
		LeaveCriticalSection(&ctx->cs);
		if (mine) {
			set_state(ctx, CROWD_SIGN_IN, "Sign in in the browser; your code is %s", code);
			SetEvent(ctx->wake);
		}

	} else if (!strcmp(kind, "application-auth-code-redeemed")) {
		EnterCriticalSection(&ctx->cs);

		bool mine = code && ctx->code[0] && !strcmp(code, ctx->code);

		if (mine)
			ctx->code_redeemed = true;
		LeaveCriticalSection(&ctx->cs);
		if (mine) {
			cc_log("sign-in code redeemed");
			SetEvent(ctx->wake);
		}

	} else if (!strcmp(kind, "application-auth-code-error")) {
		EnterCriticalSection(&ctx->cs);
		forget_code_locked(ctx);
		LeaveCriticalSection(&ctx->cs);
		set_state(ctx, CROWD_SIGN_IN, "Sign-in failed: %.120s", message ? message : "no reason given");

	} else if (!strcmp(kind, "subscription-result")) {
		int64_t exp;
		char name[128];

		EnterCriticalSection(&ctx->cs);

		bool ok = ctx->topic[0] && has_string(MTY_JSONObjGetItem(payload, "success"), ctx->topic);

		LeaveCriticalSection(&ctx->cs);
		if (ok) {
			EnterCriticalSection(&ctx->cs);
			ctx->subscribed = true;
			CrowdNetTokenClaims(ctx->token, &exp, name, sizeof(name));
			StrCopy(ctx->name, name);
			if (ctx->cfg.auto_session && ctx->session_wanted < 0 && !ctx->game_session[0])
				ctx->session_wanted = 1;

			bool running = ctx->game_session[0] != 0;
			LeaveCriticalSection(&ctx->cs);
			if (running) {
				cc_log(name[0] ? "Signed in as %s" : "Signed in", name);

			} else {
				set_state(ctx, CROWD_SIGNED_IN, name[0] ? "Signed in as %s" : "Signed in", name);
			}
			SetEvent(ctx->wake);

		} else {
			EnterCriticalSection(&ctx->cs);
			set_token_locked(ctx, "");
			LeaveCriticalSection(&ctx->cs);
			save_token(ctx, "");
			set_state(ctx, CROWD_SIGN_IN, "The sign-in expired: connect again");
		}

	} else if (!strcmp(kind, "effect-request")) {
		char *copy = payload ? MTY_JSONSerialize(payload) : NULL;

		if (copy) {
			EnterCriticalSection(&ctx->cs);
			push(ctx->in, &ctx->in_head, &ctx->in_count, copy);
			LeaveCriticalSection(&ctx->cs);
		}

	} else if (!strcmp(kind, "bad-request")) {
		cc_log("the server could not use a request: %.200s", message ? message : "");
	}
	MTY_JSONDestroy(&root);
}

static DWORD WINAPI receiver_thread(LPVOID param)
{
	struct crowd_net *ctx = param;
	HINTERNET socket = held(ctx, &ctx->socket);
	int32_t len = 0;
	DWORD got;
	WINHTTP_WEB_SOCKET_BUFFER_TYPE type;

	for (;;) {
		if (len >= MESSAGE_SIZE - 1)
			len = 0;
		if (!socket || WinHttpWebSocketReceive(socket, ctx->message + len, (DWORD) (MESSAGE_SIZE - 1 - len), &got, &type) !=
				NO_ERROR)
			break;
		if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)
			break;
		len += (int32_t) got;
		if (type == WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE || type == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE)
			continue;
		ctx->message[len] = 0;
		handle_message(ctx, ctx->message);
		len = 0;
	}

	return 0;
}


static bool open_socket(struct crowd_net *ctx)
{
	WCHAR host[256];
	WCHAR path[512];
	INTERNET_PORT port;
	bool secure;
	HINTERNET connect;
	HINTERNET request = NULL;
	HINTERNET socket = NULL;
	DWORD status = 0;
	DWORD status_size = sizeof(status);

	if (!crack(ctx->cfg.server, host, 256, path, 512, &port, &secure))
		return false;

	connect = WinHttpConnect(ctx->session, host, port, 0);
	if (connect && hold(ctx, &ctx->connect, connect)) {
		request = WinHttpOpenRequest(connect, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
			secure ? WINHTTP_FLAG_SECURE : 0);
		if (request && !hold(ctx, &ctx->request, request))
			request = NULL;
	}
	if (request && WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0) &&
			WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0) &&
			WinHttpReceiveResponse(request, NULL) &&
			WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX) && status == 101)
		socket = WinHttpWebSocketCompleteUpgrade(request, 0);

	DWORD error = GetLastError();

	release(ctx, &ctx->request);
	if (socket && !hold(ctx, &ctx->socket, socket))
		socket = NULL;
	if (!socket) {
		release(ctx, &ctx->connect);
		cc_log("could not connect to %s (HTTP %lu, error %lu)", ctx->cfg.server, status, error);
		return false;
	}

	return true;
}

static void close_socket(struct crowd_net *ctx)
{
	HINTERNET socket = held(ctx, &ctx->socket);

	if (socket)
		WinHttpWebSocketClose(socket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, NULL, 0);
	release(ctx, &ctx->socket);
	if (ctx->receiver && WaitForSingleObject(ctx->receiver, STOP_ABORT_MS) == WAIT_OBJECT_0) {
		CloseHandle(ctx->receiver);
		ctx->receiver = NULL;
	}
	release(ctx, &ctx->connect);
	EnterCriticalSection(&ctx->cs);
	ctx->subscribed = false;
	LeaveCriticalSection(&ctx->cs);
}

static bool send_text(struct crowd_net *ctx, const char *text)
{
	HINTERNET socket = held(ctx, &ctx->socket);

	return socket && WinHttpWebSocketSend(socket, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, (PVOID) text,
		(DWORD) strlen(text)) == NO_ERROR;
}

static bool keep_token(struct crowd_net *ctx, DWORD *last_try)
{
	EnterCriticalSection(&ctx->cs);

	bool have = ctx->token[0] != 0;
	bool usable = token_usable(ctx->token);
	int64_t exp = ctx->token_exp;

	LeaveCriticalSection(&ctx->cs);
	if (!have)
		return false;
	if (exp && exp - (int64_t) time(NULL) < EXTEND_BEFORE_S && (!*last_try || GetTickCount() - *last_try >=
			EXTEND_RETRY_MS)) {
		*last_try = GetTickCount() | 1;
		if (extend_token(ctx))
			return true;
	}
	if (!usable) {
		set_token(ctx, "");
		EnterCriticalSection(&ctx->cs);
		StrCopy(ctx->note, "The sign-in expired: connect again");
		LeaveCriticalSection(&ctx->cs);
	}

	return usable;
}

static void run_connection(struct crowd_net *ctx, DWORD *extend_try)
{
	DWORD connected_at = GetTickCount();
	DWORD ping_at = 0;
	DWORD last_sent = connected_at;
	DWORD ping_idle = ctx->cfg.ping_idle_ms > 0 ? (DWORD) ctx->cfg.ping_idle_ms : PING_IDLE_MS;
	DWORD pong_wait = ctx->cfg.pong_wait_ms > 0 ? (DWORD) ctx->cfg.pong_wait_ms : PONG_WAIT_MS;
	bool ok = true;

	EnterCriticalSection(&ctx->cs);
	ctx->last_message = connected_at;
	ctx->pong_at = 0;
	++ctx->connections;

	if (ctx->code_asked || ctx->code[0])
		forget_code_locked(ctx);
	LeaveCriticalSection(&ctx->cs);
	cc_log("connected to %s", ctx->cfg.server);
	if (keep_token(ctx, extend_try)) {
		subscribe(ctx);

	} else {
		char note[160];

		EnterCriticalSection(&ctx->cs);
		StrCopy(note, ctx->note);
		ctx->note[0] = 0;
		LeaveCriticalSection(&ctx->cs);
		set_state(ctx, CROWD_SIGN_IN, "%s", note[0] ? note :
			"Connected: connect your Crowd Control account from the Crowd Control window");
	}

	while (ok && !ctx->stop) {
		char *text;
		char url[512];

		WaitForSingleObject(ctx->wake, 1000);
		if (WaitForSingleObject(ctx->receiver, 0) == WAIT_OBJECT_0) {
			cc_log("the server closed the connection");
			break;
		}
		for (;;) {
			EnterCriticalSection(&ctx->cs);
			text = pop(ctx->out, &ctx->out_head, &ctx->out_count);
			LeaveCriticalSection(&ctx->cs);
			if (!text)
				break;
			ok = send_text(ctx, text);
			SecureZeroMemory(text, strlen(text));
			MemFree(text);
			last_sent = GetTickCount();
			if (!ok)
				break;
		}
		if (!ok)
			break;

		EnterCriticalSection(&ctx->cs);

		int32_t session = ctx->subscribed ? ctx->session_wanted : -1;

		if (session >= 0)
			ctx->session_wanted = -1;

		bool sign_out_now = ctx->sign_out_wanted;
		bool ask = ctx->sign_in_wanted && !sign_out_now;
		bool redeemed = ctx->code_redeemed;
		bool code_old = (ctx->code_asked || ctx->code[0]) && GetTickCount() - ctx->code_at >= CODE_LIFE_MS;
		bool subscribed = ctx->subscribed;
		DWORD last_message = ctx->last_message;
		bool ponged = ctx->pong_at != 0;

		ctx->sign_out_wanted = false;
		ctx->sign_in_wanted = false;
		StrCopy(url, ctx->code_url);
		ctx->code_url[0] = 0;
		if (code_old)
			forget_code_locked(ctx);
		LeaveCriticalSection(&ctx->cs);

		if (sign_out_now) {
			sign_out(ctx);
			break;
		}
		if (url[0] && !ctx->cfg.no_browser) {
			ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
			cc_log("sign-in page opened");
		}
		if (ask)
			ask_code(ctx);
		if (redeemed)
			redeem_code(ctx);
		if (code_old)
			set_state(ctx, CROWD_SIGN_IN, "The sign-in code expired: connect again");
		if (session == 1) {
			start_session(ctx);

		} else if (session == 0) {
			stop_session(ctx);
		}
		if (subscribed && !keep_token(ctx, extend_try))
			break;

		DWORD now = GetTickCount();
		DWORD since_message = now - last_message;
		DWORD since_sent = now - last_sent;

		if (ping_at) {
			if (ponged) {
				ping_at = 0;

			} else if (now - ping_at >= pong_wait) {
				cc_log("no answer to the keep-alive: reconnecting");
				break;
			}

		} else if (since_message >= ping_idle && since_sent >= ping_idle) {
			EnterCriticalSection(&ctx->cs);
			ctx->pong_at = 0;
			LeaveCriticalSection(&ctx->cs);
			ok = send_text(ctx, "{\"action\":\"ping\"}");
			ping_at = GetTickCount() | 1;
			last_sent = ping_at;
		}
		if (now - connected_at >= RENEW_MS) {
			cc_log("renewing the connection");
			break;
		}
	}
}

static DWORD WINAPI network_thread(LPVOID param)
{
	struct crowd_net *ctx = param;
	WCHAR agent[64];
	DWORD backoff = 1000;
	DWORD extend_try = 0;

	MultiByteToWideChar(CP_UTF8, 0, ctx->cfg.user_agent, -1, agent, 64);
	ctx->session = WinHttpOpen(agent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS, 0);
	if (!ctx->session) {
		set_state(ctx, CROWD_OFF, "WinHTTP is not available");
		return 0;
	}
	WinHttpSetTimeouts(ctx->session, 10000, 10000, 10000, 0);
	while (!ctx->stop) {
		set_state(ctx, CROWD_CONNECTING, "Connecting to %s", ctx->cfg.server);
		if (!open_socket(ctx)) {
			set_state(ctx, CROWD_CONNECTING, "Could not reach Crowd Control: retrying in %lu s", backoff / 1000);
			if (!ctx->stop)
				WaitForSingleObject(ctx->wake, backoff);
			backoff = backoff * 2 > 60000 ? 60000 : backoff * 2;
			continue;
		}
		backoff = 1000;
		ctx->receiver = CreateThread(NULL, 0, receiver_thread, ctx, 0, NULL);
		if (ctx->receiver)
			run_connection(ctx, &extend_try);
		close_socket(ctx);
		EnterCriticalSection(&ctx->cs);
		if (ctx->game_session[0])
			ctx->session_wanted = 1;
		ctx->game_session[0] = 0;
		LeaveCriticalSection(&ctx->cs);
		if (ctx->receiver) {
			cc_log("the receiving thread did not end: not reconnecting");
			break;
		}
	}
	stop_session(ctx);
	WinHttpCloseHandle(ctx->session);
	ctx->session = NULL;
	set_state(ctx, CROWD_OFF, "Stopped");

	return 0;
}


struct crowd_net *CrowdNetCreate(const struct crowd_config *config)
{
	struct crowd_net *ctx = MemAlloc(1, sizeof(struct crowd_net));
	char token[4096];

	InitializeCriticalSection(&ctx->cs);
	ctx->cfg = *config;
	ctx->session_wanted = -1;
	ctx->message = MemAlloc(MESSAGE_SIZE, 1);
	ctx->rng = GetTickCount() ^ (GetCurrentProcessId() << 16) ^ 0x9E3779B9UL;
	if (!ctx->rng)
		ctx->rng = 1;
	load_token(ctx, token, sizeof(token));
	set_token_locked(ctx, token);
	SecureZeroMemory(token, sizeof(token));
	ctx->wake = CreateEventA(NULL, FALSE, FALSE, NULL);
	ctx->stop = 0;
	ctx->thread = CreateThread(NULL, 0, network_thread, ctx, 0, NULL);
	if (!ctx->thread) {
		CloseHandle(ctx->wake);
		DeleteCriticalSection(&ctx->cs);
		SecureZeroMemory(ctx->token, sizeof(ctx->token));
		SecureZeroMemory(&ctx->cfg, sizeof(ctx->cfg));
		MemFree(ctx->message);
		MemFree(ctx);
		return NULL;
	}
	cc_log("started (%s, pack %s, %s)", ctx->cfg.server, ctx->cfg.pack_id[0] ? ctx->cfg.pack_id : "not set",
		ctx->cfg.secret[0] ? "PKCE, then the secret" : "PKCE");

	return ctx;
}

void CrowdNetDestroy(struct crowd_net **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct crowd_net *ctx = *ctx_mod;
	*ctx_mod = NULL;

	char *text;

	InterlockedExchange(&ctx->stop, 1);
	EnterCriticalSection(&ctx->cs);
	ctx->session_wanted = 0;
	LeaveCriticalSection(&ctx->cs);
	SetEvent(ctx->wake);
	if (WaitForSingleObject(ctx->thread, STOP_GRACE_MS) != WAIT_OBJECT_0) {
		cc_log("the server is not answering: closing the connection");
		cut_calls(ctx);
	}
	if (WaitForSingleObject(ctx->thread, STOP_ABORT_MS) != WAIT_OBJECT_0 ||
			(ctx->receiver && WaitForSingleObject(ctx->receiver, STOP_ABORT_MS) != WAIT_OBJECT_0)) {
		cc_log("the network thread did not end");
		return;
	}

	if (ctx->receiver)
		CloseHandle(ctx->receiver);
	CloseHandle(ctx->thread);
	CloseHandle(ctx->wake);
	while ((text = pop(ctx->in, &ctx->in_head, &ctx->in_count)) != NULL)
		MemFree(text);
	while ((text = pop(ctx->out, &ctx->out_head, &ctx->out_count)) != NULL) {
		SecureZeroMemory(text, strlen(text));
		MemFree(text);
	}
	SecureZeroMemory(ctx->token, sizeof(ctx->token));
	SecureZeroMemory(ctx->verifier, sizeof(ctx->verifier));
	SecureZeroMemory(&ctx->cfg, sizeof(ctx->cfg));
	DeleteCriticalSection(&ctx->cs);
	MemFree(ctx->message);
	MemFree(ctx);
}

bool CrowdNetPoll(struct crowd_net *ctx, char *buf, int32_t size)
{
	if (!ctx)
		return false;

	EnterCriticalSection(&ctx->cs);
	char *text = pop(ctx->in, &ctx->in_head, &ctx->in_count);
	LeaveCriticalSection(&ctx->cs);
	if (!text)
		return false;

	StrCopyN(buf, (size_t) size, text);
	MemFree(text);

	return true;
}

int32_t CrowdNetState(struct crowd_net *ctx, char *text, int32_t size)
{
	if (!ctx) {
		StrCopyN(text, (size_t) size, "Not started");
		return CROWD_OFF;
	}

	EnterCriticalSection(&ctx->cs);
	int32_t state = ctx->state;
	StrCopyN(text, (size_t) size, ctx->status);
	LeaveCriticalSection(&ctx->cs);

	return state;
}

static void send_rpc_list(struct crowd_net *ctx, const char *method, MTY_JSON *list)
{
	MTY_JSON *data = MTY_JSONObjCreate();
	MTY_JSON *call = MTY_JSONObjCreate();
	char id[40];

	EnterCriticalSection(&ctx->cs);
	MTY_JSONObjSetString(data, "token", ctx->token);
	random_id(ctx, id, sizeof(id));
	LeaveCriticalSection(&ctx->cs);
	MTY_JSONObjSetString(call, "type", "call");
	MTY_JSONObjSetString(call, "id", id);
	MTY_JSONObjSetString(call, "method", method);
	MTY_JSONObjSetItem(call, "args", list);
	MTY_JSONObjSetItem(data, "call", call);
	send_action(ctx, "rpc", data);
}

static void send_rpc(struct crowd_net *ctx, const char *method, MTY_JSON *args)
{
	MTY_JSON *list = MTY_JSONArrayCreate(1);

	MTY_JSONArraySetItem(list, 0, args);
	send_rpc_list(ctx, method, list);
}

void CrowdNetPackMetadata(struct crowd_net *ctx, const char *values_json)
{
	if (!ctx || !values_json)
		return;

	MTY_JSON *values = MTY_JSONParse(values_json);

	if (MTY_JSONGetType(values) != MTY_JSON_OBJECT) {
		MTY_JSONDestroy(&values);
		return;
	}

	MTY_JSON *list = MTY_JSONArrayCreate(2);

	MTY_JSONArraySetString(list, 0, ctx->cfg.pack_id);
	MTY_JSONArraySetItem(list, 1, values);
	send_rpc_list(ctx, "packMetadataChanged", list);
	cc_log("pack variables %s", values_json);
}

void CrowdNetRespond(struct crowd_net *ctx, const char *request_id, const char *status, const char *message,
	int32_t time_remaining_ms)
{
	char id[40];

	if (!ctx)
		return;

	MTY_JSON *args = MTY_JSONObjCreate();

	EnterCriticalSection(&ctx->cs);
	random_id(ctx, id, sizeof(id));
	LeaveCriticalSection(&ctx->cs);
	MTY_JSONObjSetString(args, "id", id);
	MTY_JSONObjSetString(args, "request", request_id);
	MTY_JSONObjSetString(args, "status", status);
	MTY_JSONObjSetString(args, "message", message ? message : "");
	if (!strcmp(status, "timedBegin") || !strcmp(status, "timedPause") || !strcmp(status, "timedResume"))
		MTY_JSONObjSetInt(args, "timeRemaining", time_remaining_ms);

	MTY_JSONObjSetNumber(args, "stamp", (double) time(NULL));
	send_rpc(ctx, "effectResponse", args);
	cc_log("answer %s: %s %s", request_id, status, message ? message : "");
}

void CrowdNetReport(struct crowd_net *ctx, const char *ids_json, const char *status)
{
	char id[40];

	if (!ctx)
		return;

	MTY_JSON *ids = ids_json ? MTY_JSONParse(ids_json) : NULL;

	if (MTY_JSONGetType(ids) != MTY_JSON_ARRAY) {
		MTY_JSONDestroy(&ids);
		return;
	}

	MTY_JSON *args = MTY_JSONObjCreate();

	EnterCriticalSection(&ctx->cs);
	random_id(ctx, id, sizeof(id));
	LeaveCriticalSection(&ctx->cs);
	MTY_JSONObjSetString(args, "id", id);
	MTY_JSONObjSetItem(args, "ids", ids);
	MTY_JSONObjSetString(args, "status", status);
	MTY_JSONObjSetNumber(args, "stamp", (double) time(NULL));
	MTY_JSONObjSetString(args, "effectType", "game");
	MTY_JSONObjSetString(args, "identifierType", "effect");
	send_rpc(ctx, "effectReport", args);
	cc_log("report %s %s", status, ids_json);
}

void CrowdNetSignIn(struct crowd_net *ctx)
{
	if (!ctx)
		return;

	EnterCriticalSection(&ctx->cs);
	ctx->sign_in_wanted = true;
	LeaveCriticalSection(&ctx->cs);
	SetEvent(ctx->wake);
}

void CrowdNetSignOut(struct crowd_net *ctx)
{
	if (!ctx)
		return;

	EnterCriticalSection(&ctx->cs);
	ctx->sign_out_wanted = true;
	LeaveCriticalSection(&ctx->cs);
	SetEvent(ctx->wake);
}

void CrowdNetSession(struct crowd_net *ctx, bool on)
{
	if (!ctx)
		return;

	EnterCriticalSection(&ctx->cs);
	ctx->session_wanted = on ? 1 : 0;
	LeaveCriticalSection(&ctx->cs);
	SetEvent(ctx->wake);
}

void CrowdNetSendRaw(struct crowd_net *ctx, const char *json)
{
	if (ctx)
		queue_send(ctx, json);
}

int32_t CrowdNetConnections(struct crowd_net *ctx)
{
	if (!ctx)
		return 0;

	EnterCriticalSection(&ctx->cs);
	int32_t n = ctx->connections;
	LeaveCriticalSection(&ctx->cs);

	return n;
}

int64_t CrowdNetTokenExpiry(struct crowd_net *ctx)
{
	if (!ctx)
		return 0;

	EnterCriticalSection(&ctx->cs);
	int64_t exp = ctx->token[0] ? ctx->token_exp : 0;
	LeaveCriticalSection(&ctx->cs);

	return exp;
}
