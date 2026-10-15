#include "rng.h"

#include <stdbool.h>

#include "log.h"

static uint32_t g_rng_state;
static uint32_t g_rng_seed;
static uint64_t g_rng_calls;
static bool g_rng_seeded;
static RngTimeFunc g_rng_time;
static RngTraceFunc g_rng_trace;

void RngSetTimeSource(RngTimeFunc func)
{
	g_rng_time = func;
}

void RngSetTrace(RngTraceFunc func)
{
	g_rng_trace = func;
}

static int32_t crt_rand(void)
{
	g_rng_state = g_rng_state * 214013u + 2531011u;

	return (int32_t) ((g_rng_state >> 16) & 0x7FFF);
}

void RngSetSeed(uint32_t seed)
{
	g_rng_seed = seed;
	g_rng_calls = 0;
	g_rng_state = seed;
	g_rng_seeded = true;
}

void RngRestore(uint32_t seed, uint64_t calls)
{
	RngSetSeed(seed);

	while (g_rng_calls < calls) {
		crt_rand();
		g_rng_calls++;
	}
}

static void seed_if_needed(void)
{
	if (!g_rng_seeded)
		RngSetSeed(g_rng_time ? g_rng_time() : 0);
}

uint32_t RngSeed(void)
{
	seed_if_needed();

	return g_rng_seed;
}

uint64_t RngCalls(void)
{
	return g_rng_calls;
}

// A 30-bit number from two generator calls, redrawn in the incomplete last stretch,
// so every value of the range is equally likely.
// Before the RNG was biased heavily
static int32_t rng_uniform(int32_t range)
{
	const uint32_t span = 1u << 30;
	uint32_t limit = span - span % (uint32_t) range;
	uint32_t v;

	do {
		uint32_t high = (uint32_t) crt_rand();
		uint32_t low = (uint32_t) crt_rand();
		v = high << 15 | low;
		g_rng_calls += 2;
	} while (v >= limit);

	return (int32_t) (v % (uint32_t) range);
}

int32_t RandRangeAvg(int32_t lo, int32_t hi, int32_t count)
{
	int32_t range = hi - lo + 1;

	if (range <= 0)
		return lo;

	if (count <= 0)
		count = 1;

	seed_if_needed();

	int32_t sum = 0;

	for (int32_t i = 0; i < count; i++)
		sum += rng_uniform(range);

	int32_t result = lo + sum / count;

	if (g_rng_trace)
		g_rng_trace(lo, hi, count, result);

	return result;
}

void RandSelect(int32_t lo, int32_t hi, int32_t k, int32_t *out)
{
	if (hi <= lo)
		LogError(LOG_ASSERT, "invalid range");

	if (hi - lo + 1 < k)
		LogError(LOG_ASSERT, "invalid return count");

	int32_t picked = 0;

	for (int32_t candidate = hi - k + 1; candidate <= hi; candidate++) {
		int32_t roll = RandRangeAvg(lo, candidate, 1);
		int32_t scan = 0;

		while (scan < picked && out[scan] != roll)
			scan++;

		out[picked++] = scan == picked ? roll : candidate;
	}
}
