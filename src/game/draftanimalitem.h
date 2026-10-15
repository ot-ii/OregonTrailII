#pragma once

#include <stdint.h>

#include "invitem.h"

struct cstr;

struct draft_animal_row {
	char name[17];
	int32_t speed;
	int32_t strained_lb;
	int32_t easy_lb;
	int32_t per_wagon;
	int32_t pace_risk;
	int32_t unread1;
	int32_t wander_risk;
	int32_t cold_risk;
	int32_t heat_risk;
	int32_t hole_risk;
	int32_t storm_risk;
	int32_t unread2;
	int32_t unread3;
	int32_t feed_need;
};

struct draft_animal_table {
	int32_t count;
	struct draft_animal_row *rows;
};

struct draft_animal_item {
	struct inv_item base;
	int32_t table_row;
};

extern struct draft_animal_table *g_draftanimal_table;

extern const struct inv_item_ops DRAFT_ANIMAL_ITEM_OPS_TABLE;

int32_t DraftAnimalItemGetSpeed(struct draft_animal_item *self);
int32_t DraftAnimalItemGetStrainedLoad(struct draft_animal_item *self);
int32_t DraftAnimalItemGetEasyLoad(struct draft_animal_item *self);
int32_t DraftAnimalItemStrainedTeamLoad(struct draft_animal_item *self, int32_t team);
int32_t DraftAnimalItemEasyTeamLoad(struct draft_animal_item *self, int32_t team);
int32_t DraftAnimalItemWorkUnits(struct draft_animal_item *self, int32_t team);
int32_t DraftAnimalItemGetPerWagon(struct draft_animal_item *self);
int32_t DraftAnimalItemGetPaceRisk(struct draft_animal_item *self);
int32_t DraftAnimalItemGetUnread1(struct draft_animal_item *self);
int32_t DraftAnimalItemGetWanderRisk(struct draft_animal_item *self);
int32_t DraftAnimalItemGetColdRisk(struct draft_animal_item *self);
int32_t DraftAnimalItemGetHeatRisk(struct draft_animal_item *self);
int32_t DraftAnimalItemGetHoleRisk(struct draft_animal_item *self);
int32_t DraftAnimalItemGetStormRisk(struct draft_animal_item *self);
int32_t DraftAnimalItemGetUnread2(struct draft_animal_item *self);
int32_t DraftAnimalItemGetUnread3(struct draft_animal_item *self);
int32_t DraftAnimalItemGetFeedNeed(struct draft_animal_item *self);

void DraftAnimalItemInit(struct draft_animal_item *self, const struct cstr *nm, int32_t qty);
struct draft_animal_item *DraftAnimalItemCreate(const struct cstr *nm, int32_t qty);
void DraftAnimalItemDestroy(struct draft_animal_item **ctx_mod);
void draft_animal_item_destroy(struct object *self);

void DraftAnimalItemLoadDraftAnimalRsrc(struct draft_animal_item *self);
char *DraftAnimalItemGetName(struct draft_animal_item *self, char *out);

void ReleaseDraftanimalRsrc(void);
