#include "../onlinescoresnet.h"

#ifdef ONLINE_SCORES

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>

#include "../onlinescoresconfig.h"
#include "../../base/str.h"

static HANDLE g_submit_thread;

void OsLog(const char *fmt, ...)
{
	char path[MAX_PATH];
	char line[768];
	FILE *f;
	va_list a;
	SYSTEMTIME now;

	if (!GetModuleFileNameA(NULL, path, MAX_PATH))
		return;

	path[MAX_PATH - 1] = 0;
	char *dot = strrchr(path, '.');
	if (dot)
		*dot = 0;

	StrCat(path, "_online.txt");
	GetLocalTime(&now);
	va_start(a, fmt);
	vsnprintf(line, sizeof(line), fmt, a);
	va_end(a);
	line[sizeof(line) - 1] = 0;

	if (fopen_s(&f, path, "a") == 0 && f) {
		fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d  %s\n", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
			now.wSecond, line);
		fclose(f);
	}
}

void OsCp1252ToUtf8(const char *in, char *out, int32_t size)
{
	WCHAR wide[256];
	int32_t n = MultiByteToWideChar(1252, 0, in, -1, wide, 256);

	out[0] = 0;
	if (n > 0 && !WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, size, NULL, NULL))
		out[0] = 0;
}

void OsUtf8ToCp1252(const char *in, char *out, int32_t size)
{
	WCHAR wide[256];
	char all[512];
	int32_t n = MultiByteToWideChar(CP_UTF8, 0, in, -1, wide, 256);

	out[0] = 0;
	if (n > 0 && WideCharToMultiByte(1252, 0, wide, -1, all, sizeof(all), "?", NULL))
		StrCopyN(out, (size_t) size, all);
}

bool OsRandom16(uint8_t out[16])
{
	return BCryptGenRandom(NULL, out, 16, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
}

bool OsHmacSha256Hex(const char *key, const char *msg, char *out)
{
	BCRYPT_ALG_HANDLE alg = NULL;
	BCRYPT_HASH_HANDLE hash = NULL;
	uint8_t digest[32];
	bool ok = false;

	if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0)
		return false;

	if (BCryptCreateHash(alg, &hash, NULL, 0, (PUCHAR) key, (ULONG) strlen(key), 0) == 0 &&
		BCryptHashData(hash, (PUCHAR) msg, (ULONG) strlen(msg), 0) == 0 &&
		BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0) {
		OsToHex(digest, 32, out);
		ok = true;
	}

	if (hash)
		BCryptDestroyHash(hash);

	BCryptCloseAlgorithmProvider(alg, 0);
	return ok;
}

static int32_t http_request(const WCHAR *verb, const char *query, const char *body, char *resp, int32_t resp_size,
	int32_t timeout_ms)
{
	WCHAR url[512];
	WCHAR host[256];
	WCHAR path[512];
	WCHAR full_path[1024];
	WCHAR wide_query[512];
	URL_COMPONENTS parts;
	HINTERNET session = NULL;
	HINTERNET connect = NULL;
	HINTERNET request = NULL;
	DWORD status = 0;
	DWORD status_size = sizeof(status);
	DWORD got = 0;
	int32_t len = 0;
	int32_t result = -1;

	resp[0] = 0;
	if (!MultiByteToWideChar(CP_UTF8, 0, ONLINE_SCORES_URL, -1, url, 512) ||
		!MultiByteToWideChar(CP_UTF8, 0, query ? query : "", -1, wide_query, 512))
		return -1;

	memset(&parts, 0, sizeof(parts));
	parts.dwStructSize = sizeof(parts);
	parts.lpszHostName = host;
	parts.dwHostNameLength = 256;
	parts.lpszUrlPath = path;
	parts.dwUrlPathLength = 512;
	if (!WinHttpCrackUrl(url, 0, 0, &parts))
		return -1;

	_snwprintf_s(full_path, 1024, _TRUNCATE, L"%s%s", path, wide_query);
	session = WinHttpOpen(L"OTII32", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS,
		0);
	if (!session)
		return -1;

	WinHttpSetTimeouts(session, timeout_ms, timeout_ms, timeout_ms, timeout_ms);
	connect = WinHttpConnect(session, host, parts.nPort, 0);
	DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
	if (connect)
		request = WinHttpOpenRequest(connect, verb, full_path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
			flags);

	if (request &&
		WinHttpSendRequest(request,
			body ? L"Content-Type: application/x-www-form-urlencoded\r\n" : WINHTTP_NO_ADDITIONAL_HEADERS,
			body ? (DWORD) -1L : 0, (LPVOID) body, body ? (DWORD) strlen(body) : 0, body ? (DWORD) strlen(body) : 0,
			0) &&
		WinHttpReceiveResponse(request, NULL) &&
		WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX)) {
		while (len < resp_size - 1 && WinHttpReadData(request, resp + len, (DWORD) (resp_size - 1 - len), &got) && got)
			len += (int32_t) got;

		resp[len] = 0;
		result = (int32_t) status;
	}

	if (request)
		WinHttpCloseHandle(request);

	if (connect)
		WinHttpCloseHandle(connect);

	WinHttpCloseHandle(session);
	return result;
}

static DWORD WINAPI submit_thread(LPVOID param)
{
	struct os_score *s = param;
	char body[4096];
	char resp[256];

	if (OsBuildSubmission(s, body, sizeof(body))) {
		int32_t status = http_request(L"POST", "", body, resp, sizeof(resp), 8000);
		OsLog("submit %s %d: status %d, %.120s", s->name, s->score, status, resp);
	}

	free(s);
	return 0;
}

void OsSubmitAsync(const struct os_score *score)
{
	if (!strcmp(ONLINE_SCORES_SECRET, "change-me") || !ONLINE_SCORES_SECRET[0]) {
		OsLog("submit %s %d: not sent -- ONLINE_SCORES_SECRET in onlinescoresconfig.h is not set", score->name,
			score->score);
		return;
	}

	struct os_score *copy = malloc(sizeof(*copy));
	if (!copy)
		return;

	*copy = *score;
	copy->name[sizeof(copy->name) - 1] = 0;
	copy->version[sizeof(copy->version) - 1] = 0;

	if (g_submit_thread) {
		CloseHandle(g_submit_thread);
		g_submit_thread = NULL;
	}

	g_submit_thread = CreateThread(NULL, 0, submit_thread, copy, 0, NULL);
	if (!g_submit_thread)
		free(copy);
}

int32_t OsFetchTop(char names[10][32], int32_t scores[10], int32_t timeout_ms)
{
	static char resp[4096];
	DWORD started = GetTickCount();

	if (g_submit_thread) {
		WaitForSingleObject(g_submit_thread, (DWORD) timeout_ms);
		DWORD waited = GetTickCount() - started;
		timeout_ms = (int32_t) waited < timeout_ms - 500 ? timeout_ms - (int32_t) waited : 500;
	}

	int32_t status = http_request(L"GET", "?action=top&limit=10", NULL, resp, sizeof(resp), timeout_ms);
	if (status != 200) {
		OsLog("top 10: status %d -- showing the local list", status);
		return 0;
	}

	int32_t count = OsParseTop(resp, names, scores);
	if (!count)
		OsLog("top 10: no scores in the answer (%.80s) -- showing the local list", resp);

	return count;
}

#endif
