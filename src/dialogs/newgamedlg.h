#pragma once

#include <stdint.h>

#include "modaldialog.h"
#include "skilldlg.h"

struct date_list_builder;
struct occup_mgr;

struct new_game_dlg_ops {
	struct modal_dialog_ops base;
};

struct new_game_dlg {
	struct modal_dialog base;
	int32_t year;
	int32_t spent;
	struct skill_flags skill_flags;
	struct occup_mgr *occmgr;
	struct date_list_builder *date_lists;
};

#define NEW_GAME_DLG_OPS(p) ((const struct new_game_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct new_game_dlg_ops NEW_GAME_DLG_OPS_TABLE;

void NewGameDlgInit(struct new_game_dlg *self);
struct new_game_dlg *NewGameDlgCreate(void);
void NewGameDlgDestroy(struct new_game_dlg **ctx_mod);
void new_game_dlg_destroy(struct object *self);

void new_game_dlg_begin(struct modal_dialog *self, intptr_t param);
void new_game_dlg_accept(struct modal_dialog *self, intptr_t out);

void NewGameDlgApplyOccupationSkills(struct new_game_dlg *self);
void NewGameDlgUpdateSkillsLabel(struct new_game_dlg *self);
void NewGameDlgUpdateCashLabel(struct new_game_dlg *self);
void NewGameDlgRandomize(struct new_game_dlg *self);
void NewGameDlgUpdateDateCombos(struct new_game_dlg *self);
void NewGameDlgUpdateMemberFields(struct new_game_dlg *self);
void NewGameDlgUpdateWagonLabels(struct new_game_dlg *self);
bool NewGameDlgValidateName(struct new_game_dlg *self);

void new_game_dlg_on_event(struct evt_handler *self, struct cmd_event *event);

#ifdef TWITCH_PARTY
struct twitch_message;

int32_t NewGamePartyPick(struct new_game_dlg *dlg, const struct twitch_message *m);

int32_t NewGameTestAge(const char *text);
int32_t NewGamePickedSlots(void);

struct new_game_dlg *NewGameDlgRunning(void);
#endif
