#pragma once

#include <stdbool.h>
#include <stdint.h>

enum month {
	MONTH_JANUARY = 1,
	MONTH_FEBRUARY,
	MONTH_MARCH,
	MONTH_APRIL,
	MONTH_MAY,
	MONTH_JUNE,
	MONTH_JULY,
	MONTH_AUGUST,
	MONTH_SEPTEMBER,
	MONTH_OCTOBER,
	MONTH_NOVEMBER,
	MONTH_DECEMBER,
};

struct date {
	int32_t month;
	int32_t day;
	int32_t year;
};

struct date DateMake(int32_t month, int32_t day, int32_t year);
void DateInit(struct date *d);
void DateSetMonth(struct date *d, int32_t m);
void DateSetDay(struct date *d, int32_t day);
void DateSetYear(struct date *d, int32_t y);
void DateNormalize(struct date *d);
int32_t DateDayOfWeek(const struct date *d);

bool DateEqual(const struct date *a, const struct date *b);
bool DateLess(const struct date *a, const struct date *b);
bool DateLessEqual(const struct date *a, const struct date *b);
bool DateGreater(const struct date *a, const struct date *b);
bool DateGreaterEqual(const struct date *a, const struct date *b);

bool DateIsLeapYear(int32_t y);
int32_t DateDaysInYear(int32_t y);
int32_t DateDaysInMonth(int32_t month, int32_t year);
