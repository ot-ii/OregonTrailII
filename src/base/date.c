#include "date.h"

static const int32_t DAYS_IN_MONTH[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static const int32_t DAYS_BEFORE_MONTH[2][13] = {
	{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
	{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366},
};

struct date DateMake(int32_t month, int32_t day, int32_t year)
{
	struct date d;
	DateInit(&d);
	DateSetYear(&d, year);
	DateSetMonth(&d, month);
	DateSetDay(&d, day);

	return d;
}

void DateInit(struct date *d)
{
	d->month = 1;
	d->day = 1;
	d->year = 1;
}

void DateSetMonth(struct date *d, int32_t m)
{
	d->month = m;
	DateNormalize(d);
}

void DateSetDay(struct date *d, int32_t day)
{
	d->day = day;
	DateNormalize(d);
}

void DateSetYear(struct date *d, int32_t y)
{
	d->year = y;
	DateNormalize(d);
}

void DateNormalize(struct date *d)
{
	if (d->month < 1) {
		int32_t years = d->month / 12 - 1;
		d->year += years;
		d->month -= 12 * years;
	}

	if (d->month > 12) {
		int32_t years = (d->month - 1) / 12;
		d->year += years;
		d->month -= 12 * years;
	}

	int32_t leap = DateIsLeapYear(d->year) ? 1 : 0;
	int32_t day_of_year = DAYS_BEFORE_MONTH[leap][d->month - 1] + d->day;

	while (day_of_year < 1) {
		d->year--;
		day_of_year += DateDaysInYear(d->year);
	}

	while (DateDaysInYear(d->year) < day_of_year) {
		day_of_year -= DateDaysInYear(d->year);
		d->year++;
	}

	leap = DateIsLeapYear(d->year) ? 1 : 0;

	int32_t m = 0;
	while (m < 13 && DAYS_BEFORE_MONTH[leap][m] < day_of_year)
		m++;

	d->month = m;
	d->day = day_of_year - DAYS_BEFORE_MONTH[leap][m - 1];
}

int32_t DateDayOfWeek(const struct date *d)
{
	static const int32_t MONTH_OFFSET[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

	int32_t y = d->year - (d->month < 3);
	int32_t m = d->month >= 1 && d->month <= 12 ? d->month : 1;

	return ((y + y / 4 - y / 100 + y / 400 + MONTH_OFFSET[m - 1] + d->day) % 7 + 7) % 7;
}

static int32_t date_key(const struct date *d)
{
	return d->day + 31 * d->month + 372 * d->year;
}

bool DateEqual(const struct date *a, const struct date *b)
{
	return a->month == b->month && a->day == b->day && a->year == b->year;
}

bool DateLess(const struct date *a, const struct date *b)
{
	return date_key(a) < date_key(b);
}

bool DateLessEqual(const struct date *a, const struct date *b)
{
	return date_key(a) <= date_key(b);
}

bool DateGreater(const struct date *a, const struct date *b)
{
	return date_key(a) > date_key(b);
}

bool DateGreaterEqual(const struct date *a, const struct date *b)
{
	return date_key(a) >= date_key(b);
}

bool DateIsLeapYear(int32_t y)
{
	return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}

int32_t DateDaysInYear(int32_t y)
{
	return DateIsLeapYear(y) ? 366 : 365;
}

int32_t DateDaysInMonth(int32_t month, int32_t year)
{
	if (month < 1 || month > 12)
		return 0;

	int32_t days = DAYS_IN_MONTH[month - 1];

	if (month == 2 && DateIsLeapYear(year))
		days++;

	return days;
}
