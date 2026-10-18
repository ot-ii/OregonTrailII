#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "appvideo.h"
#include "../game/journal.h"

struct doc_file;
struct main_ui;
struct path;
struct quick_start_choices;
struct rsrc;
struct settings;
struct snd_obj;

enum app_state {
	APP_STATE_TITLE = 0,
	APP_STATE_TRAVELING = 1,
	APP_STATE_AT_NODE = 2,
	APP_STATE_EVENT = 3,
	APP_STATE_SCENE = 4,
	APP_STATE_HUNTING = 5,
	APP_STATE_SUPPLIES = 7,
	APP_STATE_NEW_GAME = 8,
	APP_STATE_DIARY = 9,
};

enum menu_cmd {
	MCMD_EXPORT_DIARY = 2000,
	MCMD_SIM_SPEED = 6000,
	MCMD_LEGENDS = 6001,
	MCMD_TOGGLE_SFX = 6002,
	MCMD_TOGGLE_MUSIC = 6003,
	MCMD_TOGGLE_SPEECH = 6004,
	MCMD_GLOSSARY = 6005,
	MCMD_TIME_OUT = 6006,
	MCMD_VIEW_MAP = 6007,
	MCMD_HELP_CONTENTS = 6008,
};

enum tool_button {
	TOOL_MAP = 5000,
	TOOL_JOURNAL = 5001,
	TOOL_HEALTH = 5002,
	TOOL_SUPPLIES = 5003,
	TOOL_HUNT = 5300,
	TOOL_TRADE = 5301,
	TOOL_DUMP = 5302,
	TOOL_TURN = 5303,
};

enum quick_start_command {
	CMD_NEW_GAME_OPTIONS = 7000,
};

enum cue_sheet {
	CUE_SHEET_OTHER = 100,
	CUE_SHEET_TRAVEL = 200,
};

enum music_cue {
	CUE_RESTING_GOOD = 16,
	CUE_RESTING_FAIR = 17,
	CUE_RESTING_POOR = 18,
	CUE_HILLS = 22,
	CUE_DESERT_OR_DEATH = 25,
	CUE_RAFTING = 27,
	CUE_JOURNEY_END = 28,
	CUE_TITLE = 29,
};

struct cue_entry {
	int32_t situation;
	int32_t start_cue;
	int32_t loop_cue;
	int32_t end_cue;
};

void CueListReadHeader(const uint8_t *data, int32_t *track, int32_t *count);
struct cue_entry CueListReadEntry(const uint8_t *data, int32_t index);
int32_t SndPoolReadCount(const uint8_t *data);
void SndPoolReadEntry(const uint8_t *data, int32_t index, uint32_t *track, struct cue_entry *cues);

struct otii_app;

struct otii_app_ops {
	struct app_ops base;
	bool (*NoGameDocOpen)(struct otii_app *self);
	int32_t (*OpenGameDocument)(struct otii_app *self, struct path *pathspec);
};

struct otii_app {
	struct app_video base;
	int32_t app_state;
	int32_t prev_app_state;
	int32_t saved_state;
	struct journal journal;
	struct settings *settings;
	struct rsrc *cue_rsrc;
	int32_t cue_sheet_id;
	int32_t music_situation;
	int32_t music_start_cue;
	int32_t music_loop_cue;
	int32_t music_end_cue;
	int32_t music_track;
	struct snd_obj *music_stream;
	bool music_loop;
	int32_t speech_id;
	struct snd_obj *speech_stream;
	int32_t sfx_id;
	struct snd_obj *sfx_stream;
	struct main_ui *ui;
	struct quick_start_choices *new_game_choices;
};

#define OTII_APP_OPS(p) ((const struct otii_app_ops *) ((const struct object *) (p))->ops)

extern const struct otii_app_ops OTII_APP_OPS_TABLE;

void OtiiAppInit(struct otii_app *self);
void otii_app_destroy(struct object *self);

void otii_app_on_event(struct evt_handler *self, struct cmd_event *ev);
void otii_app_register_object_types(struct app *self);
void otii_app_update_edit_menu_state(struct app *self, int32_t command);
void otii_app_on_menus_ready(struct app *self);
void otii_app_on_init_complete(struct app *self);
void otii_app_on_app_init(struct app *self);
void otii_app_on_idle_spare(struct evt_handler *self, void *task);
bool otii_app_no_game_doc_open(struct otii_app *self);
int32_t otii_app_open_game_document(struct otii_app *self, struct path *pathspec);

void OtiiAppSaveToDataFile(struct otii_app *self, struct doc_file *file);
void OtiiAppLoadFromDataFile(struct otii_app *self, struct doc_file *file);
void OtiiAppApplyScreenMode(struct otii_app *self);
void OtiiAppInitMainGameScreen(struct otii_app *self);
void OtiiAppNewGameSetup(struct otii_app *self, int32_t command);
bool OtiiAppShouldEnableAtLandmark(struct otii_app *self);
void OtiiAppHandleDocCommand(struct otii_app *self, int32_t command, void *hwnd);
void OtiiAppSetMusicLevel(struct otii_app *self, int32_t level);
void OtiiAppPlayMusicCue(struct otii_app *self, int32_t cue);
void OtiiAppLoadMusicCue(struct otii_app *self, int32_t cue_id);
void OtiiAppPlayPoolMusic(struct otii_app *self, int32_t cue);
void OtiiAppPlayMusic(struct otii_app *self, int32_t track, bool loop);
void OtiiAppPauseMusic(struct otii_app *self);
void OtiiAppResumeMusic(struct otii_app *self);
void OtiiAppStopMusic(struct otii_app *self);
void OtiiAppSetMusicEnabled(struct otii_app *self, int32_t on);
void OtiiAppPlaySpeech(struct otii_app *self, int32_t id, bool loop, bool start);
void OtiiAppStopSpeech(struct otii_app *self);
void OtiiAppSetSpeechEnabled(struct otii_app *self, int32_t on);
void OtiiAppLipsyncAdvanceSpeech(struct otii_app *self);
void OtiiAppPauseSfx(struct otii_app *self);
void OtiiAppResumeSfx(struct otii_app *self);
void OtiiAppPlaySfx(struct otii_app *self, int32_t id, bool loop);
void OtiiAppStopSfx(struct otii_app *self);
void OtiiAppSetSfxEnabled(struct otii_app *self, int32_t on);

void InventoryAddHuntedMeat(int32_t pounds);

void ForceSaveAddMenuItem(void *menu_bar);
void ForceSaveRun(void);
void PlayIntroAddMenuItem(void *menu_bar);
void PlayIntroRun(void);

void OtiiAppFatalMessage(const char *text);
void OtiiAppWinHelp(struct otii_app *self, uint32_t command);
void OtiiAppEnableMenuCommand(uint32_t id, bool enable);

extern bool g_about_open;
extern bool g_hunt_started;
