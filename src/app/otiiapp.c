#include "otiiapp.h"

#include <stddef.h>
#include <string.h>

#include "../data/assets.h"
#include "../features/autopilot.h"
#include "autosave.h"
#include "../base/byteorder.h"
#include "../ui/combobox.h"
#include "../ui/control.h"
#include "../dialogs/convdlg.h"
#include "../features/crowdcontrol.h"
#include "../base/cstr.h"
#include "../dialogs/datelistbuilder.h"
#include "../data/dataversion.h"
#include "../data/docfile.h"
#include "docmanager.h"
#include "../game/draftanimalitem.h"
#include "../game/endgame.h"
#include "event.h"
#include "../ui/framewin.h"
#include "../game/game.h"
#include "../game/gamecmd.h"
#include "../game/gamedoc.h"
#include "../data/gfxdev.h"
#include "../dialogs/hiscoredlg.h"
#include "../game/invitem.h"
#include "../game/invlist.h"
#include "license.h"
#include "../base/log.h"
#include "../screens/mainui.h"
#include "../media/mcivideo.h"
#include "../draw/mediabase.h"
#include "../game/member.h"
#include "../base/memory.h"
#include "../ui/menubar.h"
#include "../dialogs/modaldialog.h"
#include "../dialogs/msgbox.h"
#include "../dialogs/newgamedlg.h"
#include "../game/newgamescenario.h"
#include "../trail/node.h"
#include "../game/occupmgr.h"
#include "../features/onlinescores.h"
#include "../game/party.h"
#include "../base/path.h"
#include "../ui/picview.h"
#include "../ui/pictctrl.h"
#include "../dialogs/quickstartdlg.h"
#include "registrar.h"
#include "replay.h"
#include "../base/rng.h"
#include "../data/rsrc.h"
#include "../screens/sceneframe.h"
#include "../draw/server.h"
#include "settings.h"
#include "../media/sndobj.h"
#include "../media/sndstream.h"
#include "../data/strrsrc.h"
#include "../media/strmdsnd.h"
#include "../screens/titleanim.h"
#include "../features/twitchcontrol.h"
#include "../features/twitchirc.h"
#include "../features/twitchparty.h"
#include "../ui/view.h"
#include "../game/wagonitem.h"
#include "../ui/window.h"

#ifdef OTII_TEST
#include "../../tests/testframework.h"
#endif

#define SND_RSRC_BASE (SND_KIND_STREAMED << 16)

const char *g_cmdline = "OregonII";
struct health_win *g_health_window;

static int32_t g_view_id = 0;

static struct app *as_app(struct otii_app *self)
{
	return &self->base.base;
}

static struct evt_handler *as_eh(struct otii_app *self)
{
	return &self->base.base.base;
}

static int32_t next_view_id(void)
{
	g_view_id = (int16_t) (g_view_id + 1);
	return g_view_id;
}

const struct otii_app_ops OTII_APP_OPS_TABLE = {
	{
		{
			{otii_app_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			otii_app_on_event,
			evt_handler_on_idle,
			otii_app_on_idle_spare,
		},
		app_get_mode,
		app_get_doc_manager,
		app_video_pump_tick,
		app_get_app_path,
		app_noop13,
		otii_app_register_object_types,
		app_on_activate,
		app_run_until_quit,
		app_get_resource_path,
		app_set_license,
		app_create_menu_bar,
		app_wait_game_ticks,
		app_on_deactivate,
		app_is_suspended,
		otii_app_update_edit_menu_state,
		otii_app_on_app_init,
		otii_app_on_menus_ready,
		otii_app_on_init_complete,
	},
	otii_app_no_game_doc_open,
	otii_app_open_game_document,
};

void CueListReadHeader(const uint8_t *data, int32_t *track, int32_t *count)
{
	*track = ReadLe16(data);
	*count = ReadLe16(data + 2);
}

struct cue_entry CueListReadEntry(const uint8_t *data, int32_t index)
{
	const uint8_t *p = data + 4 + 8 * index;
	struct cue_entry e;

	e.situation = ReadLe16(p);
	e.start_cue = ReadLe16(p + 2);
	e.loop_cue = ReadLe16(p + 4);
	e.end_cue = ReadLe16(p + 6);
	return e;
}

int32_t SndPoolReadCount(const uint8_t *data)
{
	return ReadLe16(data);
}

void SndPoolReadEntry(const uint8_t *data, int32_t index, uint32_t *track, struct cue_entry *cues)
{
	const uint8_t *p = data + 2 + 10 * index;

	*track = ReadLeU16(p);
	cues->situation = ReadLe16(p + 2);
	cues->start_cue = ReadLe16(p + 4);
	cues->loop_cue = ReadLe16(p + 6);
	cues->end_cue = ReadLe16(p + 8);
}

static void set_music_cues(struct otii_app *self, const struct cue_entry *e)
{
	self->music_situation = e->situation;
	self->music_start_cue = e->start_cue;
	self->music_loop_cue = e->loop_cue;
	self->music_end_cue = e->end_cue;
}

static struct snd_obj *acquire_sound(int32_t key)
{
	if (!g_sound_server)
		return NULL;
	return (struct snd_obj *) SERVER_OPS(g_sound_server)->AcquireObject((struct server *) g_sound_server, key, false);
}

static void replay_checkpoint(void)
{
	uint32_t h = 2166136261u;
	int32_t day = -1, cash = 0, i;

	if (g_game && g_game->party) {
		struct party *p = g_game->party;
		struct list_node *node;
		day = g_game->days_traveled;
		cash = (int32_t) (p->cash * 100);
		for (i = 0; i < p->member_count && i < 6; ++i) {
			struct member *m = PartyGetMember(p, i);
			if (m)
				h = (h ^ (uint32_t) (m->health * 16 + m->state)) * 16777619u;
		}
		for (node = p->inventory.base.head; node; node = node->next)
			h = (h ^ (uint32_t) InvItemGetCount(InvItemFromNode(node), false)) * 16777619u;
	}
	RpCheckpoint(h, day, cash, (uint32_t) RngCalls());
}

void OtiiAppInit(struct otii_app *self)
{
	struct rsrc *probe_data_h;
	struct rsrc *probe_text_b;
	struct rsrc *probe_demo_e;
	struct cmd_event *startup_event_n;
	int32_t handler_ids_e[23];
	bool data_available_p;
	bool text_available_n;

	AppVideoInit(&self->base, g_cmdline);
	as_app(self)->base.base.ops = &OTII_APP_OPS_TABLE.base.base.base;
	JournalInit(&self->journal);

	self->new_game_choices = NULL;
	startup_event_n = CmdEventCreate(GCMD_APP_STARTUP);

	handler_ids_e[0] = GCMD_APP_STARTUP;
	handler_ids_e[1] = CMD_CLOSE_WINDOW;
	handler_ids_e[2] = CMD_NEW_GAME;
	handler_ids_e[3] = CMD_LOAD_GAME;
	handler_ids_e[4] = CMD_END_GAME;
	handler_ids_e[5] = CMD_SAVE_GAME;
	handler_ids_e[6] = CMD_SAVE_GAME_AS;
	handler_ids_e[7] = MCMD_EXPORT_DIARY;
	handler_ids_e[8] = MCMD_SIM_SPEED;
	handler_ids_e[9] = MCMD_LEGENDS;
	handler_ids_e[10] = MCMD_TOGGLE_SFX;
	handler_ids_e[11] = MCMD_TOGGLE_MUSIC;
	handler_ids_e[12] = MCMD_TOGGLE_SPEECH;
	handler_ids_e[13] = MCMD_VIEW_MAP;
	handler_ids_e[14] = MCMD_GLOSSARY;
	handler_ids_e[15] = MCMD_TIME_OUT;
	handler_ids_e[16] = CMD_CONTROL_ACTION;
	handler_ids_e[17] = GCMD_HUNT_SHOT;
	handler_ids_e[18] = GCMD_HUNT_DONE;
	handler_ids_e[19] = GCMD_OPEN_DROP_DLG;
	handler_ids_e[20] = GCMD_OPEN_TRADE_DLG;
	handler_ids_e[21] = MCMD_HELP_CONTENTS;
	handler_ids_e[22] = CMD_NEW_GAME_OPTIONS;

	self->sfx_stream = NULL;
	self->speech_stream = self->sfx_stream;
	self->music_stream = self->speech_stream;
	self->sfx_id = 0;
	self->speech_id = self->sfx_id;
	self->music_track = self->speech_id;
	self->cue_rsrc = NULL;
	self->cue_sheet_id = 0;
	self->app_state = APP_STATE_TITLE;
	self->prev_app_state = self->app_state;

	APP_OPS(self)->RegisterObjectTypes(as_app(self));
	AppInitAndRun(as_app(self), g_cmdline);

	data_available_p = g_data_lib && AssetsIsAvailable(g_data_lib);
	text_available_n = g_text_lib && AssetsIsAvailable(g_text_lib);
	if (data_available_p) {
		probe_data_h = AssetsGetElement(g_data_lib, "CBMP", 601, false);
		if (!probe_data_h)
			data_available_p = false;
		RsrcDestroy(&probe_data_h);
	}
	if (!data_available_p)
		OtiiAppFatalMessage("Oregon Trail II cannot locate \"OregonII.Dat\". Make sure the Oregon "
			"Trail II CD is in the CD-ROM drive.");
	if (text_available_n) {
		probe_text_b = AssetsGetElement(g_text_lib, "CONVR", 90, false);
		if (!probe_text_b)
			text_available_n = false;
		RsrcDestroy(&probe_text_b);
	}
	if (!text_available_n)
		OtiiAppFatalMessage("Oregon Trail II cannot locate \"OregonII.Eng\".");
	if (as_app(self)->license_mode != LICENSE_DEMO) {
		probe_demo_e = AssetsGetElement(g_data_lib, "MPIDX", 4002, false);
		if (!probe_demo_e)
			OtiiAppFatalMessage("The CD in your drive is a demonstration version, and cannot be used.");
		RsrcDestroy(&probe_demo_e);
	}

	EvtHandlerBindHandler(as_eh(self), 23, handler_ids_e);
	self->settings = SettingsCreate();
	DispatchGlobalEvent(startup_event_n);
	APP_OPS(self)->UpdateEditMenuState(as_app(self), 0);
	RpSetCheckpoint(replay_checkpoint);
#ifdef OTII_TWITCH
	TwitchControlStart();
#endif
#ifdef CROWD_CONTROL
	CrowdControlStart();
#endif
#ifdef TWITCH_PARTY
	TwitchPartyStart();
#endif
#ifdef OTII_AUTO_MODE
	AutoBatchStart();
#endif
#ifdef OTII_TEST
	TestStart();
#endif
}

struct app *OtiiAppCreate(void)
{
	struct otii_app *self = MemAlloc(1, sizeof(struct otii_app));
	OtiiAppInit(self);

	return as_app(self);
}

void otii_app_destroy(struct object *obj)
{
	struct otii_app *self = (struct otii_app *) obj;

#ifdef CROWD_CONTROL
	CrowdControlStop();
#endif
#ifdef TWITCH_PARTY
	TwitchPartyStop();
#endif
#ifdef OTII_TWITCH
	TwitchControlStop();
	TwitchStop();
#endif
	OtiiAppWinHelp(self, 2);
	if (g_game)
		GameDestroy(&g_game);
	if (g_parent_win)
		FrameWinDestroy(&g_parent_win);
	if (self->cue_rsrc)
		RsrcDestroy(&self->cue_rsrc);
	OtiiAppStopMusic(self);
	OtiiAppStopSpeech(self);
	OtiiAppStopSfx(self);
	ReleaseItemCatalog();
	ReleaseDraftanimalRsrc();
	ReleaseWagonRsrc();
	StrRsrcDestroy(&g_portrait_table);

	journal_destroy((struct object *) &self->journal);
	obj->ops = &APP_VIDEO_OPS_TABLE.base.base;
	app_video_destroy(obj);
}

void otii_app_register_object_types(struct app *self)
{
	app_register_object_types(self);
	RegistrarRegister(257, ComboCreateFromDesc);
}

struct game *GetGameObj(void)
{
	return g_game;
}

void OtiiAppSetMusicLevel(struct otii_app *self, int32_t level)
{
	if (self->music_stream)
		SND_OBJ_OPS(self->music_stream)->SetLevel(self->music_stream, level);
}

void OtiiAppPlayMusicCue(struct otii_app *self, int32_t cue)
{
	if (self->music_situation != cue && self->cue_rsrc) {
		const uint8_t *cue_list_j = RsrcData(self->cue_rsrc);
		int32_t cue_size = RsrcSize(self->cue_rsrc);
		int32_t list_track = 0, list_count = 0;
		int32_t first_track_l;

		if (cue_list_j && cue_size >= 4)
			CueListReadHeader(cue_list_j, &list_track, &list_count);
		cue--;

		if (list_count > cue && cue >= 0 && 4 + 8 * (cue + 1) <= cue_size) {
			struct cue_entry entry = CueListReadEntry(cue_list_j, cue);
			set_music_cues(self, &entry);
			if (self->music_track != list_track) {
				OtiiAppStopMusic(self);
				first_track_l = list_track;
				self->music_stream = acquire_sound((uint16_t) first_track_l + SND_RSRC_BASE);
			}
			if (self->music_stream) {
				SND_OBJ_OPS(self->music_stream)->PlayCueRange(self->music_stream, self->music_start_cue,
					self->music_loop_cue, self->music_end_cue, 0);
				if (SettingsMusicOn(self->settings)) {
					if (self->music_loop_cue > 0)
						SND_OBJ_OPS(self->music_stream)->Begin(self->music_stream, true);
					else
						SND_OBJ_OPS(self->music_stream)->Begin(self->music_stream, false);
				}
				self->music_track = list_track;
			}
		}
	}
}

void OtiiAppLoadMusicCue(struct otii_app *self, int32_t cue_id)
{
	if (self->cue_sheet_id != cue_id) {
		if (self->cue_rsrc)
			RsrcDestroy(&self->cue_rsrc);
		self->cue_sheet_id = cue_id;
		self->cue_rsrc = AssetsGetElement(g_data_lib, "Cue", self->cue_sheet_id, true);
		if (!self->cue_rsrc)
			return;
		if (RsrcStorage(self->cue_rsrc) == RSRC_TEXT)
			LogError(LOG_ERROR, "Attempt load a raw resource");
		RsrcLoadAndUnpack(self->cue_rsrc);
	}
}

void OtiiAppPlayPoolMusic(struct otii_app *self, int32_t cue)
{
	struct rsrc *pool_rsrc_n;
	const uint8_t *pool_d;
	uint32_t track_p;
	int32_t pool_index_e;
	int32_t pool_track = (int16_t) -(int16_t) cue;

	track_p = 0;

	if (self->music_track == pool_track)
		return;
	pool_index_e = GetGameObj()->cur_node->rand_seed;
	pool_rsrc_n = AssetsGetElement(g_data_lib, "Pool", (int16_t) cue, true);
	if (!pool_rsrc_n)
		return;
	if (RsrcStorage(pool_rsrc_n) == RSRC_TEXT)
		LogError(LOG_ERROR, "Attempt load a raw resource");
	if (self->cue_rsrc)
		RsrcDestroy(&self->cue_rsrc);
	self->cue_rsrc = NULL;
	self->cue_sheet_id = -1;
	RsrcLoadAndUnpack(pool_rsrc_n);
	OtiiAppStopMusic(self);
	pool_d = RsrcData(pool_rsrc_n);
	int32_t pool_size = RsrcSize(pool_rsrc_n);
	int32_t pool_count = pool_d && pool_size >= 2 ? SndPoolReadCount(pool_d) : 0;
	if (pool_count) {
		pool_index_e %= pool_count;

		if (pool_index_e >= 0 && 2 + 10 * (pool_index_e + 1) <= pool_size) {
			struct cue_entry entry;
			SndPoolReadEntry(pool_d, pool_index_e, &track_p, &entry);
			set_music_cues(self, &entry);
		}
	}
	self->music_stream = acquire_sound((int32_t) track_p + SND_RSRC_BASE);
	if (self->music_stream) {
		SND_OBJ_OPS(self->music_stream)->PlayCueRange(self->music_stream, self->music_start_cue,
			self->music_loop_cue, self->music_end_cue, 0);
		if (SettingsMusicOn(self->settings)) {
			if (self->music_loop_cue > 0)
				SND_OBJ_OPS(self->music_stream)->Begin(self->music_stream, true);
			else
				SND_OBJ_OPS(self->music_stream)->Begin(self->music_stream, false);
		}
		self->music_track = pool_track;
	}
	RsrcDestroy(&pool_rsrc_n);
}

void OtiiAppPlayMusic(struct otii_app *self, int32_t track, bool loop)
{
	self->music_situation = 0;
	if (track == 0 || self->music_track == track)
		return;

	self->music_loop = true;
	if (self->cue_rsrc)
		RsrcDestroy(&self->cue_rsrc);
	self->cue_rsrc = NULL;
	self->cue_sheet_id = 0;
	if (SettingsMusicOn(self->settings)) {
		OtiiAppStopMusic(self);
		self->music_stream = acquire_sound((uint16_t) track + SND_RSRC_BASE);
		if (self->music_stream) {
			SND_OBJ_OPS(self->music_stream)->Begin(self->music_stream, loop);
			self->music_track = track;
		}

	} else {
		self->music_track = track;
	}
}

void OtiiAppPauseMusic(struct otii_app *self)
{
	if (!self->music_stream)
		return;
	SND_OBJ_OPS(self->music_stream)->Start(self->music_stream);
}

void OtiiAppResumeMusic(struct otii_app *self)
{
	if (!self->music_stream)
		return;
	SND_OBJ_OPS(self->music_stream)->Finish(self->music_stream);
}

void OtiiAppStopMusic(struct otii_app *self)
{
	if (!self->music_stream)
		return;
	SND_OBJ_OPS(self->music_stream)->Stop(self->music_stream);
	ReleaseDrawable((struct media_base *) self->music_stream, false);
	self->music_stream = NULL;
	self->music_track = 0;
}

static void restart_music(struct otii_app *self)
{
	int32_t track = self->music_track;

	OtiiAppStopMusic(self);
	self->music_track = 0;
	if (self->cue_rsrc && self->music_situation > 0) {
		int32_t cue = self->music_situation;

		self->music_situation = 0;
		OtiiAppPlayMusicCue(self, cue);

	} else if (track < 0) {
		if (GetGameObj() && GetGameObj()->cur_node)
			OtiiAppPlayPoolMusic(self, -track);

	} else if (track > 0) {
		OtiiAppPlayMusic(self, track, self->music_loop);
	}
}

void OtiiAppSetMusicEnabled(struct otii_app *self, int32_t on)
{
	if (SettingsMusicOn(self->settings) == on)
		return;
	SettingsSetFlag19(self->settings, on);
	if (on) {
		restart_music(self);

	} else {
		int32_t track = self->music_track;

		OtiiAppStopMusic(self);
		self->music_track = track;
	}
}

void OtiiAppPlaySpeech(struct otii_app *self, int32_t id, bool loop, bool start)
{
	if (id == 0 || self->speech_id == id)
		return;
	OtiiAppStopSpeech(self);
	if (SettingsSpeechOn(self->settings)) {
		self->speech_stream = acquire_sound((uint16_t) id + SND_RSRC_BASE);
		if (self->speech_stream) {
			if (start)
				SND_OBJ_OPS(self->speech_stream)->Begin(self->speech_stream, loop);
			self->speech_id = id;
		}

	} else {
		self->speech_id = id;
	}
}

void OtiiAppStopSpeech(struct otii_app *self)
{
	if (!self->speech_stream)
		return;
	SND_OBJ_OPS(self->speech_stream)->Stop(self->speech_stream);
	ReleaseDrawable((struct media_base *) self->speech_stream, false);
	self->speech_stream = NULL;
	self->speech_id = 0;
}

void OtiiAppSetSpeechEnabled(struct otii_app *self, int32_t on)
{
	int32_t old;

	old = self->speech_id;
	if (SettingsSpeechOn(self->settings) == on)
		return;
	SettingsSetFlag1A(self->settings, on);
	if (on) {
		self->speech_id = 0;

	} else {
		OtiiAppStopSpeech(self);
		self->speech_id = old;
	}
}

void OtiiAppLipsyncAdvanceSpeech(struct otii_app *self)
{
	if (SettingsSpeechOn(self->settings)) {
		if (self->speech_stream) {
			if (SND_OBJ_OPS(self->speech_stream)->GetStatus(self->speech_stream) != 1)
				SND_OBJ_OPS(self->speech_stream)->Begin(self->speech_stream, false);
		}
	}
}

void OtiiAppPauseSfx(struct otii_app *self)
{
	if (!self->sfx_stream)
		return;
	SND_OBJ_OPS(self->sfx_stream)->Start(self->sfx_stream);
}

void OtiiAppResumeSfx(struct otii_app *self)
{
	if (!self->sfx_stream)
		return;
	SND_OBJ_OPS(self->sfx_stream)->Finish(self->sfx_stream);
}

void OtiiAppPlaySfx(struct otii_app *self, int32_t id, bool loop)
{
	if (id == 0)
		return;
	OtiiAppStopSfx(self);
	if (SettingsSoundEffectsOn(self->settings)) {
		self->sfx_stream = acquire_sound((uint16_t) id + SND_RSRC_BASE);
		if (self->sfx_stream) {
			SND_OBJ_OPS(self->sfx_stream)->Begin(self->sfx_stream, loop);
			self->sfx_id = id;
		}

	} else {
		self->sfx_id = id;
	}
}

void OtiiAppStopSfx(struct otii_app *self)
{
	if (!self->sfx_stream)
		return;
	SND_OBJ_OPS(self->sfx_stream)->Stop(self->sfx_stream);
	ReleaseDrawable((struct media_base *) self->sfx_stream, false);
	self->sfx_stream = NULL;
	self->sfx_id = 0;
}

void OtiiAppSetSfxEnabled(struct otii_app *self, int32_t on)
{
	int32_t old;

	old = self->sfx_id;
	if (SettingsSoundEffectsOn(self->settings) == on)
		return;
	SettingsSetFlag1B(self->settings, on);
	if (on) {
		self->sfx_id = 0;

	} else {
		OtiiAppStopSfx(self);
		self->sfx_id = old;
	}
}

bool otii_app_no_game_doc_open(struct otii_app *self)
{
	struct doc_manager *docmgr;
	struct doc_file *doc;

	docmgr = APP_OPS(self)->GetDocManager(as_app(self));
	doc = DocManagerGetSingleDoc(docmgr);
	if (doc)
		return false;
	return true;
}

int32_t otii_app_open_game_document(struct otii_app *self, struct path *pathspec)
{
	struct doc_manager *docmgr = APP_OPS(self)->GetDocManager(as_app(self));
	int32_t result;
	struct game_doc *doc;

	result = 0;
	if (!pathspec->status) {
		doc = GameDocCreatePath(pathspec);
		if (DocFileGetStatus(&doc->base)) {
			GameDocDestroy(&doc);
			result = 13;

		} else {
			DocManagerAddDoc(docmgr, &doc->base);
			DocFileSetDirtyV(&doc->base, false);
		}

	} else {
		PathDestroy(&pathspec);
		result = 13;
	}
	return result;
}

void OtiiAppLoadFromDataFile(struct otii_app *self, struct doc_file *file)
{
	if (APP_OPS(self)->GetMode(as_app(self)) == LICENSE_DEMO)
		return;

	struct inv_list *temp_inventory_n = InvListCreate();
	struct occup_mgr *temp_occupations_a = OccupMgrCreate();
	struct date_list_builder *temp_date_list_f = DateListBuilderCreate();
	void *state_mem_g = NULL;
	int32_t state_size = 0;
	uint8_t state_words_n[4] = {0};

	InvListDestroy(&temp_inventory_n);
	OccupMgrDestroy(&temp_occupations_a);
	DateListBuilderDestroy(&temp_date_list_f);
	EvtHandlerRemoveIdleHandler(as_eh(self), false);
	DocFileReadObject2(file, &state_mem_g, &state_size);

	if (state_mem_g)
		memcpy(state_words_n, state_mem_g, state_size < 4 ? (state_size > 0 ? (size_t) state_size : 0) : 4);
	MemFree(state_mem_g);
	self->app_state = ReadLe16(state_words_n);
	self->prev_app_state = ReadLe16(state_words_n + 2);

	if (self->app_state != APP_STATE_TRAVELING && self->app_state != APP_STATE_AT_NODE &&
		self->app_state != APP_STATE_SCENE && self->app_state != APP_STATE_HUNTING)
	{
		if (self->prev_app_state == APP_STATE_TRAVELING || self->prev_app_state == APP_STATE_AT_NODE ||
			self->prev_app_state == APP_STATE_SCENE)
		{
			self->app_state = self->prev_app_state;

		} else {
			self->app_state = APP_STATE_AT_NODE;
		}
	}
	SettingsDeserialize(self->settings, file);
	OtiiAppSetMusicEnabled(self, SettingsMusicOn(self->settings));
	OtiiAppSetSpeechEnabled(self, SettingsSpeechOn(self->settings));
	OtiiAppSetSfxEnabled(self, SettingsSoundEffectsOn(self->settings));
	OtiiAppInitMainGameScreen(self);
	GameLoadState(g_game, file);
	JournalLoadState(&self->journal, file);

	if (self->app_state == APP_STATE_SCENE) {
		struct node_base *node = g_game ? g_game->cur_node : NULL;
		int32_t scene = node && NodeBaseGetId(node) != NODE_KIND_TRAIL ?
			LNodeGetLandmarkConv((struct lnode *) node, g_game) : 0;

		if ((int16_t) scene > 0)
			SceneFrameStartWalk(scene);
		else
			self->app_state = APP_STATE_AT_NODE;
	}
	self->prev_app_state = self->app_state;
	self->app_state = APP_STATE_TITLE;
	OtiiAppApplyScreenMode(self);
	GameRestoreLoadedRng();
	APP_OPS(self)->UpdateEditMenuState(as_app(self), 0);
	g_hunt_started = false;
	AutosaveSkipCurrent();
	AutosaveStart();
}

void OtiiAppNewGameSetup(struct otii_app *self, int32_t command)
{
	struct new_game_scenario new_game_settings_a;
	struct cmd_event *intro_event_p;
	struct new_game_dlg *new_game_dlg_i;
	int32_t dialog_result_p;

	AutosaveStart();
	intro_event_p = CmdEventCreate(CMD_MEDIA_DONE);
	EvtHandlerRemoveIdleHandler(as_eh(self), false);
	DispatchGlobalEvent(intro_event_p);
	OtiiAppLoadMusicCue(self, CUE_SHEET_TRAVEL);
	OtiiAppPlayMusicCue(self, CUE_TITLE);
	OtiiAppSetMusicEnabled(self, SettingsMusicOn(self->settings));
	OtiiAppSetSpeechEnabled(self, SettingsSpeechOn(self->settings));
	OtiiAppSetSfxEnabled(self, SettingsSoundEffectsOn(self->settings));

	if (command == CMD_NEW_GAME_OPTIONS && self->new_game_choices) {
		APP_OPS(self)->UpdateEditMenuState(as_app(self), 0);
		OtiiAppInitMainGameScreen(self);
		GameStartQuickGame(g_game, self->new_game_choices);
		MemFree(self->new_game_choices);
		self->new_game_choices = NULL;
		g_hunt_started = false;
		return;
	}
	if (APP_OPS(self)->GetMode(as_app(self)) == LICENSE_DEMO) {
		struct cstr *msg = CStrCreateFrom("Welcome to the demo version of Oregon Trail II! ");
		CStrAppendBuf(msg, "Many features have been limited or disabled for the demo, ");
		CStrAppendBuf(msg, "so you will only get a glimpse of the full product. ");
		CStrAppendBuf(msg, "When you are ready to play the full game, call 1-800-685-MECC, ext. 269 to order. ");
		CStrAppendBuf(msg, "For details on the limitations of the demo, see the ReadMe file on the CD.");
		struct msg_box box;
		MsgBoxInit(&box, msg, false);
		ModalDialogRun(&box.base, 0, (struct window *) g_parent_win);
		msg_box_destroy((struct object *) &box);
	}
	memset(&new_game_settings_a, 0, sizeof(new_game_settings_a));
	new_game_dlg_i = NewGameDlgCreate();
	if (self->app_state != APP_STATE_NEW_GAME) {
		self->saved_state = self->prev_app_state;
		self->prev_app_state = self->app_state;
		self->app_state = APP_STATE_NEW_GAME;
	}
	APP_OPS(self)->UpdateEditMenuState(as_app(self), 0);
	dialog_result_p = ModalDialogRun((struct modal_dialog *) new_game_dlg_i, (intptr_t) &new_game_settings_a,
		(struct window *) g_parent_win);
	ObjectDelete(new_game_dlg_i);
	if (dialog_result_p != DLG_CANCEL) {
		APP_OPS(self)->UpdateEditMenuState(as_app(self), 0);
		OtiiAppInitMainGameScreen(self);
		GameStartNewGame(g_game, &new_game_settings_a);
	}
	g_hunt_started = false;
}

static void add_pic(struct view *root, int32_t h, int32_t v, int32_t img_id)
{
	struct point pic_origin_m;

	pic_origin_m.h = h;
	pic_origin_m.v = v;
	struct pic_view *pic_e = PicViewCreatePoint(next_view_id(), &pic_origin_m, img_id);
	VIEW_OPS(root)->AddChild(root, (struct view *) pic_e);
}

void OtiiAppInitMainGameScreen(struct otii_app *self)
{
	struct rect screen_rect_c;
	struct point pic_origin_m;
	struct point button_origin_j = {38, 0};
	struct view *root_view_a;
	struct view *old_root_b;
	int32_t button_index_l;

	TitleAnimStop();
	screen_rect_c.top = 0;
	screen_rect_c.left = 0;
	screen_rect_c.right = 640;
	screen_rect_c.bottom = 442;
	root_view_a = ViewCreateId(next_view_id(), &screen_rect_c);
	old_root_b = WindowSetRootView((struct window *) g_parent_win, root_view_a);
	if (old_root_b)
		ObjectDelete(old_root_b);
	ViewNotifyChanged(root_view_a);
	add_pic(root_view_a, 0, 0, 130);
	add_pic(root_view_a, 0, 14, 131);
	add_pic(root_view_a, 618, 0, 132);
	add_pic(root_view_a, 377, 373, 133);
	add_pic(root_view_a, 377, 436, 134);
	add_pic(root_view_a, 433, 380, 135);
	add_pic(root_view_a, 495, 380, 136);
	add_pic(root_view_a, 557, 380, 137);
	add_pic(root_view_a, 619, 380, 138);
	button_origin_j.v = 380;
	button_origin_j.h = 377;
	for (button_index_l = 0; button_index_l < 4; ++button_index_l) {
		struct pict_ctrl *button_p = PictCtrlCreate(button_index_l + TOOL_MAP, &as_eh(self)->eh_event,
			&button_origin_j, button_index_l + 981, 0, 0, NULL, 0, 0, 0);
		VIEW_OPS(root_view_a)->AddChild(root_view_a, (struct view *) button_p);
		button_origin_j.h = button_origin_j.h + 62;
	}
	if (g_game)
		GameDestroy(&g_game);
	JournalClear(&self->journal);
	g_game = GameCreate(100, root_view_a);
	pic_origin_m.h = 22;
	pic_origin_m.v = 373;
	self->ui = MainUiCreate(next_view_id(), &pic_origin_m, 149);
	VIEW_OPS(root_view_a)->AddChild(root_view_a, (struct view *) self->ui);
}

void OtiiAppSaveToDataFile(struct otii_app *self, struct doc_file *file)
{
	uint8_t state_blk_g[8] = {0};

	WriteLe16(state_blk_g, (int16_t) self->app_state);
	WriteLe16(state_blk_g + 2, (int16_t) self->prev_app_state);
	DocFileWriteObject(file, state_blk_g, 8);
	SettingsSerialize(self->settings, file);
	GameSaveState(g_game, file);
	JournalSaveState(&self->journal, file);
}

static int32_t final_score(struct game *g)
{
	if (!g->end_score)
		g->end_score = EndGameCreate(g);
	return g->end_score->final_total;
}

static bool close_current_doc(struct otii_app *self, struct doc_manager *doc_manager_f, struct doc_file *current_doc_e)
{
	bool proceed_f = true;
	int32_t close_choice_c;

	if (current_doc_e) {
		if (DocFileIsDirty(current_doc_e)) {
			close_choice_c = OtiiAppShouldEnableAtLandmark(self) ? DocManagerCloseProceed(doc_manager_f) : 1;
			if (!close_choice_c) {
				DOC_FILE_OPS(current_doc_e)->OpenDefault(current_doc_e);
				DocManagerCloseSingle(doc_manager_f);

			} else if (close_choice_c == 1) {
				DocManagerCloseSingle(doc_manager_f);

			} else {
				proceed_f = false;
			}

		} else {
			DocManagerCloseSingle(doc_manager_f);
		}
	}
	return proceed_f;
}

void OtiiAppHandleDocCommand(struct otii_app *self, int32_t command, void *hwnd)
{
	struct doc_manager *doc_manager_f;
	struct doc_file *current_doc_e;
	struct game_doc *new_doc_e;
	struct path *chosen_path_k;
	int32_t node_id_a;

	doc_manager_f = APP_OPS(self)->GetDocManager(as_app(self));
	if (!doc_manager_f)
		return;
	current_doc_e = DocManagerGetSingleDoc(doc_manager_f);
	switch (command) {
		case CMD_NEW_GAME_OPTIONS:
			if (!DataIs13())
				break;
			// FALL THROUGH INTENTIONAL!
		case CMD_NEW_GAME:
			if (close_current_doc(self, doc_manager_f, current_doc_e)) {
				if (self->app_state) {
					OtiiAppStopSfx(self);
					OtiiAppStopMusic(self);
					OtiiAppPlayMusic(self, 0, false);
					OtiiAppStopSpeech(self);
				}
				if (g_game)
					GameDestroy(&g_game);
				if (self->app_state) {
					self->saved_state = self->prev_app_state;
					self->prev_app_state = self->app_state;
					self->app_state = APP_STATE_TITLE;
				}
				APP_OPS(self)->UpdateEditMenuState(as_app(self), 0);
				if (command == CMD_NEW_GAME_OPTIONS) {
					struct quick_start_dlg *quick_dlg_n;
					int32_t quick_result_n;

					EvtHandlerRemoveIdleHandler(as_eh(self), false);
					MemFree(self->new_game_choices);
					self->new_game_choices = MemAlloc(1, sizeof(struct quick_start_choices));
					quick_dlg_n = QuickStartDlgCreate();
					quick_result_n = ModalDialogRun((struct modal_dialog *) quick_dlg_n,
						(intptr_t) self->new_game_choices, (struct window *) g_parent_win);
					ObjectDelete(quick_dlg_n);
					if (quick_result_n == DLG_CANCEL) {
						PartyDestroy(&self->new_game_choices->party);
						MemFree(self->new_game_choices);
						self->new_game_choices = NULL;
						break;
					}
				}
				new_doc_e = GameDocCreate(command);
				DocManagerAddDoc(doc_manager_f, &new_doc_e->base);
				DocFileSetDirtyV(&new_doc_e->base, false);
			}
			break;
		case CMD_LOAD_GAME:
			if (close_current_doc(self, doc_manager_f, current_doc_e)) {
				chosen_path_k = MakeFile(1, NULL, "Saved Games (*.ot2)", "*.ot2", hwnd);
				if (chosen_path_k->exists) {
					new_doc_e = GameDocCreatePath(chosen_path_k);
					DocManagerAddDoc(doc_manager_f, &new_doc_e->base);
					DocFileSetDirtyV(&new_doc_e->base, false);

				} else {
					PathDestroy(&chosen_path_k);
				}
			}
			break;
		case CMD_END_GAME:
			if (close_current_doc(self, doc_manager_f, current_doc_e)) {
				OtiiAppStopSfx(self);
				OtiiAppStopMusic(self);
				OtiiAppPlayMusic(self, 0, false);
				OtiiAppStopSpeech(self);
				if (self->app_state) {
					self->saved_state = self->prev_app_state;
					self->prev_app_state = self->app_state;
					self->app_state = APP_STATE_TITLE;
				}
				APP_OPS(self)->UpdateEditMenuState(as_app(self), 0);
				if (g_game) {
					node_id_a = g_game->cur_node->node_id;
					switch (node_id_a) {
						case 265:
						case 266:
						case 506:
						case 507:
						case 619:
						case 620:
						case 753: {
							struct cstr *leader_name_a = CStrCreateCopy(PartyGetMember(g_game->party, 0)->name);
							HiscoreInsert(leader_name_a, final_score(g_game));
#ifdef ONLINE_SCORES
							OnlineScoresSubmitGame(g_game, leader_name_a, final_score(g_game));
#endif
							struct hi_score_dlg hiscore_dlg_j;
							HiScoreDlgInit(&hiscore_dlg_j);
							ModalDialogRun(&hiscore_dlg_j.base, 0, (struct window *) g_parent_win);
							hi_score_dlg_destroy((struct object *) &hiscore_dlg_j);
							CStrDestroy(&leader_name_a);
							break;
						}
					}
				}
				int32_t media_done = CMD_MEDIA_DONE;
				EvtHandlerBindHandler(as_eh(self), 1, &media_done);
				struct cmd_event *title_event_p = CmdEventCreate(CMD_MEDIA_DONE);
				EVT_HANDLER_OPS(self)->OnEvent(as_eh(self), title_event_p);
				CmdEventDestroy(&title_event_p);
				WINDOW_OPS(g_parent_win)->Draw((struct window *) g_parent_win, NULL);
				WindowValidate((struct window *) g_parent_win, NULL);
				if (g_game)
					GameDestroy(&g_game);
			}
			break;
		case CMD_SAVE_GAME:
			if (current_doc_e && DocFileIsDirty(current_doc_e))
				DOC_FILE_OPS(current_doc_e)->OpenDefault(current_doc_e);
			break;
		case CMD_SAVE_GAME_AS:
			if (current_doc_e) {
				struct cstr *doc_path = DocFileGetPath(current_doc_e);
				chosen_path_k = MakeFile(0, doc_path, "Saved Games (*.ot2)", "*.ot2", NULL);
				CStrDestroy(&doc_path);
				if (chosen_path_k)
					DocFileSetSource(current_doc_e, chosen_path_k);
			}
			break;
	}
	APP_OPS(self)->UpdateEditMenuState(as_app(self), MENU_ITEM_FILE);
}

void ForceSaveRun(void)
{
	struct otii_app *app = (struct otii_app *) g_app;
	struct doc_file *doc = app && as_app(app)->docmgr ? DocManagerGetSingleDoc(as_app(app)->docmgr) : NULL;

	if (g_game && doc && GetActiveWindowObj() == (struct window *) g_parent_win)
		DOC_FILE_OPS(doc)->OpenDefault(doc);
}

static int32_t intro_movie_id(void)
{
	int32_t size_101 = MciVideoFileSize(101);
	int32_t size_100 = MciVideoFileSize(100);
	// 1.2 intro is 101
	// 1.3 intro is 100

	return size_101 > size_100 ? 101 : 100;
}

static bool intro_playing(void)
{
	struct event query = EventMake(EVCLS_ANY, 98, -1);

	return g_parent_win && g_parent_win->base.root_view &&
		ViewFindByName(g_parent_win->base.root_view, query) != NULL;
}

static bool play_intro_allowed(void)
{
	struct otii_app *app = (struct otii_app *) g_app;

	return app && !g_game && app->app_state == APP_STATE_TITLE && !g_about_open &&
		GetActiveWindowObj() == (struct window *) g_parent_win && !intro_playing();
}

void PlayIntroRun(void)
{
	if (play_intro_allowed())
		EVT_HANDLER_OPS(g_app)->OnIdleSpare((struct evt_handler *) g_app, NULL);
}

void otii_app_update_edit_menu_state(struct app *app, int32_t command)
{
	struct otii_app *self = (struct otii_app *) app;
	struct window *active_window_k;
	struct menu_bar *menubar_m;
	struct window *parent = (struct window *) g_parent_win;

	menubar_m = g_menubar;
	app_update_edit_menu_state(app, command);

	if (!command || command == MENU_ITEM_EDIT) {
		if (self->app_state == APP_STATE_DIARY) {
			MenuBarEnableItem(menubar_m, MENU_ITEM_CUT, true);
			MenuBarEnableItem(menubar_m, MENU_ITEM_COPY, true);
			MenuBarEnableItem(menubar_m, MENU_ITEM_PASTE, true);
			MenuBarEnableItem(menubar_m, MENU_ITEM_DELETE, true);

		} else {
			MenuBarEnableItem(menubar_m, MENU_ITEM_CUT, false);
			MenuBarEnableItem(menubar_m, MENU_ITEM_COPY, false);
			MenuBarEnableItem(menubar_m, MENU_ITEM_PASTE, false);
			MenuBarEnableItem(menubar_m, MENU_ITEM_DELETE, false);
		}
	}

	if (!command || command == MENU_ITEM_FILE) {
		active_window_k = GetActiveWindowObj();
		if (active_window_k != parent || self->app_state) {
			MenuBarEnableItem(menubar_m, MENU_ITEM_LOAD_GAME, false);
			MenuBarEnableItem(menubar_m, MENU_ITEM_NEW_GAME, false);
		}
		if (self->app_state != APP_STATE_AT_NODE) {
			MenuBarEnableItem(menubar_m, MENU_ITEM_EXPORT_DIARY, false);

		} else {
			if (active_window_k == parent) {
				MenuBarEnableItem(menubar_m, MENU_ITEM_EXPORT_DIARY, true);
				MenuBarEnableItem(menubar_m, MENU_ITEM_SAVE_GAME, true);
				MenuBarEnableItem(menubar_m, MENU_ITEM_END_GAME, true);
			}
		}
		if (active_window_k != parent || self->app_state != APP_STATE_AT_NODE) {
			MenuBarEnableItem(menubar_m, MENU_ITEM_SAVE_GAME, false);
			if (active_window_k != parent) {
				MenuBarEnableItem(menubar_m, MENU_ITEM_END_GAME, false);
				MenuBarEnableItem(menubar_m, MENU_ITEM_EXIT, false);

			} else {
				MenuBarEnableItem(menubar_m, MENU_ITEM_EXIT, true);
			}

		} else {
			MenuBarEnableItem(menubar_m, MENU_ITEM_EXIT, true);
		}
		if (APP_OPS(self)->GetMode(app) == LICENSE_DEMO)
			MenuBarEnableItem(menubar_m, MENU_ITEM_LOAD_GAME, false);

		OtiiAppEnableMenuCommand(FORCE_SAVE_MENU_ID,
			g_game && app->docmgr && DocManagerGetSingleDoc(app->docmgr) && active_window_k == parent);
		OtiiAppEnableMenuCommand(PLAY_INTRO_MENU_ID, play_intro_allowed());
	}

	if (!command || command == MENU_ITEM_GAME) {
		MenuBarCheckItem(menubar_m, MENU_ITEM_SOUND_EFFECTS, SettingsSoundEffectsOn(self->settings) != 0);
		MenuBarCheckItem(menubar_m, MENU_ITEM_MUSIC, SettingsMusicOn(self->settings) != 0);
		MenuBarCheckItem(menubar_m, MENU_ITEM_SPEECH, SettingsSpeechOn(self->settings) != 0);
	}
}

static void enter_scene_state(struct otii_app *ga)
{
	if (ga->app_state != APP_STATE_SCENE) {
		ga->saved_state = ga->prev_app_state;
		ga->prev_app_state = ga->app_state;
		ga->app_state = APP_STATE_SCENE;
	}
	APP_OPS(ga)->UpdateEditMenuState(as_app(ga), 0);
}

void OtiiAppApplyScreenMode(struct otii_app *self)
{
	struct event handler_query_d = EventMake(EVCLS_ANY, -1, -1);
	struct control *button_e;
	struct cmd_event *redraw_event_d;
	struct otii_app *app = (struct otii_app *) g_app;

	self->app_state = self->prev_app_state;
	APP_OPS(self)->UpdateEditMenuState(as_app(self), 0);
	self->prev_app_state = self->saved_state;
	self->saved_state = 0;
	switch (self->app_state) {
		case APP_STATE_TRAVELING:
			OtiiAppStopSfx(app);
			GameEnterNode(GetGameObj(), 0);
			break;
		case APP_STATE_AT_NODE:
		case APP_STATE_HUNTING:
			redraw_event_d = CmdEventCreate(GCMD_NODE_CONTINUE);
			DispatchGlobalEvent(redraw_event_d);
			WINDOW_OPS(g_parent_win)->Draw((struct window *) g_parent_win, NULL);
			WindowValidate((struct window *) g_parent_win, NULL);
			break;
		case APP_STATE_SCENE: {
			struct view *doc = GetGameObj()->doc;
			struct event scene_query_g = EventMake(EVCLS_VIEW, -1, -1);
			struct view *old_view_o = VIEW_OPS(doc)->RemoveChild(doc, &scene_query_g);
			if (old_view_o)
				ObjectDelete(old_view_o);
			struct rect doc_bounds = VIEW_OPS(doc)->GetBounds(doc);
			old_view_o = ViewCreateId(-2, &doc_bounds);
			VIEW_OPS(doc)->AddChild(doc, old_view_o);
			{
				struct point pt = {14, 22};
				struct scene_frame *scene_frame_j = SceneFrameCreate(200, &pt, -1);
				enter_scene_state(app);
				if (app->ui)
					MainUiShowNodeDescPanel(app->ui);
				VIEW_OPS(old_view_o)->AddChild(old_view_o, (struct view *) scene_frame_j);
			}
			WINDOW_OPS(g_parent_win)->Draw((struct window *) g_parent_win, NULL);
			WindowValidate((struct window *) g_parent_win, NULL);
			break;
		}
		case APP_STATE_EVENT:
			GameRun(GetGameObj(), NULL);
			break;
	}
	EventSetId(&handler_query_d, TOOL_SUPPLIES);
	EVT_HANDLER_OPS(self)->SetHandledFor(as_eh(self), &handler_query_d);
	button_e = (struct control *) EvtHandlerFindGlobalHandler(as_eh(self), &handler_query_d);
	if (button_e)
		CONTROL_OPS(button_e)->SetValue(button_e, 0, true);
}

void otii_app_on_app_init(struct app *app)
{
	(void) app;
}

void otii_app_on_menus_ready(struct app *self)
{
	(void) self;
}

void otii_app_on_init_complete(struct app *self)
{
	(void) self;
}

void otii_app_on_idle_spare(struct evt_handler *eh, void *param)
{
	struct otii_app *self = (struct otii_app *) eh;
	int32_t movie_id_h;
	struct mci_video *intro_video_g;

	if (param)
		return;
	if (!g_about_open) {
		movie_id_h = intro_movie_id();
		OtiiAppStopSfx(self);
		OtiiAppStopMusic(self);
		OtiiAppPlayMusic(self, 0, false);
		OtiiAppStopSpeech(self);
		if (g_sound_server)
			SND_STREAM_OPS(g_sound_server)->Pause((struct snd_stream *) g_sound_server);
		struct view *root = g_parent_win->base.root_view;
		struct rect root_bounds = VIEW_OPS(root)->GetBounds(root);
		intro_video_g = MciVideoCreate(98, &eh->eh_event, &root_bounds, movie_id_h, false, false);
		ControlSetCommand((struct control *) intro_video_g, CMD_MEDIA_DONE, 0, 0);
		VIEW_OPS(root)->AddChild(root, (struct view *) intro_video_g);
		mci_video_play(intro_video_g);
		int32_t media_done = CMD_MEDIA_DONE;
		EvtHandlerBindHandler(eh, 1, &media_done);
		WINDOW_OPS(g_parent_win)->Draw((struct window *) g_parent_win, NULL);
		WindowValidate((struct window *) g_parent_win, NULL);
	}
}

bool OtiiAppShouldEnableAtLandmark(struct otii_app *self)
{
	(void) self;

	if (!g_game)
		return false;
	if (!g_game->party)
		return false;

	return true;
}

void InventoryAddHuntedMeat(int32_t pounds)
{
	struct inv_list *party_inventory_p;
	struct inv_item *salt_item_p;
	int32_t salt_on_hand_m;
	int32_t salted_pounds_a;
	int32_t unsalted_pounds_o;
	int32_t salt_needed_h;

	struct cstr *meat_name_i = CStrCreateFrom("meat [from hunting]");
	struct cstr *salted_meat_name_n = CStrCreateFrom("salted meat [from hunting]");
	party_inventory_p = &GetGameObj()->party->inventory;
	struct cstr *salt_name_f = CStrCreateFrom("salt");
	salt_needed_h = (pounds + 7) / 8;
	salt_item_p = InvListFindByName(party_inventory_p, salt_name_f);
	if (salt_item_p) {
		salt_on_hand_m = (int16_t) InvItemTotalPortions(salt_item_p);
		if (salt_on_hand_m >= salt_needed_h) {
			salted_pounds_a = pounds;
			unsalted_pounds_o = 0;

		} else {
			salted_pounds_a = 8 * salt_on_hand_m;
			salt_needed_h = salt_on_hand_m;
			unsalted_pounds_o = pounds - salted_pounds_a;
		}
		if (salted_pounds_a) {
			InvListAdd(&GetGameObj()->party->inventory, salted_meat_name_n, salted_pounds_a, 0.0);
			InvItemConsumePortions(salt_item_p, salt_needed_h);
		}
		if (unsalted_pounds_o)
			InvListAdd(&GetGameObj()->party->inventory, meat_name_i, unsalted_pounds_o, 0.0);

	} else {
		InvListAdd(&GetGameObj()->party->inventory, meat_name_i, pounds, 0.0);
	}
	CStrDestroy(&salt_name_f);
	CStrDestroy(&salted_meat_name_n);
	CStrDestroy(&meat_name_i);
}
