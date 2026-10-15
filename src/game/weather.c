#include "weather.h"

#include <stdio.h>
#include <string.h>

#include "../data/assets.h"
#include "../base/byteorder.h"
#include "../base/date.h"
#include "../data/docfile.h"
#include "game.h"
#include "../data/gfxdev.h"
#include "../base/log.h"
#include "../screens/mainui.h"
#include "../base/memory.h"
#include "../base/rng.h"
#include "../data/rsrc.h"

struct weather *WeatherCreate(void)
{
	struct weather *self = MemAlloc(1, sizeof(struct weather));

	self->region = -1;
	self->snow_fall = 0;
	memset(self->day, 0, sizeof(self->day));
	self->snow_depth = 0;
	self->month = -1;
	self->day_count = 0;
	WeatherLoadTables(self);

	return self;
}

void WeatherDestroy(struct weather **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct weather *ctx = *ctx_mod;
	*ctx_mod = NULL;

	MemFree(ctx->normals);
	MemFree(ctx->precip_tbl);

	MemFree(ctx);
}

void WeatherInit(struct weather *self, int32_t region, int32_t month)
{
	struct date date = DateMake(month, -60, 1964);

	WeatherSetRegion(self, region);
	WeatherSetMonth(self, date.month);

	int32_t high = (WeatherNormalHigh(self) - WeatherNormalAverage(self)) / 2 + WeatherNormalAverage(self);
	int32_t low = WeatherNormalAverage(self) - (WeatherNormalAverage(self) - WeatherNormalLow(self)) / 2;

	for (int32_t day_index = 0; day_index < 30; day_index++) {
		self->day[day_index].high = high;
		self->day[day_index].low = low;
		self->day[day_index].precip = 0;
	}

	for (int32_t day_index = 0; day_index < 60; day_index++) {
		WeatherSetMonth(self, date.month);
		WeatherSimulateDay(self);
		DateSetDay(&date, date.day + 1);
	}

	WeatherSetMonth(self, month);
}

void WeatherSetRegion(struct weather *self, int32_t region)
{
	if (self->region != region) {
		struct date *cur_date = GetGameObj()->date;
		struct date date = DateMake(cur_date->month, cur_date->day - 60, cur_date->year);

		self->region = region;

		for (int32_t day_index = 0; day_index < 60; day_index++) {
			WeatherSetMonth(self, date.month);
			WeatherSimulateDay(self);
			DateSetDay(&date, date.day + 1);
		}

		WeatherSetMonth(self, cur_date->month);

	} else {
		self->region = region;
	}
}

void WeatherSetMonth(struct weather *self, int32_t month)
{
	if (self->month != month - 1) {
		self->month = month - 1;
		self->day_count = 0;
	}
}

void WeatherSimulateDay(struct weather *self)
{
	int32_t normal_high = WeatherNormalHigh(self);
	int32_t normal_average = WeatherNormalAverage(self);
	int32_t norm_low = WeatherNormalLow(self);
	double normal_chance = WeatherPrecipChance(self) / 100.0;
	int32_t month_total = WeatherPrecipTotal(self);
	int32_t year = GetGameObj()->date->year;
	int32_t month_day = GetGameObj()->date->day;
	int32_t month_num = self->month + 1;

	for (int32_t day_index = 29; day_index > 0; day_index--)
		self->day[day_index] = self->day[day_index - 1];

	int32_t high_ceiling = self->day[1].high + 17 < normal_high ? self->day[1].high + 17 : normal_high;
	int32_t high_floor = normal_average - 17 > norm_low ? normal_average - 17 : norm_low;
	high_floor = self->day[1].high - 20 > high_floor ? self->day[1].high - 20 : high_floor;

	if (high_floor > high_ceiling)
		high_floor = high_ceiling - 3;

	int32_t new_high = (2 * RandRangeAvg(high_floor, high_ceiling, 1) + self->day[1].high) / 3;

	if (self->snow_depth) {
		self->day[0].high = new_high - 3;

	} else {
		self->day[0].high = new_high;
	}

	if (self->day[0].high < high_floor)
		self->day[0].high = high_floor;

	int32_t temp_range = normal_high - norm_low;
	double drop_pct = RandRangeAvg(20, 40, 1) / 100.0;
	int32_t low_drop = (int32_t) (temp_range * drop_pct);
	int32_t low_ceiling = self->day[0].high - 2;
	int32_t low_floor = self->day[0].high - low_drop > norm_low ? self->day[0].high - low_drop : norm_low;
	int32_t new_low = (2 * RandRangeAvg(low_floor, low_ceiling, 1) + self->day[1].low) / 3;

	if (new_low >= new_high)
		new_low = new_high - 1;

	self->day[0].low = new_low;

	double chance = normal_chance;

	if (self->day[1].precip) {
		chance = chance * 1.25;

	} else {
		chance = chance * 0.75;
	}

	self->day[0].precip = 0;

	if (year == 1850 && month_num > MONTH_MARCH && month_num < MONTH_NOVEMBER &&
		(self->region == REGION_NORTH_PLATTE || self->region == REGION_SWEETWATER || self->region < 5)) {
		chance = chance / 3.0;
		normal_chance = normal_chance / 3.0;
		month_total /= 3;
	}

	if (chance * 100.0 >= (double) RandRangeAvg(1, 100, 1)) {
		int32_t precip_days = (int32_t) (normal_chance * 31.0);

		if (precip_days < 1)
			precip_days = 1;

		int32_t avg_precip = month_total / precip_days;
		int32_t precip_so_far = WeatherSumPrecip(self, self->day_count);
		int32_t roll = RandRangeAvg(0, 100, 1);
		int32_t threshold = (int32_t) (month_total * 0.85);

		if (precip_so_far > month_total) {
			self->day[0].precip = 1;

		} else if (roll > 50 || precip_so_far > threshold) {
			self->day[0].precip = RandRangeAvg(1, 2 * avg_precip, 1);

		} else if (roll > 40) {
			self->day[0].precip = RandRangeAvg((int32_t) (month_total * 0.3), month_total, 1);

		} else {
			self->day[0].precip = RandRangeAvg(1, (int32_t) (month_total * 0.3), 1);
		}
	}

	if (year == 1844 && (month_num == MONTH_APRIL || month_num == MONTH_MAY)) {
		if (self->region < 3 && RandRangeAvg(0, 99, 1) < 85)
			self->day[0].precip = RandRangeAvg(3, 10, 1);

	} else if (year == 1846 && (self->region == REGION_SIERRA || self->region == REGION_LASSEN) &&
		month_num == MONTH_OCTOBER && month_day > 27) {
		if (self->snow_depth < 300)
			self->snow_depth = 300;

		self->day[0].precip = RandRangeAvg(5, 15, 1);
		self->day[0].low = RandRangeAvg(-5, 5, 1);
		self->day[0].high = RandRangeAvg(10, 20, 1);

	} else if (year == 1849 && month_num == MONTH_JUNE && month_day > 8 && month_day < 12 &&
		self->region == REGION_NORTH_PLATTE) {
		self->day[0].precip = RandRangeAvg(7, 15, 1);

	} else if (year == 1856 && self->region == REGION_SWEETWATER && month_num == MONTH_NOVEMBER &&
		(month_day == 3 || month_day == 4)) {
		self->day[0].precip = RandRangeAvg(15, 20, 1);
		self->day[0].low = RandRangeAvg(-5, 5, 1);
		self->day[0].high = RandRangeAvg(10, 20, 1);
	}

	int32_t precip_kind;
	int32_t precip_amount = WeatherGetPrecipType(self, &precip_kind, 0);

	self->snow_fall = 0;

	if (precip_kind == PRECIP_SNOW) {
		self->snow_fall = 6 * precip_amount;
		self->snow_depth += self->snow_fall;
	}

	if (self->snow_depth) {
		int32_t melt = 0;
		double melt_rate = 0.0;

		if (precip_kind == PRECIP_RAIN)
			melt += 4 * precip_amount;

		if (WeatherGetLowTemp(self, 0) > 32) {
			melt_rate = (WeatherGetHighTemp(self, 0) - 32) * 9.0;

		} else if (WeatherAvgTemp(self, 0) > 32) {
			melt_rate = (WeatherGetHighTemp(self, 0) - 32) * 6.0;

		} else if (WeatherGetHighTemp(self, 0) > 32) {
			melt_rate = (WeatherGetHighTemp(self, 0) - 32) * 3.0;
		}

		melt_rate = melt_rate / 100.0 + 0.03;
		melt += (int32_t) (self->snow_depth * melt_rate);

		if (!melt && melt_rate > 0.03)
			melt = 1;

		if (self->snow_depth < melt)
			melt = self->snow_depth;

		self->snow_depth -= melt;
		self->day[1].precip += melt / 6;
	}

	++self->day_count;
}

int32_t WeatherGetHighTemp(struct weather *self, int32_t day)
{
	return self->day[day].high;
}

int32_t WeatherGetLowTemp(struct weather *self, int32_t day)
{
	return self->day[day].low;
}

int32_t WeatherAvgTemp(struct weather *self, int32_t day)
{
	return (self->day[day].low + self->day[day].high) / 2;
}

int32_t WeatherGetPrecipType(struct weather *self, int32_t *type, int32_t day)
{
	if (!self->day[day].precip) {
		*type = PRECIP_NONE;

	} else if (WeatherAvgTemp(self, day) < 30) {
		*type = PRECIP_SNOW;

	} else {
		*type = PRECIP_RAIN;
	}

	return self->day[day].precip;
}

int32_t WeatherSumPrecip(struct weather *self, int32_t day)
{
	int32_t sum = 0;

	if (day >= 30)
		day = 29;

	for (int32_t day_index = 0; day_index <= day; day_index++)
		sum += self->day[day_index].precip;

	return sum;
}

int32_t WeatherCountFreezingDays(struct weather *self)
{
	int32_t freezing_days = 0;

	for (int32_t day_index = 0; day_index < 30; day_index++) {
		if (WeatherAvgTemp(self, day_index) <= 32)
			++freezing_days;
	}

	return freezing_days;
}

static int32_t table_index(struct weather *self, int32_t count)
{
	int32_t index = self->month + 12 * self->region;

	if (index < 0 || index >= count)
		return -1;

	return index;
}

int32_t WeatherPrecipTotal(struct weather *self)
{
	int32_t index = table_index(self, self->precip_count);

	return index < 0 ? 0 : self->precip_tbl[index].total;
}

int32_t WeatherPrecipChance(struct weather *self)
{
	int32_t index = table_index(self, self->precip_count);

	return index < 0 ? 0 : self->precip_tbl[index].chance;
}

int32_t WeatherNormalHigh(struct weather *self)
{
	int32_t index = table_index(self, self->normal_count);

	return index < 0 ? 0 : self->normals[index].high;
}

int32_t WeatherNormalLow(struct weather *self)
{
	int32_t index = table_index(self, self->normal_count);

	return index < 0 ? 0 : self->normals[index].low;
}

int32_t WeatherNormalAverage(struct weather *self)
{
	int32_t index = table_index(self, self->normal_count);

	return index < 0 ? 0 : self->normals[index].average;
}

void WeatherUpdateFlags(struct weather *self)
{
	int32_t kind;

	WeatherGetPrecipType(self, &kind, 0);

	switch (kind) {
		case 0:
			MainUiClearCatFlag(CAT_SNOWING);
			MainUiClearCatFlag(CAT_RAINY);
			break;
		case 1:
			MainUiSetCatFlag(CAT_RAINY);
			MainUiClearCatFlag(CAT_SNOWING);
			break;
		case 2:
			MainUiSetCatFlag(CAT_SNOWING);
			MainUiClearCatFlag(CAT_RAINY);
			break;
	}
}

static void parse_temp_table(struct weather *self, struct rsrc *raw)
{
	const uint8_t *data = RsrcData(raw);

	self->normal_count = RsrcSize(raw) / 4;
	self->normals = MemAlloc((size_t) self->normal_count + 1, sizeof(struct weather_normal));

	for (int32_t i = 0; i < self->normal_count; i++) {
		self->normals[i].high = (int8_t) data[4 * i + 0];
		self->normals[i].low = (int8_t) data[4 * i + 1];
		self->normals[i].average = (int8_t) data[4 * i + 2];
		self->normals[i].unread = (int8_t) data[4 * i + 3];
	}
}

static void parse_precip_table(struct weather *self, struct rsrc *raw)
{
	const uint8_t *data = RsrcData(raw);

	self->precip_count = RsrcSize(raw) / 2;
	self->precip_tbl = MemAlloc((size_t) self->precip_count + 1, sizeof(struct weather_precip));

	for (int32_t i = 0; i < self->precip_count; i++) {
		self->precip_tbl[i].chance = (int8_t) data[2 * i + 0];
		self->precip_tbl[i].total = (int8_t) data[2 * i + 1];
	}
}

void WeatherLoadTables(struct weather *self)
{
	struct rsrc *normals = AssetsGetElement(g_data_lib, "TEMP", 2300, true);

	if (normals && RsrcStorage(normals) == RSRC_TEXT) {
		LogError(LOG_ERROR, "Attempt load a raw resource");

	} else if (normals) {
		parse_temp_table(self, normals);
	}

	RsrcDestroy(&normals);

	struct rsrc *precip = AssetsGetElement(g_data_lib, "PRECP", 2300, true);

	if (precip && RsrcStorage(precip) == RSRC_TEXT) {
		LogError(LOG_ERROR, "Attempt load a raw resource");

	} else if (precip) {
		parse_precip_table(self, precip);
	}

	RsrcDestroy(&precip);
}

static char *next_text_line(char **text, bool *done)
{
	char *line = *text;
	char *end;

	for (end = line; *end != 13 && *end; ++end)
		continue;

	if (!*end) {
		*done = true;

	} else {
		*end = 0;
	}

	*text = end + 1;

	return line;
}

void WeatherBuildTempTable(struct weather *self, struct rsrc *raw)
{
	int32_t rows = 30;
	char *text = (char *) RsrcData(raw);
	struct weather_normal *buf = MemAlloc((size_t) (12 * rows), sizeof(struct weather_normal));
	int32_t row = 0;
	bool done = false;
	bool skipped_header = false;

	while (!done) {
		if (!skipped_header) {
			while (*text != 13)
				++text;

			++text;
			skipped_header = true;
		}

		char *line = next_text_line(&text, &done);

		if (row == rows) {
			++rows;
			buf = MemRealloc(buf, (size_t) (12 * rows), sizeof(struct weather_normal));
		}

		char name[4];
		int32_t highs[12], averages[12], lows[12];
		int32_t n = sscanf(line,
			"%3[^\t] %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d "
			"%d %d %d %d",
			name, &highs[0], &averages[0], &lows[0], &highs[1], &averages[1], &lows[1], &highs[2], &averages[2],
			&lows[2], &highs[3], &averages[3], &lows[3], &highs[4], &averages[4], &lows[4], &highs[5], &averages[5],
			&lows[5], &highs[6], &averages[6], &lows[6], &highs[7], &averages[7], &lows[7], &highs[8], &averages[8],
			&lows[8], &highs[9], &averages[9], &lows[9], &highs[10], &averages[10], &lows[10], &highs[11],
			&averages[11], &lows[11]);

		if (n < 37)
			break;

		for (int32_t j = 0; j < 12; ++j) {
			buf[j + 12 * row].high = (int8_t) highs[j];
			buf[j + 12 * row].average = (int8_t) averages[j];
			buf[j + 12 * row].low = (int8_t) lows[j];
		}

		++row;
	}

	MemFree(self->normals);
	self->normals = buf;
	self->normal_count = 12 * rows;
}

void WeatherBuildPrecipTable(struct weather *self, struct rsrc *raw)
{
	int32_t rows = 30;
	char *text = (char *) RsrcData(raw);
	struct weather_precip *buf = MemAlloc((size_t) (12 * rows), sizeof(struct weather_precip));
	int32_t row = 0;
	bool done = false;
	bool skipped_header = false;

	while (!done) {
		if (!skipped_header) {
			while (*text != 13)
				++text;

			++text;
			skipped_header = true;
		}

		char *line = next_text_line(&text, &done);

		if (rows == row) {
			++rows;
			buf = MemRealloc(buf, (size_t) (12 * rows), sizeof(struct weather_precip));
		}

		char name[4];
		int32_t chances[12];
		float amounts[12];
		int32_t n = sscanf(line,
			"%3[^\t] %d %f %d %f %d %f %d %f %d %f %d %f %d %f %d %f %d %f %d %f %d %f %d %f",
			name, &chances[0], &amounts[0], &chances[1], &amounts[1], &chances[2], &amounts[2], &chances[3],
			&amounts[3], &chances[4], &amounts[4], &chances[5], &amounts[5], &chances[6], &amounts[6], &chances[7],
			&amounts[7], &chances[8], &amounts[8], &chances[9], &amounts[9], &chances[10], &amounts[10],
			&chances[11], &amounts[11]);

		if (n < 25)
			break;

		for (int32_t j = 0; j < 12; ++j) {
			buf[j + 12 * row].chance = (int8_t) chances[j];
			buf[j + 12 * row].total = (int8_t) (int32_t) (amounts[j] * 10.0f);
		}

		++row;
	}

	MemFree(self->precip_tbl);
	self->precip_tbl = buf;
	self->precip_count = 12 * rows;
}

static int32_t block_word(const uint8_t *blk, int32_t size, int32_t i)
{
	if (!blk || 2 * i + 2 > size)
		return 0;

	return ReadLe16(blk + 2 * i);
}

void WeatherLoadState(struct weather *self, struct doc_file *file)
{
	void *mem = NULL;
	int32_t size = 0;

	DocFileReadObject2(file, &mem, &size);
	const uint8_t *blk = mem;
	int32_t word = 0;

	self->region = block_word(blk, size, word++);
	self->month = block_word(blk, size, word++);
	self->snow_depth = block_word(blk, size, word++);
	self->snow_fall = block_word(blk, size, word++);
	self->day_count = block_word(blk, size, word++);

	for (int32_t day_index = 0; day_index < 30; day_index++) {
		self->day[day_index].high = block_word(blk, size, word++);
		self->day[day_index].low = block_word(blk, size, word++);
		self->day[day_index].precip = block_word(blk, size, word++);
	}

	MemFree(mem);
}

void WeatherSaveState(struct weather *self, struct doc_file *file)
{
	uint8_t blk[190] = {0};
	uint8_t *cursor = blk;

	WriteLe16(cursor, self->region);
	cursor += 2;
	WriteLe16(cursor, self->month);
	cursor += 2;
	WriteLe16(cursor, self->snow_depth);
	cursor += 2;
	WriteLe16(cursor, self->snow_fall);
	cursor += 2;
	WriteLe16(cursor, self->day_count);
	cursor += 2;

	for (int32_t day_index = 0; day_index < 30; day_index++) {
		WriteLe16(cursor, self->day[day_index].high);
		cursor += 2;
		WriteLe16(cursor, self->day[day_index].low);
		cursor += 2;
		WriteLe16(cursor, self->day[day_index].precip);
		cursor += 2;
	}

	DocFileWriteObject(file, blk, 190);
}
