#pragma once

#include <stdint.h>

struct doc_file;
struct rsrc;

enum precip_kind {
	PRECIP_NONE = 0,
	PRECIP_RAIN = 1,
	PRECIP_SNOW = 2,
};

enum weather_region {
	REGION_SWEETWATER = 6,
	REGION_LASSEN = 25,
	REGION_NORTH_PLATTE = 26,
	REGION_SIERRA = 29,
};

struct weather_normal {
	int32_t high;
	int32_t low;
	int32_t average;
	int32_t unread;
};

struct weather_precip {
	int32_t chance;
	int32_t total;
};

struct weather_day {
	int32_t high;
	int32_t low;
	int32_t precip;
};

struct weather {
	struct weather_normal *normals;
	int32_t normal_count;
	struct weather_precip *precip_tbl;
	int32_t precip_count;
	int32_t region;
	int32_t month;
	int32_t snow_depth;
	int32_t snow_fall;
	int32_t day_count;
	struct weather_day day[30];
};

struct weather *WeatherCreate(void);
void WeatherDestroy(struct weather **ctx_mod);

void WeatherInit(struct weather *self, int32_t region, int32_t month);
void WeatherSetRegion(struct weather *self, int32_t region);
void WeatherSetMonth(struct weather *self, int32_t month);
void WeatherSimulateDay(struct weather *self);
int32_t WeatherGetHighTemp(struct weather *self, int32_t day);
int32_t WeatherGetLowTemp(struct weather *self, int32_t day);
int32_t WeatherAvgTemp(struct weather *self, int32_t day);
int32_t WeatherGetPrecipType(struct weather *self, int32_t *type, int32_t day);
int32_t WeatherSumPrecip(struct weather *self, int32_t day);
int32_t WeatherCountFreezingDays(struct weather *self);
int32_t WeatherPrecipTotal(struct weather *self);
int32_t WeatherPrecipChance(struct weather *self);
int32_t WeatherNormalHigh(struct weather *self);
int32_t WeatherNormalLow(struct weather *self);
int32_t WeatherNormalAverage(struct weather *self);
void WeatherUpdateFlags(struct weather *self);
void WeatherLoadTables(struct weather *self);
void WeatherBuildTempTable(struct weather *self, struct rsrc *raw);
void WeatherBuildPrecipTable(struct weather *self, struct rsrc *raw);
void WeatherLoadState(struct weather *self, struct doc_file *file);
void WeatherSaveState(struct weather *self, struct doc_file *file);
