#include "settings.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "../base/date.h"
#include "../data/docfile.h"
#include "../game/game.h"
#include "../base/ini.h"
#include "../base/memory.h"
#include "../game/party.h"
#include "../base/str.h"

#define SETTINGS_BLOCK_SIZE 11

void SettingsInit(struct settings *self)
{
	self->occupation = OCCUP_BANKER;
	self->start_month = MONTH_MAY;
	self->start_year = 1848;
	self->start_town = 0;
	self->destination = 0;
	self->level = LEVEL_GREENHORN;
	self->speed = SIM_SPEED_MEDIUM;
	self->unused_18 = 1;
	SettingsLoad(self);
}

struct settings *SettingsCreate(void)
{
	struct settings *self = MemAlloc(1, sizeof(struct settings));
	SettingsInit(self);

	return self;
}

void SettingsDestroy(struct settings **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct settings *ctx = *ctx_mod;
	*ctx_mod = NULL;

	MemFree(ctx);
}

void SettingsDeserialize(struct settings *self, struct doc_file *file)
{
	void *mem = NULL;
	int32_t size = 0;
	uint8_t blk[SETTINGS_BLOCK_SIZE] = {0};

	DocFileReadObject2(file, &mem, &size);
	if (mem)
		memcpy(blk, mem, size < SETTINGS_BLOCK_SIZE ? (size > 0 ? (size_t) size : 0) : SETTINGS_BLOCK_SIZE);
	MemFree(mem);

	const uint8_t *cursor_h = blk;
	self->occupation = *cursor_h++;
	self->start_month = *cursor_h++;
	self->start_year = *cursor_h++ + 1840;
	self->start_town = *cursor_h++;
	self->destination = *cursor_h++;
	self->level = *cursor_h++;
	self->speed = *cursor_h++;
	self->unused_18 = (int8_t) *cursor_h++;
	self->music_enabled = *cursor_h++;
	self->speech_enabled = *cursor_h++;
	self->sfx_enabled = *cursor_h++;
}

void SettingsSerialize(struct settings *self, struct doc_file *file)
{
	uint8_t blk[SETTINGS_BLOCK_SIZE] = {0};
	uint8_t *cursor_h = blk;

	*cursor_h++ = (uint8_t) self->occupation;
	*cursor_h++ = (uint8_t) self->start_month;
	*cursor_h++ = (uint8_t) (self->start_year - 1840);
	*cursor_h++ = (uint8_t) self->start_town;
	*cursor_h++ = (uint8_t) self->destination;
	*cursor_h++ = (uint8_t) self->level;
	*cursor_h++ = (uint8_t) self->speed;
	*cursor_h++ = (uint8_t) self->unused_18;
	*cursor_h++ = (uint8_t) self->music_enabled;
	*cursor_h++ = (uint8_t) self->speech_enabled;
	*cursor_h++ = (uint8_t) self->sfx_enabled;
	DocFileWriteObject(file, blk, SETTINGS_BLOCK_SIZE);
}

void SettingsSave(struct settings *self)
{
	char value_text_a[64];

	StrPrintf(value_text_a, "%s", self->music_enabled ? "on" : "off");
	IniSetString("sound", "music", value_text_a);
	StrPrintf(value_text_a, "%s", self->sfx_enabled ? "on" : "off");
	IniSetString("sound", "sfx", value_text_a);
	StrPrintf(value_text_a, "%s", self->speech_enabled ? "on" : "off");
	IniSetString("sound", "speech", value_text_a);
	StrPrintf(value_text_a, "%" PRId32, (int32_t) (int16_t) self->speed);
	IniSetString("game", "speed", value_text_a);
}

void SettingsLoad(struct settings *self)
{
	char value_text_j[64];
	char default_text_h[64];

	StrPrintf(default_text_h, "%s", "on");
	IniGetString("sound", "music", default_text_h, value_text_j, sizeof(value_text_j));
	self->music_enabled = !strcmp(value_text_j, "on");
	IniGetString("sound", "sfx", default_text_h, value_text_j, sizeof(value_text_j));
	self->sfx_enabled = !strcmp(value_text_j, "on");
	IniGetString("sound", "speech", default_text_h, value_text_j, sizeof(value_text_j));
	self->speech_enabled = !strcmp(value_text_j, "on");
	StrPrintf(default_text_h, "%d", 1);
	IniGetString("game", "speed", default_text_h, value_text_j, sizeof(value_text_j));
	(void) sscanf(value_text_j, "%" SCNd32, &self->speed);
}

int32_t SettingsGetField4(struct settings *self)
{
	return self->occupation;
}

int32_t SettingsGetField6(struct settings *self)
{
	return self->start_month;
}

int32_t SettingsGetFieldA(struct settings *self)
{
	return self->start_year;
}

int32_t SettingsGetFieldC(struct settings *self)
{
	return self->start_town;
}

int32_t SettingsGetFieldE(struct settings *self)
{
	return self->destination;
}

int32_t SettingsGetField10(struct settings *self)
{
	return self->level;
}

int32_t SettingsGetSpeed(struct settings *self)
{
	return self->speed;
}

int32_t SettingsGetFlag18(struct settings *self)
{
	return self->unused_18;
}

int32_t SettingsMusicOn(struct settings *self)
{
	return self->music_enabled;
}

int32_t SettingsSpeechOn(struct settings *self)
{
	return self->speech_enabled;
}

int32_t SettingsSoundEffectsOn(struct settings *self)
{
	return self->sfx_enabled;
}

void SettingsSetFlag19(struct settings *self, int32_t v)
{
	self->music_enabled = (uint8_t) v;
	SettingsSave(self);
}

void SettingsSetFlag1A(struct settings *self, int32_t v)
{
	self->speech_enabled = (uint8_t) v;
	SettingsSave(self);
}

void SettingsSetFlag1B(struct settings *self, int32_t v)
{
	self->sfx_enabled = (uint8_t) v;
	SettingsSave(self);
}
