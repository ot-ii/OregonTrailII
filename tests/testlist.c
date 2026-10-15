#include "testframework.h"

#ifdef OTII_TEST

extern const struct test_reg TESTS_scn_crowd[];
extern const struct test_reg TESTS_scn_daily[];
extern const struct test_reg TESTS_scn_endgame[];
extern const struct test_reg TESTS_scn_events[];
extern const struct test_reg TESTS_scn_hill[];
extern const struct test_reg TESTS_scn_host[];
extern const struct test_reg TESTS_scn_hunt[];
extern const struct test_reg TESTS_scn_illness[];
extern const struct test_reg TESTS_scn_nodes[];
extern const struct test_reg TESTS_scn_raft[];
extern const struct test_reg TESTS_scn_river[];
extern const struct test_reg TESTS_scn_saveload[];
extern const struct test_reg TESTS_scn_scaler[];
extern const struct test_reg TESTS_scn_store[];
extern const struct test_reg TESTS_scn_towns[];
extern const struct test_reg TESTS_scn_twitch[];
extern const struct test_reg TESTS_scn_twitchplays[];
extern const struct test_reg TESTS_unit_autopilot[];
extern const struct test_reg TESTS_unit_cinepak[];
extern const struct test_reg TESTS_unit_crowd[];
extern const struct test_reg TESTS_unit_data[];
extern const struct test_reg TESTS_unit_date[];
extern const struct test_reg TESTS_unit_ini[];
extern const struct test_reg TESTS_unit_inv[];
extern const struct test_reg TESTS_unit_mov[];
extern const struct test_reg TESTS_unit_quickstart[];
extern const struct test_reg TESTS_unit_rng[];
extern const struct test_reg TESTS_unit_rsrc[];
extern const struct test_reg TESTS_unit_scaler[];
extern const struct test_reg TESTS_unit_snd[];
extern const struct test_reg TESTS_unit_str[];
extern const struct test_reg TESTS_unit_twitch[];

const struct test_reg *const TEST_SUITES[] = {
	TESTS_scn_crowd,
	TESTS_scn_daily,
	TESTS_scn_endgame,
	TESTS_scn_events,
	TESTS_scn_hill,
	TESTS_scn_host,
	TESTS_scn_hunt,
	TESTS_scn_illness,
	TESTS_scn_nodes,
	TESTS_scn_raft,
	TESTS_scn_river,
	TESTS_scn_saveload,
	TESTS_scn_scaler,
	TESTS_scn_store,
	TESTS_scn_towns,
	TESTS_scn_twitch,
	TESTS_scn_twitchplays,
	TESTS_unit_autopilot,
	TESTS_unit_cinepak,
	TESTS_unit_crowd,
	TESTS_unit_data,
	TESTS_unit_date,
	TESTS_unit_ini,
	TESTS_unit_inv,
	TESTS_unit_mov,
	TESTS_unit_quickstart,
	TESTS_unit_rng,
	TESTS_unit_rsrc,
	TESTS_unit_scaler,
	TESTS_unit_snd,
	TESTS_unit_str,
	TESTS_unit_twitch,
	NULL,
};

#endif
