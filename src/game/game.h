#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../app/evthandler.h"
#include "../base/geom.h"

struct clip_view;
struct cmd_event;
struct cstr;
struct date;
struct doc_file;
struct end_game;
struct map_view;
struct msg;
struct new_game_scenario;
struct node_base;
struct party;
struct quick_start_choices;
struct region;
struct strrsrc;
struct view;
struct weather;

enum travel_pace {
	PACE_8_HOURS = 0,
	PACE_10_HOURS = 1,
	PACE_EXHAUSTING = 2,
};

enum travel_state {
	TRAVEL_MOVING = 0,
	TRAVEL_RESTING = 1,
	TRAVEL_STOPPED = 3,
};

enum destination {
	DEST_WILLAMETTE = 0,
	DEST_SACRAMENTO = 1,
	DEST_ROGUE_RIVER = 2,
	DEST_SALT_LAKE = 3,
};

enum start_town {
	START_INDEPENDENCE = 0,
	START_ST_JOSEPH = 1,
	START_KANESVILLE = 2,
	START_NAUVOO = 3,
};

enum player_level {
	LEVEL_GREENHORN = 0,
	LEVEL_ADVENTURER = 1,
	LEVEL_TRAIL_GUIDE = 2,
};

enum season {
	SEASON_SUMMER = 0,
	SEASON_AUTUMN = 1,
	SEASON_WINTER = 2,
	SEASON_SNOW = 3,
};

enum node_cmd {
	NODE_CMD_CONTINUE = 0,
	NODE_CMD_ROUTE_1 = 1,
	NODE_CMD_ROUTE_2 = 2,
	NODE_CMD_CAULK_AND_FLOAT = 3,
	NODE_CMD_CHECK_RUINS = 4,
	NODE_CMD_CHECK_CONDITIONS = 5,
	NODE_CMD_GO_DOWN = 6,
	NODE_CMD_CONTINUE_RAFTING = 7,
	NODE_CMD_GO_UP = 8,
	NODE_CMD_CROSS_ICE = 9,
	NODE_CMD_DOUBLE_TEAM = 10,
	NODE_CMD_FORD = 11,
	NODE_CMD_GATHER_HAY = 12,
	NODE_CMD_GO_INTO_TOWN = 13,
	NODE_CMD_HIRE_INDIANS = 14,
	NODE_CMD_HIRE_RAFTERS = 15,
	NODE_CMD_PORTAGE = 16,
	NODE_CMD_LIGHTEN_LOAD = 17,
	NODE_CMD_LOCK_WHEELS = 18,
	NODE_CMD_PASS_QUARANTINE = 19,
	NODE_CMD_PAY_TOLL = 20,
	NODE_CMD_REST = 21,
	NODE_CMD_WAIT_FOR_SPRING = 22,
	NODE_CMD_FERRY = 23,
	NODE_CMD_TOLL_BRIDGE = 24,
	NODE_CMD_LOOK_AROUND = 25,
	NODE_CMD_ROPES_AND_CHAINS = 26,
	NODE_CMD_WAIT_FOR_SUNSET = 27,
	NODE_CMD_ANCHOR_WAGON = 29,
	NODE_CMD_READY_TO_GO = 31,
	NODE_CMD_TALK = 32,
	NODE_CMD_LOOK_FOR_SOMEONE_ELSE = 33,
	NODE_CMD_DONE_LOOKING = 34,
	NODE_CMD_STAKE_LAND_CLAIM = 35,
	NODE_CMD_PROSPECT_FOR_GOLD = 36,
	NODE_CMD_FINAL_SCORE = 37,
	NODE_CMD_SEE_WHAT_LIES_AHEAD = 38,
	NODE_CMD_END_GAME = 39,
	NODE_CMD_SETTLE_HERE = 40,
	NODE_CMD_SAVE_DIARY = 41,
	NODE_CMD_TRAVEL_BY_DAY = 42,
	NODE_CMD_EVENT_END_GAME = 999,
};

enum crossing_outcome {
	CROSS_OUTCOME_SUCCESS = 0,
	CROSS_OUTCOME_TIPPED_DEEP = 1,
	CROSS_OUTCOME_STUCK = 2,
	CROSS_OUTCOME_SWAMPED_DEEP = 3,
	CROSS_OUTCOME_TIPPED = 4,
	CROSS_OUTCOME_SWAMPED = 5,
	CROSS_OUTCOME_THROUGH_ICE = 6,
};

enum choice_cond {
	COND_ALWAYS = -1,
	COND_CAN_AFFORD_FERRY = -2,
	COND_CAN_AFFORD_TOLL = -3,
	COND_CAN_AFFORD_RAFTERS = -4,
	COND_HAS_GUN = -5,
	COND_NO_LAND_CLAIM = -6,
	COND_NO_GOLD_CLAIM = -7,
	COND_NO_CONTAGION = -8,
	COND_NO_RAFTERS_HIRED = -9,
	COND_HILL_BELOW_4 = -10,
	COND_HILL_BELOW_5 = -11,
	COND_HAY_AVAILABLE = -12,
	COND_RUINS_UNCHECKED = -13,
};

#define GAME_PATH_MAX 200

struct game;

struct game_ops {
	struct evt_handler_ops base;
	void (*NodeAction)(struct game *self, struct cmd_event *evt);
	void (*BuildMainScreen)(struct game *self, struct cmd_event *evt);
	void (*RiverCrossing)(struct game *self, struct cmd_event *evt);
	void (*RunNpcConv)(struct game *self, struct cmd_event *evt);
	void (*ExecuteCommand)(struct game *self, struct cmd_event *cmd);
};

struct game {
	struct evt_handler base;
	struct point portrait_anchors[650];
	struct view *doc;
	struct map_view *map_view;
	struct region *region;
	struct strrsrc *node_names;
	struct strrsrc *arrival_journal_lines;
	struct msg *event_node;
	int32_t game_mode;
	struct node_base *cur_node;
	int32_t player_level;
	int32_t days_traveled;
	int32_t event_days;
	int32_t miles_today;
	int32_t total_miles;
	int32_t miles_to_go;
	int32_t end_node_id;
	int32_t start_town;
	int32_t prev_node_id;
	int32_t cur_node_id;
	int32_t next_node_id;
	int32_t destination;
	int32_t route_index;
	int32_t map_id;
	int32_t shown_map_id;
	int32_t path_nodes[GAME_PATH_MAX];
	int32_t path_len;
	int32_t travel_pace;
	int32_t travel_mod;
	int32_t daily_mod;
	int32_t train_wagons;
	int32_t train_people;
	int32_t train_wagons_start;
	int32_t train_people_start;
	int32_t unused_C1E;
	int32_t hired_rafters;
	struct party *party;
	struct weather *weather;
	struct date *date;
	struct date *start_date;
	int32_t travel_state;
	int32_t day_tick;
	int32_t mile_frac;
	struct end_game *end_score;
	int32_t score_multiplier;
	int32_t scenery_switch;
};

#define GAME_OPS(p) ((const struct game_ops *) ((const struct object *) (p))->ops)

extern const struct game_ops GAME_OPS_TABLE;

void GameInit(struct game *self, int32_t game_mode, struct view *doc);
struct game *GameCreate(int32_t game_mode, struct view *doc);
void GameDestroy(struct game **ctx_mod);
void game_destroy(struct object *self);

void game_node_action(struct game *self, struct cmd_event *evt);
void game_build_main_screen(struct game *self, struct cmd_event *evt);
void game_river_crossing(struct game *self, struct cmd_event *evt);
void game_run_npc_conv(struct game *self, struct cmd_event *evt);
void game_execute_command(struct game *self, struct cmd_event *cmd);
void game_on_event(struct evt_handler *self, struct cmd_event *evt);
void game_on_idle_spare(struct evt_handler *self, void *task);

void GameStartNewGame(struct game *self, struct new_game_scenario *scenario);
void GameStartQuickGame(struct game *self, struct quick_start_choices *choices);
int32_t GameRest(struct game *self, int32_t hours, int32_t flag);
void GameTravelTick(struct game *self, bool flag);
void GameLoadState(struct game *self, struct doc_file *file);
void GameSaveState(struct game *self, struct doc_file *file);
void GameUpdateMusic(struct game *self, int32_t cue);
void GameRun(struct game *self, struct msg *msg);
struct view *GameShowEventDialog(struct game *self, int32_t rsrc_id, int32_t asset_id, int32_t sub_id,
	struct cstr *str, struct game *game);
bool GameIsEventAllowed(struct game *self, int32_t cmd);
void GameUpdateCmdFlags(struct game *self);
int32_t GameGetSeason(struct game *self);
void GameSetTravelState(struct game *self, int32_t state);
void GameRiverCrossingAttempt(struct game *self, int32_t method, int32_t node_id, struct clip_view *crossing_view,
	struct cmd_event *evt);
void GameEnterNode(struct game *self, int32_t node_id);
void GameNextNode(struct game *self);

bool GameEvalChoiceCondition(int32_t condition, struct party *caller_party);
void GameRestoreLoadedRng(void);

struct game *GetGameObj(void);

extern struct game *g_game;

#ifdef OTII_TEST
int32_t GameTestNodeSpeakerConv(struct node_base *node, int32_t index);
#endif
extern int32_t g_item_conestoga;

#ifdef OTII_TEST
void GameTestSetFerryWait(bool pending, int32_t days);
bool GameTestFerryWaitPending(void);
#endif
