#include "ailment.h"

#include <string.h>

#include "../data/assets.h"
#include "../base/byteorder.h"
#include "../base/cstr.h"
#include "../data/docfile.h"
#include "../app/event.h"
#include "../app/evthandler.h"
#include "game.h"
#include "gamecmd.h"
#include "../data/gfxdev.h"
#include "../base/list.h"
#include "member.h"
#include "../base/memory.h"
#include "msg.h"
#include "msgcontroller.h"
#include "msgqueue.h"
#include "party.h"
#include "../base/rng.h"
#include "../data/rsrc.h"
#include "../data/saveformat.h"
#include "../data/strrsrc.h"

enum severity_level {
	SEVERITY_KEEP = 0,
	SEVERITY_MIN = 1,
	SEVERITY_MAX = 2,
};

#define HPROB_WORDS (HPROB_SYMPTOMS + 2 * HPROB_SYMPTOM_SLOTS)

#define AILMENT_BLOCK_SIZE 53

static int32_t g_prev_beriberi_risk;
static int32_t g_prev_scurvy_risk;

static int32_t g_ailment_effect_severity;
static int32_t g_ailment_followup24_chance;
static int32_t g_ailment19_keep_chance;
static int32_t g_ailment_followup23_chance;
static int32_t g_ailment27_keep_chance;
static int32_t g_ailment_followup30_chance;
static int32_t g_ailment_followup19_chance;
static int32_t g_ailment_fatal_chance;

const struct ailment_ops AILMENT_OPS_TABLE = {
	{ailment_destroy},
	ailment_check_trigger,
	ailment_tick,
	ailment_recover_check,
	ailment_mark_affected,
	ailment_apply_effects,
	ailment_is_known,
	ailment_get_text,
	ailment_advance_stage,
	ailment_daily_damage,
	ailment_roll_initial,
	ailment_progress_tick,
	ailment_begin,
};

struct hprob_def *HprobLoad(int32_t ailment_id, bool fatal)
{
	struct rsrc *r = AssetsGetElement(g_data_lib, "HPROB", ailment_id + 15000, fatal);

	if (!r)
		return NULL;

	RsrcLoadAndUnpack(r);

	const uint8_t *data = RsrcData(r);
	int32_t size = RsrcSize(r);
	int32_t words[HPROB_WORDS];

	for (int32_t i = 0; i < HPROB_WORDS; i++)
		words[i] = 2 * i + 2 <= size ? ReadLe16(data + 2 * i) : 0;

	RsrcDestroy(&r);

	struct hprob_def *def = MemAlloc(1, sizeof(struct hprob_def));

	def->id = words[HPROB_ID];
	def->category = words[HPROB_CATEGORY];
	def->unread = words[HPROB_UNREAD];
	def->incubation_min = words[HPROB_INCUBATION_MIN];
	def->incubation_max = words[HPROB_INCUBATION_MAX];
	def->acute_days_min = words[HPROB_ACUTE_DAYS_MIN];
	def->acute_days_max = words[HPROB_ACUTE_DAYS_MAX];
	def->recovery_days_min = words[HPROB_RECOVERY_DAYS_MIN];
	def->recovery_days_max = words[HPROB_RECOVERY_DAYS_MAX];
	def->severity_min = words[HPROB_SEVERITY_MIN];
	def->severity_max = words[HPROB_SEVERITY_MAX];
	def->recovery_severity = words[HPROB_RECOVERY_SEVERITY];
	def->flags = words[HPROB_FLAGS];
	def->param1 = words[HPROB_PARAM1];
	def->param2 = words[HPROB_PARAM2];
	def->identify_chance = words[HPROB_IDENTIFY_CHANCE];

	for (int32_t i = 0; i < HPROB_SYMPTOM_SLOTS; i++) {
		def->symptoms[i].id = words[HPROB_SYMPTOMS + 2 * i];
		def->symptoms[i].chance = words[HPROB_SYMPTOMS + 2 * i + 1];
	}

	return def;
}

void AilmentInit(struct ailment *self, struct member *member, struct hprob_def *def, int32_t stage)
{
	ObjectInit(&self->base);
	self->base.ops = &AILMENT_OPS_TABLE.base;

	int32_t symptom_total = 0;
	bool neg_symptom_taken = false;
	bool neg_roll_done = false;

	self->member = member;
	self->def = def;

	for (int32_t i = 0; i < HPROB_SYMPTOM_SLOTS; ++i)
		self->symptoms[i] = 0;

	self->symptom_count = 0;

	for (int32_t i = 0; i < HPROB_SYMPTOM_SLOTS; ++i) {
		if (!self->def->symptoms[i].id)
			break;

		int32_t symptom_id = self->def->symptoms[i].id;
		++symptom_total;
		int32_t chance_percent = self->def->symptoms[i].chance;

		if (chance_percent == 100) {
			self->symptoms[self->symptom_count] = symptom_id;
			self->symptom_count++;

		} else if (chance_percent < 0) {
			if (!neg_roll_done) {
				neg_roll_done = true;

				if (RandRangeAvg(0, 100, 1) < 0 - chance_percent) {
					self->symptoms[self->symptom_count++] = symptom_id;
					neg_symptom_taken = true;
				}

			} else if (!neg_symptom_taken && neg_roll_done) {
				self->symptoms[self->symptom_count] = symptom_id;
				self->symptom_count++;
			}

		} else {
			if (RandRangeAvg(0, 100, 1) < chance_percent) {
				self->symptoms[self->symptom_count] = symptom_id;
				self->symptom_count++;
			}
		}
	}

	if (symptom_total && !self->symptom_count) {
		int32_t pick = RandRangeAvg(0, symptom_total - 1, 1);
		self->symptoms[self->symptom_count] = self->def->symptoms[pick].id;
		self->symptom_count++;
	}

	self->stage = stage;
	self->days_in_stage = 0;
	self->last_treatment = 0;

	if (GetGameObj()->party->skills[SKILL_MEDICAL]) {
		self->is_known = true;

	} else if (self->def->identify_chance > RandRangeAvg(0, 100, 1)) {
		self->is_known = true;

	} else {
		self->is_known = false;
	}

	self->text = StrRsrcLoad(g_text_lib, self->is_known ? self->def->id + 15000 : 15000, true);
	self->worsened = false;
	AILMENT_OPS(self)->ProgressTick(self);
	self->followup_chance_incubation = 0;
	self->followup_chance_acute = 0;
	self->followup_id_incubation = 0;
	self->followup_id_acute = 0;
}

void AilmentInitEmpty(struct ailment *self, struct member *member)
{
	ObjectInit(&self->base);
	self->base.ops = &AILMENT_OPS_TABLE.base;

	self->member = member;
}

struct ailment *AilmentCreate(struct member *member, struct hprob_def *def, int32_t stage)
{
	struct ailment *self = MemAlloc(1, sizeof(struct ailment));

	AilmentInit(self, member, def, stage);

	return self;
}

struct ailment *AilmentCreateEmpty(struct member *member)
{
	struct ailment *self = MemAlloc(1, sizeof(struct ailment));

	AilmentInitEmpty(self, member);

	return self;
}

void AilmentDestroy(struct ailment **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

static void forget_in_queue(struct msg_queue *queue, struct ailment *self)
{
	if (!queue)
		return;

	for (struct list_node *node = queue->base.head; node; node = node->next) {
		struct msg *m = MsgFromNode(node);

		if (m->subject == (intptr_t) self)
			m->subject = 0;
	}
}

void ailment_destroy(struct object *self)
{
	struct ailment *a = (struct ailment *) self;

	forget_in_queue(g_msgq_main_a, a);
	forget_in_queue(g_msgq_main_b, a);

	if (g_game && g_game->event_node && g_game->event_node->subject == (intptr_t) a)
		g_game->event_node->subject = 0;

	MemFree(a->def);
	a->def = NULL;

	if (a->text)
		StrRsrcDestroy(&a->text);

	object_destroy(self);
}

int32_t ailment_check_trigger(struct ailment *self)
{
	int32_t triggered = 0;

	switch (self->def->id) {
		case AILMENT_GANGRENE:
			triggered = 1;
			break;
		case AILMENT_BERIBERI:
			triggered = g_prev_beriberi_risk != 0 && GetGameObj()->party->beriberi_risk < g_prev_beriberi_risk ? 0 : 1;
			g_prev_beriberi_risk = GetGameObj()->party->beriberi_risk;
			break;
		case AILMENT_SCURVY:
			triggered = g_prev_scurvy_risk != 0 && GetGameObj()->party->scurvy_risk < g_prev_scurvy_risk ? 0 : 1;
			g_prev_scurvy_risk = GetGameObj()->party->scurvy_risk;
			break;
		case AILMENT_THIRST:
			triggered = GetAuxMsgqueue()->dry_days ? 1 : 0;
			break;
		case AILMENT_STARVATION:
			triggered = GetGameObj()->party->days_starving ? 1 : 0;
			break;
		default:
			break;
	}

	return triggered;
}

int32_t ailment_tick(struct ailment *self)
{
	int32_t severity = 0;

	if (self->stage == STAGE_ACUTE && (self->def->flags & HPROB_FLAG_TRIGGERED) != 0) {
		self->days_left = AILMENT_OPS(self)->CheckTrigger(self);

	} else {
		--self->days_left;
	}

	if (self->days_left <= 0) {
		AILMENT_OPS(self)->AdvanceStage(self);

	} else if (self->severity) {
		if ((!self->days_in_stage && (self->def->flags & HPROB_FLAG_FIRST_DAY_DAMAGE) != 0) ||
			(self->days_in_stage && (self->def->flags & HPROB_FLAG_LATER_DAY_DAMAGE) != 0)) {
			severity = self->def->param1;

		} else {
			severity = self->severity;
		}

		if ((self->def->flags & HPROB_FLAG_SEVERITY_DRIFT) != 0 && self->def->param2 < self->severity)
			self->severity = (int16_t) (self->severity + self->def->param1);

		++self->days_in_stage;
	}

	if (self->stage == STAGE_ACUTE) {
		if (self->followup_chance_acute && RandRangeAvg(1, 1000, 1) <= self->followup_chance_acute) {
			MemberSetPendingAilment(self->member, self->followup_id_acute, STAGE_ACUTE);
			self->stage = STAGE_RECOVERY;
			AILMENT_OPS(self)->AdvanceStage(self);
		}

		if (self->followup_chance_incubation && RandRangeAvg(1, 1000, 1) <= self->followup_chance_incubation)
			MemberSetPendingAilment(self->member, self->followup_id_incubation, STAGE_INCUBATION);
	}

	return severity;
}

struct ailment *AilmentCreateById(struct member *member, int32_t ailment_id, int32_t start_stage)
{
	struct hprob_def *def = HprobLoad(ailment_id, true);

	if (!def)
		return NULL;

	int32_t category = def->category;
	bool found = false;
	struct ailment *node = OblistFirst(member->nodes);

	while (node) {
		if (node->def && node->def->category == category) {
			found = true;
			break;
		}

		node = OblistNext(member->nodes, true);
	}

	if (found) {
		MemFree(def);
		return NULL;
	}

	return AilmentCreate(member, def, start_stage);
}

void ailment_advance_stage(struct ailment *self)
{
	bool fail = false;

	self->days_in_stage = 0;

	if (self->stage == STAGE_NONE) {
		self->stage = STAGE_INCUBATION;
		fail = AILMENT_OPS(self)->RollInitial(self);
	}

	if (!fail && self->stage == STAGE_INCUBATION) {
		self->stage = STAGE_ACUTE;
		fail = AILMENT_OPS(self)->DailyDamage(self);
	}

	if (!fail && self->stage == STAGE_ACUTE) {
		self->stage = STAGE_RECOVERY;
		fail = AILMENT_OPS(self)->Begin(self);
	}

	if (!fail && self->stage == STAGE_RECOVERY) {
		self->stage = STAGE_NONE;
		self->severity = 0;
	}
}

bool ailment_recover_check(struct ailment *self)
{
	if ((self->def->flags & HPROB_FLAG_RELAPSE) != 0 && self->stage == STAGE_NONE) {
		self->def->incubation_min = self->def->param1;
		self->def->incubation_max = self->def->param2;
		self->stage = STAGE_INCUBATION;
		AILMENT_OPS(self)->RollInitial(self);
	}

	self->target_severity = 0;

	return self->stage == STAGE_NONE;
}

bool ailment_daily_damage(struct ailment *self)
{
	struct member *member = self->member;

	member->collapse_threshold = member->health / 2;

	if (self->target_severity < 1) {
		self->severity = RandRangeAvg(self->def->severity_min, self->def->severity_max, 1);

	} else {
		self->severity = self->target_severity;
	}

	if (!self->def->acute_days_max)
		return false;

	if ((self->def->flags & HPROB_FLAG_SUDDEN_DEATH) != 0 && self->def->param2 > RandRangeAvg(0, 100, 1)) {
		self->severity = 3000;

	} else if ((self->def->flags & HPROB_FLAG_TRIGGERED) != 0) {
		self->days_left = 2;

	} else if ((self->def->flags & HPROB_FLAG_DURATION_FROM_SEVERITY) != 0) {
		int32_t severity_range = self->def->severity_max - self->def->severity_min;
		int32_t days_range = self->def->acute_days_max - self->def->acute_days_min;

		if (!severity_range)
			severity_range = 1;

		self->days_left = (self->severity - self->def->severity_min) * days_range / severity_range;

	} else {
		self->days_left = RandRangeAvg(self->def->acute_days_min, self->def->acute_days_max, 1);
	}

	member->health = member->health - self->severity;

	if (member->health < 1) {
		struct cmd_event *death_event = CmdEventCreate(GCMD_MEMBER_DIED);
		struct cstr *death_text = AILMENT_OPS(self)->GetText(self, AILTEXT_DIED_AT_ONSET);

		member->state = MEMBER_DEAD;

		if (member->state == MEMBER_DEAD)
			member->health = 0;

		death_event->param1 = (intptr_t) member;
		death_event->param2 = (intptr_t) death_text;
		DispatchGlobalEvent(death_event);

	} else {
		struct cmd_event *damage_event = CmdEventCreate(GCMD_AILMENT_WORSENED);

		damage_event->param1 = (intptr_t) member;
		damage_event->param2 = (intptr_t) self;
		DispatchGlobalEvent(damage_event);
	}

	return true;
}

bool ailment_roll_initial(struct ailment *self)
{
	self->severity = 0;
	self->target_severity = RandRangeAvg(self->def->severity_min, self->def->severity_max, 1);

	if (!self->def->incubation_max)
		return false;

	self->days_left = RandRangeAvg(self->def->incubation_min, self->def->incubation_max, 1);

	if ((self->def->flags & HPROB_FLAG_INCUBATION_PLUS_AGE) != 0)
		self->days_left = self->member->age + self->days_left;

	if ((self->def->flags & HPROB_FLAG_SEVERITY_FROM_INCUBATION) != 0) {
		int32_t days_range = self->def->incubation_max - self->def->incubation_min;
		int32_t severity_range = self->def->severity_max - self->def->severity_min;

		if (!days_range)
			days_range = 1;

		self->target_severity = (self->def->incubation_max - self->days_left) * severity_range / days_range;
	}

	return true;
}

void ailment_progress_tick(struct ailment *self)
{
	bool fail = false;

	self->days_in_stage = 0;
	self->target_severity = 0;

	if (self->stage == STAGE_INCUBATION) {
		fail = AILMENT_OPS(self)->RollInitial(self);

		if (!fail)
			self->stage = STAGE_ACUTE;
	}

	if (!fail && self->stage == STAGE_ACUTE) {
		fail = AILMENT_OPS(self)->DailyDamage(self);

		if (!fail)
			self->stage = STAGE_RECOVERY;
	}

	if (!fail && self->stage == STAGE_RECOVERY) {
		fail = AILMENT_OPS(self)->Begin(self);

		if (!fail)
			self->stage = STAGE_NONE;
	}
}

bool ailment_begin(struct ailment *self)
{
	self->severity = self->def->recovery_severity;

	if (!self->def->recovery_days_max)
		return false;

	self->days_left = RandRangeAvg(self->def->recovery_days_min, self->def->recovery_days_max, 1);

	return true;
}

void AilmentSetSeverity(struct ailment *self, int32_t level)
{
	switch (level) {
		case SEVERITY_KEEP:
			break;
		case SEVERITY_MIN:
			self->severity = self->def->severity_min;
			break;
		case SEVERITY_MAX:
			self->severity = self->def->severity_max;
			break;
	}
}

bool ailment_mark_affected(struct ailment *self, int32_t *out)
{
	bool has_symptoms = false;

	if (self->stage == STAGE_ACUTE) {
		has_symptoms = self->symptom_count > 0;

		if (out) {
			for (int32_t i = 0; self->symptom_count > i; ++i)
				out[self->symptoms[i] - 1] = 1;
		}
	}

	return has_symptoms;
}

struct cstr *ailment_get_text(struct ailment *self, int32_t index)
{
	struct cstr *message = StrRsrcGetCStr(self->text, index);
	const char *animal_name = RandRangeAvg(1, 100, 1) < 50 ? "mountain lion" : "bear";

	if (!message)
		message = CStrCreate();

	CStrReplaceStr(message, "@1", self->member->name);
	CStrReplaceAll(message, "@2", animal_name);

	return message;
}

void AilmentLoadState(struct ailment *self, struct doc_file *file)
{
	void *mem = NULL;
	int32_t size = 0;

	DocFileReadObject2(file, &mem, &size);

	uint8_t blk[AILMENT_BLOCK_SIZE + 8] = {0};

	if (mem && size > 0)
		memcpy(blk, mem, (size_t) (size < (int32_t) sizeof(blk) ? size : (int32_t) sizeof(blk)));

	MemFree(mem);

	const uint8_t *bytes = blk;
	int32_t ailment_id = *bytes++;

	self->def = HprobLoad(ailment_id, true);
	self->is_known = *bytes++ != 0;

	self->text = StrRsrcLoad(g_text_lib, self->is_known && self->def ? self->def->id + 15000 : 15000, true);

	const uint8_t *words = bytes;

	self->stage = ReadLe16(words);
	self->severity = ReadLe16(words + 2);
	self->last_treatment = ReadLe16(words + 4);
	self->days_left = ReadLe16(words + 6);
	self->target_severity = ReadLe16(words + 8);
	self->days_in_stage = ReadLe16(words + 10);
	self->symptom_count = ReadLe16(words + 12);
	words += 14;

	for (int32_t i = 0; i < HPROB_SYMPTOM_SLOTS; i++) {
		self->symptoms[i] = ReadLe16(words);
		words += 2;
	}

	self->followup_chance_incubation = ReadLe16(words);
	self->followup_chance_acute = ReadLe16(words + 2);
	words += 4;

	int32_t word = ReadLe16(words);

	if ((file->version & 0xFFFF) >= SAVE_FORMAT_2_0) {
		self->followup_id_incubation = word;
		words += 2;
		self->followup_id_acute = ReadLe16(words);

	} else if ((word >> 8) & 0xFF) {
		self->followup_id_incubation = word & 0xFF;
		self->followup_id_acute = (word >> 8) & 0xFF;

	} else {
		self->followup_id_incubation = word;
		self->followup_id_acute = word;
	}

	words += 2;
	self->worsened = *words != 0;
}

void AilmentSaveState(struct ailment *self, struct doc_file *file)
{
	uint8_t blk[AILMENT_BLOCK_SIZE] = {0};
	uint8_t *bytes = blk;

	*bytes++ = (uint8_t) (self->def ? self->def->id : 0);
	*bytes++ = self->is_known;

	uint8_t *words = bytes;

	WriteLe16(words, self->stage);
	WriteLe16(words + 2, self->severity);
	WriteLe16(words + 4, self->last_treatment);
	WriteLe16(words + 6, self->days_left);
	WriteLe16(words + 8, self->target_severity);
	WriteLe16(words + 10, self->days_in_stage);
	WriteLe16(words + 12, self->symptom_count);
	words += 14;

	for (int32_t i = 0; i < HPROB_SYMPTOM_SLOTS; i++) {
		WriteLe16(words, self->symptoms[i]);
		words += 2;
	}

	WriteLe16(words, self->followup_chance_incubation);
	WriteLe16(words + 2, self->followup_chance_acute);
	WriteLe16(words + 4, self->followup_id_incubation);
	WriteLe16(words + 6, self->followup_id_acute);
	words += 8;
	*words = self->worsened;

	DocFileWriteObject(file, blk, AILMENT_BLOCK_SIZE);
}

void ailment_apply_effects(struct ailment *self, int32_t treatment, bool flag)
{
	g_ailment_followup24_chance = 0;
	g_ailment_fatal_chance = 0;
	g_ailment_followup30_chance = 0;
	g_ailment19_keep_chance = 0;
	g_ailment27_keep_chance = 0;
	g_ailment_followup23_chance = 0;
	g_ailment_followup19_chance = 0;
	g_ailment_effect_severity = SEVERITY_KEEP;

	struct game *game = GetGameObj();

	self->party_has_medical = game->party->skills[SKILL_MEDICAL];
	self->last_treatment = treatment;

	int32_t ailment_type = self->def->id;

	switch (self->last_treatment) {
		case TREAT_CONTINUE:
			AilmentEffectContinueAsUsual(self, ailment_type);
			break;
		case TREAT_REST:
			AilmentEffectRest(self, ailment_type);
			break;
		case TREAT_INCREASE_RATIONS:
			AilmentEffectIncreaseRations(self, ailment_type);
			break;
		case TREAT_CLEAN_AND_DRESS:
			AilmentEffectCleanAndDress(self, ailment_type);
			break;
		case TREAT_ANTISEPTIC:
			AilmentEffectAntiseptic(self, ailment_type);
			break;
		case TREAT_ASTRINGENT:
			g_ailment_followup24_chance = self->party_has_medical ? 10 * self->severity / 30 : 10 * self->severity / 20;
			break;
		case TREAT_HARMFUL_REMEDY:
			AilmentEffectHarmfulRemedy(self, ailment_type);
			break;
		case TREAT_FLUIDS:
			AilmentEffectFluids(self, ailment_type);
			break;
		case TREAT_PEPPERMINT:
			AilmentEffectPeppermint(self, ailment_type);
			break;
		case TREAT_LAUDANUM:
			AilmentEffectLaudanum(self, ailment_type);
			break;
		case TREAT_SLOW_DOWN:
			AilmentEffectSlowDown(self, ailment_type);
			break;
		case TREAT_VINEGAR:
			AilmentEffectVinegar(self, ailment_type);
			break;
		case TREAT_OLIVE_OIL:
			AilmentEffectOliveOil(self, ailment_type);
			break;
		case TREAT_CAMPHOR:
			AilmentEffectCamphor(self, ailment_type);
			break;
		case TREAT_EXERCISE:
			AilmentEffectExercise(self, ailment_type);
			break;
		case TREAT_MAGNESIA:
			AilmentEffectMagnesia(self, ailment_type);
			break;
		case TREAT_LECITHIN:
			AilmentEffectLecithin(self, ailment_type);
			break;
		case TREAT_QUININE:
			AilmentEffectQuinine(self, ailment_type);
			break;
		case TREAT_SASSAFRAS:
			AilmentEffectSassafras(self, ailment_type);
			break;
		case TREAT_ALOE:
			AilmentEffectAloe(self, ailment_type);
			break;
		case TREAT_CONTROL_FEVER:
			AilmentEffectControlFever(self, ailment_type);
			break;
		case TREAT_DOVERS_POWDER:
			AilmentEffectDoversPowder(self, ailment_type);
			break;
		case TREAT_DUFFYS_ELIXIR:
			AilmentEffectDuffysElixir(self, ailment_type);
			break;
		case TREAT_WARM_GRADUALLY:
			AilmentEffectWarmGradually(self, ailment_type);
			break;
		case TREAT_WARM_QUICKLY:
			AilmentEffectWarmQuickly(self, ailment_type);
			break;
		case TREAT_AMPUTATE:
			AilmentEffectAmputate(self, ailment_type);
			break;
		case TREAT_SULFUR:
			AilmentEffectSulfur(self, ailment_type);
			break;
		case TREAT_TOURNIQUET:
			AilmentEffectTourniquet(self, ailment_type);
			break;
		case TREAT_FLOUR_OR_GREASE:
			AilmentEffectFlourOrGrease(self, ailment_type);
			break;
		case TREAT_GO_FASTER:
			break;
	}

	AilmentSetSeverity(self, g_ailment_effect_severity);

	if (g_ailment_followup24_chance) {
		if (!flag)
			g_ailment_followup24_chance *= 2;

		self->followup_chance_incubation = g_ailment_followup24_chance;
		self->followup_id_incubation = AILMENT_INFECTION;
	}

	if (g_ailment_fatal_chance && RandRangeAvg(1, 1000, 1) <= g_ailment_fatal_chance) {
		self->severity = 3000;
		self->def->flags = 0;
	}

	if (g_ailment_followup30_chance) {
		if (self->member->age > 50)
			g_ailment_followup30_chance *= 10;

		self->followup_chance_acute = g_ailment_followup30_chance;
		self->followup_id_acute = AILMENT_PNEUMONIA;
	}

	if (self->def->id == AILMENT_MALARIA && RandRangeAvg(1, 1000, 1) > g_ailment27_keep_chance)
		self->def->flags = 0;

	if (g_ailment_followup23_chance) {
		self->followup_chance_acute = g_ailment_followup23_chance;
		self->followup_id_acute = AILMENT_GANGRENE;
	}

	if (self->def->id == AILMENT_DYSENTERY && RandRangeAvg(1, 1000, 1) > g_ailment19_keep_chance)
		self->def->flags = 0;

	if (g_ailment_followup19_chance) {
		self->followup_chance_incubation = g_ailment_followup19_chance;
		self->followup_id_incubation = AILMENT_DYSENTERY;
	}
}

void AilmentEffectContinueAsUsual(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_ACCIDENTAL_GUNSHOT:
		case AILMENT_ANIMAL_BITE:
		case AILMENT_ANIMAL_MAULING:
		case AILMENT_CUTS_AND_ABRASIONS:
			g_ailment_followup24_chance = self->party_has_medical ? 10 * self->severity / 20 : 10 * self->severity / 10;
			break;
		case AILMENT_CHOLERA:
			g_ailment_fatal_chance = 100;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_BAD_COLD:
			g_ailment_followup30_chance = self->party_has_medical ? 3 : 5;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_CONCUSSION:
		case AILMENT_CONSUMPTION:
		case AILMENT_DIPHTHERIA:
		case AILMENT_GANGRENE:
		case AILMENT_LOCKJAW:
		case AILMENT_MOUNTAIN_FEVER:
		case AILMENT_PNEUMONIA:
		case AILMENT_RABIES:
		case AILMENT_SPRAINED_WRIST:
		case AILMENT_SPRAINED_ELBOW:
		case AILMENT_SPRAINED_KNEE:
		case AILMENT_SPRAINED_ANKLE:
		case AILMENT_SPRAINED_SHOULDER:
		case AILMENT_TYPHUS:
		case AILMENT_WATER_POISONING:
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_DYSENTERY:
			g_ailment19_keep_chance = self->party_has_medical ? 50 : 100;
			break;
		case AILMENT_THE_GRIPPE:
			g_ailment_followup30_chance = self->party_has_medical ? 3 : 5;
			g_ailment_effect_severity = SEVERITY_MAX;
			break;
		case AILMENT_MALARIA:
			g_ailment27_keep_chance = self->party_has_medical ? 250 : 500;
			break;
		case AILMENT_MEASLES:
		case AILMENT_SCARLET_FEVER:
		case AILMENT_SMALLPOX:
		case AILMENT_TYPHOID_FEVER:
			g_ailment_followup30_chance = self->party_has_medical ? 5 : 10;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_FROSTBITE:
			g_ailment_followup23_chance = self->party_has_medical ? 50 : 100;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_INTERNAL_INJURIES:
		case AILMENT_SNAKEBITE:
			g_ailment_fatal_chance = 250;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_FOOD_POISONING:
			g_ailment_followup19_chance = self->party_has_medical ? 25 : 50;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_INFECTION:
			g_ailment_followup23_chance = self->party_has_medical ? 25 : 50;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_BURNS:
		case AILMENT_WOUND:
			g_ailment_followup24_chance = self->party_has_medical ? 50 : 100;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_FREEZING:
			g_ailment_fatal_chance = 300;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_ALKALI_SICKNESS:
			g_ailment_followup19_chance = self->party_has_medical ? 25 : 50;
			break;
		case AILMENT_BROKEN_ARM:
		case AILMENT_BROKEN_LEG:
		case AILMENT_BROKEN_FOOT:
		case AILMENT_BROKEN_HIP:
		case AILMENT_BROKEN_WRIST:
			g_ailment_followup24_chance = self->party_has_medical ? 5 : 10;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		default:
			break;
	}
}

void AilmentEffectRest(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_ACCIDENTAL_GUNSHOT:
		case AILMENT_ANIMAL_BITE:
		case AILMENT_ANIMAL_MAULING:
		case AILMENT_CUTS_AND_ABRASIONS:
			g_ailment_followup24_chance = self->party_has_medical ? 10 * self->severity / 20 : 10 * self->severity / 10;
			break;
		case AILMENT_CHOLERA:
			g_ailment_fatal_chance = 50;
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		case AILMENT_BAD_COLD:
			g_ailment_followup30_chance = self->party_has_medical ? 0 : 5;
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		case AILMENT_CONCUSSION:
		case AILMENT_CONSUMPTION:
		case AILMENT_DIPHTHERIA:
		case AILMENT_DYSENTERY:
		case AILMENT_LOCKJAW:
		case AILMENT_MOUNTAIN_FEVER:
		case AILMENT_PNEUMONIA:
		case AILMENT_TYPHUS:
		case AILMENT_WATER_POISONING:
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		case AILMENT_THE_GRIPPE:
			g_ailment_followup30_chance = self->party_has_medical ? 0 : 1;
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		case AILMENT_MALARIA:
			g_ailment27_keep_chance = self->party_has_medical ? 50 : 100;
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		case AILMENT_MEASLES:
			g_ailment_followup30_chance = self->party_has_medical ? 0 : 1;
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		case AILMENT_FROSTBITE:
			g_ailment_followup23_chance = self->party_has_medical ? 5 : 10;
			g_ailment_effect_severity = SEVERITY_MAX;
			break;
		case AILMENT_GANGRENE:
		case AILMENT_RABIES:
			g_ailment_effect_severity = SEVERITY_MAX;
			break;
		case AILMENT_SCARLET_FEVER:
		case AILMENT_SMALLPOX:
		case AILMENT_TYPHOID_FEVER:
			g_ailment_effect_severity = SEVERITY_MIN;

			if (self->member->age > 50)
				g_ailment_followup30_chance = 1;

			break;
		case AILMENT_INTERNAL_INJURIES:
		case AILMENT_SNAKEBITE:
			g_ailment_fatal_chance = 100;
			break;
		case AILMENT_INFECTION:
			g_ailment_followup23_chance = self->party_has_medical ? 5 : 10;
			break;
		case AILMENT_BURNS:
		case AILMENT_WOUND:
			g_ailment_followup24_chance = self->party_has_medical ? 25 : 50;
			break;
		case AILMENT_FREEZING:
			g_ailment_fatal_chance = 200;
			g_ailment_effect_severity = SEVERITY_MAX;
			break;
		case AILMENT_ALKALI_SICKNESS:
			g_ailment_followup19_chance = self->party_has_medical ? 5 : 10;
			break;
		case AILMENT_BROKEN_ARM:
		case AILMENT_BROKEN_LEG:
		case AILMENT_BROKEN_FOOT:
		case AILMENT_BROKEN_HIP:
		case AILMENT_BROKEN_WRIST:
			g_ailment_followup24_chance = self->party_has_medical ? 2 : 5;
			break;
		default:
			break;
	}
}

void AilmentEffectIncreaseRations(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_ACCIDENTAL_GUNSHOT:
		case AILMENT_ANIMAL_BITE:
		case AILMENT_ANIMAL_MAULING:
		case AILMENT_CUTS_AND_ABRASIONS:
			g_ailment_followup24_chance = self->party_has_medical ? 10 * self->severity / 10 : 2 * self->severity;
			break;
		case AILMENT_CHOLERA:
			g_ailment_fatal_chance = 100;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_BAD_COLD:
			g_ailment_followup30_chance = self->party_has_medical ? 0 : 1;
			break;
		case AILMENT_DYSENTERY:
			g_ailment19_keep_chance = self->party_has_medical ? 100 : 200;
			break;
		case AILMENT_MALARIA:
			g_ailment27_keep_chance = self->party_has_medical ? 150 : 300;
			break;
		case AILMENT_MEASLES:
		case AILMENT_SCARLET_FEVER:
		case AILMENT_SMALLPOX:
		case AILMENT_TYPHOID_FEVER:
			g_ailment_followup30_chance = self->party_has_medical ? 2 : 5;
			break;
		case AILMENT_FROSTBITE:
			g_ailment_followup23_chance = self->party_has_medical ? 25 : 50;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_GANGRENE:
		case AILMENT_LOCKJAW:
		case AILMENT_RABIES:
			g_ailment_effect_severity = SEVERITY_MAX;
			break;
		case AILMENT_SNAKEBITE:
			g_ailment_fatal_chance = 200;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_FOOD_POISONING:
			g_ailment_followup19_chance = self->party_has_medical ? 50 : 100;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_INFECTION:
			g_ailment_followup23_chance = self->party_has_medical ? 5 : 10;
			break;
		case AILMENT_BURNS:
			g_ailment_followup24_chance = self->party_has_medical ? 30 : 60;
			break;
		case AILMENT_INTERNAL_INJURIES:
			g_ailment_fatal_chance = 100;
			break;
		case AILMENT_THIRST:
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		case AILMENT_ALKALI_SICKNESS:
			g_ailment_followup19_chance = self->party_has_medical ? 25 : 50;
			break;
		case AILMENT_BROKEN_ARM:
		case AILMENT_BROKEN_LEG:
		case AILMENT_BROKEN_FOOT:
		case AILMENT_BROKEN_HIP:
		case AILMENT_BROKEN_WRIST:
			g_ailment_followup24_chance = self->party_has_medical ? 2 : 5;
			break;
		default:
			break;
	}
}

void AilmentEffectCleanAndDress(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_ACCIDENTAL_GUNSHOT:
		case AILMENT_ANIMAL_BITE:
		case AILMENT_ANIMAL_MAULING:
		case AILMENT_CUTS_AND_ABRASIONS:
			g_ailment_followup24_chance = self->party_has_medical ? 10 * self->severity / 50 : 10 * self->severity / 20;
			break;
		case AILMENT_BURNS:
		case AILMENT_INFECTION:
		case AILMENT_WOUND:
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		case AILMENT_BROKEN_ARM:
		case AILMENT_BROKEN_LEG:
		case AILMENT_BROKEN_FOOT:
		case AILMENT_BROKEN_HIP:
		case AILMENT_BROKEN_WRIST:
		case AILMENT_SPRAINED_WRIST:
		case AILMENT_SPRAINED_ELBOW:
		case AILMENT_SPRAINED_KNEE:
		case AILMENT_SPRAINED_ANKLE:
		case AILMENT_SPRAINED_SHOULDER:
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		default:
			break;
	}
}

void AilmentEffectAntiseptic(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_ACCIDENTAL_GUNSHOT:
		case AILMENT_ANIMAL_BITE:
		case AILMENT_ANIMAL_MAULING:
		case AILMENT_CUTS_AND_ABRASIONS:
			g_ailment_followup24_chance = self->party_has_medical ? 10 * self->severity / 50 : 10 * self->severity / 20;
			break;
		case AILMENT_SMALLPOX:
			g_ailment_followup30_chance = self->party_has_medical ? 3 : 5;
			break;
		case AILMENT_SNAKEBITE:
			g_ailment_fatal_chance = 200;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_BURNS:
		case AILMENT_INFECTION:
		case AILMENT_WOUND:
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		default:
			break;
	}
}

void AilmentEffectHarmfulRemedy(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_ACCIDENTAL_GUNSHOT:
		case AILMENT_ANIMAL_BITE:
		case AILMENT_ANIMAL_MAULING:
		case AILMENT_CUTS_AND_ABRASIONS:
			g_ailment_followup24_chance = self->party_has_medical ? 10 * self->severity / 10 : self->severity * 2;
			break;
		case AILMENT_CHOLERA:
			g_ailment_fatal_chance = self->party_has_medical ? 300 : 500;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_MEASLES:
		case AILMENT_SMALLPOX:
			g_ailment_followup30_chance = self->party_has_medical ? 5 : 10;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_CONSUMPTION:
		case AILMENT_LOCKJAW:
		case AILMENT_PNEUMONIA:
		case AILMENT_RABIES:
		case AILMENT_SCARLET_FEVER:
		case AILMENT_SPRAINED_WRIST:
		case AILMENT_SPRAINED_ELBOW:
		case AILMENT_SPRAINED_KNEE:
		case AILMENT_SPRAINED_ANKLE:
		case AILMENT_SPRAINED_SHOULDER:
		case AILMENT_TYPHUS:
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_FROSTBITE:
			g_ailment_followup23_chance = self->party_has_medical ? 100 : 200;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_FOOD_POISONING:
			g_ailment_followup19_chance = self->party_has_medical ? 100 : 200;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_INFECTION:
			g_ailment_followup23_chance = self->party_has_medical ? 30 : 50;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_BURNS:
		case AILMENT_WOUND:
			g_ailment_followup24_chance = self->party_has_medical ? 100 : 200;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_ALKALI_SICKNESS:
			g_ailment_followup19_chance = self->party_has_medical ? 250 : 500;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_BROKEN_ARM:
		case AILMENT_BROKEN_LEG:
		case AILMENT_BROKEN_FOOT:
		case AILMENT_BROKEN_HIP:
		case AILMENT_BROKEN_WRIST:
			g_ailment_followup24_chance = 50;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		default:
			break;
	}
}

void AilmentEffectFluids(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_CHOLERA:
			g_ailment_fatal_chance = 50;
			break;
		case AILMENT_BAD_COLD:
		case AILMENT_SCARLET_FEVER:
			if (self->member->age > 50)
				g_ailment_followup30_chance = 1;

			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		case AILMENT_DIPHTHERIA:
		case AILMENT_DYSENTERY:
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		case AILMENT_THE_GRIPPE:
			g_ailment_followup30_chance = self->party_has_medical ? 1 : 3;
			break;
		case AILMENT_MEASLES:
			g_ailment_followup30_chance = 1;
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		default:
			break;
	}
}

void AilmentEffectPeppermint(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_CHOLERA:
			g_ailment_fatal_chance = 200;
			break;
		case AILMENT_DYSENTERY:
		case AILMENT_WATER_POISONING:
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		default:
			break;
	}
}

void AilmentEffectLaudanum(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_CHOLERA:
			g_ailment_fatal_chance = 100;
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		case AILMENT_DYSENTERY:
		case AILMENT_FOOD_POISONING:
		case AILMENT_LOCKJAW:
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		case AILMENT_TYPHOID_FEVER:
			g_ailment_followup30_chance = self->party_has_medical ? 3 : 5;
			break;
		case AILMENT_RABIES:
			g_ailment_effect_severity = SEVERITY_MAX;
			break;
		case AILMENT_SCARLET_FEVER:
			if (self->member->age > 50)
				g_ailment_followup30_chance = 1;

			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		case AILMENT_INTERNAL_INJURIES:
			g_ailment_fatal_chance = 100;
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		case AILMENT_WOUND:
			g_ailment_followup24_chance = self->party_has_medical ? 30 : 50;
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		default:
			break;
	}
}

void AilmentEffectSlowDown(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_BAD_COLD:
			g_ailment_followup30_chance = self->party_has_medical ? 1 : 2;
			break;
		case AILMENT_THE_GRIPPE:
			g_ailment_followup30_chance = self->party_has_medical ? 2 : 3;
			break;
		case AILMENT_MALARIA:
			g_ailment27_keep_chance = self->party_has_medical ? 200 : 300;
			break;
		case AILMENT_MEASLES:
		case AILMENT_SCARLET_FEVER:
		case AILMENT_SMALLPOX:
			g_ailment_followup30_chance = self->party_has_medical ? 3 : 5;
			break;
		case AILMENT_FROSTBITE:
			g_ailment_followup23_chance = self->party_has_medical ? 30 : 50;
			break;
		case AILMENT_GANGRENE:
		case AILMENT_RABIES:
		case AILMENT_SPRAINED_WRIST:
		case AILMENT_SPRAINED_ELBOW:
		case AILMENT_SPRAINED_KNEE:
		case AILMENT_SPRAINED_ANKLE:
		case AILMENT_SPRAINED_SHOULDER:
			g_ailment_effect_severity = SEVERITY_MAX;
			break;
		case AILMENT_SNAKEBITE:
			g_ailment_fatal_chance = 200;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		case AILMENT_INTERNAL_INJURIES:
			g_ailment_fatal_chance = 150;
			break;
		case AILMENT_FREEZING:
			g_ailment_fatal_chance = 250;
			g_ailment_effect_severity = SEVERITY_MAX;
			break;
		case AILMENT_BROKEN_ARM:
		case AILMENT_BROKEN_LEG:
		case AILMENT_BROKEN_FOOT:
		case AILMENT_BROKEN_HIP:
		case AILMENT_BROKEN_WRIST:
			g_ailment_followup24_chance = self->party_has_medical ? 3 : 5;
			break;
		default:
			break;
	}
}

void AilmentEffectVinegar(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_BAD_COLD:
			g_ailment_followup30_chance = self->party_has_medical ? 1 : 2;
			break;
		case AILMENT_THE_GRIPPE:
			g_ailment_followup30_chance = self->party_has_medical ? 2 : 3;
			break;
		case AILMENT_ALKALI_SICKNESS:
		case AILMENT_CONCUSSION:
		case AILMENT_DYSENTERY:
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		default:
			break;
	}
}

void AilmentEffectOliveOil(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_BAD_COLD:
			g_ailment_followup30_chance = self->party_has_medical ? 3 : 5;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		default:
			break;
	}
}

void AilmentEffectCamphor(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_BROKEN_ARM:
		case AILMENT_BROKEN_LEG:
		case AILMENT_BROKEN_FOOT:
		case AILMENT_BROKEN_HIP:
		case AILMENT_BROKEN_WRIST:
			g_ailment_followup24_chance = self->party_has_medical ? 5 : 10;
			g_ailment_effect_severity = self->party_has_medical ? SEVERITY_KEEP : SEVERITY_MAX;
			break;
		default:
			break;
	}
}

void AilmentEffectExercise(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_CONCUSSION:
			g_ailment_effect_severity = SEVERITY_MAX;
			break;
		case AILMENT_SNAKEBITE:
			g_ailment_fatal_chance = 900;
			g_ailment_effect_severity = SEVERITY_MAX;
			break;
		default:
			break;
	}
}

void AilmentEffectMagnesia(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_DYSENTERY:
			g_ailment19_keep_chance = self->party_has_medical ? 200 : 300;
			break;
		case AILMENT_MALARIA:
			g_ailment27_keep_chance = self->party_has_medical ? 300 : 500;
			break;
		case AILMENT_MOUNTAIN_FEVER:
		case AILMENT_RABIES:
			g_ailment_effect_severity = SEVERITY_MAX;
			break;
		case AILMENT_INTERNAL_INJURIES:
			g_ailment_fatal_chance = 200;
			g_ailment_effect_severity = SEVERITY_MAX;
			break;
		default:
			break;
	}
}

void AilmentEffectLecithin(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_THE_GRIPPE:
			g_ailment_followup30_chance = self->party_has_medical ? 0 : 1;
			break;
		default:
			break;
	}
}

void AilmentEffectQuinine(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_THE_GRIPPE:
			g_ailment_followup30_chance = self->party_has_medical ? 3 : 5;
			break;
		case AILMENT_MALARIA:
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		default:
			break;
	}
}

void AilmentEffectSassafras(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_MALARIA:
			g_ailment27_keep_chance = self->party_has_medical ? 300 : 500;
			break;
		default:
			break;
	}
}

void AilmentEffectAloe(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_MEASLES:
		case AILMENT_SMALLPOX:
			g_ailment_followup30_chance = self->party_has_medical ? 3 : 5;
			break;
		case AILMENT_TYPHOID_FEVER:
			g_ailment_followup30_chance = self->party_has_medical ? 5 : 10;
			g_ailment_effect_severity = SEVERITY_MAX;
			break;
		case AILMENT_TYPHUS:
			g_ailment_effect_severity = SEVERITY_MAX;
			break;
		default:
			break;
	}
}

void AilmentEffectControlFever(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_MOUNTAIN_FEVER:
		case AILMENT_PNEUMONIA:
		case AILMENT_TYPHOID_FEVER:
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		default:
			break;
	}
}

void AilmentEffectDoversPowder(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_DIPHTHERIA:
			g_ailment_effect_severity = SEVERITY_MAX;
			break;
		default:
			break;
	}
}

void AilmentEffectDuffysElixir(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_GANGRENE:
			g_ailment_effect_severity = SEVERITY_MAX;
			break;
		default:
			break;
	}
}

void AilmentEffectWarmGradually(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_CONSUMPTION:
		case AILMENT_FROSTBITE:
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		case AILMENT_FREEZING:
			g_ailment_fatal_chance = 100;
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		default:
			break;
	}
}

void AilmentEffectWarmQuickly(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_FROSTBITE:
			g_ailment_followup23_chance = self->party_has_medical ? 30 : 50;
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		default:
			break;
	}
}

void AilmentEffectAmputate(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_GANGRENE:
			self->followup_chance_acute = 1000;
			self->followup_id_acute = AILMENT_WOUND;
			break;
		default:
			break;
	}
}

void AilmentEffectSulfur(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_GANGRENE:
			g_ailment_effect_severity = SEVERITY_MAX;
			break;
		case AILMENT_INFECTION:
		case AILMENT_WOUND:
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		default:
			break;
	}
}

void AilmentEffectTourniquet(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_SNAKEBITE:
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		default:
			break;
	}
}

void AilmentEffectFlourOrGrease(struct ailment *self, int32_t code)
{
	switch (code) {
		case AILMENT_ALKALI_SICKNESS:
			g_ailment_effect_severity = SEVERITY_MIN;
			break;
		default:
			break;
	}
}

bool ailment_is_known(struct ailment *self)
{
	return self->is_known;
}
