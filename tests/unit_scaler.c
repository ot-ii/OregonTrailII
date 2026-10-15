#include "testframework.h"

#ifdef OTII_TEST

#include "../src/ui/scaler.h"

TEST(unit_scaler, floor_division)
{
	CHECK_EQ(ScalerFloorDiv(5, 1), 5);
	CHECK_EQ(ScalerFloorDiv(-5, 1), -5);
	CHECK_EQ(ScalerFloorDiv(3, 2), 1);
	CHECK_EQ(ScalerFloorDiv(-1, 2), -1);
	CHECK_EQ(ScalerFloorDiv(-2, 2), -1);
	CHECK_EQ(ScalerFloorDiv(-3, 2), -2);
	CHECK_EQ(ScalerFloorDiv(7, 4), 1);
	CHECK_EQ(ScalerFloorDiv(-4, 4), -1);
	CHECK_EQ(ScalerFloorDiv(0, 3), 0);
	for (int32_t s = 1; s <= SCALER_MAX; ++s)
		for (int32_t v = -40; v <= 40; ++v)
			for (int32_t r = 0; r < s; ++r) {
				TCase("scale %d, game %d, pixel %d of it", s, v, r);
				CHECK_EQ(ScalerFloorDiv(v * s + r, s), v);
			}
	TCase("");
}

TEST(unit_scaler, largest_fit)
{
	CHECK_EQ(ScalerLargestFit(638, 440, 16, 59, 3840, 2100), 4);
	CHECK_EQ(ScalerLargestFit(638, 440, 16, 59, 2560, 1400), 3);
	CHECK_EQ(ScalerLargestFit(638, 440, 16, 59, 1920, 1160), 2);
	CHECK_EQ(ScalerLargestFit(638, 440, 16, 59, 1280, 720), 1);
	CHECK_EQ(ScalerLargestFit(638, 440, 16, 59, 600, 400), 1);
	CHECK_EQ(ScalerLargestFit(100, 100, 0, 0, 300, 300), 3);
	CHECK_EQ(ScalerLargestFit(100, 100, 0, 0, 299, 400), 2);
}

const struct test_reg TESTS_unit_scaler[] = {
	TEST_ENTRY(unit_scaler, floor_division),
	TEST_ENTRY(unit_scaler, largest_fit),
	TEST_END
};

#endif
