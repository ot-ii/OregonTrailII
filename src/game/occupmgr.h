#pragma once

#include <stdbool.h>
#include <stdint.h>

struct rsrc;
struct strrsrc;

struct occup_row {
	int32_t starting_cash;
	int32_t score_mult_x10;
	int32_t skill[4];
};

struct occup_mgr {
	struct occup_row *table;
	int32_t table_count;
	struct strrsrc *skill_names;
	struct strrsrc *skill_costs;
	struct strrsrc *names;
};

struct occup_mgr *OccupMgrCreate(void);
void OccupMgrDestroy(struct occup_mgr **ctx_mod);

int32_t OccupMgrGetStartingCash(struct occup_mgr *self, int32_t row);
int32_t OccupMgrGetScoreMultX10(struct occup_mgr *self, int32_t row);
bool OccupMgrHasSkill(struct occup_mgr *self, int32_t row, int32_t value);
int32_t OccupMgrGetSkillCost(struct occup_mgr *self, int32_t line);
void OccupMgrBuildTable(struct occup_mgr *self, struct rsrc *raw);
