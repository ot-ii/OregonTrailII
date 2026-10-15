#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/object.h"

struct ailment;
struct cstr;
struct doc_file;
struct member;
struct strrsrc;

enum ailment_id {
	AILMENT_UNKNOWN = 0,
	AILMENT_ACCIDENTAL_GUNSHOT = 1,
	AILMENT_ALKALI_SICKNESS = 2,
	AILMENT_ANIMAL_BITE = 3,
	AILMENT_ANIMAL_MAULING = 4,
	AILMENT_BAD_COLD = 5,
	AILMENT_BERIBERI = 6,
	AILMENT_BROKEN_ARM = 7,
	AILMENT_BROKEN_LEG = 8,
	AILMENT_BROKEN_FOOT = 9,
	AILMENT_BROKEN_HIP = 10,
	AILMENT_BROKEN_WRIST = 11,
	AILMENT_BURNS = 12,
	AILMENT_CHOLERA = 13,
	AILMENT_CONCUSSION = 14,
	AILMENT_CONSUMPTION = 15,
	AILMENT_CUTS_AND_ABRASIONS = 16,
	AILMENT_DIPHTHERIA = 17,
	AILMENT_DROWNING = 18,
	AILMENT_DYSENTERY = 19,
	AILMENT_FOOD_POISONING = 20,
	AILMENT_FREEZING = 21,
	AILMENT_FROSTBITE = 22,
	AILMENT_GANGRENE = 23,
	AILMENT_INFECTION = 24,
	AILMENT_INTERNAL_INJURIES = 25,
	AILMENT_LOCKJAW = 26,
	AILMENT_MALARIA = 27,
	AILMENT_MEASLES = 28,
	AILMENT_MOUNTAIN_FEVER = 29,
	AILMENT_PNEUMONIA = 30,
	AILMENT_RABIES = 31,
	AILMENT_SCARLET_FEVER = 32,
	AILMENT_SCURVY = 33,
	AILMENT_SMALLPOX = 34,
	AILMENT_SNAKEBITE = 35,
	AILMENT_SPRAINED_WRIST = 36,
	AILMENT_SPRAINED_ELBOW = 37,
	AILMENT_SPRAINED_KNEE = 38,
	AILMENT_SPRAINED_ANKLE = 39,
	AILMENT_SPRAINED_SHOULDER = 40,
	AILMENT_STARVATION = 41,
	AILMENT_THE_GRIPPE = 42,
	AILMENT_THIRST = 43,
	AILMENT_TYPHOID_FEVER = 44,
	AILMENT_TYPHUS = 45,
	AILMENT_WATER_POISONING = 46,
	AILMENT_WOUND = 47,
};

enum ailment_stage {
	STAGE_NONE = 0,
	STAGE_INCUBATION = 1,
	STAGE_ACUTE = 2,
	STAGE_RECOVERY = 3,
};

enum treatment {
	TREAT_CONTINUE = 0x0,
	TREAT_REST = 0x1,
	TREAT_INCREASE_RATIONS = 0x2,
	TREAT_CLEAN_AND_DRESS = 0x3,
	TREAT_ANTISEPTIC = 0x4,
	TREAT_ASTRINGENT = 0x5,
	TREAT_HARMFUL_REMEDY = 0x6,
	TREAT_FLUIDS = 0x7,
	TREAT_PEPPERMINT = 0x8,
	TREAT_LAUDANUM = 0x9,
	TREAT_SLOW_DOWN = 0xA,
	TREAT_GO_FASTER = 0xB,
	TREAT_VINEGAR = 0xC,
	TREAT_OLIVE_OIL = 0xD,
	TREAT_CAMPHOR = 0xE,
	TREAT_EXERCISE = 0xF,
	TREAT_MAGNESIA = 0x10,
	TREAT_LECITHIN = 0x11,
	TREAT_QUININE = 0x12,
	TREAT_SASSAFRAS = 0x13,
	TREAT_ALOE = 0x14,
	TREAT_CONTROL_FEVER = 0x15,
	TREAT_DUFFYS_ELIXIR = 0x16,
	TREAT_DOVERS_POWDER = 0x17,
	TREAT_WARM_GRADUALLY = 0x18,
	TREAT_WARM_QUICKLY = 0x19,
	TREAT_AMPUTATE = 0x1A,
	TREAT_SULFUR = 0x1B,
	TREAT_TOURNIQUET = 0x1C,
	TREAT_FLOUR_OR_GREASE = 0x1D,
};

enum ailment_text {
	AILTEXT_NAME = 0,
	AILTEXT_ONSET = 1,
	AILTEXT_WORSENED = 2,
	AILTEXT_DIED = 3,
	AILTEXT_DIED_AT_ONSET = 4,
};

enum hprob_field {
	HPROB_ID = 0,
	HPROB_CATEGORY = 1,
	HPROB_UNREAD = 2,
	HPROB_INCUBATION_MIN = 3,
	HPROB_INCUBATION_MAX = 4,
	HPROB_ACUTE_DAYS_MIN = 5,
	HPROB_ACUTE_DAYS_MAX = 6,
	HPROB_RECOVERY_DAYS_MIN = 7,
	HPROB_RECOVERY_DAYS_MAX = 8,
	HPROB_SEVERITY_MIN = 9,
	HPROB_SEVERITY_MAX = 10,
	HPROB_RECOVERY_SEVERITY = 11,
	HPROB_FLAGS = 12,
	HPROB_PARAM1 = 13,
	HPROB_PARAM2 = 14,
	HPROB_IDENTIFY_CHANCE = 15,
	HPROB_SYMPTOMS = 16,
};

enum hprob_flag {
	HPROB_FLAG_FIRST_DAY_DAMAGE = 0x1,
	HPROB_FLAG_INCUBATION_PLUS_AGE = 0x2,
	HPROB_FLAG_LATER_DAY_DAMAGE = 0x4,
	HPROB_FLAG_SEVERITY_DRIFT = 0x8,
	HPROB_FLAG_SEVERITY_FROM_INCUBATION = 0x10,
	HPROB_FLAG_DURATION_FROM_SEVERITY = 0x20,
	HPROB_FLAG_RELAPSE = 0x40,
	HPROB_FLAG_TRIGGERED = 0x80,
	HPROB_FLAG_SUDDEN_DEATH = 0x100,
};

#define HPROB_SYMPTOM_SLOTS 14

struct hprob_def {
	int32_t id;
	int32_t category;
	int32_t unread;
	int32_t incubation_min;
	int32_t incubation_max;
	int32_t acute_days_min;
	int32_t acute_days_max;
	int32_t recovery_days_min;
	int32_t recovery_days_max;
	int32_t severity_min;
	int32_t severity_max;
	int32_t recovery_severity;
	int32_t flags;
	int32_t param1;
	int32_t param2;
	int32_t identify_chance;
	struct {
		int32_t id;
		int32_t chance;
	} symptoms[HPROB_SYMPTOM_SLOTS];
};

struct hprob_def *HprobLoad(int32_t ailment_id, bool fatal);

struct ailment_ops {
	struct object_ops base;
	int32_t (*CheckTrigger)(struct ailment *self);
	int32_t (*Tick)(struct ailment *self);
	bool (*RecoverCheck)(struct ailment *self);
	bool (*MarkAffected)(struct ailment *self, int32_t *out);
	void (*ApplyEffects)(struct ailment *self, int32_t treatment, bool flag);
	bool (*IsKnown)(struct ailment *self);
	struct cstr *(*GetText)(struct ailment *self, int32_t index);
	void (*AdvanceStage)(struct ailment *self);
	bool (*DailyDamage)(struct ailment *self);
	bool (*RollInitial)(struct ailment *self);
	void (*ProgressTick)(struct ailment *self);
	bool (*Begin)(struct ailment *self);
};

struct ailment {
	struct object base;
	struct member *member;
	int32_t stage;
	int32_t severity;
	int32_t last_treatment;
	int32_t days_left;
	int32_t target_severity;
	int32_t days_in_stage;
	bool is_known;
	bool party_has_medical;
	struct hprob_def *def;
	struct strrsrc *text;
	int32_t symptom_count;
	int32_t symptoms[HPROB_SYMPTOM_SLOTS];
	int32_t followup_chance_incubation;
	int32_t followup_chance_acute;
	int32_t followup_id_incubation;
	int32_t followup_id_acute;
	bool worsened;
};

#define AILMENT_OPS(p) ((const struct ailment_ops *) ((const struct object *) (p))->ops)

extern const struct ailment_ops AILMENT_OPS_TABLE;

void AilmentInit(struct ailment *self, struct member *member, struct hprob_def *def, int32_t stage);
void AilmentInitEmpty(struct ailment *self, struct member *member);
struct ailment *AilmentCreate(struct member *member, struct hprob_def *def, int32_t stage);
struct ailment *AilmentCreateEmpty(struct member *member);
void AilmentDestroy(struct ailment **ctx_mod);
void ailment_destroy(struct object *self);

struct ailment *AilmentCreateById(struct member *member, int32_t ailment_id, int32_t start_stage);

int32_t ailment_check_trigger(struct ailment *self);
int32_t ailment_tick(struct ailment *self);
bool ailment_recover_check(struct ailment *self);
bool ailment_mark_affected(struct ailment *self, int32_t *out);
void ailment_apply_effects(struct ailment *self, int32_t treatment, bool flag);
bool ailment_is_known(struct ailment *self);
struct cstr *ailment_get_text(struct ailment *self, int32_t index);
void ailment_advance_stage(struct ailment *self);
bool ailment_daily_damage(struct ailment *self);
bool ailment_roll_initial(struct ailment *self);
void ailment_progress_tick(struct ailment *self);
bool ailment_begin(struct ailment *self);

void AilmentEffectContinueAsUsual(struct ailment *self, int32_t code);
void AilmentEffectRest(struct ailment *self, int32_t code);
void AilmentEffectIncreaseRations(struct ailment *self, int32_t code);
void AilmentEffectVinegar(struct ailment *self, int32_t code);
void AilmentEffectCleanAndDress(struct ailment *self, int32_t code);
void AilmentEffectHarmfulRemedy(struct ailment *self, int32_t code);
void AilmentEffectAntiseptic(struct ailment *self, int32_t code);
void AilmentEffectFluids(struct ailment *self, int32_t code);
void AilmentEffectPeppermint(struct ailment *self, int32_t code);
void AilmentEffectMagnesia(struct ailment *self, int32_t code);
void AilmentEffectLecithin(struct ailment *self, int32_t code);
void AilmentEffectQuinine(struct ailment *self, int32_t code);
void AilmentEffectSassafras(struct ailment *self, int32_t code);
void AilmentEffectLaudanum(struct ailment *self, int32_t code);
void AilmentEffectSlowDown(struct ailment *self, int32_t code);
void AilmentEffectOliveOil(struct ailment *self, int32_t code);
void AilmentEffectCamphor(struct ailment *self, int32_t code);
void AilmentEffectExercise(struct ailment *self, int32_t code);
void AilmentEffectAloe(struct ailment *self, int32_t code);
void AilmentEffectControlFever(struct ailment *self, int32_t code);
void AilmentEffectDoversPowder(struct ailment *self, int32_t code);
void AilmentEffectDuffysElixir(struct ailment *self, int32_t code);
void AilmentEffectWarmGradually(struct ailment *self, int32_t code);
void AilmentEffectWarmQuickly(struct ailment *self, int32_t code);
void AilmentEffectAmputate(struct ailment *self, int32_t code);
void AilmentEffectSulfur(struct ailment *self, int32_t code);
void AilmentEffectTourniquet(struct ailment *self, int32_t code);
void AilmentEffectFlourOrGrease(struct ailment *self, int32_t code);
void AilmentSetSeverity(struct ailment *self, int32_t level);
void AilmentSaveState(struct ailment *self, struct doc_file *file);
void AilmentLoadState(struct ailment *self, struct doc_file *file);
