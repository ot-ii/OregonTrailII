#pragma once

#include <stdint.h>

struct doc_file;

enum sim_speed {
	SIM_SPEED_SLOW = 0,
	SIM_SPEED_MEDIUM = 1,
	SIM_SPEED_FAST = 2,
};

struct settings {
	int32_t occupation;
	int32_t start_month;
	int32_t start_year;
	int32_t start_town;
	int32_t destination;
	int32_t level;
	int32_t speed;
	int32_t unused_18;

	// these are bools
	int32_t music_enabled;
	int32_t speech_enabled;
	int32_t sfx_enabled;
};

void SettingsInit(struct settings *self);
struct settings *SettingsCreate(void);
void SettingsDestroy(struct settings **ctx_mod);

void SettingsDeserialize(struct settings *self, struct doc_file *file);
void SettingsSerialize(struct settings *self, struct doc_file *file);
int32_t SettingsGetField4(struct settings *self);
int32_t SettingsGetField6(struct settings *self);
int32_t SettingsGetFieldA(struct settings *self);
int32_t SettingsGetFieldC(struct settings *self);
int32_t SettingsGetFieldE(struct settings *self);
int32_t SettingsGetField10(struct settings *self);
int32_t SettingsGetSpeed(struct settings *self);
int32_t SettingsGetFlag18(struct settings *self);
int32_t SettingsMusicOn(struct settings *self);
int32_t SettingsSpeechOn(struct settings *self);
int32_t SettingsSoundEffectsOn(struct settings *self);
void SettingsSetFlag19(struct settings *self, int32_t v);
void SettingsSetFlag1A(struct settings *self, int32_t v);
void SettingsSetFlag1B(struct settings *self, int32_t v);

void SettingsLoad(struct settings *self);
void SettingsSave(struct settings *self);
