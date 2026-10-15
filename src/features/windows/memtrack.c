#include "../memtrack.h"

#ifdef DIAG_BUILD

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "../../base/str.h"

#define MAX_SITES 4096
#define INTERVAL_MS 20000
#define TOP_SITES 20

struct site {
	void *caller;
	void *caller2;
	bool used;
	int32_t live_blocks;
	int64_t live_bytes;
	int32_t total_blocks;
};

static struct site g_sites[MAX_SITES];
static int32_t g_site_count;
static int32_t g_live_blocks;
static int64_t g_live_bytes;
static CRITICAL_SECTION g_cs;
static volatile bool g_ready;
static INIT_ONCE g_init_once = INIT_ONCE_STATIC_INIT;
static bool g_reporting;
static DWORD g_next_report;

static void report(const char *when, int32_t top);

static DWORD WINAPI report_thread(LPVOID param)
{
	(void) param;

	for (;;) {
		Sleep(INTERVAL_MS);
		EnterCriticalSection(&g_cs);
		report("", TOP_SITES);
		LeaveCriticalSection(&g_cs);
	}
}

static void report_at_exit(void)
{
	EnterCriticalSection(&g_cs);
	report("  at exit (globals the game frees only with the process are expected here)", MAX_SITES);
	LeaveCriticalSection(&g_cs);
}

static BOOL CALLBACK init_once(PINIT_ONCE once, PVOID param, PVOID *context)
{
	DWORD tid;
	HANDLE thread;

	(void) once;
	(void) param;
	(void) context;

	InitializeCriticalSection(&g_cs);
	g_next_report = GetTickCount() + INTERVAL_MS;
	thread = CreateThread(NULL, 0, report_thread, NULL, 0, &tid);
	if (thread)
		CloseHandle(thread);
	atexit(report_at_exit);
	g_ready = true;

	return TRUE;
}

static void ensure_ready(void)
{
	if (!g_ready)
		InitOnceExecuteOnce(&g_init_once, init_once, NULL, NULL);
}

static int32_t site_for(void *caller, void *caller2)
{
	uint32_t i = (uint32_t) (((uintptr_t) caller >> 2) * 2654435761u ^ ((uintptr_t) caller2 >> 2));

	for (int32_t probe = 0; probe < MAX_SITES; ++probe) {
		struct site *s;

		i &= MAX_SITES - 1;
		s = &g_sites[i];
		if (!s->used) {
			if (g_site_count >= MAX_SITES * 3 / 4)
				return -1;
			s->used = true;
			s->caller = caller;
			s->caller2 = caller2;
			++g_site_count;
			return (int32_t) i;
		}
		if (s->caller == caller && s->caller2 == caller2)
			return (int32_t) i;
		++i;
	}

	return -1;
}

static void report(const char *when, int32_t top)
{
	static int32_t order[MAX_SITES];
	char path[MAX_PATH];
	char *dot;
	FILE *f;
	int32_t i, j, n = 0;
	char *base = (char *) GetModuleHandleA(NULL);

	if (!GetModuleFileNameA(NULL, path, MAX_PATH))
		return;
	path[MAX_PATH - 1] = 0;
	dot = strrchr(path, '.');
	if (dot && !strchr(dot, '\\'))
		*dot = 0;
	StrCat(path, "_memtrack.txt");

	if (fopen_s(&f, path, "a") != 0)
		return;

	fprintf(f, "\n=== %lu ms  live blocks %ld  live bytes %lld%s\n", (unsigned long) GetTickCount(),
		(long) g_live_blocks, (long long) g_live_bytes, when);

	for (i = 0; i < MAX_SITES; ++i)
		if (g_sites[i].used)
			order[n++] = i;

	for (i = 0; i < top && i < n; ++i) {
		int32_t best = i;
		const struct site *s;

		for (j = i + 1; j < n; ++j)
			if (g_sites[order[j]].live_bytes > g_sites[order[best]].live_bytes)
				best = j;
		j = order[i];
		order[i] = order[best];
		order[best] = j;
		s = &g_sites[order[i]];
		if (s->live_bytes <= 0)
			break;
		fprintf(f, "  rva %08lX <- %08lX  live %6ld blocks  %9lld bytes  (%ld ever)\n",
			(unsigned long) ((char *) s->caller - base), (unsigned long) ((char *) s->caller2 - base),
			(long) s->live_blocks, (long long) s->live_bytes, (long) s->total_blocks);
	}

	fclose(f);
}

static void maybe_report(void)
{
	if (g_reporting)
		return;
	if ((LONG) (GetTickCount() - g_next_report) < 0)
		return;

	g_reporting = true;
	g_next_report = GetTickCount() + INTERVAL_MS;
	report("", TOP_SITES);
	g_reporting = false;
}

int32_t MemTrackAlloc(void *caller, void *caller2, size_t size)
{
	int32_t site;

	ensure_ready();
	EnterCriticalSection(&g_cs);
	site = site_for(caller, caller2);
	if (site >= 0) {
		++g_sites[site].live_blocks;
		++g_sites[site].total_blocks;
		g_sites[site].live_bytes += (int64_t) size;
	}
	++g_live_blocks;
	g_live_bytes += (int64_t) size;
	maybe_report();
	LeaveCriticalSection(&g_cs);

	return site;
}

void MemTrackFree(int32_t site, size_t size)
{
	ensure_ready();
	EnterCriticalSection(&g_cs);
	if (site >= 0 && site < MAX_SITES) {
		--g_sites[site].live_blocks;
		g_sites[site].live_bytes -= (int64_t) size;
	}
	--g_live_blocks;
	g_live_bytes -= (int64_t) size;
	LeaveCriticalSection(&g_cs);
}

void MemTrackReportNow(void)
{
	ensure_ready();
	EnterCriticalSection(&g_cs);
	report("", TOP_SITES);
	LeaveCriticalSection(&g_cs);
}

void MemTrackLive(int32_t *blocks, int64_t *bytes)
{
	ensure_ready();
	EnterCriticalSection(&g_cs);
	*blocks = g_live_blocks;
	*bytes = g_live_bytes;
	LeaveCriticalSection(&g_cs);
}

#endif
