#include "sceneassets.h"

#include "../game/game.h"
#include "../trail/node.h"
#include "../game/weather.h"

static const int32_t g_sky_sets_snow[4] = {21310, 21510, 21800, 21920};
static const int32_t g_sky_sets_rain[7] = {21310, 21510, 21800, 21920, 21200, 21500, 21910};
static const int32_t g_sky_sets_clear[11] = {21110, 21600, 21400, 21940, 21100, 21210, 21300, 21410, 21700, 21900, 21930};
static const int32_t g_variant_snow_n = 4;
static const int32_t g_variant_rain_n = 7;
static const int32_t g_variant_clear_n = 11;

int32_t RegionalAssetIdOffset(int32_t base)
{
	return base + GetSkySetId() + 1;
}

int32_t GetSkySetId(void)
{
	struct game *game_o;
	struct game *game2_f;
	struct weather *weather_d;
	int32_t precip_kind_k = 0;
	int32_t variant_id_j;
	int32_t node;
	struct node_base *cur_node_b;

	game_o = GetGameObj();
	weather_d = game_o->weather;
	WeatherGetPrecipType(weather_d, &precip_kind_k, 0);
	game2_f = GetGameObj();
	cur_node_b = game2_f->cur_node;
	node = cur_node_b->rand_seed;
	switch (precip_kind_k) {
		case PRECIP_NONE:
			variant_id_j = g_sky_sets_clear[node % g_variant_clear_n];
			break;
		case PRECIP_RAIN:
			variant_id_j = g_sky_sets_rain[node % g_variant_rain_n];
			break;
		case PRECIP_SNOW:
			variant_id_j = g_sky_sets_snow[node % g_variant_snow_n];
			break;
		default:
			variant_id_j = g_sky_sets_clear[node % g_variant_clear_n];
			break;
	}
	return variant_id_j;
}

int32_t RegionalAssetId1000(void)
{
	return GetSkySetId() + 1000;
}

int32_t RegionalAssetId2000(void)
{
	return GetSkySetId() + 2000;
}

int32_t RegionalAssetId3000(void)
{
	return GetSkySetId() + 3000;
}
