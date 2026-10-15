#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/list.h"
#include "../base/object.h"

struct cstr;
struct strrsrc;

enum msg_id {
	MSG_BLIZZARD = 0,
	MSG_SNOWBOUND = 1,
	MSG_THUNDERSTORM = 2,
	MSG_HAILSTORM = 3,
	MSG_HEAVY_FOG = 4,
	MSG_FALLEN_TIMBERS = 5,
	MSG_FALLEN_ROCKS = 6,
	MSG_FLOODED_TRAIL = 7,
	MSG_BROKEN_WHEEL = 8,
	MSG_BROKEN_AXLE = 9,
	MSG_BROKEN_TONGUE = 10,
	MSG_MISSING_ANIMAL = 11,
	MSG_MISSING_ANIMALS = 12,
	MSG_MISSING_PERSON = 13,
	MSG_SICK_LIVESTOCK = 14,
	MSG_INJURED_LIVESTOCK = 15,
	MSG_WILD_FRUIT = 16,
	MSG_ABANDONED_WAGON = 17,
	MSG_BUFFALO_STAMPEDE = 18,
	MSG_QUICKSAND_AHEAD = 19,
	MSG_ANIMAL_IN_QUICKSAND = 20,
	MSG_PRAIRIE_FIRE = 21,
	MSG_DUSTSTORM = 22,
	MSG_LOCUSTS = 23,
	MSG_EXTREME_HEAT = 24,
	MSG_EXTREME_COLD = 25,
	MSG_INDIANS_AHEAD = 26,
	MSG_STRANGERS_AHEAD = 27,
	MSG_ACCIDENTAL_GUNSHOT = 28,
	MSG_ANIMAL_BITE = 29,
	MSG_ANIMAL_MAULING = 30,
	MSG_WAGON_FIRE = 31,
	MSG_THEFT = 32,
	MSG_CROWDED_RIVER = 33,
	MSG_FERRY_DELAY = 34,
	MSG_TIPPED_WAGON = 35,
	MSG_ANIMAL_DIED_ACCIDENT = 36,
	MSG_ANIMAL_DIED_ILLNESS = 37,
	MSG_STUCK_IN_MUD = 38,
	MSG_STUCK_IN_SAND = 39,
	MSG_HILL_TOO_STEEP = 40,
	MSG_ANIMALS_EXHAUSTED = 41,
	MSG_CANNOT_PAY_TOLL = 42,
	MSG_THICK_DUST = 43,
	MSG_INDEPENDENCE_DAY = 44,
	MSG_SUNDAY = 45,
	MSG_CUTS_AND_ABRASIONS = 46,
	MSG_ALKALI_SICKNESS = 47,
	MSG_ANIMAL_ALKALI_SICKNESS = 48,
	MSG_BROKEN_BONE = 49,
	MSG_SPRAIN = 50,
	MSG_LOST_SUPPLIES = 51,
	MSG_CHOLERA = 52,
	MSG_BAD_COLD = 53,
	MSG_CONCUSSION = 54,
	MSG_DYSENTERY = 55,
	MSG_THE_GRIPPE = 56,
	MSG_MALARIA = 57,
	MSG_MEASLES = 58,
	MSG_MOUNTAIN_FEVER = 59,
	MSG_PNEUMONIA = 60,
	MSG_BERIBERI = 61,
	MSG_DIPHTHERIA = 62,
	MSG_FROSTBITE = 63,
	MSG_GANGRENE = 64,
	MSG_SCURVY = 65,
	MSG_SMALLPOX = 66,
	MSG_SNAKEBITE = 67,
	MSG_TYPHOID_FEVER = 68,
	MSG_RABIES = 69,
	MSG_TYPHUS = 70,
	MSG_SCARLET_FEVER = 71,
	MSG_FOOD_POISONING = 72,
	MSG_INFECTION = 73,
	MSG_CONSUMPTION = 74,
	MSG_BURNS = 75,
	MSG_DROWNING = 76,
	MSG_ELECTED_CAPTAIN = 77,
	MSG_LOW_MORALE = 78,
	MSG_DISMISSED = 79,
	MSG_REELECTED = 80,
	MSG_HIRED_AS_GUIDE = 81,
	MSG_VERY_LOW_MORALE = 82,
	MSG_FIRED = 83,
	MSG_TRAIN_SPLITTING = 84,
	MSG_WAGONS_JOIN = 85,
	MSG_MEMBER_DIED = 86,
	MSG_MOSQUITOES = 87,
	MSG_ANIMAL_IN_HOLE = 88,
	MSG_FREEZING = 89,
	MSG_STARVATION = 90,
	MSG_BROKEN_YOKE = 91,
	MSG_LOCKJAW = 92,
	MSG_INTERNAL_INJURIES = 93,
	MSG_WOUND = 94,
	MSG_WATER_POISONING = 95,
	MSG_THIRST = 96,
	MSG_WILD_VEGETABLES = 97,
	MSG_SWAMPED_WAGON = 98,
	MSG_FELL_THROUGH_ICE = 99,
	MSG_CAUGHT_FISH = 100,
	MSG_LEADER_DIED = 101,
	MSG_RAFTING_LOSSES = 102,
	MSG_UNKNOWN_AILMENT = 1004,
};

enum node_feature {
	NODEFEAT_NO_GRASS = 1006,
	NODEFEAT_DUSTY = 1011,
	NODEFEAT_QUICKSAND = 1012,
	NODEFEAT_ROCKFALL = 1013,
	NODEFEAT_RATTLESNAKES = 1014,
	NODEFEAT_h = 1015,
	NODEFEAT_CHOLERA = 1016,
	NODEFEAT_T = 1017,
	NODEFEAT_F = 1018,
	NODEFEAT_P = 1019,
	NODEFEAT_S = 1020,
	NODEFEAT_N = 1021,
	NODEFEAT_H = 1022,
	NODEFEAT_U = 1023,
	NODEFEAT_O = 1024,
	NODEFEAT_SCENERY_SWITCH_ON = 1025,
	NODEFEAT_SCENERY_SWITCH_OFF = 1026,
	NODEFEAT_NO_GREENHORN_ALLOWANCE = 1027,
	NODEFEAT_Z = 1028,
};

#define MSG_FLAG_COUNT 9

struct msg {
	struct object base;
	struct list_node node;
	int32_t number_index;
	int32_t choice_state;
	struct cstr *payload;
	intptr_t member;
	intptr_t subject;
	int32_t rsrc_index;
	struct cstr *title;
	int32_t scene_variant;
	bool flags[MSG_FLAG_COUNT];
	bool journal_posted;
	struct cstr *member_name;
};

extern const struct object_ops MSG_OPS_TABLE;

extern struct strrsrc *g_numberwords_rsrc;

void MsgInit(struct msg *self, int32_t number, struct cstr *text, intptr_t member, intptr_t subject);
struct msg *MsgCreate(int32_t number, struct cstr *text, intptr_t member, intptr_t subject);
void MsgDestroy(struct msg **ctx_mod);
void msg_destroy(struct object *self);

struct msg *MsgFromNode(struct list_node *node);

int32_t MsgGetNumberValue(struct msg *self);
struct cstr *MsgGetSubjectText(struct msg *self);
void MsgReplaceGrammar(struct msg *self, struct cstr *text);
void MsgReplacePlace(struct msg *self, struct cstr *text);
void MsgPresentText(struct msg *self, struct cstr *obj, struct cstr *line1, struct cstr *line2);
void MsgClearFlag(struct msg *self, int32_t i);
void MsgSetFlag(struct msg *self, int32_t i);

void StripTrailingPeriod(struct cstr *s);
