//

#pragma once

#ifdef OTII_TEST

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*TestFunc)(void);

struct test_reg {
	const char *suite;
	const char *name;
	TestFunc fn;
	bool needs_game;
};

#define TEST(suite, name) static void test_##suite##_##name(void)
#define TEST_GAME(suite, name) TEST(suite, name)
#define TEST_ENTRY(suite, name) {#suite, #name, test_##suite##_##name, false}
#define TEST_GAME_ENTRY(suite, name) {#suite, #name, test_##suite##_##name, true}
#define TEST_END {0, 0, 0, false}

void TCheck(bool ok, const char *expr, const char *file, int32_t line, const char *fmt, ...);
void TCase(const char *fmt, ...);
void TKnown(const char *id);
void TNote(const char *fmt, ...);
uint32_t TSeed(void);
int32_t TStatRuns(void);

#define CHECK(c) TCheck(!!(c), #c, __FILE__, __LINE__, 0)
#define CHECK_MSG(c, ...) TCheck(!!(c), #c, __FILE__, __LINE__, __VA_ARGS__)
#define CHECK_EQ(a, b) do { int64_t t_a_ = (int64_t) (a), t_b_ = (int64_t) (b); \
	TCheck(t_a_ == t_b_, #a " == " #b, __FILE__, __LINE__, "%" PRId64 " vs %" PRId64, t_a_, t_b_); } while (0)
#define CHECK_RANGE(v, lo, hi) do { int64_t t_v_ = (int64_t) (v); \
	TCheck(t_v_ >= (int64_t) (lo) && t_v_ <= (int64_t) (hi), #v " in [" #lo ", " #hi "]", __FILE__, __LINE__, \
		"%" PRId64, t_v_); } while (0)
#define CHECK_NEAR(a, b, tol) do { double t_a_ = (double) (a), t_b_ = (double) (b); \
	TCheck(t_a_ - t_b_ <= (tol) && t_b_ - t_a_ <= (tol), #a " ~ " #b, __FILE__, __LINE__, "%g vs %g (tol %g)", \
		t_a_, t_b_, (double) (tol)); } while (0)
#define CHECK_RATE(hits, n, p) TCheckRate((hits), (n), (p), #hits, __FILE__, __LINE__)
void TCheckRate(int32_t hits, int32_t n, double p, const char *expr, const char *file, int32_t line);

bool TGuarded(void (*fn)(void *), void *arg);

void TFatal(const char *func, const char *error, const char *file, int32_t line);
void TExpectFatal(bool expected);
int32_t TFatalCount(void);

void TestStart(void);
bool TestActive(void);
void TestRunGameSuites(void);

extern const struct test_reg *const TEST_SUITES[];

bool TestCallGuarded(void (*fn)(void *), void *arg, uint32_t *code, uint32_t *rva);
void TestRaiseFatal(void);
#define TEST_FATAL_EXCEPTION 0xE0FA7A10u
const char *TestCommandLine(void);
void TestExeBase(char *path, size_t size);
void TestExitProcess(int32_t code);
void TestSnapChildWindows(void);
void TestListChildWindows(void (*out)(const char *line));
void TestGameStart(void);

#endif
