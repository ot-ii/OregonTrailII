#include "testframework.h"

#ifdef OTII_TEST

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/base/rng.h"
#include "../src/base/str.h"
#include "testsupport.h"

#define MAX_LINES 25

static FILE *g_out;
static bool g_active;
static char g_filter[1024];
static uint32_t g_seed = 12345;
static int32_t g_stat_runs = 2000;

static const struct test_reg *g_cur;
static char g_case[160];
static char g_known[64];
static int32_t g_cur_fail, g_cur_known, g_cur_checks;
static int32_t g_guard_depth;
static bool g_expect_fatal;
static int32_t g_fatals;
static FILE *g_case_out;
static int32_t g_tests_run, g_tests_failed, g_tests_known, g_checks, g_fails, g_knowns;

static void out(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	if (g_out) {
		vfprintf(g_out, fmt, args);
		fflush(g_out);
	}
	va_end(args);
}

static void out_line(const char *line)
{
	out("%s\n", line);
}

static bool matches_one(const struct test_reg *t, const char *f, size_t n)
{
	size_t sl = strlen(t->suite);

	if (n == 3 && !strncmp(f, "all", 3))
		return true;
	if (n == sl && !strncmp(f, t->suite, n))
		return true;
	return n > sl && !strncmp(f, t->suite, sl) && f[sl] == '.' && n - sl - 1 == strlen(t->name) &&
		!strncmp(f + sl + 1, t->name, n - sl - 1);
}

static bool matches(const struct test_reg *t)
{
	const char *f = g_filter;

	while (*f) {
		const char *end = strchr(f, '+');
		size_t n = end ? (size_t) (end - f) : strlen(f);
		if (matches_one(t, f, n))
			return true;
		if (!end)
			break;
		f = end + 1;
	}
	return false;
}

static void call_test(void *arg)
{
	((const struct test_reg *) arg)->fn();
}

static bool guarded(void (*fn)(void *), void *arg, uint32_t *code, uint32_t *rva)
{
	++g_guard_depth;
	bool ok = TestCallGuarded(fn, arg, code, rva);
	--g_guard_depth;
	return ok;
}

static void run_one(const struct test_reg *t)
{
	uint32_t code = 0, rva = 0;

	g_cur = t;
	out("START %s.%s\n", t->suite, t->name);
	g_case[0] = 0;
	g_known[0] = 0;
	g_cur_fail = g_cur_known = g_cur_checks = 0;
	RngSetSeed(g_seed);
	g_expect_fatal = false;
	if (!guarded(call_test, (void *) t, &code, &rva) && code != TEST_FATAL_EXCEPTION) {
		out("FAIL  %s.%s%s%s%s  CRASH exception 0x%08X at exe+0x%X\n", t->suite, t->name, g_case[0] ? "[" : "",
			g_case, g_case[0] ? "]" : "", code, rva);
		++g_cur_fail;
		++g_fails;
	}
	++g_tests_run;
	if (g_cur_fail) {
		++g_tests_failed;
		out("FAILED %s.%s  (%d checks, %d failed)\n", t->suite, t->name, g_cur_checks, g_cur_fail);

	} else if (g_cur_known) {
		++g_tests_known;
		out("KNOWN %s.%s  (%d checks, %d known)\n", t->suite, t->name, g_cur_checks, g_cur_known);

	} else {
		out("PASS  %s.%s  (%d checks)\n", t->suite, t->name, g_cur_checks);
	}
	g_cur = NULL;
}

static void open_out(void)
{
	char path[512];

	TestExeBase(path, sizeof(path));
	if (!path[0])
		return;
	if (strchr(g_filter, '+')) {
		StrCat(path, "_test_multi.txt");

	} else if (strcmp(g_filter, "all") && strcmp(g_filter, "list")) {
		StrCat(path, "_test_");
		StrCat(path, g_filter);
		StrCat(path, ".txt");

	} else {
		StrCat(path, "_test.txt");
	}
	g_out = fopen(path, "w");
	if (strlen(path) > 4) {
		path[strlen(path) - 4] = 0;
		StrCat(path, "_case.txt");
		g_case_out = fopen(path, "w");
	}
}

void TCheck(bool ok, const char *expr, const char *file, int32_t line, const char *fmt, ...)
{
	char detail[512];
	const char *base;
	va_list args;

	++g_cur_checks;
	++g_checks;
	if (ok)
		return;
	detail[0] = 0;
	if (fmt) {
		va_start(args, fmt);
		vsnprintf(detail, sizeof(detail) - 1, fmt, args);
		detail[sizeof(detail) - 1] = 0;
		va_end(args);
	}
	base = strrchr(file, '\\');
	base = base ? base + 1 : file;
	if (g_known[0]) {
		++g_cur_known;
		++g_knowns;
		if (g_cur_known > MAX_LINES)
			return;
		out("  known[%s] %s.%s%s%s%s  %s  %s  (%s:%d)\n", g_known, g_cur ? g_cur->suite : "?",
			g_cur ? g_cur->name : "?", g_case[0] ? "[" : "", g_case, g_case[0] ? "]" : "", expr, detail, base, line);

	} else {
		++g_cur_fail;
		++g_fails;
		if (g_cur_fail == MAX_LINES + 1)
			out("FAIL  %s.%s  ... further failures only counted\n", g_cur ? g_cur->suite : "?",
				g_cur ? g_cur->name : "?");
		if (g_cur_fail > MAX_LINES)
			return;
		out("FAIL  %s.%s%s%s%s  %s  %s  (%s:%d)\n", g_cur ? g_cur->suite : "?", g_cur ? g_cur->name : "?",
			g_case[0] ? "[" : "", g_case, g_case[0] ? "]" : "", expr, detail, base, line);
	}
}

void TCheckRate(int32_t hits, int32_t n, double p, const char *expr, const char *file, int32_t line)
{
	double mean = n * p;
	double sd = sqrt(n * p * (1.0 - p));
	double tol = 4.5 * sd + 1.0;

	TCheck(fabs(hits - mean) <= tol, expr, file, line, "%d of %d = %.2f%%, expected %.2f%% (+-%.2f%%)", hits, n,
		n ? 100.0 * hits / n : 0.0, 100.0 * p, n ? 100.0 * tol / n : 0.0);
}

void TCase(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vsnprintf(g_case, sizeof(g_case) - 1, fmt, args);
	g_case[sizeof(g_case) - 1] = 0;
	va_end(args);
	if (g_case_out && g_case[0]) {
		rewind(g_case_out);
		fprintf(g_case_out, "%s.%s[%s]%-80s\n", g_cur ? g_cur->suite : "?", g_cur ? g_cur->name : "?", g_case, "");
		fflush(g_case_out);
	}
}

void TKnown(const char *id)
{
	StrCopy(g_known, id ? id : "");
}

void TNote(const char *fmt, ...)
{
	char text[512];
	va_list args;

	va_start(args, fmt);
	vsnprintf(text, sizeof(text) - 1, fmt, args);
	text[sizeof(text) - 1] = 0;
	va_end(args);
	out("  note %s.%s  %s\n", g_cur ? g_cur->suite : "?", g_cur ? g_cur->name : "?", text);
}

uint32_t TSeed(void)
{
	return g_seed;
}

int32_t TStatRuns(void)
{
	return g_stat_runs;
}

bool TGuarded(void (*fn)(void *), void *arg)
{
	uint32_t code = 0, rva = 0;

	if (guarded(fn, arg, &code, &rva))
		return true;
	if (code != TEST_FATAL_EXCEPTION)
		TCheck(false, "no crash", "testframework.c", __LINE__, "CRASH exception 0x%08X at exe+0x%X", code, rva);
	return false;
}

void TFatal(const char *func, const char *error, const char *file, int32_t line)
{
	const char *base = file ? file : "?";

	if (!g_active || !g_cur || !g_guard_depth)
		return;
	++g_fatals;
	for (const char *p = base; *p; p++) {
		if (*p == '\\' || *p == '/')
			base = p + 1;
	}
	if (g_expect_fatal)
		TNote("expected fatal error: %s: %s (%s:%d)", func ? func : "?", error ? error : "?", base, line);
	else
		TCheck(false, "no fatal error", base, line, "FATAL ERROR %s: %s", func ? func : "?", error ? error : "?");
	TestRaiseFatal();
}

void TExpectFatal(bool expected)
{
	g_expect_fatal = expected;
}

int32_t TFatalCount(void)
{
	return g_fatals;
}

bool TestActive(void)
{
	return g_active;
}

void TestRunGameSuites(void)
{
	bool have_base = false;

	for (int32_t s = 0; TEST_SUITES[s]; ++s)
		for (const struct test_reg *t = TEST_SUITES[s]; t->suite; ++t)
			if (t->needs_game && matches(t)) {
				if (!have_base)
					have_base = TestSave("host_base");
				else
					TestLoad("host_base");
				run_one(t);
			}
	if (have_base) {
		TestPump(500);
		TestScreenshot("screen_after_tests");
		TestSnapChildWindows();
		TestLoad("host_base");
		TestPump(1500);
		TestScreenshot("screen_after_load");
		TestListChildWindows(out_line);
	}
	out("SUMMARY tests=%d failed=%d known=%d checks=%d failures=%d known_failures=%d seed=%u\n", g_tests_run,
		g_tests_failed, g_tests_known, g_checks, g_fails, g_knowns, g_seed);
	if (g_out)
		fclose(g_out);
	TestExitProcess(g_tests_failed);
}

void TestStart(void)
{
	const char *cmd = TestCommandLine();
	const char *p = strstr(cmd, "/test=");
	bool any_game = false;
	int32_t i;

	if (!p)
		return;
	p += 6;
	for (i = 0; p[i] && p[i] != ',' && p[i] != ' ' && p[i] != '"' && i < (int32_t) sizeof(g_filter) - 1; ++i)
		g_filter[i] = p[i];
	g_filter[i] = 0;
	p += i;
	if (*p == ',') {
		g_seed = (uint32_t) strtoul(p + 1, NULL, 10);
		while (*p && *p != ' ' && *p != '"')
			++p;
	}
	if ((p = strstr(cmd, "/stat=")) != NULL && atoi(p + 6) > 0)
		g_stat_runs = atoi(p + 6);
	g_active = true;
	open_out();
	if (!strcmp(g_filter, "list")) {
		for (int32_t s = 0; TEST_SUITES[s]; ++s)
			for (const struct test_reg *t = TEST_SUITES[s]; t->suite; ++t)
				out("%s.%s%s\n", t->suite, t->name, t->needs_game ? "  (game)" : "");
		if (g_out)
			fclose(g_out);
		TestExitProcess(0);
	}
	out("RUN %s seed=%u stat=%d\n", g_filter, g_seed, g_stat_runs);
	for (int32_t s = 0; TEST_SUITES[s]; ++s)
		for (const struct test_reg *t = TEST_SUITES[s]; t->suite; ++t) {
			if (!matches(t))
				continue;
			if (t->needs_game)
				any_game = true;
			else
				run_one(t);
		}
	if (any_game) {
		TestGameStart();
		return;
	}
	TestRunGameSuites();
}

#endif
