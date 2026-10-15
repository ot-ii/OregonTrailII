#include "testframework.h"

#ifdef OTII_TEST

#include "../src/base/date.h"

static int32_t leap(int32_t y)
{
	return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

static int32_t month_days(int32_t m, int32_t y)
{
	static const int32_t days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	return days[m - 1] + (m == 2 && leap(y));
}

static int32_t ordinal(int32_t y, int32_t m, int32_t d)
{
	int32_t n = 0;

	for (int32_t i = 1800; i < y; ++i)
		n += 365 + leap(i);
	for (int32_t i = 1; i < m; ++i)
		n += month_days(i, y);
	return n + d - 1;
}

static void from_ordinal(int32_t n, int32_t *y, int32_t *m, int32_t *d)
{
	*y = 1800;
	while (n >= 365 + leap(*y)) {
		n -= 365 + leap(*y);
		++*y;
	}
	*m = 1;
	while (n >= month_days(*m, *y)) {
		n -= month_days(*m, *y);
		++*m;
	}
	*d = n + 1;
}

TEST(unit_date, leap_years)
{
	for (int32_t y = 1800; y <= 2004; ++y) {
		TCase("%d", y);
		CHECK_EQ(!!DateIsLeapYear(y), leap(y));
		CHECK_EQ(DateDaysInYear(y), 365 + leap(y));
	}
}

TEST(unit_date, month_lengths)
{
	for (int32_t y = 1836; y <= 1872; ++y)
		for (int32_t m = 1; m <= 12; ++m) {
			TCase("%d-%02d", y, m);
			CHECK_EQ(DateDaysInMonth(m, y), month_days(m, y));
		}
}

TEST(unit_date, normalize_day_overflow)
{
	int32_t base = ordinal(1840, 1, 1), bad = 0;

	for (int32_t k = -400; k < 11700 && bad < 10; ++k) {
		struct date d = DateMake(1, 1, 1840);
		int32_t y, m, dd;
		d.day = 1 + k;
		DateNormalize(&d);
		from_ordinal(base + k, &y, &m, &dd);
		if (d.year != y || d.month != m || d.day != dd) {
			TCase("1840-01-01 %+d days", k);
			CHECK_MSG(0, "got %d-%02d-%02d, want %d-%02d-%02d", d.year, d.month, d.day, y, m, dd);
			++bad;
		}
	}
	CHECK_EQ(bad, 0);
}

TEST(unit_date, normalize_month_overflow)
{
	int32_t bad = 0;

	for (int32_t m = -40; m <= 60; ++m) {
		struct date d = DateMake(m, 15, 1850);
		int32_t want_y = 1850 + (m - 1 >= 0 ? (m - 1) / 12 : -((12 - m) / 12));
		int32_t want_m = ((m - 1) % 12 + 12) % 12 + 1;
		DateNormalize(&d);
		if (d.year != want_y || d.month != want_m || d.day != 15) {
			TCase("month %d of 1850", m);
			CHECK_MSG(0, "got %d-%02d-%02d, want %d-%02d-15", d.year, d.month, d.day, want_y, want_m);
			++bad;
		}
	}
	CHECK_EQ(bad, 0);
}

TEST(unit_date, normalize_edges)
{
	static const struct { int32_t m, d, y, wm, wd, wy; } cases[] = {
		{2, 29, 1848, 2, 29, 1848}, {2, 29, 1849, 3, 1, 1849}, {2, 30, 1848, 3, 1, 1848},
		{3, 0, 1848, 2, 29, 1848}, {3, 0, 1849, 2, 28, 1849}, {1, 0, 1850, 12, 31, 1849},
		{12, 32, 1850, 1, 1, 1851}, {13, 1, 1850, 1, 1, 1851}, {0, 1, 1850, 12, 1, 1849},
		{14, 29, 1851, 2, 29, 1852}, {1, 366, 1848, 12, 31, 1848}, {1, 367, 1848, 1, 1, 1849},
		{1, -364, 1849, 1, 2, 1848}, {12, 31, 1860, 12, 31, 1860},
	};

	for (int32_t i = 0; i < (int32_t) (sizeof(cases) / sizeof(cases[0])); ++i) {
		struct date d = DateMake(cases[i].m, cases[i].d, cases[i].y);
		TCase("%d-%02d-%02d", cases[i].y, cases[i].m, cases[i].d);
		DateNormalize(&d);
		CHECK_EQ(d.year, cases[i].wy);
		CHECK_EQ(d.month, cases[i].wm);
		CHECK_EQ(d.day, cases[i].wd);
	}
}

TEST(unit_date, day_of_week)
{
	int32_t prev = -1, bad = 0, saturday;
	struct date jan1 = DateMake(1, 1, 1848);

	saturday = DateDayOfWeek(&jan1);
	CHECK_EQ(saturday, 6);
	for (int32_t k = 0; k < 11000 && bad < 5; ++k) {
		int32_t y, m, dd, w;
		from_ordinal(ordinal(1840, 1, 1) + k, &y, &m, &dd);
		struct date d = DateMake(m, dd, y);
		w = DateDayOfWeek(&d);
		if (w < 0 || w > 6 || (prev >= 0 && w != (prev + 1) % 7)) {
			TCase("%d-%02d-%02d", y, m, dd);
			CHECK_MSG(0, "weekday %d after %d", w, prev);
			++bad;
		}
		if ((ordinal(y, m, dd) - ordinal(1848, 1, 1)) % 7 == 0 && w != saturday) {
			TCase("%d-%02d-%02d", y, m, dd);
			CHECK_MSG(0, "a Saturday gave %d, not %d", w, saturday);
			++bad;
		}
		prev = w;
	}
	CHECK_EQ(bad, 0);
}

TEST(unit_date, comparisons)
{
	static const int32_t dates[][3] = {
		{1848, 1, 1}, {1848, 1, 2}, {1848, 2, 1}, {1849, 1, 1}, {1847, 12, 31}, {1848, 12, 31}, {1860, 6, 15},
		{1840, 3, 1}, {1848, 2, 29},
	};
	int32_t n = (int32_t) (sizeof(dates) / sizeof(dates[0]));

	for (int32_t i = 0; i < n; ++i)
		for (int32_t j = 0; j < n; ++j) {
			struct date a = DateMake(dates[i][1], dates[i][2], dates[i][0]);
			struct date b = DateMake(dates[j][1], dates[j][2], dates[j][0]);
			int32_t oa = ordinal(dates[i][0], dates[i][1], dates[i][2]);
			int32_t ob = ordinal(dates[j][0], dates[j][1], dates[j][2]);
			TCase("%d-%02d-%02d vs %d-%02d-%02d", dates[i][0], dates[i][1], dates[i][2], dates[j][0], dates[j][1],
				dates[j][2]);
			CHECK_EQ(!!DateEqual(&a, &b), oa == ob);
			CHECK_EQ(!DateEqual(&a, &b), oa != ob);
			CHECK_EQ(!!DateLess(&a, &b), oa < ob);
			CHECK_EQ(!!DateLessEqual(&a, &b), oa <= ob);
			CHECK_EQ(!!DateGreater(&a, &b), oa > ob);
			CHECK_EQ(!!DateGreaterEqual(&a, &b), oa >= ob);
		}
}

TEST(unit_date, assign_and_setters)
{
	struct date a = DateMake(5, 17, 1852), b;

	DateInit(&b);
	b = a;
	CHECK(DateEqual(&a, &b));
	DateSetMonth(&b, 7);
	DateSetDay(&b, 4);
	DateSetYear(&b, 1853);
	CHECK_EQ(b.month, 7);
	CHECK_EQ(b.day, 4);
	CHECK_EQ(b.year, 1853);
	CHECK(DateLess(&a, &b));
}

const struct test_reg TESTS_unit_date[] = {
	TEST_ENTRY(unit_date, leap_years),
	TEST_ENTRY(unit_date, month_lengths),
	TEST_ENTRY(unit_date, normalize_day_overflow),
	TEST_ENTRY(unit_date, normalize_month_overflow),
	TEST_ENTRY(unit_date, normalize_edges),
	TEST_ENTRY(unit_date, day_of_week),
	TEST_ENTRY(unit_date, comparisons),
	TEST_ENTRY(unit_date, assign_and_setters),
	TEST_END
};

#endif
