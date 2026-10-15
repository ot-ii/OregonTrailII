#include "member.h"

#include <string.h>

#include "ailment.h"
#include "../base/byteorder.h"
#include "../base/cstr.h"
#include "../data/docfile.h"
#include "../app/event.h"
#include "../app/evthandler.h"
#include "gamecmd.h"
#include "../data/gfxdev.h"
#include "../base/list.h"
#include "../base/memory.h"
#include "../base/rng.h"
#include "../data/strrsrc.h"

const struct object_ops MEMBER_OPS_TABLE = {
	member_destroy,
};

void MemberInit(struct member *self, int32_t unused, struct cstr *name, int32_t age, int32_t a4, int32_t a5)
{
	(void) unused;

	ObjectInit(&self->base);
	self->base.ops = &MEMBER_OPS_TABLE;

	self->health = 0;
	self->hours_accum = self->health;
	self->nodes = OblistCreate(ObjectDelete);
	MemberClearPendingAilment(self);
	self->health_band = HEALTH_GOOD;
	self->name = name;
	self->age = age;
	self->unused_E = a4;
	self->unused_10 = a5;

	if (self->age <= 10) {
		self->max_health = 800;
		self->stamina = 80;

	} else if (self->age <= 20) {
		self->max_health = 900;
		self->stamina = 100;

	} else if (self->age <= 40) {
		self->max_health = 1000;
		self->stamina = 100;

	} else if (self->age <= 50) {
		self->max_health = 900;
		self->stamina = 80;

	} else {
		self->max_health = 800;
		self->stamina = 70;
	}

	self->health = self->max_health;
	self->collapse_threshold = 0;
	self->had_ailment_42 = false;

	int32_t roll = RandRangeAvg(1, 100, 1);
	int32_t limit = RandRangeAvg(35, 75, 1);
	self->had_ailment_28 = roll <= limit;

	self->had_ailment_29 = false;

	roll = RandRangeAvg(1, 100, 1);
	limit = RandRangeAvg(35, 75, 1);
	self->had_ailment_17 = roll <= limit;

	roll = RandRangeAvg(1, 100, 1);
	limit = RandRangeAvg(1, 20, 1);
	self->had_ailment_34 = roll <= limit;

	self->had_ailment_32 = false;
	self->state = MEMBER_ALIVE;
}

void MemberInitEmpty(struct member *self)
{
	ObjectInit(&self->base);
	self->base.ops = &MEMBER_OPS_TABLE;

	MemberClearPendingAilment(self);
	self->health_band = HEALTH_GOOD;
	self->nodes = OblistCreate(ObjectDelete);
}

struct member *MemberCreate(int32_t unused, struct cstr *name, int32_t age, int32_t a4, int32_t a5)
{
	struct member *self = MemAlloc(1, sizeof(struct member));

	MemberInit(self, unused, name, age, a4, a5);

	return self;
}

struct member *MemberCreateEmpty(void)
{
	struct member *self = MemAlloc(1, sizeof(struct member));

	MemberInitEmpty(self);

	return self;
}

void MemberDestroy(struct member **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void member_destroy(struct object *self)
{
	struct member *m = (struct member *) self;

	OblistDestroy(&m->nodes);

	if (m->name)
		CStrDestroy(&m->name);

	object_destroy(self);
}

bool MemberHealthTick(struct member *self, int32_t hours, int32_t a3)
{
	struct ailment *worst_ailment = NULL;
	int32_t worst_damage = 0;
	bool died = false;

	MemberClearPendingAilment(self);

	if (self->health) {
		self->hours_accum += hours;

		if (self->hours_accum >= 24) {
			self->hours_accum -= 24;

			struct ailment *ailment = OblistFirst(self->nodes);

			while (ailment) {
				int32_t daily_damage = AILMENT_OPS(ailment)->Tick(ailment);

				if (ailment->stage == STAGE_ACUTE && (daily_damage > worst_damage || !worst_damage)) {
					worst_damage = daily_damage;
					worst_ailment = ailment;
				}

				if (self->health > 0)
					self->health -= daily_damage;

				ailment = OblistNext(self->nodes, true);
			}

			if (self->health > 0)
				self->health = self->stamina - a3 + self->health;

			if (self->max_health < self->health)
				self->health = self->max_health;

			if (self->max_health / 2 > self->health) {
				if (--self->max_health < 700)
					self->max_health = 700;

				if (--self->stamina < 70)
					self->stamina = 70;
			}

			if (self->collapse_threshold >= self->health && self->health > 0 && worst_ailment) {
				if (worst_ailment->worsened) {
					self->health = 0;

				} else {
					worst_ailment->worsened = true;
					self->collapse_threshold = self->health / 2;

					struct cmd_event *worsen_event = CmdEventCreate(GCMD_AILMENT_WORSENED);
					worsen_event->param1 = (intptr_t) self;
					worsen_event->param2 = (intptr_t) worst_ailment;
					DispatchGlobalEvent(worsen_event);
				}
			}

			if (self->health <= 0 && self->state == MEMBER_ALIVE) {
				struct cmd_event *death_event = CmdEventCreate(GCMD_MEMBER_DIED);
				struct cstr *msg;

				if (worst_ailment) {
					msg = AILMENT_OPS(worst_ailment)->GetText(worst_ailment, AILTEXT_DIED);

				} else {
					msg = CStrCreateCopy(self->name);
					CStrAppendBuf(msg, " died.");
				}

				death_event->param1 = (intptr_t) self;
				death_event->param2 = (intptr_t) msg;
				DispatchGlobalEvent(death_event);
				died = true;
				self->state = MEMBER_DEAD;

				if (!self->state)
					self->health = 0;
			}

			MemberPurgeExpiredAilments(self);
		}
	}

	if (!died) {
		double health_f = (double) self->health;
		double max_health_f = (double) self->max_health;
		double band_value = health_f / max_health_f * 5.0;
		int32_t band = (int32_t) band_value > 1 ? (int32_t) band_value : 1;
		band--;

		struct ailment *first_ailment = OblistFirst(self->nodes);

		if (first_ailment && AILMENT_OPS(first_ailment)->MarkAffected(first_ailment, NULL) && band > HEALTH_FAIR)
			band = HEALTH_FAIR;

		self->health_band = band;
		MemberApplyPendingAilment(self);

	} else {
		self->health_band = HEALTH_BAD;
	}

	return died;
}

bool MemberAddAilment(struct member *self, int32_t ailment_id, int32_t start_stage)
{
	if (self->state != MEMBER_ALIVE)
		return false;

	if (ailment_id == AILMENT_THE_GRIPPE) {
		if (self->had_ailment_42)
			return false;

		self->had_ailment_42 = true;
	}

	if (ailment_id == AILMENT_MEASLES) {
		if (self->had_ailment_28)
			return false;

		self->had_ailment_28 = true;
	}

	if (ailment_id == AILMENT_MOUNTAIN_FEVER) {
		if (self->had_ailment_29)
			return false;

		self->had_ailment_29 = true;
	}

	if (ailment_id == AILMENT_DIPHTHERIA) {
		if (self->had_ailment_17)
			return false;

		self->had_ailment_17 = true;
	}

	if (ailment_id == AILMENT_SMALLPOX) {
		if (self->had_ailment_34)
			return false;

		self->had_ailment_34 = true;
	}

	if (ailment_id == AILMENT_SCARLET_FEVER) {
		if (self->had_ailment_32)
			return false;

		self->had_ailment_32 = true;
	}

	struct ailment *ailment = AilmentCreateById(self, ailment_id, start_stage);

	if (!ailment)
		return false;

	OblistPushFront(self->nodes, ailment);

	return true;
}

void MemberPurgeExpiredAilments(struct member *self)
{
	struct ailment *ailment = OblistFirst(self->nodes);

	while (ailment) {
		struct ailment *next_ailment = OblistNext(self->nodes, true);

		if (AILMENT_OPS(ailment)->RecoverCheck(ailment))
			OblistRemove(self->nodes, ailment);

		ailment = next_ailment;
	}
}

bool MemberHasAilment(struct member *self, int32_t ailment_id)
{
	bool found = false;
	struct ailment *ailment = OblistFirst(self->nodes);

	while (ailment && !found) {
		if (ailment->def && ailment->def->id == ailment_id)
			found = true;

		ailment = OblistNext(self->nodes, true);
	}

	return found;
}

struct cstr *MemberGetStatusText(struct member *self)
{
	bool is_first = true;
	struct cstr *text;

	if (!self->state) {
		text = CStrCreateFrom("dead");

	} else if (self->state == MEMBER_MISSING) {
		text = CStrCreateFrom("missing");

	} else {
		struct strrsrc *symptom_names = StrRsrcLoad(g_text_lib, 16000, true);
		int32_t name_count = symptom_names ? StrRsrcCount(symptom_names) : 0;
		int32_t *counts = MemAlloc((size_t) name_count, sizeof(int32_t));

		struct ailment *ailment = OblistFirst(self->nodes);

		while (ailment) {
			AILMENT_OPS(ailment)->MarkAffected(ailment, counts);
			ailment = OblistNext(self->nodes, true);
		}

		text = CStrCreate();

		char separator[12] = {0};

		if (symptom_names)
			StrRsrcCopyTo(symptom_names, name_count - 2, separator, 10);

		for (int32_t effect_index = 0; name_count - 2 > effect_index; ++effect_index) {
			if (counts[effect_index] > 0) {
				if (!is_first) {
					CStrAppendBuf(text, separator);

				} else {
					is_first = false;
				}

				CStrAppendBuf(text, StrRsrcGetAt(symptom_names, effect_index));
			}
		}

		MemFree(counts);
		StrRsrcDestroy(&symptom_names);

		if (is_first) {
			CStrDestroy(&text);

			switch (MemberGetHealthBand(self)) {
				case HEALTH_GOOD:
					text = CStrCreateFrom("feeling good");
					break;
				case HEALTH_OK:
					text = CStrCreateFrom("feeling OK");
					break;
				case HEALTH_FAIR:
					text = CStrCreateFrom("feeling fair");
					break;
				case HEALTH_POOR:
					text = CStrCreateFrom("feeling poor");
					break;
				case HEALTH_BAD:
					text = CStrCreateFrom("feeling bad");
					break;
			}
		}
	}

	return text;
}

struct cstr *MemberBuildAilmentText(struct member *self)
{
	bool is_first = true;
	struct cstr *text = CStrCreate();
	struct ailment *ailment = OblistFirst(self->nodes);

	while (ailment) {
		if (!is_first) {
			CStrAppendBuf(text, ", ");

		} else {
			is_first = false;
		}

		struct cstr *ailment_name;

		if (ailment->worsened) {
			ailment_name = CStrCreateFrom("a turn for the worse");

		} else {
			ailment_name = StrRsrcGetCStr(ailment->text, AILTEXT_NAME);
		}

		CStrAppendStr(text, ailment_name);
		CStrDestroy(&ailment_name);
		ailment = OblistNext(self->nodes, true);
	}

	return text;
}

int32_t MemberGetHealthBand(struct member *self)
{
	return self->health_band;
}

void MemberLoadState(struct member *self, struct doc_file *file)
{
	void *mem = NULL;
	int32_t size = 0;

	DocFileReadObject2(file, &mem, &size);

	if (size < 0)
		size = 0;

	uint8_t *blk = MemAlloc((size_t) size + 64, 1);

	if (mem && size > 0)
		memcpy(blk, mem, (size_t) size);

	MemFree(mem);

	const uint8_t *cursor = blk;
	int32_t ailment_count = *cursor++;

	self->name = CStrCreateFrom((const char *) cursor);
	cursor += CStrLength(self->name) + 1;

	self->age = *cursor++;
	self->unused_E = *cursor++;
	self->unused_10 = *cursor++;
	self->state = *cursor++;
	self->had_ailment_42 = *cursor++ != 0;
	self->had_ailment_28 = *cursor++ != 0;
	self->had_ailment_29 = *cursor++ != 0;
	self->had_ailment_17 = *cursor++ != 0;
	self->had_ailment_34 = *cursor++ != 0;
	self->had_ailment_32 = *cursor++ != 0;
	self->health = ReadLe16(cursor);
	self->hours_accum = ReadLe16(cursor + 2);
	self->max_health = ReadLe16(cursor + 4);
	self->stamina = ReadLe16(cursor + 6);
	self->collapse_threshold = ReadLe16(cursor + 8);

	MemFree(blk);

	for (int32_t index = 0; ailment_count > index; index++) {
		struct ailment *ailment = AilmentCreateEmpty(self);
		AilmentLoadState(ailment, file);
		OblistPushBack(self->nodes, ailment);
	}
}

void MemberSaveState(struct member *self, struct doc_file *file)
{
	int32_t ailment_count = 0;
	const char *name_text = CStrGet(self->name);
	int32_t name_len = (int32_t) strlen(name_text);

	int32_t blk_size = name_len + 1 + 25;
	uint8_t *blk = MemAlloc((size_t) blk_size, 1);
	uint8_t *cursor = blk;

	struct ailment *ailment = OblistFirst(self->nodes);

	while (ailment) {
		++ailment_count;
		ailment = OblistNext(self->nodes, true);
	}

	*cursor++ = (uint8_t) ailment_count;

	for (int32_t i = 0; name_text[i]; ++i)
		*cursor++ = (uint8_t) name_text[i];

	*cursor++ = 0;
	*cursor++ = (uint8_t) self->age;
	*cursor++ = (uint8_t) self->unused_E;
	*cursor++ = (uint8_t) self->unused_10;
	*cursor++ = (uint8_t) self->state;
	*cursor++ = self->had_ailment_42;
	*cursor++ = self->had_ailment_28;
	*cursor++ = self->had_ailment_29;
	*cursor++ = self->had_ailment_17;
	*cursor++ = self->had_ailment_34;
	*cursor++ = self->had_ailment_32;
	WriteLe16(cursor, self->health);
	WriteLe16(cursor + 2, self->hours_accum);
	WriteLe16(cursor + 4, self->max_health);
	WriteLe16(cursor + 6, self->stamina);
	WriteLe16(cursor + 8, self->collapse_threshold);

	DocFileWriteObject(file, blk, blk_size);
	MemFree(blk);

	ailment = OblistFirst(self->nodes);

	while (ailment) {
		AilmentSaveState(ailment, file);
		ailment = OblistNext(self->nodes, true);
	}
}

void MemberClearPendingAilment(struct member *self)
{
	self->pending_ailment = 0;
	self->pending_stage = 0;
}

void MemberSetPendingAilment(struct member *self, int32_t ailment_id, int32_t start_stage)
{
	self->pending_stage = start_stage;
	self->pending_ailment = ailment_id;
}

void MemberApplyPendingAilment(struct member *self)
{
	if (self->pending_stage)
		MemberAddAilment(self, self->pending_ailment, self->pending_stage);

	MemberClearPendingAilment(self);
}
