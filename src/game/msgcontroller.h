#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../app/evthandler.h"

struct cmd_event;
struct inv_item;
struct member;
struct msg;

struct msg_controller {
	struct evt_handler base;
	bool trail_muddy;
	bool fallen_timbers_armed;
	bool fallen_rocks_armed;
	int32_t unused_1C;
	int32_t strangers_node_id;
	int32_t prairie_fire_cooldown;
	int32_t alkali_exposure;
	int32_t unused_24;
	int32_t days_since_dismissal;
	int32_t dry_days;
	bool low_morale_warned;
	int32_t wagon_dust_cooldown;
	bool thirst_warned;
	int32_t mosquito_cooldown;
};

extern const struct evt_handler_ops MSG_CONTROLLER_OPS_TABLE;

extern bool g_conditions_checked;

extern int32_t g_item_soap;
extern int32_t g_item_ax;
extern int32_t g_item_hatchet;
extern int32_t g_item_pick_ax;
extern int32_t g_item_saw;
extern int32_t g_item_shovel;
extern int32_t g_item_anvil;
extern int32_t g_item_hammer;
extern int32_t g_item_nails;
extern int32_t g_item_rope;
extern int32_t g_item_chain;
extern int32_t g_item_blanket;
extern int32_t g_item_winter_coat;
extern int32_t g_item_winter_scarf;
extern int32_t g_item_mittens;
extern int32_t g_item_boots;
extern int32_t g_item_long_underwear;
extern int32_t g_item_matches;
extern int32_t g_item_set_of_cooking_utensils;
extern int32_t g_item_water_keg_empty_holds_4_;
extern int32_t g_item_canteen;
extern int32_t g_item_grease;
extern int32_t g_item_turpentine;
extern int32_t g_item_spare_horse_or_mule_harn;
extern int32_t g_item_bridle;
extern int32_t g_item_rifle_or_shotgun_sheath;

void MsgControllerInit(struct msg_controller *self);
struct msg_controller *MsgControllerCreate(void);
void MsgControllerDestroy(struct msg_controller **ctx_mod);
void msg_controller_destroy(struct object *self);

void MsgControllerAdjustMorale(struct msg_controller *self, int32_t delta);
int32_t MsgControllerPickRandomBrokenBone(struct msg_controller *self);
void MsgControllerDispatchTalkEvent(struct msg_controller *self, intptr_t target);
void MsgControllerStartHunt(struct msg_controller *self, struct msg *node);
int32_t MsgControllerShowMsgboxText(struct msg_controller *self, const char *text, bool two_buttons);
void MsgControllerConsumeSuppliesAmount(struct msg_controller *self, struct msg *dlg);
int32_t MsgControllerGetInventoryQty(struct msg_controller *self, int32_t catalog_index);
bool MsgControllerPartyHasSkill(struct msg_controller *self, int32_t index);
int32_t MsgControllerWeatherHazardChance(struct msg_controller *self, int32_t terrain_type, int32_t month,
	int32_t day);
bool MsgControllerRollRandomAccident(struct msg_controller *self, int32_t chance);
bool MsgControllerRollRandomMisfortune(struct msg_controller *self, int32_t chance);
bool MsgControllerRollMisfortune2(struct msg_controller *self, int32_t chance);
void MsgControllerReportSupplyLoss(struct msg_controller *self, int32_t event_code, int32_t amount, bool flag,
	bool flag2);
void MsgControllerKillDraftAnimals(struct msg_controller *self, int32_t count, int32_t msg_id);
bool MsgControllerPartyHasItem(struct msg_controller *self, int32_t group);
bool MsgControllerPartyWellEquipped(struct msg_controller *self);
int32_t MsgControllerPartyGetOccupation(struct msg_controller *self);
int32_t MsgControllerGetPeopleCount(struct msg_controller *self);
struct member *MsgControllerPickRandomMember(struct msg_controller *self);
void MsgControllerEventLightningKillsAnimal(struct msg_controller *self);
struct inv_item *MsgControllerPickRandomDraftAnimal(struct msg_controller *self);
struct msg *MsgControllerPostMemberMissing(struct msg_controller *self);
int32_t MsgControllerDoRestDays(struct msg_controller *self, int32_t days);
void MsgControllerSetPacePenalty(struct msg_controller *self, int32_t cap);
int32_t MsgControllerPickRandomSprain(struct msg_controller *self);
bool MsgControllerPartyHasPlentyOfFood(struct msg_controller *self);
void MsgControllerRunDailySim(struct msg_controller *self, bool travelling);
void MsgControllerRollDailyConditions(struct msg_controller *self);
void MsgControllerRollDailyHazards(struct msg_controller *self);
void MsgControllerRollMemberDailyEvents(struct msg_controller *self);
void MsgControllerGenerateDailyEvents(struct msg_controller *self);
void MsgControllerRollFoodSpoilage(struct msg_controller *self);
void MsgControllerHandleEvent(struct msg_controller *self, struct msg *node);

void msg_controller_on_event(struct evt_handler *self, struct cmd_event *event);

bool EventRollAilment(int32_t code, int32_t node_id);
bool EventRollAilment2(int32_t code, int32_t node_id);
void InventoryAddMeat(int32_t pounds);
