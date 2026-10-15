#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/object.h"

struct cstr;
struct doc_file;
struct oblist;

enum member_state {
	MEMBER_DEAD = 0,
	MEMBER_MISSING = 1,
	MEMBER_ALIVE = 2,
};

enum health_band {
	HEALTH_BAD = 0,
	HEALTH_POOR = 1,
	HEALTH_FAIR = 2,
	HEALTH_OK = 3,
	HEALTH_GOOD = 4,
};

struct member {
	struct object base;
	struct oblist *nodes;
	struct cstr *name;
	int32_t age;
	int32_t unused_E;
	int32_t unused_10;
	int32_t health;
	int32_t hours_accum;
	int32_t max_health;
	int32_t stamina;
	int32_t state;
	bool had_ailment_42;
	bool had_ailment_28;
	bool had_ailment_29;
	bool had_ailment_17;
	bool had_ailment_34;
	bool had_ailment_32;
	int32_t collapse_threshold;
	int32_t pending_ailment;
	int32_t pending_stage;
	int32_t health_band;
};

extern const struct object_ops MEMBER_OPS_TABLE;

void MemberInit(struct member *self, int32_t unused, struct cstr *name, int32_t age, int32_t a4, int32_t a5);
void MemberInitEmpty(struct member *self);
struct member *MemberCreate(int32_t unused, struct cstr *name, int32_t age, int32_t a4, int32_t a5);
struct member *MemberCreateEmpty(void);
void MemberDestroy(struct member **ctx_mod);
void member_destroy(struct object *self);

void MemberClearPendingAilment(struct member *self);
bool MemberAddAilment(struct member *self, int32_t ailment_id, int32_t start_stage);
void MemberPurgeExpiredAilments(struct member *self);
bool MemberHealthTick(struct member *self, int32_t hours, int32_t a3);
bool MemberHasAilment(struct member *self, int32_t ailment_id);
int32_t MemberGetHealthBand(struct member *self);
struct cstr *MemberGetStatusText(struct member *self);
void MemberSetPendingAilment(struct member *self, int32_t ailment_id, int32_t start_stage);
void MemberSaveState(struct member *self, struct doc_file *file);
void MemberLoadState(struct member *self, struct doc_file *file);
void MemberApplyPendingAilment(struct member *self);
struct cstr *MemberBuildAilmentText(struct member *self);
