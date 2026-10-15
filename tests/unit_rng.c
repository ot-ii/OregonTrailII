#include "testframework.h"

#ifdef OTII_TEST

#include <limits.h>

#include "../src/base/rng.h"

TEST(unit_rng, bounds)
{
	static const int32_t ranges[][2] = {
		{1, 1}, {0, 1}, {1, 2}, {1, 6}, {1, 10}, {1, 100}, {0, 99}, {-5, 5}, {-100, -1}, {10, 40}, {800, 1000},
		{1, 1000}, {0, 32766}, {0, 32767}
	};

	for (int32_t r = 0; r < (int32_t) (sizeof(ranges) / sizeof(ranges[0])); ++r)
		for (int32_t count = 1; count <= 3; ++count) {
			int32_t lo = ranges[r][0], hi = ranges[r][1], seen_lo = INT32_MAX, seen_hi = INT32_MIN, bad = 0;
			TCase("%d..%d count %d", lo, hi, count);
			for (int32_t i = 0; i < TStatRuns(); ++i) {
				int32_t v = RandRangeAvg(lo, hi, count);
				if (v < lo || v > hi)
					++bad;
				if (v < seen_lo)
					seen_lo = v;
				if (v > seen_hi)
					seen_hi = v;
			}
			CHECK_MSG(bad == 0, "%d results outside, seen %d..%d", bad, seen_lo, seen_hi);
			if (hi - lo < 50 && count == 1) {
				CHECK_EQ(seen_lo, lo);
				CHECK_EQ(seen_hi, hi);
			}
		}
}

TEST(unit_rng, uniform_1_100)
{
	static int32_t bucket[101];
	int32_t n = TStatRuns() * 50;
	int32_t v;

	for (v = 0; v <= 100; ++v)
		bucket[v] = 0;
	for (int32_t i = 0; i < n; ++i) {
		v = RandRangeAvg(1, 100, 1);
		if (v >= 1 && v <= 100)
			++bucket[v];
	}
	for (v = 1; v <= 100; ++v) {
		TCase("value %d", v);
		CHECK_RATE(bucket[v], n, 0.01);
	}
}

TEST(unit_rng, averaged_rolls)
{
	for (int32_t count = 1; count <= 4; ++count) {
		int32_t n = TStatRuns() * 10, ends = 0;
		double sum = 0;
		TCase("1..100 count %d", count);
		for (int32_t i = 0; i < n; ++i) {
			int32_t v = RandRangeAvg(1, 100, count);
			sum += v;
			if (v <= 10 || v >= 91)
				++ends;
		}
		CHECK_NEAR(sum / n, 50.5 - (count - 1) / (2.0 * count), 1.0);
		if (count == 1)
			CHECK_RATE(ends, n, 0.20);
		else
			CHECK_MSG(ends < n * 0.20 * 0.75, "%d of %d at the ends", ends, n);
	}
}

TEST(unit_rng, empty_and_reversed_ranges)
{
	CHECK_EQ(RandRangeAvg(5, 5, 1), 5);
	CHECK_EQ(RandRangeAvg(7, 3, 1), 7);
	CHECK_EQ(RandRangeAvg(-3, -4, 2), -3);
}

TEST(unit_rng, zero_and_negative_count)
{
	int32_t v;

	v = RandRangeAvg(1, 10, 0);
	CHECK_RANGE(v, 1, 10);
	v = RandRangeAvg(1, 10, -1);
	CHECK_RANGE(v, 1, 10);
}

TEST(unit_rng, wide_range)
{
	int32_t top = 0;

	for (int32_t i = 0; i < TStatRuns() * 5; ++i) {
		int32_t v = RandRangeAvg(0, 99999, 1);
		if (v > top)
			top = v;
	}
	CHECK_MSG(top > 90000, "highest of %d rolls in 0..99999 was %d", TStatRuns() * 5, top);
}

TEST(unit_rng, one_in_10000)
{
	int32_t n = TStatRuns() * 500, low = 0;

	for (int32_t i = 0; i < n; ++i)
		if (RandRangeAvg(1, 10000, 1) <= 100)
			++low;
	CHECK_RATE(low, n, 0.01);
}

TEST(unit_rng, seed_reproducible)
{
	int32_t first[64];
	bool same = true;

	RngSetSeed(4242);
	for (int32_t i = 0; i < 64; ++i)
		first[i] = RandRangeAvg(1, 1000, 1 + i % 3);
	RngSetSeed(4242);
	for (int32_t i = 0; i < 64; ++i)
		if (RandRangeAvg(1, 1000, 1 + i % 3) != first[i])
			same = false;
	CHECK(same);
	CHECK_EQ(RngSeed(), 4242);
	RngSetSeed(4243);
	same = true;
	for (int32_t i = 0; i < 64; ++i)
		if (RandRangeAvg(1, 1000, 1 + i % 3) != first[i])
			same = false;
	CHECK(!same);
}

TEST(unit_rng, call_count)
{
	RngSetSeed(99);
	CHECK_EQ(RngCalls(), 0);
	RandRangeAvg(1, 6, 1);
	CHECK_EQ(RngCalls(), 2);
	RandRangeAvg(1, 6, 3);
	CHECK_EQ(RngCalls(), 8);
	RandRangeAvg(4, 4, 3);
	CHECK_EQ(RngCalls(), 14);
	RandRangeAvg(5, 4, 3);
	CHECK_EQ(RngCalls(), 14);
	for (int32_t i = 0; i < 100; ++i)
		RandRangeAvg(1, 100, 2);
	CHECK_EQ(RngCalls(), 414);
}

TEST(unit_rng, restore_continues_stream)
{
	int32_t values[40];

	RngSetSeed(777);
	for (int32_t i = 0; i < 40; ++i)
		values[i] = RandRangeAvg(1, 30000, 1);
	for (int32_t k = 0; k < 40; k += 7) {
		TCase("restore at roll %d", k);
		RngRestore(777, (uint64_t) (2 * k));
		CHECK_EQ(RngCalls(), 2 * k);
		CHECK_EQ(RandRangeAvg(1, 30000, 1), values[k]);
	}
}

const struct test_reg TESTS_unit_rng[] = {
	TEST_ENTRY(unit_rng, bounds),
	TEST_ENTRY(unit_rng, uniform_1_100),
	TEST_ENTRY(unit_rng, averaged_rolls),
	TEST_ENTRY(unit_rng, empty_and_reversed_ranges),
	TEST_ENTRY(unit_rng, zero_and_negative_count),
	TEST_ENTRY(unit_rng, wide_range),
	TEST_ENTRY(unit_rng, one_in_10000),
	TEST_ENTRY(unit_rng, seed_reproducible),
	TEST_ENTRY(unit_rng, call_count),
	TEST_ENTRY(unit_rng, restore_continues_stream),
	TEST_END
};

#endif
