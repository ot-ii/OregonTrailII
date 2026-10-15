#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "invlist.h"

struct cstr;
struct doc_file;
struct inv_item;
struct member;
struct new_game_scenario;

enum party_skill {
	SKILL_MEDICAL = 0,
	SKILL_RIVERWORK = 1,
	SKILL_SHARPSHOOTING = 2,
	SKILL_BLACKSMITHING = 3,
	SKILL_CARPENTRY = 4,
	SKILL_FARMING = 5,
	SKILL_TRACKING = 6,
	SKILL_BOTANY = 7,
	SKILL_COMMERCE = 8,
	SKILL_COOKING = 9,
	SKILL_MUSICAL = 10,
	SKILL_SEWING = 11,
	SKILL_SPANISH = 12,
};

enum occupation {
	OCCUP_BANKER = 0,
	OCCUP_DOCTOR = 1,
	OCCUP_MERCHANT = 2,
	OCCUP_PHARMACIST = 3,
	OCCUP_WAINWRIGHT = 4,
	OCCUP_GUNSMITH = 5,
	OCCUP_MASON = 6,
	OCCUP_BLACKSMITH = 7,
	OCCUP_WHEELWRIGHT = 8,
	OCCUP_CARPENTER = 9,
	OCCUP_SADDLEMAKER = 10,
	OCCUP_BRICKMAKER = 11,
	OCCUP_PROSPECTOR = 12,
	OCCUP_TRAPPER = 13,
	OCCUP_SURVEYOR = 14,
	OCCUP_SHOEMAKER = 15,
	OCCUP_JOURNALIST = 16,
	OCCUP_PRINTER = 17,
	OCCUP_BUTCHER = 18,
	OCCUP_BAKER = 19,
	OCCUP_TAILOR = 20,
	OCCUP_FARMER = 21,
	OCCUP_PASTOR = 22,
	OCCUP_ARTIST = 23,
	OCCUP_TEACHER = 24,
};

enum rations {
	RATIONS_BARE_BONES = 0,
	RATIONS_MEAGER = 1,
	RATIONS_FILLING = 2,
};

enum pull_mode {
	PULL_SINGLE_EASY = 0,
	PULL_SINGLE_STRAINED = 1,
	PULL_TEAM_EASY = 2,
	PULL_TEAM_STRAINED = 3,
	PULL_CANNOT_MOVE = 4,
};

struct party {
	struct inv_list animals;
	struct inv_list spare_animals;
	struct inv_list wagons;
	struct inv_list inventory;
	int32_t pull_mode;
	struct inv_item *hitched;
	struct member *members[6];
	int32_t member_count;
	int32_t occupation;
	double cash;
	int32_t score_multiplier;
	int32_t unused_skill_points;
	bool skills[16];
	int32_t reputation;
	int32_t days_starving;
	int32_t ration_level;
	int32_t scurvy_risk;
	int32_t beriberi_risk;
	int8_t meat_spoiled;
	int32_t daily_health_drain;
};

struct party *PartyCreateScenario(struct new_game_scenario *scenario);
struct party *PartyCreateQuickStart(int32_t command);
struct party *PartyCreate(void);
void PartyDestroy(struct party **ctx_mod);

void PartySaveState(struct party *self, struct doc_file *file);
void PartyLoadState(struct party *self, struct doc_file *file);
void PartyDailyHealthUpdate(struct party *self, int32_t hours);
void PartyDistributeLoot(struct party *self, struct inv_list *loot);
void PartyLoadItem(struct party *self, struct inv_item *item);
bool PartyUnhitchDraft(struct party *self);
bool PartyUnhitchSpare(struct party *self);
void PartyPurgeEmptyAnimals(struct party *self);
void PartyAddDraftAnimal(struct party *self, struct inv_list *list, const struct cstr *name, int32_t n);
struct inv_list *PartyBuildInventoryList(struct party *self);
void PartyAddWagon(struct party *self, struct inv_list *list, const struct cstr *name, int32_t n);
int32_t PartyCalcPullingMode(struct party *self);
bool PartyConsumeDailyFood(struct party *self);
bool PartyConsumeRandomFood(struct party *self, int32_t group, int32_t amount, bool *hit_group81, bool *hit_group82);
int32_t PartyMemberCount(struct party *self, bool alive_only);
struct member *PartyGetMember(struct party *self, int32_t index);
int32_t PartyGetFoodPounds(struct party *self);
bool PartyHasPlentyOfFood(struct party *self);
int32_t PartyGetLoadUnits(struct party *self);
int32_t PartySumSpares(struct party *self);
int32_t PartyCountFood(struct party *self);
int32_t PartyTotalWagonCapacity(struct party *self);
bool PartyHasDraftAnimals(struct party *self);
void PartyRefreshCatFlags(struct party *self);
int32_t PartyCountDraftAnimals(struct party *self);
void PartyRemoveOneAnimal(struct party *self);

int32_t LevelStringToInt(struct cstr *s);
void InventoryAddMeat3(int32_t pounds);
