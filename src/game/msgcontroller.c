#include "msgcontroller.h"

#include <stddef.h>
#include <stdlib.h>

#include "ailment.h"
#include "../app/app.h"
#include "../base/cstr.h"
#include "../base/date.h"
#include "draftanimalitem.h"
#include "../app/event.h"
#include "../ui/framewin.h"
#include "game.h"
#include "gamecmd.h"
#include "../trail/hillrules.h"
#include "invitem.h"
#include "invlist.h"
#include "../screens/mainui.h"
#include "member.h"
#include "../base/memory.h"
#include "msg.h"
#include "../dialogs/msgbox.h"
#include "msgqueue.h"
#include "../trail/node.h"
#include "../app/otiiapp.h"
#include "party.h"
#include "../base/rng.h"
#include "../base/str.h"
#include "wagonitem.h"
#include "weather.h"

int32_t g_item_soap;
int32_t g_item_ax;
int32_t g_item_hatchet;
int32_t g_item_pick_ax;
int32_t g_item_saw;
int32_t g_item_shovel;
int32_t g_item_anvil;
int32_t g_item_hammer;
int32_t g_item_nails;
int32_t g_item_rope;
int32_t g_item_chain;
int32_t g_item_blanket;
int32_t g_item_winter_coat;
int32_t g_item_winter_scarf;
int32_t g_item_mittens;
int32_t g_item_boots;
int32_t g_item_long_underwear;
int32_t g_item_matches;
int32_t g_item_set_of_cooking_utensils;
int32_t g_item_water_keg_empty_holds_4_;
int32_t g_item_canteen;
int32_t g_item_grease;
int32_t g_item_turpentine;
int32_t g_item_spare_horse_or_mule_harn;
int32_t g_item_bridle;
int32_t g_item_rifle_or_shotgun_sheath;

bool g_conditions_checked = false;

const struct evt_handler_ops MSG_CONTROLLER_OPS_TABLE = {
	{msg_controller_destroy},
	evt_handler_is_handled,
	evt_handler_set_handled_for,
	evt_handler_set_handled,
	evt_handler_clear_handled_for,
	evt_handler_clear_handled,
	msg_controller_on_event,
	evt_handler_on_idle,
	evt_handler_on_idle_spare,
};

static int32_t lookup_item(struct cstr *scratch, const char *name)
{
	CStrAssignBuf(scratch, name);

	return LookupItemId(scratch);
}

void MsgControllerInit(struct msg_controller *self)
{
	EvtHandlerInit(&self->base, EVCLS_ANY, 1234);
	self->base.base.ops = &MSG_CONTROLLER_OPS_TABLE.base;

	int32_t handlers[2];
	struct cstr *scratch = CStrCreateFrom("soap");

	g_item_soap = LookupItemId(scratch);
	g_item_ax = lookup_item(scratch, "ax");
	g_item_hatchet = lookup_item(scratch, "hatchet");
	g_item_pick_ax = lookup_item(scratch, "pick ax");
	g_item_saw = lookup_item(scratch, "saw");
	g_item_shovel = lookup_item(scratch, "shovel");
	g_item_anvil = lookup_item(scratch, "anvil");
	g_item_hammer = lookup_item(scratch, "hammer");
	g_item_nails = lookup_item(scratch, "nails");
	g_item_rope = lookup_item(scratch, "rope");
	g_item_chain = lookup_item(scratch, "chain");
	g_item_blanket = lookup_item(scratch, "blanket");
	g_item_winter_coat = lookup_item(scratch, "winter coat");
	g_item_winter_scarf = lookup_item(scratch, "winter scarf");
	g_item_mittens = lookup_item(scratch, "mittens");
	g_item_boots = lookup_item(scratch, "boots");
	g_item_long_underwear = lookup_item(scratch, "long underwear");
	g_item_matches = lookup_item(scratch, "matches");
	g_item_set_of_cooking_utensils = lookup_item(scratch, "set of cooking utensils");
	g_item_water_keg_empty_holds_4_ = lookup_item(scratch, "water keg [empty; holds 4 gallons full]");
	g_item_canteen = lookup_item(scratch, "canteen");
	g_item_grease = lookup_item(scratch, "grease");
	g_item_turpentine = lookup_item(scratch, "turpentine");
	g_item_spare_horse_or_mule_harn = lookup_item(scratch, "spare horse or mule harness");
	g_item_bridle = lookup_item(scratch, "bridle");
	g_item_rifle_or_shotgun_sheath = lookup_item(scratch, "rifle or shotgun sheath");

	self->trail_muddy = false;
	self->fallen_timbers_armed = true;
	self->fallen_rocks_armed = true;
	self->unused_1C = 0;
	self->strangers_node_id = 0;
	self->prairie_fire_cooldown = 0;
	self->unused_24 = 0;
	self->days_since_dismissal = 0;
	self->low_morale_warned = false;
	self->wagon_dust_cooldown = 0;
	self->thirst_warned = false;
	self->dry_days = 0;
	self->alkali_exposure = 0;
	self->mosquito_cooldown = 0;

	handlers[0] = 13;
	handlers[1] = 14;
	EvtHandlerBindHandler(&self->base, 2, handlers);

	CStrDestroy(&scratch);
}

struct msg_controller *MsgControllerCreate(void)
{
	struct msg_controller *self = MemAlloc(1, sizeof(struct msg_controller));
	MsgControllerInit(self);

	return self;
}

void MsgControllerDestroy(struct msg_controller **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void msg_controller_destroy(struct object *self)
{
	self->ops = &EVT_HANDLER_OPS_TABLE.base;
	evt_handler_destroy(self);
}

void MsgControllerRunDailySim(struct msg_controller *self, bool travelling)
{
	if (travelling) {
		MsgControllerRollDailyConditions(self);
		MsgControllerRollMemberDailyEvents(self);
		MsgControllerGenerateDailyEvents(self);

	} else {
		MsgControllerRollFoodSpoilage(self);
	}
}

static void party_add_reputation(struct party *p, int32_t d)
{
	p->reputation = p->reputation + d;
	if (p->reputation > 0x64)
		p->reputation = 0x64;
	if (p->reputation < -0x32)
		p->reputation = -0x32;
}

static void party_add_health_drain(struct party *p, int32_t d)
{
	p->daily_health_drain = (int16_t) (p->daily_health_drain + d);
}

static void set_player_level(int32_t m)
{
	struct game *g = GetGameObj();

	g->player_level = m;
}

static void apply_treatment(struct ailment *ailm, int32_t eff, uint8_t soap_qty)
{
	if (ailm)
		AILMENT_OPS(ailm)->ApplyEffects(ailm, eff, soap_qty != 0);
}

static void event_check_animal(struct msg *node)
{
	static struct inv_item *standin[3];
	struct party *party = GetGameObj()->party;
	struct list_node *item;
	int32_t kind;

	switch (node->number_index) {
	case MSG_MISSING_ANIMAL:
	case MSG_MISSING_ANIMALS:
	case MSG_SICK_LIVESTOCK:
	case MSG_INJURED_LIVESTOCK:
	case MSG_ANIMAL_IN_QUICKSAND:
	case MSG_ANIMAL_DIED_ACCIDENT:
	case MSG_ANIMAL_IN_HOLE:
		break;
	default:
		return;
	}

	for (item = party->animals.base.head; item; item = item->next)
		if ((intptr_t) InvItemFromNode(item) == node->subject)
			return;

	for (item = party->spare_animals.base.head; item; item = item->next)
		if ((intptr_t) InvItemFromNode(item) == node->subject)
			return;

	kind = node->scene_variant >= 0 && node->scene_variant <= 2 ? node->scene_variant : 0;
	if (!standin[kind]) {
		static const char *const names[3] = {"ox", "horse", "mule"};
		struct cstr *name = CStrCreateFrom(names[kind]);
		standin[kind] = InvItemCreate(name, 0, 1.0);
		CStrDestroy(&name);
	}

	node->subject = (intptr_t) standin[kind];
}

static struct cstr *report_supply_loss_rest(struct inv_list *lost)
{
	struct cstr *all = InvListFormatContentsAll(lost, true);
	InvListDestroy(&lost);

	return all;
}

static void dispatch_command(int32_t command)
{
	struct cmd_event *cmd = CmdEventCreate(command);
	DispatchGlobalEvent(cmd);
}

void MsgControllerHandleEvent(struct msg_controller *self, struct msg *node)
{
	struct game *game = GetGameObj();
	struct cstr *line2_a;
	uint8_t soap_qty_i;
	struct cstr *line1_a;
	int32_t terrain_d;
	struct cstr *title_g;
	char text_b[128];
	int32_t morale_c;
	bool retry_b;
	int32_t retries_left_o;
	int32_t chance_i;
	int32_t chance_hi_g;
	int32_t outcome_e;
	int32_t rest_days_a;

	if (PartyMemberCount(game->party, true) == 0) {
		node->choice_state = 2;
		return;
	}

	event_check_animal(node);

	retries_left_o = 7;
	morale_c = 0;
	outcome_e = 2;
	chance_i = 0;
	chance_hi_g = 0;
	soap_qty_i = (uint8_t) MsgControllerGetInventoryQty(self, g_item_soap);
	title_g = NULL;
	line1_a = NULL;
	line2_a = NULL;
	rest_days_a = -1;
	retry_b = false;

	do {
		if (NodeBaseGetId(game->cur_node) == NODE_KIND_TRAIL) {
			terrain_d = NodeGetTerrain((struct node *) game->cur_node);

		} else {
			terrain_d = 0;
		}

		switch (node->number_index) {
		case MSG_BLIZZARD:
			switch (node->rsrc_index) {
			case 0:
				MsgControllerSetPacePenalty(self, 0x32);
				morale_c = MsgControllerRollMisfortune2(self, 3) ? 1 : -2;
				break;
			case 1:
				MsgControllerSetPacePenalty(self, 0x21);
				morale_c = MsgControllerRollMisfortune2(self, 2) ? 2 : -2;
				break;
			case 2:
				rest_days_a = 25;
				morale_c = MsgControllerPartyHasPlentyOfFood(self) ? 1 : -1;
				break;
			}
			break;

		case MSG_SNOWBOUND:
			if (game->weather->snow_depth > node->member / 2) {
				if (game->weather->snow_depth >= 150)
					outcome_e = 1;
			}
			switch (node->rsrc_index) {
			case 0:
				MsgControllerSetPacePenalty(self, 0x21);
				morale_c = MsgControllerRollMisfortune2(self, 3) ? 1 : -2;
				break;
			case 1:
				MsgControllerSetPacePenalty(self, 0x10);
				morale_c = MsgControllerRollMisfortune2(self, 2) ? 2 : -2;
				break;
			case 2:
				rest_days_a = 25;
				morale_c = MsgControllerPartyHasPlentyOfFood(self) ? 1 : -1;
				break;
			}
			break;

		case MSG_THUNDERSTORM:
			switch (node->rsrc_index) {
			case 0:
				MsgControllerSetPacePenalty(self, 0x32);
				morale_c = MsgControllerRollMisfortune2(self, 3) ? 1 : -2;
				if (RandRangeAvg(1, 10000, 1) <= 20) {
					MsgControllerEventLightningKillsAnimal(self);
					line1_a = CStrCreateFrom("Lightning killed one of our draft animals.");
				}
				break;
			case 1:
				MsgControllerSetPacePenalty(self, 0x21);
				morale_c = MsgControllerRollMisfortune2(self, 2) ? 2 : -2;
				if (RandRangeAvg(1, 10000, 1) <= 10) {
					MsgControllerEventLightningKillsAnimal(self);
					line1_a = CStrCreateFrom("Lightning killed one of our draft animals.");
				}
				break;
			case 2:
				rest_days_a = 25;
				morale_c = MsgControllerPartyHasPlentyOfFood(self) ? 1 : -1;
				if (RandRangeAvg(1, 10000, 1) <= 5) {
					MsgControllerEventLightningKillsAnimal(self);
					line1_a = CStrCreateFrom("Lightning killed one of our draft animals.");
				}
				break;
			}
			break;

		case MSG_HAILSTORM:
			switch (node->rsrc_index) {
			case 0:
				MsgControllerSetPacePenalty(self, 0x32);
				morale_c = MsgControllerRollMisfortune2(self, 3) ? 1 : -2;
				break;
			case 1:
				MsgControllerSetPacePenalty(self, 0x21);
				morale_c = MsgControllerRollMisfortune2(self, 2) ? 1 : -2;
				break;
			case 2:
				rest_days_a = 25;
				morale_c = MsgControllerPartyHasPlentyOfFood(self) ? 1 : -1;
				break;
			}
			if (RandRangeAvg(1, 10000, 1) <= 10) {
				MsgControllerEventLightningKillsAnimal(self);
				line1_a = CStrCreateFrom("Lightning killed one of our draft animals.");
			}
			break;

		case MSG_HEAVY_FOG:
			switch (node->rsrc_index) {
			case 0:
				MsgControllerSetPacePenalty(self, 0x4B);
				morale_c = MsgControllerRollMisfortune2(self, 3) ? 1 : -2;
				break;
			case 1:
				MsgControllerSetPacePenalty(self, 0x32);
				morale_c = MsgControllerRollMisfortune2(self, 2) ? 2 : -2;
				break;
			case 2:
				rest_days_a = 25;
				morale_c = MsgControllerPartyHasPlentyOfFood(self) ? 1 : -1;
				break;
			}
			break;

		case MSG_FALLEN_TIMBERS:
		case MSG_FALLEN_ROCKS:
			switch (node->rsrc_index) {
			case 0:
				if (RandRangeAvg(1, 100, 1) <= 50) {
					MsgControllerSetPacePenalty(self, 0x19);
					morale_c = MsgControllerRollRandomMisfortune(self, 0x14) ? 2 : -2;

				} else {
					MsgControllerShowMsgboxText(self, "You were unable to continue over the obstruction.", false);
					morale_c = MsgControllerRollRandomMisfortune(self, 0x1E) ? -1 : -3;
					outcome_e = 0;
					MsgClearFlag(node, node->rsrc_index);
					rest_days_a = 25;
					line1_a = CStrCreateFrom("We were unable to do continue.");
				}
				break;
			case 1:
				if ((node->number_index == MSG_FALLEN_TIMBERS
					&& (MsgControllerGetInventoryQty(self, g_item_ax) || MsgControllerGetInventoryQty(self, g_item_hatchet)
					|| MsgControllerGetInventoryQty(self, g_item_pick_ax) || MsgControllerGetInventoryQty(self, g_item_saw)))
					|| (node->number_index == MSG_FALLEN_ROCKS
					&& (MsgControllerGetInventoryQty(self, g_item_pick_ax) || MsgControllerGetInventoryQty(self, g_item_shovel)))) {
					chance_i = 40;
					chance_hi_g = 10;
				}
				if (MsgControllerGetPeopleCount(self) > 50) {
					chance_i = chance_i + 40;
					chance_hi_g = 5;
				}
				if (RandRangeAvg(1, 100, 1) <= chance_i) {
					morale_c = 2;
					MsgControllerSetPacePenalty(self, 0x19);

				} else {
					MsgControllerShowMsgboxText(self, "You were unable to clear the path.", false);
					outcome_e = 0;
					MsgClearFlag(node, node->rsrc_index);
					rest_days_a = 24;
					line1_a = CStrCreateFrom("We were unable to clear the path.");
				}
				if (!MsgControllerRollRandomAccident(self, chance_hi_g))
					morale_c = morale_c - 1;
				break;
			case 2:
				chance_i = MsgControllerPartyHasSkill(self, SKILL_TRACKING) ? 50 : 25;
				if (RandRangeAvg(1, 100, 1) <= chance_i) {
					MsgControllerSetPacePenalty(self, 0x19);
					morale_c = MsgControllerRollRandomMisfortune(self, 5) ? 2 : 0;

				} else {
					MsgControllerShowMsgboxText(self, "You were unable to find another path.", false);
					morale_c = MsgControllerRollRandomMisfortune(self, 5) ? -1 : -3;
					outcome_e = 0;
					MsgClearFlag(node, node->rsrc_index);
					rest_days_a = 24;
					line1_a = CStrCreateFrom("We were unable to find another path.");
				}
				break;
			case 3:
				morale_c = -3;
				if (RandRangeAvg(1, 100, 1) <= 15) {
					const char *msg_text_h = "Another group of emigrants cleared the path.";
					MsgControllerShowMsgboxText(self, msg_text_h, false);
					line1_a = CStrCreateFrom(msg_text_h);

				} else {
					outcome_e = 1;
					rest_days_a = 25;
				}
				break;
			}
			break;

		case MSG_FLOODED_TRAIL:
			switch (node->rsrc_index) {
			case 0:
				chance_i = MsgControllerPartyHasSkill(self, SKILL_RIVERWORK) ? 5 : 10;
				morale_c = 1;
				MsgControllerSetPacePenalty(self, 0x19);
				if (RandRangeAvg(1, 100, 1) <= chance_i) {
					MsgControllerReportSupplyLoss(self, MSG_TIPPED_WAGON, 0xA, true, false);
					morale_c = morale_c - 2;

				} else {
					if (RandRangeAvg(1, 100, 1) <= chance_i)
						MsgQueueEnqueue(GetMessageQueue(), MSG_STUCK_IN_MUD, NULL, 0, 0, NULL, -1);
				}
				break;
			case 1:
				chance_i = MsgControllerPartyHasSkill(self, SKILL_TRACKING) ? 50 : 25;
				if (RandRangeAvg(1, 100, 1) <= chance_i) {
					MsgControllerSetPacePenalty(self, 0x19);
					morale_c = MsgControllerRollRandomMisfortune(self, 5) ? 2 : 0;

				} else {
					MsgControllerShowMsgboxText(self, "You were unable to find another path.", false);
					morale_c = MsgControllerRollRandomMisfortune(self, 5) ? -1 : -3;
					outcome_e = 0;
					MsgClearFlag(node, node->rsrc_index);
					rest_days_a = 24;
					line1_a = CStrCreateFrom("We were unable to find another path.");
				}
				break;
			case 2:
				morale_c = MsgControllerPartyHasPlentyOfFood(self) ? 1 : -1;
				if (WeatherSumPrecip(game->weather, 3)) {
					outcome_e = 1;
					rest_days_a = 25;
					if (node->choice_state == 1)
						line1_a = CStrCreateFrom("We are still waiting.");
				}
				break;
			}
			break;

		case MSG_BROKEN_WHEEL:
		case MSG_BROKEN_AXLE:
		case MSG_BROKEN_TONGUE:
		case MSG_BROKEN_YOKE: {
			struct inv_item *repl_item;
			int32_t part_id_e = 0;
			struct cstr *spare_name_n = CStrCreateFrom("spare ");
			struct cstr *part_name_f = CStrCreateFrom("");

			switch (node->number_index) {
			case MSG_BROKEN_WHEEL:
				CStrAssignBuf(part_name_f, "wagon wheel");
				part_id_e = 20;
				break;
			case MSG_BROKEN_AXLE:
				CStrAssignBuf(part_name_f, "wagon axle");
				part_id_e = 21;
				break;
			case MSG_BROKEN_TONGUE:
				CStrAssignBuf(part_name_f, "wagon tongue");
				part_id_e = 22;
				break;
			case MSG_BROKEN_YOKE:
				CStrAssignBuf(part_name_f, "ox yoke");
				part_id_e = 23;
				break;
			}

			switch (node->rsrc_index) {
			case 0:
				chance_i = 10;
				if (MsgControllerPartyHasSkill(self, SKILL_BLACKSMITHING))
					chance_i = chance_i + 40;
				if (MsgControllerPartyHasSkill(self, SKILL_CARPENTRY))
					chance_i = chance_i + 40;
				if (MsgControllerGetInventoryQty(self, g_item_anvil))
					chance_i = chance_i + 5;
				if (MsgControllerGetInventoryQty(self, g_item_hammer))
					chance_i = chance_i + 5;
				if (MsgControllerGetInventoryQty(self, g_item_nails))
					chance_i = chance_i + 5;
				if (MsgControllerGetInventoryQty(self, g_item_saw))
					chance_i = chance_i + 5;
				if (RandRangeAvg(1, 100, 1) <= chance_i) {
					morale_c = 1;
					line1_a = CStrCreateFrom("We were successful.");

				} else {
					line1_a = CStrCreateFrom("We were unable to make the repair.");
					StrCopy(text_b, "You were unable to repair the ");
					StrCat(text_b, CStrGet(part_name_f));
					CStrAppendStr(spare_name_n, part_name_f);
					repl_item = InvListFindByName(&game->party->inventory, spare_name_n);
					if (!repl_item) {
						StrCat(text_b, ".");
						outcome_e = 0;
						MsgClearFlag(node, node->rsrc_index);

					} else {
						StrCat(text_b, " and had to replace it from supplies.");
						InvItemConsumePortions(repl_item, 1);
						line2_a = CStrCreateFrom("We were able to replace it from supplies.");
					}
					MsgControllerShowMsgboxText(self, text_b, false);
					rest_days_a = 24;
				}
				break;
			case 1:
				CStrAppendStr(spare_name_n, part_name_f);
				repl_item = InvListFindByName(&game->party->inventory, spare_name_n);
				if (repl_item)
					InvItemConsumePortions(repl_item, 1);
				break;
			case 2: {
				struct cmd_event *item_cmd_a = CmdEventCreate(GCMD_OPEN_TRADE_DLG);
				item_cmd_a->param1 = (part_id_e << 16) | 0xFFFF;
				item_cmd_a->param2 = -1;
				DispatchGlobalEvent(item_cmd_a);
				CStrAppendStr(spare_name_n, part_name_f);
				repl_item = InvListFindByName(&game->party->inventory, spare_name_n);
				if (!repl_item) {
					outcome_e = 0;

				} else {
					InvItemConsumePortions(repl_item, 1);
				}
				break;
			}
			}
			rest_days_a = 24;

			CStrDestroy(&part_name_f);
			CStrDestroy(&spare_name_n);
			break;
		}

		case MSG_MISSING_ANIMAL:
		case MSG_MISSING_ANIMALS: {
			int32_t day_d;
			bool stop_b;
			int32_t rest_len_l;

			title_g = InvItemGetNamePlural((struct inv_item *) node->subject);
			switch (node->rsrc_index) {
			case 0:
				if (PartyCalcPullingMode(game->party) >= node->member)
					morale_c = -1;
				else
					morale_c = -2;
				break;
			case 1:
				rest_days_a = 24;
				if (MsgControllerPartyHasSkill(self, SKILL_FARMING))
					chance_i = 0x41;
				else
					chance_i = 0x32;
				if (MsgControllerPartyHasSkill(self, SKILL_TRACKING))
					chance_i = 0x50;
				if (RandRangeAvg(1, 100, 1) <= chance_i) {
					if (MsgControllerRollRandomAccident(self, 5))
						morale_c = 0;
					else
						morale_c = -1;
					MsgControllerConsumeSuppliesAmount(self, node);
					if (node->number_index == MSG_MISSING_ANIMAL) {
						MsgControllerShowMsgboxText(self, "You found the missing animal.", false);
						line1_a = CStrCreateFrom("We found the missing animal.");

					} else {
						MsgControllerShowMsgboxText(self, "You found the missing animals.", false);
						line1_a = CStrCreateFrom("We found the missing animals.");
					}

				} else {
					if (node->number_index == MSG_MISSING_ANIMAL)
						MsgControllerShowMsgboxText(self, "You were unable to find the missing animal.", false);
					else
						MsgControllerShowMsgboxText(self, "You were unable to find the missing animals.", false);
					if (MsgControllerRollRandomAccident(self, 5))
						morale_c = -1;
					else
						morale_c = -2;
				}
				if (RandRangeAvg(1, 100, 1) <= 1) {
					MsgControllerPostMemberMissing(self);
					morale_c = morale_c - 2;
				}
				break;
			case 2:
				rest_len_l = MsgControllerDoRestDays(self, 0);
				stop_b = false;
				if (rest_len_l == 0) {
					outcome_e = 0;
					break;
				}
				for (day_d = 0; day_d < rest_len_l && !stop_b; day_d++) {
					if (day_d == 0)
						chance_i = 0x1F4;
					else
						chance_i = 5;
					if (RandRangeAvg(1, 10000, 1) <= chance_i)
						stop_b = true;
				}
				if (stop_b) {
					MsgControllerConsumeSuppliesAmount(self, node);
					if (node->number_index == MSG_MISSING_ANIMAL) {
						MsgControllerShowMsgboxText(self, "The missing animal has returned.", false);
						line1_a = CStrCreateFrom("The missing animal returned.");

					} else {
						MsgControllerShowMsgboxText(self, "The missing animals have returned.", false);
						line1_a = CStrCreateFrom("The missing animals returned.");
					}

				} else {
					if (node->number_index == MSG_MISSING_ANIMAL) {
						MsgControllerShowMsgboxText(self, "The missing animal did not return.", false);
						line1_a = CStrCreateFrom("The missing animal did not return.");

					} else {
						MsgControllerShowMsgboxText(self, "The missing animals did not return.", false);
						line1_a = CStrCreateFrom("The missing animals did not return.");
					}
					morale_c = (day_d > 2) ? -2 : -1;
				}
				break;
			}
			break;
		}

		case MSG_MISSING_PERSON: {
			int32_t day2_d;
			bool stop2_j;
			int32_t rest_len2_o;
			struct member *member_a = (struct member *) node->member;

			title_g = CStrCreateCopy(member_a->name);
			switch (node->rsrc_index) {
			case 0:
				if (MsgControllerShowMsgboxText(self, "Are you sure you want to continue without the missing person?  It may have serious consequences.", true))
					outcome_e = 0;
				else
					morale_c = -20;
				break;
			case 1:
				if (MsgControllerPartyHasSkill(self, SKILL_TRACKING))
					chance_i = 0x64;
				else
					chance_i = 0x5F;
				if (RandRangeAvg(1, 100, 1) <= chance_i) {
					morale_c = 3;
					member_a->state = MEMBER_ALIVE;
					if (member_a->state == MEMBER_DEAD)
						member_a->health = 0;
					MsgControllerShowMsgboxText(self, "You found the missing person.", false);
					line1_a = CStrCreateFrom("The search was successful.");

				} else {
					morale_c = -8;
					MsgControllerShowMsgboxText(self, "You were unable to find the missing person.", false);
					line1_a = CStrCreateFrom("The search was unsuccessful.");
				}
				if (!MsgControllerRollRandomAccident(self, 5))
					morale_c = morale_c + 1;
				break;
			case 2:
				rest_len2_o = MsgControllerDoRestDays(self, 0);
				stop2_j = false;
				if (rest_len2_o == 0) {
					outcome_e = 0;
					break;
				}
				for (day2_d = 0; day2_d < rest_len2_o && !stop2_j; day2_d++) {
					if (day2_d == 0)
						chance_i = 0x1388;
					else
						chance_i = 0x1F4;
					if (RandRangeAvg(1, 10000, 1) <= chance_i)
						stop2_j = true;
				}
				if (stop2_j) {
					member_a->state = MEMBER_ALIVE;
					if (member_a->state == MEMBER_DEAD)
						member_a->health = 0;
					MsgControllerShowMsgboxText(self, "The missing person has returned.", false);
					line1_a = CStrCreateCopy(title_g);
					CStrAppendBuf(line1_a, " returned.");

				} else {
					MsgControllerShowMsgboxText(self, "The missing person has not returned.", false);
					line1_a = CStrCreateCopy(title_g);
					CStrAppendBuf(line1_a, " never did return.");
				}
				break;
			}
			break;
		}

		case MSG_SICK_LIVESTOCK:
			title_g = InvItemGetNamePlural((struct inv_item *) node->subject);
			break;

		case MSG_INJURED_LIVESTOCK: {
			struct inv_item *gift_animal;
			struct inv_list *spares;

			title_g = InvItemGetNamePlural((struct inv_item *) node->subject);
			switch (node->rsrc_index) {
			case 0:
				if (PartyCalcPullingMode(game->party) < node->member)
					morale_c = -1;
				break;
			case 1:
				gift_animal = (struct inv_item *) node->subject;
				spares = &game->party->spare_animals;
				rest_days_a = 0;
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = 1;
				else
					morale_c = -1;
				if (RandRangeAvg(1, 100, 1) <= 0x32) {
					InvItemAddUnits(gift_animal, 1);

				} else {
					PartyAddDraftAnimal(game->party, spares, InvItemGetNameCStr(gift_animal), 1);
					morale_c = -2;
				}
				break;
			case 2:
				gift_animal = (struct inv_item *) node->subject;
				spares = &game->party->spare_animals;
				MsgControllerSetPacePenalty(self, 0x32);
				PartyAddDraftAnimal(game->party, spares, InvItemGetNameCStr(gift_animal), 1);
				morale_c = -1;
				break;
			case 3:
				MsgControllerSetPacePenalty(self, 0x64);
				if (PartyCalcPullingMode(game->party) < node->member)
					morale_c = -2;
				else
					morale_c = -1;
				break;
			}
			break;
		}

		case MSG_WILD_FRUIT:
		case MSG_WILD_VEGETABLES: {
			const char *forage_name_e;
			int32_t forage_lbs_p;
			struct cstr *forage_item_name_l = CStrCreate();

			if (node->number_index == MSG_WILD_VEGETABLES)
				forage_name_e = "fresh vegetables";
			else
				forage_name_e = "fresh fruit";
			switch (node->rsrc_index) {
			case 0:
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = 0;
				else
					morale_c = -2;
				break;
			case 1:
			case 2:
				if (node->rsrc_index == 2) {
					chance_i = 9;
					MsgControllerSetPacePenalty(self, 0x32);

				} else {
					chance_i = 0x19;
					rest_days_a = 0xC;
				}
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = -1;
				else
					morale_c = 1;
				if (MsgControllerPartyHasSkill(self, SKILL_BOTANY)) {
					chance_i = chance_i * 15 / 10;
					chance_hi_g = 4;

				} else {
					chance_hi_g = 2;
				}
				forage_lbs_p = RandRangeAvg(chance_hi_g, chance_i, 1);
				StrPrintf(text_b, "You've added %d %s of %s to your supplies.", forage_lbs_p, PoundsWord(forage_lbs_p),
					forage_name_e);
				CStrAssignBuf(forage_item_name_l, forage_name_e);
				CStrAppendBuf(forage_item_name_l, " [from trail]");
				InvListAdd(&game->party->inventory, forage_item_name_l, forage_lbs_p, 0.0);
				MsgControllerShowMsgboxText(self, text_b, false);
				StrPrintf(text_b, "We gathered %d %s.", forage_lbs_p, PoundsWord(forage_lbs_p));
				line1_a = CStrCreateFrom(text_b);
				break;
			}

			CStrDestroy(&forage_item_name_l);
			break;
		}

		case MSG_ABANDONED_WAGON:
			switch (node->rsrc_index) {
			case 0:
				break;
			case 1: {
				struct inv_list *found_list_c = GenRandomFoundItems(RandRangeAvg(1, 100, 1));
				if (found_list_c->base.count != 0) {
					struct cstr *found_text_j = InvListFormatContentsAll(found_list_c, true);
					ShowItemListBox("You found:", found_list_c);
					PartyDistributeLoot(game->party, found_list_c);
					line1_a = CStrCreateFrom("We found: ");
					CStrAppendStr(line1_a, found_text_j);
					CStrDestroy(&found_text_j);

				} else {
					MsgControllerShowMsgboxText(self, "You didn't find anything useful.", false);
					line1_a = CStrCreateFrom("We did not find anything useful.");
					morale_c = -1;
				}
				InvListDestroy(&found_list_c);
				break;
			}
			}
			break;

		case MSG_BUFFALO_STAMPEDE:
			switch (node->rsrc_index) {
			case 0:
				if (RandRangeAvg(1, 100, 1) <= 5) {
					MsgControllerKillDraftAnimals(self, 1, MSG_INJURED_LIVESTOCK);
					morale_c = -1;
				}
				if (!MsgControllerRollRandomAccident(self, 5))
					morale_c = morale_c - 3;
				if (!MsgControllerRollRandomMisfortune(self, 5))
					morale_c = morale_c - 2;
				break;
			case 1:
				rest_days_a = 0x19;
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = 1;
				else
					morale_c = -1;
				break;
			case 2:
				morale_c = 2;
				MsgControllerStartHunt(self, node);
			}
			break;

		case MSG_QUICKSAND_AHEAD:
			switch (node->rsrc_index) {
			case 0:
				if (MsgControllerPartyHasSkill(self, SKILL_FARMING))
					chance_i = 5;
				else
					chance_i = 0xA;
				if (RandRangeAvg(1, 100, 1) <= chance_i)
					MsgControllerKillDraftAnimals(self, 1, MSG_ANIMAL_IN_QUICKSAND);
				break;
			case 1:
				MsgControllerSetPacePenalty(self, 0x32);
				if (MsgControllerPartyHasSkill(self, SKILL_FARMING))
					chance_i = 2;
				else
					chance_i = 3;
				if (RandRangeAvg(1, 100, 1) <= chance_i)
					MsgControllerKillDraftAnimals(self, 1, MSG_ANIMAL_IN_QUICKSAND);
				break;
			case 2:
				MsgControllerSetPacePenalty(self, 0x4B);
				if (MsgControllerPartyHasSkill(self, SKILL_FARMING))
					chance_i = 2;
				else
					chance_i = 3;
				if (RandRangeAvg(1, 100, 1) <= chance_i)
					MsgControllerKillDraftAnimals(self, 1, MSG_ANIMAL_IN_QUICKSAND);
				break;
			case 3:
				rest_days_a = 0x19;
				break;
			}
			if (GameIsEventAllowed(GetGameObj(), MSG_QUICKSAND_AHEAD))
				outcome_e = 1;
			break;

		case MSG_ANIMAL_IN_QUICKSAND:
			title_g = InvItemGetNamePlural((struct inv_item *) node->subject);
			switch (node->rsrc_index) {
			case 0:
				morale_c = -1;
				break;
			case 1:
				if (MsgControllerGetInventoryQty(self, g_item_rope))
					chance_i = 0x4B;
				else
					chance_i = 0x19;
				if (MsgControllerPartyHasSkill(self, SKILL_FARMING))
					chance_i = chance_i + 0x19;
				if (RandRangeAvg(1, 100, 1) <= chance_i) {
					MsgControllerConsumeSuppliesAmount(self, node);
					MsgControllerShowMsgboxText(self, "You were able to rescue the animal.", false);
					line1_a = CStrCreateFrom("We were successful.");

				} else {
					MsgControllerShowMsgboxText(self, "You were unable to rescue the animal.", false);
					line1_a = CStrCreateFrom("We were unsuccessful.");
					morale_c = -1;
				}
				if (MsgControllerRollRandomAccident(self, 5))
					morale_c = morale_c - 2;
				rest_days_a = 0xC;
				break;
			}
			break;

		case MSG_PRAIRIE_FIRE: {
			int32_t mishap_chance_a = 0;

			switch (node->rsrc_index) {
			case 0:
				if (RandRangeAvg(1, 100, 1) <= 0xA) {
					MsgQueueEnqueue(GetMessageQueue(), MSG_WAGON_FIRE, NULL, 0, 0, NULL, -1);
					morale_c = -3;

				} else {
					morale_c = 1;
				}
				mishap_chance_a = 0x14;
				break;
			case 1:
				MsgControllerSetPacePenalty(self, 0x32);
				if (RandRangeAvg(1, 100, 1) <= 5) {
					MsgQueueEnqueue(GetMessageQueue(), MSG_WAGON_FIRE, NULL, 0, 0, NULL, -1);
					morale_c = -3;

				} else {
					morale_c = 1;
				}
				mishap_chance_a = 0xA;
				break;
			case 2:
				rest_days_a = 0x19;
				if (RandRangeAvg(1, 1000, 1) <= 5)
					MsgQueueEnqueue(GetMessageQueue(), MSG_WAGON_FIRE, NULL, 0, 0, NULL, -1);
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = 0;
				else
					morale_c = -1;
				break;
			}
			if (RandRangeAvg(1, 100, 1) <= mishap_chance_a)
				MemberAddAilment(MsgControllerPickRandomMember(self), AILMENT_BURNS, STAGE_INCUBATION);
			break;
		}

		case MSG_DUSTSTORM:
			switch (node->rsrc_index) {
			case 0:
				MsgControllerSetPacePenalty(self, 0x32);
				if (MsgControllerRollMisfortune2(self, 3))
					morale_c = 1;
				else
					morale_c = -2;
				party_add_health_drain(game->party, 0x19);
				break;
			case 1:
				MsgControllerSetPacePenalty(self, 0x21);
				if (MsgControllerRollMisfortune2(self, 2))
					morale_c = 2;
				else
					morale_c = -2;
				break;
			case 2:
				rest_days_a = 0x19;
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = 1;
				else
					morale_c = -1;
				break;
			}
			break;

		case MSG_LOCUSTS:
			switch (node->rsrc_index) {
			case 0:
				MsgControllerSetPacePenalty(self, 0x32);
				if (MsgControllerRollMisfortune2(self, 3))
					morale_c = 1;
				else
					morale_c = -2;
				break;
			case 1:
				MsgControllerSetPacePenalty(self, 0x21);
				if (MsgControllerRollMisfortune2(self, 2))
					morale_c = 2;
				else
					morale_c = -2;
				break;
			case 2:
				rest_days_a = 0x19;
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = 1;
				else
					morale_c = -1;
				if (RandRangeAvg(1, 100, 1) <= 0x32)
					outcome_e = 1;
				break;
			}
			break;

		case MSG_EXTREME_HEAT:
		case MSG_EXTREME_COLD:
			if (node->number_index == MSG_EXTREME_HEAT) {
				if (WeatherGetHighTemp(game->weather, 0) >= 0x5F)
					outcome_e = 1;
			}
			if (node->number_index == MSG_EXTREME_COLD) {
				if (WeatherGetLowTemp(game->weather, 0) <= 0xA)
					outcome_e = 1;
			}
			switch (node->rsrc_index) {
			case 0:
				party_add_health_drain(game->party, 0x23);
				break;
			case 1:
				MsgControllerSetPacePenalty(self, 0x32);
				party_add_health_drain(game->party, 0x14);
				break;
			case 2:
				rest_days_a = 0x19;
				break;
			}
			break;

		case MSG_INDIANS_AHEAD:
		case MSG_STRANGERS_AHEAD:
			switch (node->rsrc_index) {
			case 0:
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = 1;
				else
					morale_c = -1;
				break;
			case 1:
				if (RandRangeAvg(1, 100, 1) <= 0x19) {
					morale_c = 0;
					MsgControllerShowMsgboxText(self, "The strangers moved away as you approached them.", false);
					line1_a = CStrCreateFrom("They moved on.");

				} else {
					MsgControllerDispatchTalkEvent(self, (intptr_t) node);
					line1_a = CStrCreateFrom("We talked to them.");
				}
				break;
			case 2:
				if (RandRangeAvg(1, 100, 1) <= 0x32) {
					if (MsgControllerPartyHasPlentyOfFood(self))
						morale_c = 0;
					else
						morale_c = 1;
					MsgControllerDispatchTalkEvent(self, (intptr_t) node);
					line1_a = CStrCreateFrom("They approached us and we talked.");

				} else {
					MsgControllerShowMsgboxText(self, "The strangers continued on their way.", false);
					line1_a = CStrCreateFrom("They moved on.");
					if (MsgControllerPartyHasPlentyOfFood(self))
						morale_c = 0;
					else
						morale_c = -1;
				}
				break;
			}
			break;

		case MSG_ACCIDENTAL_GUNSHOT:
		case MSG_ANIMAL_BITE:
		case MSG_ANIMAL_MAULING:
		case MSG_CUTS_AND_ABRASIONS: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -5; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -2; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: morale_c = -3; eff = TREAT_INCREASE_RATIONS; break;
			case 3: morale_c = -1; eff = TREAT_CLEAN_AND_DRESS; break;
			case 4: morale_c = -1; eff = TREAT_ANTISEPTIC; break;
			case 5: morale_c = -2; eff = TREAT_ASTRINGENT; break;
			case 6: morale_c = -5; eff = TREAT_HARMFUL_REMEDY; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			if (node->number_index == MSG_ANIMAL_BITE || node->number_index == MSG_ANIMAL_MAULING) {
				if (RandRangeAvg(1, 1000, 1) <= 5) {
					if (ailm)
						MemberAddAilment(ailm->member, AILMENT_RABIES, STAGE_INCUBATION);
				}
			}
			if ((node->number_index == MSG_CUTS_AND_ABRASIONS || node->number_index == MSG_ANIMAL_MAULING)
				&& eff != 3 && eff != 4) {
				if (RandRangeAvg(1, 100, 1) <= 5) {
					if (ailm)
						MemberAddAilment(ailm->member, AILMENT_LOCKJAW, STAGE_INCUBATION);
				}
			}
			break;
		}

		case MSG_WAGON_FIRE: {
			int32_t wagon_chance_c = 0;
			struct inv_list *lost_list_c = NULL;

			switch (node->rsrc_index) {
			case 0:
				if (RandRangeAvg(1, 100, 1) <= 0x62) {
					lost_list_c = InvListLoseRandom(&game->party->inventory, RandRangeAvg(0x19, 0x4B, 1), false);
					ShowItemListBox("You were able to put out the fire, but lost: ", lost_list_c);
					title_g = InvListFormatContentsAll(lost_list_c, true);
					StripTrailingPeriod(title_g);
					morale_c = -1;

				} else {
					line1_a = CStrCreateFrom("We were unable to put out the fire.");
					if (PartyCountDraftAnimals(game->party) > 1) {
						MsgControllerShowMsgboxText(self, "You were unable to put out the fire.  A wagon and all your supplies are lost.", false);
						title_g = CStrCreateFrom("a wagon and everything in it");
						PartyRemoveOneAnimal(game->party);
						lost_list_c = InvListLoseRandom(&game->party->inventory, 0x64, false);
						line2_a = CStrCreateFrom("We are very fortunate to have another wagon.");

					} else {
						MsgControllerShowMsgboxText(self, "You were unable to put out the fire.  All your supplies are lost.", false);
						title_g = CStrCreateFrom("everything in the wagon");
						lost_list_c = InvListLoseRandom(&game->party->inventory, 0xC8, false);
					}
					morale_c = -2;
				}
				wagon_chance_c = 0x14;
				InvListDestroy(&lost_list_c);
				break;
			case 1:
				if (RandRangeAvg(1, 100, 1) <= 2) {
					struct inv_list *lost_list2_i = InvListLoseRandom(&game->party->inventory,
						RandRangeAvg(0x32, 0x64, 1), false);
					ShowItemListBox("The fire destroyed: ", lost_list2_i);
					title_g = InvListFormatContentsAll(lost_list2_i, true);
					StripTrailingPeriod(title_g);
					InvListDestroy(&lost_list2_i);
					morale_c = -3;

				} else {
					if (PartyCountDraftAnimals(game->party) > 1) {
						MsgControllerShowMsgboxText(self, "The fire destroyed a wagon and everything in it.", false);
						title_g = CStrCreateFrom("a wagon and everything in it");
						PartyRemoveOneAnimal(game->party);
						lost_list_c = InvListLoseRandom(&game->party->inventory, 0x64, false);
						line1_a = CStrCreateFrom("We are very fortunate to have another wagon.");

					} else {
						MsgControllerShowMsgboxText(self, "The fire destroyed everything in the wagon.", false);
						title_g = CStrCreateFrom("everything in the wagon");
						lost_list_c = InvListLoseRandom(&game->party->inventory, 0xC8, false);
					}
					morale_c = -4;
				}
				InvListDestroy(&lost_list_c);
				break;
			}
			if (RandRangeAvg(1, 100, 1) <= wagon_chance_c)
				MemberAddAilment(MsgControllerPickRandomMember(self), AILMENT_BURNS, STAGE_INCUBATION);
			break;
		}

		case MSG_THEFT: {
			struct inv_list *loot_list_g = (struct inv_list *) node->member;

			title_g = InvListFormatContentsAll(loot_list_g, false);
			switch (node->rsrc_index) {
			case 0:
				morale_c = -1;
				break;
			case 1:
				if (RandRangeAvg(1, 100, 1) <= 0xA) {
					PartyDistributeLoot(game->party, loot_list_g);
					MsgControllerShowMsgboxText(self, "You didn't find the thief but recovered your supplies.", false);
					line1_a = CStrCreateFrom("We didn't find the thief but recovered our supplies.");
					morale_c = 3;

				} else {
					MsgControllerShowMsgboxText(self, "You were unable to find the thief and your supplies.", false);
					line1_a = CStrCreateFrom("We did not find the thief or our supplies.");
					morale_c = -2;
				}
				break;
			}
			InvListDestroy(&loot_list_g);
			break;
		}

		case MSG_CROWDED_RIVER:
			break;

		case MSG_FERRY_DELAY:
			switch (node->rsrc_index) {
			case 0: {
				struct lnode *cur_node_g = (struct lnode *) game->cur_node;
				if (NodeBaseGetId(&cur_node_g->base) != NODE_KIND_TRAIL) {
					GameRest(GetGameObj(), cur_node_g->ferry_wait_days, 1);
					cur_node_g->ferry_wait_days = 0;
				}
				dispatch_command(GCMD_FERRY_WAIT_DONE);
				break;
			}
			}
			break;

		case MSG_TIPPED_WAGON:
		case MSG_SWAMPED_WAGON:
		case MSG_FELL_THROUGH_ICE:
			if (node->member) {
				title_g = report_supply_loss_rest((struct inv_list *) node->member);
				node->member = 0;

			} else {
				title_g = CStrCreateCopy(node->payload);
				int32_t colon_pos_j = CStrFindChar(title_g, ':');
				CStrSubstr(title_g, colon_pos_j + 2, 0x3E7);
			}
			StripTrailingPeriod(title_g);
			switch (node->rsrc_index) {
			case 0:
				morale_c = -1;
				if (MsgControllerGetInventoryQty(self, g_item_rope) || MsgControllerGetInventoryQty(self, g_item_chain)) {
					rest_days_a = 0xC;

				} else {
					rest_days_a = 0x18;
					morale_c--;
				}
				if (GetGameObj()->travel_pace == PACE_10_HOURS)
					morale_c--;
				if (GetGameObj()->travel_pace == PACE_EXHAUSTING)
					morale_c = morale_c - 2;
				if (MsgControllerRollRandomAccident(self, 5))
					morale_c--;
				break;
			}
			if (RandRangeAvg(1, 100, 1) <= 5)
				MemberAddAilment(MsgControllerPickRandomMember(self), AILMENT_INTERNAL_INJURIES, STAGE_INCUBATION);
			break;

		case MSG_RAFTING_LOSSES:
			if (node->member) {
				title_g = report_supply_loss_rest((struct inv_list *) node->member);
				node->member = 0;

			} else {
				title_g = CStrCreateCopy(node->payload);
				int32_t colon_pos2_k = CStrFindChar(title_g, ':');
				CStrSubstr(title_g, colon_pos2_k + 2, 0x3E7);
			}
			StripTrailingPeriod(title_g);
			break;

		case 1001:
			if (RandRangeAvg(1, 100, 1) <= 5)
				MemberAddAilment(MsgControllerPickRandomMember(self), AILMENT_INTERNAL_INJURIES, STAGE_INCUBATION);
			break;

		case MSG_ANIMAL_DIED_ACCIDENT: {
			int32_t hunt_meat_lbs_a;

			title_g = InvItemGetNamePlural((struct inv_item *) node->subject);
			morale_c = -1;
			switch (node->rsrc_index) {
			case 0:
				break;
			case 1:
				if (MsgControllerPartyHasSkill(self, SKILL_COOKING)) {
					chance_i = 0xC8;
					chance_hi_g = 0x12C;

				} else {
					chance_i = 0x64;
					chance_hi_g = 0xFA;
				}
				hunt_meat_lbs_a = RandRangeAvg(chance_i, chance_hi_g, 1);
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = -3;
				else
					morale_c = 0;
				InventoryAddMeat(hunt_meat_lbs_a);
				StrPrintf(text_b, "%d pounds of meat will be added to your supplies.", hunt_meat_lbs_a);
				MsgControllerShowMsgboxText(self, text_b, false);
				StrPrintf(text_b, "We salvaged %d pounds of meat.", hunt_meat_lbs_a);
				line1_a = CStrCreateFrom(text_b);
				break;
			}
			break;
		}

		case MSG_ANIMAL_DIED_ILLNESS:
			morale_c = -1;
			break;

		case MSG_STUCK_IN_MUD:
		case MSG_STUCK_IN_SAND:
			switch (node->rsrc_index) {
			case 0: {
				int32_t pull_mode_i = game->party->pull_mode;
				if (pull_mode_i != PULL_SINGLE_EASY && pull_mode_i != PULL_TEAM_EASY) {
					outcome_e = 0;
					MsgClearFlag(node, 0);
					MsgControllerShowMsgboxText(self, "The wagon is still stuck.", false);
				}
				rest_days_a = 0x18;
				break;
			}
			case 1:
				outcome_e = 0;
				dispatch_command(GCMD_OPEN_DROP_DLG);
				MsgSetFlag(node, 0);
				break;
			}
			break;

		case MSG_HILL_TOO_STEEP:
			switch (node->rsrc_index) {
			case 0:
				break;
			case 1:
				outcome_e = 0;
				dispatch_command(GCMD_OPEN_DROP_DLG);
				break;
			}
			break;

		case MSG_ANIMALS_EXHAUSTED:
			switch (node->rsrc_index) {
			case 0:
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = 0;
				else
					morale_c = -1;
				rest_days_a = 0;
				break;
			case 1:
				rest_days_a = 0xC;
				MsgControllerSetPacePenalty(self, 0x21);
				if (MsgControllerPartyHasSkill(self, SKILL_FARMING))
					chance_i = 0xF;
				else
					chance_i = 0x1E;
				if (RandRangeAvg(1, 100, 1) < chance_i)
					MsgControllerKillDraftAnimals(self, 1, MSG_INJURED_LIVESTOCK);
				break;
			case 2:
				outcome_e = 0;
				dispatch_command(GCMD_OPEN_DROP_DLG);
				break;
			}
			break;

		case MSG_THICK_DUST:
			switch (node->rsrc_index) {
			case 0:
				if (WeatherSumPrecip(game->weather, 0) == 0) {
					outcome_e = 1;
					MsgControllerSetPacePenalty(self, 0x4B);
					morale_c = -1;
					party_add_health_drain(game->party, 0x19);
				}
				break;
			case 1:
				MsgControllerSetPacePenalty(self, 0x32);
				morale_c = -1;
				break;
			case 2:
				rest_days_a = 0x19;
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = 0;
				else
					morale_c = -1;
				break;
			}
			break;

		case MSG_INDEPENDENCE_DAY:
			switch (node->rsrc_index) {
			case 0:
				OtiiAppPlaySfx((struct otii_app *) g_app, 0x6928, false);
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = 0xA;
				else
					morale_c = 4;
				MsgClearFlag(node, 0);
				outcome_e = 0;
				break;
			case 1:
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = -5;
				else
					morale_c = -2;
				break;
			}
			break;

		case MSG_ALKALI_SICKNESS: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -3; break;
			case 1: morale_c = -2; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: morale_c = -2; eff = TREAT_INCREASE_RATIONS; break;
			case 3: morale_c = -1; eff = TREAT_VINEGAR; break;
			case 4: eff = TREAT_FLOUR_OR_GREASE; morale_c = -1; break;
			case 5: eff = TREAT_FLOUR_OR_GREASE; morale_c = -1; break;
			case 6: eff = TREAT_HARMFUL_REMEDY; morale_c = -5; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_ANIMAL_ALKALI_SICKNESS:
			break;

		case MSG_BROKEN_BONE: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -3; break;
			case 1: morale_c = -2; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -2; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -2; eff = TREAT_INCREASE_RATIONS; break;
			case 4: morale_c = -1; eff = TREAT_CLEAN_AND_DRESS; break;
			case 5: morale_c = -3; eff = TREAT_CAMPHOR; break;
			case 6: eff = TREAT_HARMFUL_REMEDY; morale_c = -5; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_SPRAIN: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -1; break;
			case 1:
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = 0;
				else
					morale_c = -1;
				eff = TREAT_REST;
				rest_days_a = 0;
				break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = 1; eff = TREAT_CLEAN_AND_DRESS; break;
			case 4: morale_c = 1; eff = TREAT_CLEAN_AND_DRESS; break;
			case 5: eff = TREAT_ANTISEPTIC; break;
			case 6: eff = TREAT_HARMFUL_REMEDY; morale_c = -1; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_CHOLERA: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -2; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: morale_c = -2; eff = TREAT_INCREASE_RATIONS; break;
			case 3: morale_c = -1; eff = TREAT_FLUIDS; break;
			case 4: morale_c = -2; eff = TREAT_PEPPERMINT; break;
			case 5: morale_c = -1; eff = TREAT_LAUDANUM; break;
			case 6: morale_c = -5; eff = TREAT_HARMFUL_REMEDY; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_BAD_COLD: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0:
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = 0;
				else
					morale_c = -1;
				eff = TREAT_CONTINUE;
				break;
			case 1:
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = 0;
				else
					morale_c = -1;
				eff = TREAT_REST;
				rest_days_a = 0;
				break;
			case 2:
				MsgControllerSetPacePenalty(self, 0x32);
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = 0;
				else
					morale_c = -1;
				eff = TREAT_SLOW_DOWN;
				break;
			case 3:
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = 0;
				else
					morale_c = -1;
				eff = TREAT_INCREASE_RATIONS;
				break;
			case 4: morale_c = 0; eff = TREAT_FLUIDS; break;
			case 5: morale_c = 0; eff = TREAT_VINEGAR; break;
			case 6: morale_c = -1; eff = TREAT_OLIVE_OIL; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_CONCUSSION: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -3; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -1; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -2; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -2; eff = TREAT_INCREASE_RATIONS; break;
			case 4: morale_c = -2; eff = TREAT_LAUDANUM; break;
			case 5: morale_c = -1; eff = TREAT_VINEGAR; break;
			case 6: morale_c = -4; eff = TREAT_EXERCISE; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_DYSENTERY: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -1; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: morale_c = -1; eff = TREAT_INCREASE_RATIONS; break;
			case 3: morale_c = -1; eff = TREAT_FLUIDS; break;
			case 4: morale_c = -2; eff = TREAT_LAUDANUM; break;
			case 5: morale_c = -1; eff = TREAT_VINEGAR; break;
			case 6: morale_c = -4; eff = TREAT_MAGNESIA; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_THE_GRIPPE: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -1; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -3; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -2; eff = TREAT_FLUIDS; break;
			case 4: morale_c = -2; eff = TREAT_VINEGAR; break;
			case 5: morale_c = -1; eff = TREAT_LECITHIN; break;
			case 6: morale_c = -4; eff = TREAT_QUININE; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_MALARIA: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -2; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -2; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -1; eff = TREAT_INCREASE_RATIONS; break;
			case 4: morale_c = 0; eff = TREAT_QUININE; break;
			case 5: morale_c = -1; eff = TREAT_SASSAFRAS; break;
			case 6: morale_c = -2; eff = TREAT_MAGNESIA; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_MEASLES: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -1; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -2; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -2; eff = TREAT_INCREASE_RATIONS; break;
			case 4: morale_c = -1; eff = TREAT_FLUIDS; break;
			case 5: morale_c = -1; eff = TREAT_ALOE; break;
			case 6: morale_c = -4; eff = TREAT_HARMFUL_REMEDY; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_MOUNTAIN_FEVER: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -1; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -2; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -2; eff = TREAT_INCREASE_RATIONS; break;
			case 4: morale_c = -1; eff = TREAT_CONTROL_FEVER; break;
			case 5: morale_c = -2; eff = TREAT_ALOE; break;
			case 6: morale_c = -4; eff = TREAT_MAGNESIA; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_PNEUMONIA: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -1; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -2; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -2; eff = TREAT_INCREASE_RATIONS; break;
			case 4: morale_c = -2; eff = TREAT_LAUDANUM; break;
			case 5: morale_c = -1; eff = TREAT_CONTROL_FEVER; break;
			case 6: morale_c = -4; eff = TREAT_HARMFUL_REMEDY; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_BERIBERI: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -4; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -4; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -3; eff = TREAT_INCREASE_RATIONS; break;
			case 4: morale_c = -4; eff = TREAT_DUFFYS_ELIXIR; break;
			case 5: morale_c = -4; eff = TREAT_DOVERS_POWDER; break;
			case 6: morale_c = -2; MsgControllerStartHunt(self, node); break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_DIPHTHERIA: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -1; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -2; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -2; eff = TREAT_INCREASE_RATIONS; break;
			case 4: morale_c = -1; eff = TREAT_DUFFYS_ELIXIR; break;
			case 5: morale_c = -2; eff = TREAT_VINEGAR; break;
			case 6: morale_c = -4; eff = TREAT_DOVERS_POWDER; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_FROSTBITE: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -2; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -3; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -3; eff = TREAT_INCREASE_RATIONS; break;
			case 4: morale_c = -1; eff = TREAT_WARM_GRADUALLY; break;
			case 5: morale_c = -3; eff = TREAT_WARM_QUICKLY; break;
			case 6: morale_c = -5; eff = TREAT_HARMFUL_REMEDY; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_GANGRENE: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -6; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -4; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -6; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -4; eff = TREAT_INCREASE_RATIONS; break;
			case 4: morale_c = -3; eff = TREAT_AMPUTATE; break;
			case 5: morale_c = -6; eff = TREAT_SULFUR; break;
			case 6: morale_c = -6; eff = TREAT_DUFFYS_ELIXIR; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_SCURVY: {
			int32_t fruit_lbs_e;
			int32_t forage_chance_k;
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -4; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -4; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -3; eff = TREAT_INCREASE_RATIONS; break;
			case 4:
				forage_chance_k = MsgControllerWeatherHazardChance(self, terrain_d, game->date->month, game->date->day)
					* 3;
				if (RandRangeAvg(1, 100, 1) <= forage_chance_k) {
					struct cstr *veg_name1_i = CStrCreateFrom("fresh vegetables [from trail]");
					fruit_lbs_e = RandRangeAvg(2, 0x19, 1);
					StrPrintf(text_b, "You found %d %s of fresh fruit and vegetables.", fruit_lbs_e,
						PoundsWord(fruit_lbs_e));
					InvListAdd(&game->party->inventory, veg_name1_i, fruit_lbs_e, 0.0);
					MsgControllerShowMsgboxText(self, text_b, false);
					morale_c = -1;
					CStrDestroy(&veg_name1_i);

				} else {
					morale_c = -5;
					MsgControllerShowMsgboxText(self, "You were unable to find any edible plants.", false);
				}
				eff = TREAT_CONTINUE;
				rest_days_a = 0x18;
				break;
			case 5: morale_c = -1; eff = TREAT_DOVERS_POWDER; break;
			case 6: morale_c = -4; eff = TREAT_MAGNESIA; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_SMALLPOX: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -1; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -2; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -2; eff = TREAT_INCREASE_RATIONS; break;
			case 4: morale_c = -2; eff = TREAT_ALOE; break;
			case 5: morale_c = -2; eff = TREAT_ANTISEPTIC; break;
			case 6: morale_c = -4; eff = TREAT_HARMFUL_REMEDY; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_SNAKEBITE: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -5; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -3; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -5; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -5; eff = TREAT_INCREASE_RATIONS; break;
			case 4: morale_c = -1; eff = TREAT_TOURNIQUET; break;
			case 5: morale_c = -3; eff = TREAT_ANTISEPTIC; break;
			case 6: morale_c = -5; eff = TREAT_EXERCISE; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_TYPHOID_FEVER: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -1; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -2; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -2; eff = TREAT_INCREASE_RATIONS; break;
			case 4: morale_c = -1; eff = TREAT_CONTROL_FEVER; break;
			case 5: morale_c = -2; eff = TREAT_LAUDANUM; break;
			case 6: morale_c = -3; eff = TREAT_ALOE; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_RABIES: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -3; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -4; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -3; eff = TREAT_INCREASE_RATIONS; break;
			case 4: morale_c = -2; eff = TREAT_REST; rest_days_a = 24; break;
			case 5: morale_c = -2; eff = TREAT_LAUDANUM; break;
			case 6: morale_c = -4; eff = TREAT_MAGNESIA; break;
			case 7: morale_c = -4; eff = TREAT_HARMFUL_REMEDY; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_TYPHUS: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -1; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -2; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -2; eff = TREAT_INCREASE_RATIONS; break;
			case 4: morale_c = -2; eff = TREAT_LAUDANUM; break;
			case 5: morale_c = -3; eff = TREAT_ALOE; break;
			case 6: morale_c = -5; eff = TREAT_HARMFUL_REMEDY; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_SCARLET_FEVER: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -1; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -2; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -2; eff = TREAT_INCREASE_RATIONS; break;
			case 4: morale_c = -1; eff = TREAT_FLUIDS; break;
			case 5: morale_c = -1; eff = TREAT_LAUDANUM; break;
			case 6: morale_c = -5; eff = TREAT_HARMFUL_REMEDY; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_FOOD_POISONING: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -2; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -1; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: morale_c = -2; eff = TREAT_INCREASE_RATIONS; break;
			case 3: morale_c = -1; eff = TREAT_FLUIDS; break;
			case 4: morale_c = -1; eff = TREAT_LAUDANUM; break;
			case 5: morale_c = -1; eff = TREAT_PEPPERMINT; break;
			case 6: morale_c = -3; eff = TREAT_HARMFUL_REMEDY; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_INFECTION: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -2; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: morale_c = -2; eff = TREAT_INCREASE_RATIONS; break;
			case 3: morale_c = -1; eff = TREAT_CLEAN_AND_DRESS; break;
			case 4: morale_c = -1; eff = TREAT_SULFUR; break;
			case 5: morale_c = -1; eff = TREAT_ANTISEPTIC; break;
			case 6: morale_c = -5; eff = TREAT_HARMFUL_REMEDY; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_CONSUMPTION: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -1; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); morale_c = -2; eff = TREAT_SLOW_DOWN; break;
			case 3: morale_c = -2; eff = TREAT_INCREASE_RATIONS; break;
			case 4: morale_c = -2; eff = TREAT_LAUDANUM; break;
			case 5: morale_c = -1; eff = TREAT_WARM_GRADUALLY; break;
			case 6: morale_c = -5; eff = TREAT_HARMFUL_REMEDY; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_BURNS: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -3; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -2; eff = TREAT_REST; rest_days_a = 0; break;
			case 2: morale_c = -2; eff = TREAT_INCREASE_RATIONS; break;
			case 3: morale_c = -1; eff = TREAT_CLEAN_AND_DRESS; break;
			case 4: morale_c = -2; eff = TREAT_ALOE; break;
			case 5: morale_c = -1; eff = TREAT_ANTISEPTIC; break;
			case 6: morale_c = -3; eff = TREAT_HARMFUL_REMEDY; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_DROWNING: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: morale_c = -4; eff = TREAT_CONTINUE; break;
			case 1: morale_c = -2; eff = TREAT_REST; rest_days_a = 0; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_REELECTED:
			switch (node->rsrc_index) {
			case 0:
				set_player_level(1);
				party_add_reputation(game->party, 0x28);
				break;
			case 1:
				set_player_level(0);
				party_add_reputation(game->party, 0x14);
				break;
			}
			break;

		case MSG_ELECTED_CAPTAIN:
			switch (node->rsrc_index) {
			case 0:
				set_player_level(1);
				break;
			case 1:
				if (MsgControllerShowMsgboxText(self, "Refusal of the position of captain will result in your being demoted to the level of \"Greenhorn\".  Do you want to continue as a Greenhorn?", true)) {
					outcome_e = 0;

				} else {
					set_player_level(0);
				}
				break;
			}
			break;

		case MSG_LOW_MORALE:
		case MSG_VERY_LOW_MORALE:
			switch (node->rsrc_index) {
			case 0:
				break;
			case 1:
				rest_days_a = 0;
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = 0;
				else
					morale_c = -2;
				node->rsrc_index = 0;
				break;
			}
			break;

		case MSG_DISMISSED:
			switch (node->rsrc_index) {
			case 0:
				set_player_level(0);
				morale_c = 3;
				break;
			}
			break;

		case MSG_HIRED_AS_GUIDE:
			switch (node->rsrc_index) {
			case 0:
				game->party->cash = game->party->cash + 500.0;
				break;
			}
			break;

		case MSG_FIRED:
		case MSG_LEADER_DIED:
			break;

		case MSG_TRAIN_SPLITTING:
		case MSG_WAGONS_JOIN:
		case MSG_MEMBER_DIED:
			morale_c = -10;
			switch (node->rsrc_index) {
			case 0:
				morale_c = morale_c + 3;
				break;
			case 1:
				if (MsgControllerShowMsgboxText(self, "Are you sure you wish to continue without providing a proper burial?  There may be serious consequences.", true)) {
					outcome_e = 0;

				} else {
					morale_c = -(game->party->reputation / 3);
				}
				break;
			}
			title_g = CStrCreateCopy(node->payload);
			break;

		case MSG_MOSQUITOES:
			switch (node->rsrc_index) {
			case 0:
				break;
			case 1:
				rest_days_a = 0;
				break;
			}
			morale_c = -1;
			break;

		case 1005:
			if (RandRangeAvg(1, 100, 1) <= 0xA)
				MemberAddAilment(MsgControllerPickRandomMember(self), AILMENT_INTERNAL_INJURIES, STAGE_INCUBATION);
			break;

		case MSG_ANIMAL_IN_HOLE: {
			int32_t salvage_lbs_b;
			struct inv_item *found_animal;
			struct inv_list *spares;

			title_g = InvItemGetNamePlural((struct inv_item *) node->subject);
			switch (node->rsrc_index) {
			case 0:
				if (PartyCalcPullingMode(game->party) < node->member)
					morale_c = -1;
				break;
			case 1:
				found_animal = (struct inv_item *) node->subject;
				spares = &game->party->spare_animals;
				rest_days_a = 0;
				PartyAddDraftAnimal(game->party, spares, InvItemGetNameCStr(found_animal), 1);
				morale_c = -1;
				break;
			case 2:
				found_animal = (struct inv_item *) node->subject;
				spares = &game->party->spare_animals;
				MsgControllerSetPacePenalty(self, 0x32);
				PartyAddDraftAnimal(game->party, spares, InvItemGetNameCStr(found_animal), 1);
				morale_c = -1;
				break;
			case 3:
				MsgControllerSetPacePenalty(self, 0x64);
				if (PartyCalcPullingMode(game->party) < node->member)
					morale_c = -4;
				else
					morale_c = -3;
				break;
			case 4:
				if (MsgControllerPartyHasSkill(self, SKILL_COOKING)) {
					chance_i = 0xC8;
					chance_hi_g = 0x12C;

				} else {
					chance_i = 0x64;
					chance_hi_g = 0xFA;
				}
				salvage_lbs_b = RandRangeAvg(chance_i, chance_hi_g, 1);
				if (MsgControllerPartyHasPlentyOfFood(self))
					morale_c = -3;
				else
					morale_c = 0;
				InventoryAddMeat(salvage_lbs_b);
				StrPrintf(text_b, "%d pounds of meat will be added to your supplies.", salvage_lbs_b);
				MsgControllerShowMsgboxText(self, text_b, false);
				StrPrintf(text_b, "We salvaged %d pounds of meat.", salvage_lbs_b);
				line1_a = CStrCreateFrom(text_b);
				break;
			}
			break;
		}

		case MSG_FREEZING: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: eff = TREAT_CONTINUE; morale_c = -2; break;
			case 1: rest_days_a = 0; eff = TREAT_REST; morale_c = -2; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); eff = TREAT_SLOW_DOWN; morale_c = -2; break;
			case 3:
			case 4:
				eff = TREAT_WARM_GRADUALLY;
				break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_STARVATION: {
			int32_t veg_lbs_b;
			int32_t veg_chance_m;
			struct inv_item *lost_animal_d;
			int32_t meat_lbs_f;
			struct inv_list *animal_list_a;
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0:
				morale_c = -5;
				break;
			case 1:
				rest_days_a = 0;
				morale_c = -5;
				break;
			case 2:
				MsgControllerStartHunt(self, node);
				morale_c = -3;
				break;
			case 3:
				morale_c = -2;
				animal_list_a = &game->party->spare_animals;
				if (animal_list_a->base.count == 0)
					animal_list_a = &game->party->animals;
				if (animal_list_a->base.count == 0) {
					MsgControllerShowMsgboxText(self, "You do not have any draft animals.", false);

				} else {
					lost_animal_d = InvItemFromNode(ListNodeAt(&animal_list_a->base,
						RandRangeAvg(0, animal_list_a->base.count - 1, 1)));
					InvItemRemoveUnits(lost_animal_d, 1);
					if (MsgControllerPartyHasSkill(self, SKILL_COOKING)) {
						chance_i = 0xC8;
						chance_hi_g = 0x12C;

					} else {
						chance_i = 0x64;
						chance_hi_g = 0xFA;
					}
					meat_lbs_f = RandRangeAvg(chance_i, chance_hi_g, 1);
					InventoryAddMeat(meat_lbs_f);
					StrPrintf(text_b, "%d pounds of meat will be added to your supplies.", meat_lbs_f);
					MsgControllerShowMsgboxText(self, text_b, false);
				}
				break;
			case 4:
				veg_chance_m = MsgControllerWeatherHazardChance(self, terrain_d, game->date->month, game->date->day) * 3;
				if (RandRangeAvg(1, 100, 1) <= veg_chance_m) {
					struct cstr *veg_name2_i = CStrCreateFrom("fresh vegetables [from trail]");
					if (MsgControllerPartyHasSkill(self, SKILL_BOTANY)) {
						veg_chance_m = 0xA;
						chance_hi_g = 0x23;

					} else {
						veg_chance_m = 1;
						chance_hi_g = 0x19;
					}
					veg_lbs_b = RandRangeAvg(veg_chance_m, chance_hi_g, 1);
					InvListAdd(&game->party->inventory, veg_name2_i, veg_lbs_b, 0.0);
					StrPrintf(text_b, "%d pounds of fresh vegetables will be added to your supplies.", veg_lbs_b);
					MsgControllerShowMsgboxText(self, text_b, false);
					morale_c = -1;
					CStrDestroy(&veg_name2_i);

				} else {
					morale_c = -5;
				}
				break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_LOCKJAW: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: eff = TREAT_CONTINUE; morale_c = -4; break;
			case 1: rest_days_a = 0; eff = TREAT_REST; morale_c = -2; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); eff = TREAT_SLOW_DOWN; morale_c = -3; break;
			case 3: eff = TREAT_INCREASE_RATIONS; morale_c = -2; break;
			case 4: eff = TREAT_REST; morale_c = -2; break;
			case 5: eff = TREAT_LAUDANUM; morale_c = -2; break;
			case 6: eff = TREAT_REST; morale_c = -2; break;
			case 7: eff = TREAT_HARMFUL_REMEDY; morale_c = -5; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_INTERNAL_INJURIES: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: eff = TREAT_CONTINUE; morale_c = -4; break;
			case 1: rest_days_a = 0; eff = TREAT_REST; morale_c = -2; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); eff = TREAT_SLOW_DOWN; morale_c = -3; break;
			case 3: eff = TREAT_INCREASE_RATIONS; morale_c = -3; break;
			case 4: eff = TREAT_REST; morale_c = -2; break;
			case 5: eff = TREAT_LAUDANUM; morale_c = -2; break;
			case 6: eff = TREAT_MAGNESIA; morale_c = -5; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_WOUND: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: eff = TREAT_CONTINUE; morale_c = -4; break;
			case 1: rest_days_a = 0; eff = TREAT_REST; morale_c = -3; break;
			case 2: eff = TREAT_LAUDANUM; morale_c = -2; break;
			case 3: eff = TREAT_CLEAN_AND_DRESS; morale_c = -1; break;
			case 4: eff = TREAT_SULFUR; morale_c = -1; break;
			case 5: eff = TREAT_ANTISEPTIC; morale_c = -1; break;
			case 6: eff = TREAT_HARMFUL_REMEDY; morale_c = -5; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_WATER_POISONING: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: eff = TREAT_CONTINUE; morale_c = -3; break;
			case 1: rest_days_a = 0; morale_c = -2; eff = TREAT_REST; break;
			case 2: MsgControllerSetPacePenalty(self, 0x32); eff = TREAT_SLOW_DOWN; morale_c = -2; break;
			case 3: eff = TREAT_INCREASE_RATIONS; morale_c = -2; break;
			case 4: eff = TREAT_PEPPERMINT; morale_c = -1; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_THIRST: {
			int32_t eff = TREAT_CONTINUE;
			struct ailment *ailm = (struct ailment *) node->subject;

			switch (node->rsrc_index) {
			case 0: eff = TREAT_CONTINUE; morale_c = -2; break;
			case 1: rest_days_a = 0; eff = TREAT_REST; morale_c = -4; break;
			case 2: MsgControllerSetPacePenalty(self, 0x96); eff = TREAT_GO_FASTER; morale_c = -1; break;
			case 3: MsgControllerSetPacePenalty(self, 0x32); eff = TREAT_SLOW_DOWN; morale_c = -3; break;
			case 4: eff = TREAT_INCREASE_RATIONS; morale_c = -1; break;
			}
			apply_treatment(ailm, eff, soap_qty_i);
			break;
		}

		case MSG_CAUGHT_FISH:
			switch (node->rsrc_index) {
			case 0: {
				struct inv_item *fish_item_e;
				int32_t fish_qty_p;
				int32_t fresh_lbs_l;
				int32_t salt_units_l;
				int32_t salted_lbs_i;
				struct inv_list *inv_list_j;
				int32_t pounds_b = (int16_t) node->member;

				morale_c = morale_c + 3;
				struct cstr *fresh_fish_name_g = CStrCreateFrom("fish [from trail]");
				struct cstr *salted_fish_name_p = CStrCreateFrom("salted fish [from trail]");
				inv_list_j = &game->party->inventory;
				struct cstr *salt_name_d = CStrCreateFrom("salt");
				salt_units_l = (pounds_b + 7) / 8;
				fish_item_e = InvListFindByName(inv_list_j, salt_name_d);
				if (fish_item_e) {
					fish_qty_p = (int16_t) InvItemTotalPortions(fish_item_e);
					if (fish_qty_p >= salt_units_l) {
						salted_lbs_i = pounds_b;
						fresh_lbs_l = 0;

					} else {
						salted_lbs_i = fish_qty_p * 8;
						salt_units_l = fish_qty_p;
						fresh_lbs_l = pounds_b - salted_lbs_i;
					}
					if (salted_lbs_i != 0) {
						InvListAdd(&game->party->inventory, salted_fish_name_p, salted_lbs_i, 0.0);
						StrPrintf(text_b, "%d pounds of salted fish has been be added to your supplies.", pounds_b);
						MsgControllerShowMsgboxText(self, text_b, false);
						InvItemConsumePortions(fish_item_e, salt_units_l);
					}
					if (fresh_lbs_l != 0)
						InvListAdd(&game->party->inventory, fresh_fish_name_g, fresh_lbs_l, 0.0);

				} else {
					StrPrintf(text_b, "%d pounds of fresh fish has been added to your supplies.", pounds_b);
					MsgControllerShowMsgboxText(self, text_b, false);
					InvListAdd(&game->party->inventory, fresh_fish_name_g, pounds_b, 0.0);
				}

				CStrDestroy(&salt_name_d);
				CStrDestroy(&salted_fish_name_p);
				CStrDestroy(&fresh_fish_name_g);
				break;
			}
			case 1:
				morale_c = morale_c + 2;
				break;
			}
			break;

		case MSG_CANNOT_PAY_TOLL:
		case MSG_SUNDAY:
		case MSG_LOST_SUPPLIES:
			break;
		}

		if (!MsgControllerPartyHasPlentyOfFood(self) && outcome_e == 1)
			outcome_e = 2;
		node->choice_state = outcome_e;
		if (rest_days_a != -1 && !MsgControllerDoRestDays(self, rest_days_a)) {
			node->choice_state = 0;
			return;
		}
		if (outcome_e)
			MsgPresentText(node, title_g, line1_a, line2_a);
		MsgControllerAdjustMorale(self, morale_c);
		if (rest_days_a != -1 && outcome_e == 1) {
			if (retries_left_o) {
				--retries_left_o;
				retry_b = true;
				morale_c = 0;
				outcome_e = 2;
				chance_i = 0;
				chance_hi_g = 0;
				title_g = NULL;
				line1_a = NULL;
				line2_a = NULL;
				rest_days_a = -1;

			} else {
				MsgControllerShowMsgboxText(self, "Conditions have not improved.", false);
				node->choice_state = 0;
				retry_b = false;
			}

		} else {
			retry_b = false;
		}
	} while (retry_b);
}

void MsgControllerAdjustMorale(struct msg_controller *self, int32_t delta)
{
	struct game *game_obj_a = GetGameObj();
	struct party *party_a = game_obj_a->party;

	party_a->reputation += delta;
	if (party_a->reputation > 100)
		party_a->reputation = 100;
	if (party_a->reputation < -50)
		party_a->reputation = -50;
}

int32_t MsgControllerPickRandomBrokenBone(struct msg_controller *self)
{
	int32_t roll = RandRangeAvg(1, 100, 1);

	if (roll <= 35)
		return AILMENT_BROKEN_ARM;
	if (roll <= 50)
		return AILMENT_BROKEN_LEG;
	if (roll <= 65)
		return AILMENT_BROKEN_FOOT;
	if (roll <= 70)
		return AILMENT_BROKEN_HIP;

	return AILMENT_BROKEN_WRIST;
}

void MsgControllerDispatchTalkEvent(struct msg_controller *self, intptr_t target)
{
	struct cmd_event *cmd = CmdEventCreate(GCMD_TALK);
	cmd->param1 = -1;
	cmd->param2 = -1;
	DispatchGlobalEvent(cmd);
}

int32_t MsgControllerShowMsgboxText(struct msg_controller *self, const char *text, bool two_buttons)
{
	int32_t cancelled_ab = 0;
	struct cstr *s_ah = CStrCreateFrom(text);
	struct msg_box dlg_ah;

	MsgBoxInit(&dlg_ah, s_ah, two_buttons);
	if (ModalDialogRun(&dlg_ah.base, 0, (struct window *) g_parent_win) == DLG_CANCEL)
		cancelled_ab = 1;
	msg_box_destroy((struct object *) &dlg_ah);

	return cancelled_ab;
}

void MsgControllerConsumeSuppliesAmount(struct msg_controller *self, struct msg *dlg)
{
	struct cstr *ctrl = dlg->payload;
	int32_t amount = (int16_t) atoi(CStrGet(ctrl));
	struct inv_item *item = (struct inv_item *) dlg->subject;

	InvItemAddUnits(item, amount);
}

int32_t MsgControllerGetInventoryQty(struct msg_controller *self, int32_t catalog_index)
{
	struct game *game_obj = GetGameObj();
	struct inv_item *item = InvListFindByType(&game_obj->party->inventory, catalog_index);

	if (item)
		return InvItemGetCount(item, false);

	return 0;
}

bool MsgControllerPartyHasSkill(struct msg_controller *self, int32_t index)
{
	struct game *game_obj = GetGameObj();

	return game_obj->party->skills[index];
}

bool MsgControllerPartyHasItem(struct msg_controller *self, int32_t group)
{
	struct game *game_obj = GetGameObj();

	return InvListHasGroup(&game_obj->party->inventory, group);
}

bool MsgControllerPartyWellEquipped(struct msg_controller *self)
{
	struct game *game_obj = GetGameObj();
	int32_t members = PartyMemberCount(game_obj->party, true);
	int32_t total;

	total = MsgControllerGetInventoryQty(self, g_item_blanket);
	total = MsgControllerGetInventoryQty(self, g_item_winter_coat) + total;
	total = MsgControllerGetInventoryQty(self, g_item_winter_scarf) + total;
	total = MsgControllerGetInventoryQty(self, g_item_mittens) + total;
	total = MsgControllerGetInventoryQty(self, g_item_boots) + total;
	total = MsgControllerGetInventoryQty(self, g_item_long_underwear) + total;

	return total >= members;
}

void MsgControllerRollMemberDailyEvents(struct msg_controller *self)
{
	bool has_item_f;
	int32_t month_i;
	int32_t chance_b;
	struct msg_queue *mq_f;
	int32_t first;
	int32_t idx_p;
	int32_t roll_p;
	struct game *game_obj_e;
	int32_t members;
	struct member *member_j = NULL;
	int32_t year_d;
	int32_t boots_c;
	int32_t thirst_g;
	int32_t water_l;
	int32_t need_d;

	game_obj_e = GetGameObj();
	has_item_f = MsgControllerPartyHasItem(self, IGRP_CLOTHING);
	members = PartyMemberCount(game_obj_e->party, true);
	if (!members)
		return;

	month_i = game_obj_e->date->month;
	mq_f = GetMessageQueue();
	if (GameIsEventAllowed(game_obj_e, MSG_THIRST) && !game_obj_e->weather->snow_depth
		&& !WeatherSumPrecip(game_obj_e->weather, 1)) {
		++self->dry_days;

	} else {
		self->dry_days = 0;
		self->thirst_warned = false;
	}

	first = 0;
	if (members > 1)
		first = 1;

	for (idx_p = 0; idx_p < 6; ++idx_p) {
		member_j = PartyGetMember(game_obj_e->party, idx_p);
		if (!member_j)
			continue;
		if (member_j->state != MEMBER_ALIVE)
			continue;

		if (idx_p >= first) {
			if (GameIsEventAllowed(game_obj_e, MSG_ALKALI_SICKNESS)) {
				++self->alkali_exposure;
				if (self->alkali_exposure > 1)
					chance_b = 2;
				else if (self->alkali_exposure == 1)
					chance_b = 20;
				else
					chance_b = 0;
				if (RandRangeAvg(1, 1000, 1) <= chance_b)
					MemberAddAilment(member_j, AILMENT_ALKALI_SICKNESS, STAGE_INCUBATION);

			} else {
				self->alkali_exposure = 0;
			}
			if (RandRangeAvg(1, 10000, 1) <= 1)
				MemberAddAilment(member_j, MsgControllerPickRandomBrokenBone(self), STAGE_INCUBATION);
			if (RandRangeAvg(1, 10000, 1) <= 5)
				MemberAddAilment(member_j, MsgControllerPickRandomSprain(self), STAGE_INCUBATION);

			chance_b = 0;
			if (WeatherPrecipTotal(game_obj_e->weather) < WeatherSumPrecip(game_obj_e->weather, 30))
				chance_b = 20;
			if (game_obj_e->start_date->month > MONTH_APRIL)
				chance_b += 20;
			if (game_obj_e->start_date->month > MONTH_MAY)
				chance_b += 20;
			if (game_obj_e->train_people > 100)
				chance_b += 20;
			if (!GameIsEventAllowed(game_obj_e, NODEFEAT_CHOLERA)) {
				chance_b /= 4;

			} else {
				year_d = game_obj_e->date->year;
				if (year_d == 1849 || year_d == 1850 || year_d == 1852)
					chance_b *= 2;
			}
			if (RandRangeAvg(1, 10000, 1) <= chance_b)
				MemberAddAilment(member_j, AILMENT_CHOLERA, STAGE_INCUBATION);

			if (has_item_f)
				chance_b = 1;
			else
				chance_b = 50;
			if (WeatherGetHighTemp(game_obj_e->weather, 0) < 45)
				chance_b += 50;
			if (WeatherSumPrecip(game_obj_e->weather, 1))
				chance_b += 50;
			if (chance_b > 1 && !MsgControllerPartyWellEquipped(self))
				chance_b += 50;
			if (chance_b > 1 && !MsgControllerGetInventoryQty(self, g_item_matches))
				chance_b += 50;
			roll_p = RandRangeAvg(1, 10000, 1);
			if ((member_j->health < 800 && 2 * chance_b >= roll_p) || chance_b >= roll_p)
				MemberAddAilment(member_j, AILMENT_BAD_COLD, STAGE_INCUBATION);

			if (RandRangeAvg(1, 10000, 1) <= 5)
				MemberAddAilment(member_j, AILMENT_CONCUSSION, STAGE_INCUBATION);

			chance_b = 0;
			if (WeatherPrecipTotal(game_obj_e->weather) < WeatherSumPrecip(game_obj_e->weather, 30))
				chance_b += 1;
			if (WeatherGetHighTemp(game_obj_e->weather, 0) > 80)
				chance_b += 1;
			if (game_obj_e->start_date->month >= MONTH_MAY)
				++chance_b;
			if (game_obj_e->train_people > 100)
				chance_b += 1;
			if (RandRangeAvg(1, 1000, 1) <= chance_b)
				MemberAddAilment(member_j, AILMENT_DYSENTERY, STAGE_INCUBATION);

			chance_b = 0;
			if (month_i >= MONTH_NOVEMBER || month_i <= MONTH_FEBRUARY)
				chance_b = 3;
			if (month_i == MONTH_OCTOBER || month_i == MONTH_MARCH)
				chance_b = 1;
			if (chance_b > 0 && !MsgControllerPartyWellEquipped(self))
				chance_b += 2;
			if (chance_b && !has_item_f)
				chance_b += 4;
			if (chance_b) {
				roll_p = RandRangeAvg(1, 1000, 1);
				if ((member_j->health < 700 && 2 * chance_b >= roll_p) || chance_b >= roll_p)
					MemberAddAilment(member_j, AILMENT_THE_GRIPPE, STAGE_INCUBATION);
			}

			if (self->mosquito_cooldown)
				chance_b = 15;
			else
				chance_b = 5;
			if (RandRangeAvg(1, 10000, 1) <= chance_b)
				MemberAddAilment(member_j, AILMENT_MALARIA, STAGE_INCUBATION);

			roll_p = RandRangeAvg(1, 10000, 1);
			if ((member_j->age <= 10 && roll_p <= 10) || roll_p == 1)
				MemberAddAilment(member_j, AILMENT_MEASLES, STAGE_INCUBATION);

			if (GameIsEventAllowed(game_obj_e, MSG_MOUNTAIN_FEVER)) {
				chance_b = 0;
				if (month_i >= MONTH_MAY && month_i <= MONTH_JULY)
					chance_b = 3;
				if (month_i >= MONTH_AUGUST && month_i <= MONTH_OCTOBER)
					chance_b = 1;
				if (RandRangeAvg(1, 10000, 1) <= chance_b)
					MemberAddAilment(member_j, AILMENT_MOUNTAIN_FEVER, STAGE_INCUBATION);
			}

			if (game_obj_e->party->beriberi_risk > RandRangeAvg(60, 100, 3))
				MemberAddAilment(member_j, AILMENT_BERIBERI, STAGE_INCUBATION);

			roll_p = RandRangeAvg(1, 10000, 1);
			if ((member_j->age <= 10 && roll_p <= 5) || roll_p <= 1)
				MemberAddAilment(member_j, AILMENT_DIPHTHERIA, STAGE_INCUBATION);

			if (WeatherGetHighTemp(game_obj_e->weather, 0) < 32) {
				chance_b = 1;
				if (WeatherSumPrecip(game_obj_e->weather, 1))
					++chance_b;
				if (MsgQueueFindByCmd(mq_f, MSG_BLIZZARD) || game_obj_e->weather->snow_depth)
					chance_b = 5;
				if (MsgQueueFindByCmd(mq_f, MSG_SNOWBOUND))
					chance_b = 10;
				if (!has_item_f)
					chance_b += 5;
				if (!MsgControllerPartyWellEquipped(self))
					chance_b += 20;
				else if (!MsgControllerGetInventoryQty(self, g_item_blanket))
					chance_b += 10;
				if (!MsgControllerGetInventoryQty(self, g_item_matches))
					chance_b += 10;
				if (RandRangeAvg(1, 1000, 1) <= chance_b)
					MemberAddAilment(member_j, AILMENT_FROSTBITE, STAGE_INCUBATION);
			}

			if (game_obj_e->party->scurvy_risk > RandRangeAvg(60, 100, 3))
				MemberAddAilment(member_j, AILMENT_SCURVY, STAGE_INCUBATION);

			chance_b = 1;
			if (RandRangeAvg(1, 10000, 1) == 1)
				MemberAddAilment(member_j, AILMENT_SMALLPOX, STAGE_INCUBATION);

			if (!game_obj_e->weather->snow_depth && WeatherGetHighTemp(game_obj_e->weather, 0) > 45) {
				chance_b = 1;
				if (GameIsEventAllowed(game_obj_e, NODEFEAT_RATTLESNAKES))
					chance_b = 100;
				boots_c = MsgControllerGetInventoryQty(self, g_item_boots);
				if (!boots_c)
					chance_b += chance_b;
				else if (members > boots_c)
					chance_b += chance_b / 2;
				if (RandRangeAvg(1, 10000, 1) <= chance_b)
					MemberAddAilment(member_j, AILMENT_SNAKEBITE, STAGE_INCUBATION);
			}

			if (RandRangeAvg(1, 10000, 1) == 1)
				MemberAddAilment(member_j, AILMENT_TYPHOID_FEVER, STAGE_INCUBATION);
			if (RandRangeAvg(1, 10000, 1) == 1)
				MemberAddAilment(member_j, AILMENT_TYPHUS, STAGE_INCUBATION);

			roll_p = RandRangeAvg(1, 10000, 1);
			if ((member_j->age <= 10 && roll_p <= 5) || roll_p == 1)
				MemberAddAilment(member_j, AILMENT_SCARLET_FEVER, STAGE_INCUBATION);

			chance_b = 0;
			if (WeatherGetLowTemp(game_obj_e->weather, 0) > 60)
				++chance_b;
			if (WeatherGetHighTemp(game_obj_e->weather, 0) > 80)
				++chance_b;
			if (!MsgControllerGetInventoryQty(self, g_item_matches))
				++chance_b;
			if (!MsgControllerGetInventoryQty(self, g_item_set_of_cooking_utensils))
				++chance_b;
			if (game_obj_e->party->meat_spoiled)
				chance_b += 10;
			if (MsgControllerPartyHasSkill(self, SKILL_COOKING))
				chance_b /= 2;
			if (RandRangeAvg(1, 10000, 1) <= chance_b)
				MemberAddAilment(member_j, AILMENT_FOOD_POISONING, STAGE_INCUBATION);

			if (WeatherGetLowTemp(game_obj_e->weather, 0) < 20) {
				chance_b = 1;
				if (MsgQueueFindByCmd(GetMessageQueue(), MSG_SNOWBOUND))
					chance_b = 5;
				if (!has_item_f)
					chance_b += 5;
				if (!MsgControllerPartyWellEquipped(self))
					chance_b += 10;
				if (!MsgControllerGetInventoryQty(self, g_item_matches))
					chance_b += 5;
				if (WeatherSumPrecip(game_obj_e->weather, 1))
					chance_b *= 2;
				if (RandRangeAvg(1, 1000, 1) <= chance_b)
					MemberAddAilment(member_j, AILMENT_FREEZING, STAGE_INCUBATION);
			}

			chance_b = 0;
			if (WeatherGetHighTemp(game_obj_e->weather, 0) < 45 && WeatherSumPrecip(game_obj_e->weather, 1))
				chance_b = 2;
			if (member_j->health < 600)
				chance_b *= 2;
			if (RandRangeAvg(1, 10000, 1) < chance_b)
				MemberAddAilment(member_j, AILMENT_CONSUMPTION, STAGE_INCUBATION);

			if (GameIsEventAllowed(game_obj_e, MSG_WATER_POISONING)) {
				chance_b = 5;
				if (MemberHasAilment(member_j, AILMENT_THIRST))
					chance_b = 50;
				if (RandRangeAvg(1, 100, 1) <= chance_b)
					MemberAddAilment(member_j, AILMENT_WATER_POISONING, STAGE_INCUBATION);
			}
		}

		if (self->dry_days > 1) {
			thirst_g = (int16_t) (self->dry_days * members);
			water_l = 0;
			chance_b = 0;
			water_l = (int16_t) (water_l + 30 * MsgControllerGetInventoryQty(self, g_item_water_keg_empty_holds_4_));
			water_l = (int16_t) (water_l + 4 * MsgControllerGetInventoryQty(self, g_item_canteen));
			need_d = (int16_t) ((thirst_g - water_l) / members);
			if (need_d > 0) {
				if (need_d < 4)
					chance_b = 25 * need_d;
				else
					chance_b = 100;
				if (RandRangeAvg(1, 100, 1) <= chance_b)
					MemberAddAilment(member_j, AILMENT_THIRST, STAGE_ACUTE);
			}
		}

		if ((member_j->age < 18 || member_j->age > 55)
			&& game_obj_e->party->days_starving > RandRangeAvg(3, 5, 1))
			MemberAddAilment(member_j, AILMENT_STARVATION, STAGE_INCUBATION);
		else if (game_obj_e->party->days_starving > RandRangeAvg(5, 8, 1))
			MemberAddAilment(member_j, AILMENT_STARVATION, STAGE_INCUBATION);
	}
}

void MsgControllerKillDraftAnimals(struct msg_controller *self, int32_t count, int32_t msg_id)
{
	struct inv_item *animal_a;
	int32_t pulling_l;
	struct cstr *died_l = NULL;
	char buf_d[64];
	int32_t type_l;
	struct cstr *name_n;

	animal_a = MsgControllerPickRandomDraftAnimal(self);
	if (!animal_a)
		return;

	if (InvItemGetCount(animal_a, false) < count)
		count = InvItemGetCount(animal_a, false);
	if (count > 1)
		name_n = InvItemGetNamePlural(animal_a);
	else
		name_n = InvItemGetNameSingular(animal_a);
	StrPrintf(buf_d, "%d %s", count, CStrGet(name_n));
	CStrDestroy(&name_n);
	name_n = CStrCreateFrom(buf_d);

	type_l = -1;
	if (CStrEqualBuf(InvItemGetNameCStr(animal_a), "ox"))
		type_l = 0;
	else if (CStrEqualBuf(InvItemGetNameCStr(animal_a), "horse"))
		type_l = 1;
	else if (CStrEqualBuf(InvItemGetNameCStr(animal_a), "mule"))
		type_l = 2;

	if (msg_id == 15 && RandRangeAvg(1, 100, 1) <= 20) {
		msg_id = MSG_ANIMAL_DIED_ACCIDENT;
		died_l = CStrCreateFrom("A");
		if (!type_l)
			CStrAppendBuf(died_l, "n");
		CStrAppendBuf(died_l, " ");
		CStrAppendStr(died_l, InvItemGetNameCStr(animal_a));
		CStrAppendBuf(died_l, " died.");

	} else if (msg_id == MSG_ANIMAL_DIED_ACCIDENT || msg_id == MSG_ANIMAL_DIED_ILLNESS) {
		died_l = CStrCreateFrom("A");
		if (!type_l)
			CStrAppendBuf(died_l, "n");
		CStrAppendBuf(died_l, " ");
		CStrAppendStr(died_l, InvItemGetNameCStr(animal_a));
		CStrAppendBuf(died_l, " died.");
	}

	pulling_l = PartyCalcPullingMode(GetGameObj()->party);
	MsgQueueEnqueue(GetMessageQueue(), msg_id, name_n, pulling_l, (intptr_t) animal_a, died_l, type_l);
	InvItemRemoveUnits(animal_a, count);
}

void MsgControllerEventLightningKillsAnimal(struct msg_controller *self)
{
	struct inv_item *animal_a = MsgControllerPickRandomDraftAnimal(self);
	if (!animal_a)
		return;

	int32_t pounds_c = RandRangeAvg(100, 300, 1);
	InventoryAddMeat(pounds_c);
	InvItemRemoveUnits(animal_a, 1);

	struct cstr *name_l = CStrCreateCopy(InvItemGetNameCStr(animal_a));
	char buf_c[128];
	StrPrintf(buf_c, "Lightning struck and killed one %s.  %d pounds of meat were salvaged.", CStrGet(name_l), pounds_c);
	MsgControllerShowMsgboxText(self, buf_c, false);
	CStrDestroy(&name_l);
}

struct inv_item *MsgControllerPickRandomDraftAnimal(struct msg_controller *self)
{
	struct inv_list *hitched_h = &GetGameObj()->party->animals;
	struct inv_list *spare_b = &GetGameObj()->party->spare_animals;
	struct inv_list *chosen_c = NULL;

	if (hitched_h->base.count && spare_b->base.count) {
		chosen_c = (RandRangeAvg(1, 2, 1) == 1) ? hitched_h : spare_b;

	} else if (hitched_h->base.count) {
		chosen_c = hitched_h;

	} else if (spare_b->base.count) {
		chosen_c = spare_b;

	} else {
		return NULL;
	}

	return InvItemFromNode(ListNodeAt(&chosen_c->base, RandRangeAvg(0, chosen_c->base.count - 1, 1)));
}

struct msg *MsgControllerPostMemberMissing(struct msg_controller *self)
{
	struct member *member = MsgControllerPickRandomMember(self);
	struct cstr *str = CStrCreateCopy(member->name);

	CStrAppendBuf(str, " is missing.");
	member->state = MEMBER_MISSING;
	if (!member->state)
		member->health = 0;

	return MsgQueueEnqueue(GetMessageQueue(), MSG_MISSING_PERSON, str, (intptr_t) member, 0, NULL, -1);
}

bool MsgControllerRollMisfortune2(struct msg_controller *self, int32_t chance)
{
	int32_t roll_al;
	int32_t cmd_g;
	int32_t pct_i = 5 * chance;

	if (MsgControllerGetInventoryQty(self, g_item_grease))
		pct_i /= 2;
	if (MsgControllerGetInventoryQty(self, g_item_turpentine))
		pct_i /= 2;

	if (RandRangeAvg(1, 10000, 1) <= pct_i) {
		roll_al = RandRangeAvg(1, 100, 1);
		if (roll_al <= 25)
			cmd_g = MSG_BROKEN_WHEEL;
		else if (roll_al <= 50)
			cmd_g = MSG_BROKEN_AXLE;
		else if (roll_al <= 75)
			cmd_g = MSG_BROKEN_TONGUE;
		else
			cmd_g = MSG_TIPPED_WAGON;

		if (cmd_g == MSG_TIPPED_WAGON)
			MsgControllerReportSupplyLoss(self, MSG_TIPPED_WAGON, 20, false, false);
		else
			MsgQueueEnqueue(GetMessageQueue(), cmd_g, NULL, 0, 0, NULL, -1);

		if (RandRangeAvg(1, 100, 1) <= 2)
			MsgControllerKillDraftAnimals(self, 1, MSG_INJURED_LIVESTOCK);

		return false;
	}

	return true;
}

int32_t MsgControllerPartyGetOccupation(struct msg_controller *self)
{
	struct game *game_obj = GetGameObj();

	return game_obj->party->occupation;
}

int32_t MsgControllerGetPeopleCount(struct msg_controller *self)
{
	struct game *game_obj = GetGameObj();

	return game_obj->train_people;
}

struct member *MsgControllerPickRandomMember(struct msg_controller *self)
{
	int32_t idx_m;
	int32_t count_j = PartyMemberCount(GetGameObj()->party, true);
	struct member *member = NULL;
	int32_t tries_f = 0;

	for (;;) {
		idx_m = RandRangeAvg(1, count_j - 1, 1);
		if (++tries_f > 20)
			idx_m = 0;
		member = PartyGetMember(GetGameObj()->party, idx_m);
		if (member->state == MEMBER_ALIVE)
			break;
	}

	return member;
}

int32_t MsgControllerDoRestDays(struct msg_controller *self, int32_t days)
{
	int32_t hours = GameRest(GetGameObj(), days, 1);

	if (hours)
		return hours / 24 > 1 ? hours / 24 : 1;

	return 0;
}

void MsgControllerSetPacePenalty(struct msg_controller *self, int32_t cap)
{
	struct game *game_obj = GetGameObj();

	if (game_obj->daily_mod > cap)
		game_obj->daily_mod = cap;
}

int32_t MsgControllerPickRandomSprain(struct msg_controller *self)
{
	int32_t roll = RandRangeAvg(1, 100, 1);

	if (roll <= 20)
		return AILMENT_SPRAINED_WRIST;
	if (roll <= 30)
		return AILMENT_SPRAINED_ELBOW;
	if (roll <= 40)
		return AILMENT_SPRAINED_KNEE;
	if (roll <= 75)
		return AILMENT_SPRAINED_ANKLE;

	return AILMENT_SPRAINED_SHOULDER;
}

bool MsgControllerPartyHasPlentyOfFood(struct msg_controller *self)
{
	struct game *game_obj = GetGameObj();

	return PartyHasPlentyOfFood(game_obj->party);
}

static struct draft_animal_item *draft_item(struct game *g)
{
	if (g->party->hitched)
		return (struct draft_animal_item *) g->party->hitched;

	return (struct draft_animal_item *) InvItemFromNode(g->party->animals.base.head);
}

static bool greenhorn_allowance(struct game *g)
{
	bool r = g->player_level == LEVEL_GREENHORN;

	if (r && GameIsEventAllowed(g, NODEFEAT_NO_GREENHORN_ALLOWANCE))
		r = false;

	return r;
}

void MsgControllerGenerateDailyEvents(struct msg_controller *self)
{
	int32_t day;
	int32_t wagon_capacity_l;
	int32_t chance_b;
	int32_t month_j;
	int32_t loadpct_i;
	struct msg_queue *mq_f;
	struct game *game_obj_b;
	struct weather *weather_n;
	struct wagon_item *wagon_m;
	int32_t terrain_p;
	struct inv_list *lost_g;
	int32_t anger_n;
	int32_t hole_risk_h;
	struct draft_animal_item *draft_f;
	bool has_item_n;
	int32_t pace_risk_i;
	int32_t feed_need_g;
	bool done_l;
	int32_t cold_risk_f;
	int32_t storm_risk_l;
	int32_t heat_risk_i;
	int32_t wander_risk_i;
	int32_t pullcap2_i;

	game_obj_b = GetGameObj();
	weather_n = game_obj_b->weather;
	mq_f = GetMessageQueue();
	month_j = game_obj_b->date->month;
	day = game_obj_b->date->day;
	if (NodeBaseGetId(game_obj_b->cur_node) == NODE_KIND_TRAIL) {
		terrain_p = NodeGetTerrain((struct node *) game_obj_b->cur_node);

	} else {
		terrain_p = 0;
	}

	if (self->fallen_timbers_armed && GameIsEventAllowed(game_obj_b, MSG_FALLEN_TIMBERS)
		&& RandRangeAvg(1, 200, 1) == 33) {
		MsgQueueEnqueue(mq_f, MSG_FALLEN_TIMBERS, NULL, 0, 0, NULL, -1);
		self->fallen_timbers_armed = false;
	}
	if (self->fallen_rocks_armed && GameIsEventAllowed(game_obj_b, MSG_FALLEN_ROCKS)) {
		anger_n = 66;
		if (GameIsEventAllowed(game_obj_b, NODEFEAT_ROCKFALL))
			anger_n += 33;
		if (RandRangeAvg(1, 200, 1) < anger_n) {
			MsgQueueEnqueue(mq_f, MSG_FALLEN_ROCKS, NULL, 0, 0, NULL, -1);
			self->fallen_rocks_armed = false;
		}
	}

	chance_b = game_obj_b->event_days / 30 + 1;
	if (terrain_p && terrain_p != TERRAIN_RAFT)
		++chance_b;
	wagon_capacity_l = PartyTotalWagonCapacity(game_obj_b->party);
	if (!wagon_capacity_l)
		wagon_capacity_l = 1;
	loadpct_i = (int16_t) (100 * (PartyGetLoadUnits(game_obj_b->party) / wagon_capacity_l) - 80);
	if (loadpct_i > 0)
		chance_b += loadpct_i / 5 + 1;
	wagon_m = (struct wagon_item *) InvItemFromNode(game_obj_b->party->wagons.base.head);
	if (WagonItemGetKind(wagon_m) == WAGON_CONESTOGA)
		chance_b *= 2;
	if (RandRangeAvg(1, 1000, 1) <= chance_b)
		MsgQueueEnqueue(mq_f, MSG_BROKEN_WHEEL, NULL, 0, 0, NULL, -1);
	chance_b *= 5;
	if (RandRangeAvg(1, 10000, 1) <= chance_b)
		MsgQueueEnqueue(mq_f, MSG_BROKEN_AXLE, NULL, 0, 0, NULL, -1);
	if (RandRangeAvg(1, 10000, 1) <= chance_b)
		MsgQueueEnqueue(mq_f, MSG_BROKEN_TONGUE, NULL, 0, 0, NULL, -1);

	if (GameIsEventAllowed(game_obj_b, MSG_DUSTSTORM) && !self->trail_muddy && !weather_n->snow_depth) {
		chance_b = 0;
		if ((terrain_p == TERRAIN_SAND || terrain_p == TERRAIN_DESERT) && !WeatherSumPrecip(weather_n, 3))
			chance_b = 10;
		else if (WeatherPrecipTotal(weather_n) > WeatherSumPrecip(weather_n, 30) && !WeatherSumPrecip(weather_n, 7))
			chance_b = 1;
		if (GameIsEventAllowed(game_obj_b, NODEFEAT_DUSTY))
			chance_b += 10;
		if (RandRangeAvg(1, 100, 1) <= chance_b)
			MsgQueueEnqueue(mq_f, MSG_DUSTSTORM, NULL, 0, 0, NULL, -1);
	}

	if (MsgQueueFindByCmd(mq_f, MSG_BLIZZARD) || MsgQueueFindByCmd(mq_f, MSG_DUSTSTORM)
		|| MsgQueueFindByCmd(mq_f, MSG_SNOWBOUND))
		chance_b = 50;
	else if (MsgQueueFindByCmd(mq_f, MSG_THUNDERSTORM) || MsgQueueFindByCmd(mq_f, MSG_HAILSTORM))
		chance_b = 20;
	else
		chance_b = 1;
	if (RandRangeAvg(1, 10000, 1) <= chance_b)
		MsgControllerPostMemberMissing(self);

	chance_b = MsgControllerWeatherHazardChance(self, terrain_p, month_j, day);
	if (RandRangeAvg(1, 100, 1) <= chance_b)
		MsgQueueEnqueue(mq_f, MSG_WILD_FRUIT, NULL, 0, 0, NULL, -1);
	if (RandRangeAvg(1, 100, 1) <= chance_b)
		MsgQueueEnqueue(mq_f, MSG_WILD_VEGETABLES, NULL, 0, 0, NULL, -1);

	switch (month_j) {
	case MONTH_MAY:
	case MONTH_JUNE:
	case MONTH_JULY:
		chance_b = 10;
		break;
	case MONTH_AUGUST:
	case MONTH_SEPTEMBER:
	case MONTH_OCTOBER:
		chance_b = 15;
		break;
	default:
		chance_b = 5;
		break;
	}
	if (RandRangeAvg(1, 1000, 1) <= chance_b)
		MsgQueueEnqueue(mq_f, MSG_ABANDONED_WAGON, NULL, 0, 0, NULL, -1);

	if (!weather_n->snow_depth) {
		chance_b = 3;
		if (RandRangeAvg(1, 1000, 1) <= chance_b)
			MsgQueueEnqueue(mq_f, MSG_BUFFALO_STAMPEDE, NULL, 0, 0, NULL, -1);
	}

	if (!weather_n->snow_depth && GameIsEventAllowed(game_obj_b, MSG_QUICKSAND_AHEAD)) {
		if (GameIsEventAllowed(game_obj_b, NODEFEAT_QUICKSAND))
			MsgQueueEnqueue(mq_f, MSG_QUICKSAND_AHEAD, NULL, 0, 0, NULL, -1);
		else if ((WeatherPrecipTotal(weather_n) < WeatherSumPrecip(weather_n, 30) || WeatherSumPrecip(weather_n, 3) > 10)
			&& RandRangeAvg(1, 100, 1) <= 50)
			MsgQueueEnqueue(mq_f, MSG_QUICKSAND_AHEAD, NULL, 0, 0, NULL, -1);
	}

	if (self->prairie_fire_cooldown) {
		++self->prairie_fire_cooldown;
		self->prairie_fire_cooldown %= 14;
	}
	if (!self->prairie_fire_cooldown && !weather_n->snow_depth
		&& ((month_j > MONTH_MAY && month_j <= MONTH_OCTOBER) || (month_j == MONTH_MAY && day > 15))
		&& WeatherPrecipTotal(weather_n) > WeatherSumPrecip(weather_n, 30) && !WeatherSumPrecip(weather_n, 7)
		&& RandRangeAvg(1, 100, 1) <= 5) {
		MsgQueueEnqueue(mq_f, MSG_PRAIRIE_FIRE, NULL, 0, 0, NULL, -1);
		++self->prairie_fire_cooldown;
	}

	if (GameIsEventAllowed(game_obj_b, MSG_LOCUSTS) && !weather_n->snow_depth
		&& ((month_j > MONTH_JUNE && month_j < MONTH_OCTOBER) || (month_j == MONTH_OCTOBER && day <= 15)
		|| (month_j == MONTH_JUNE && day >= 15))
		&& terrain_p != TERRAIN_DESERT && terrain_p != TERRAIN_SAND && RandRangeAvg(1, 10000, 1) <= 5)
		MsgQueueEnqueue(mq_f, MSG_LOCUSTS, NULL, 0, 0, NULL, -1);

	if (GameIsEventAllowed(game_obj_b, MSG_STRANGERS_AHEAD)
		&& game_obj_b->cur_node->node_id != self->strangers_node_id
		&& RandRangeAvg(1, 100, 1) <= 2) {
		MsgQueueEnqueue(mq_f, MSG_STRANGERS_AHEAD, NULL, 0, 0, NULL, -1);
		self->strangers_node_id = game_obj_b->cur_node->node_id;
	}

	if (RandRangeAvg(1, 10000, 1) <= 1)
		MsgQueueEnqueue(mq_f, MSG_WAGON_FIRE, NULL, 0, 0, NULL, -1);

	chance_b = 1;
	if (MsgQueueFindByCmd(mq_f, MSG_INDIANS_AHEAD) || MsgQueueFindByCmd(mq_f, MSG_STRANGERS_AHEAD)) {
		chance_b = 500;

	} else {
		if (NodeBaseGetId(game_obj_b->cur_node) != NODE_KIND_TRAIL)
			chance_b = 100;
	}
	if (RandRangeAvg(1, 10000, 1) <= chance_b) {
		lost_g = InvListLoseRandom(&game_obj_b->party->inventory, RandRangeAvg(4, 30, 1), false);
		struct inv_item *first_page_g = InvItemFromNode(lost_g->base.head);
		MsgQueueEnqueue(mq_f, MSG_THEFT, InvListFormatContentsPage(lost_g, &first_page_g, true), (intptr_t) lost_g,
			0, NULL, -1);
	}

	chance_b = 0;
	if (terrain_p == TERRAIN_ROUGH) {
		if (game_obj_b->travel_pace == PACE_10_HOURS)
			chance_b = 1;
		else if (game_obj_b->travel_pace == PACE_EXHAUSTING)
			chance_b = 2;
		if (RandRangeAvg(1, 1000, 1) <= chance_b)
			MsgControllerReportSupplyLoss(self, MSG_TIPPED_WAGON, RandRangeAvg(2, 40, 1), false, false);
	}
	if ((terrain_p == TERRAIN_MOUNTAIN || self->trail_muddy) && RandRangeAvg(1, 100, 1) <= 1)
		MsgQueueEnqueue(mq_f, MSG_STUCK_IN_MUD, NULL, 0, 0, NULL, -1);
	if (!weather_n->snow_depth && terrain_p == TERRAIN_SAND && RandRangeAvg(1, 100, 1) <= 2)
		MsgQueueEnqueue(mq_f, MSG_STUCK_IN_SAND, NULL, 0, 0, NULL, -1);

	done_l = false;
	heat_risk_i = 2;
	cold_risk_f = 2;
	pace_risk_i = 2;
	storm_risk_l = 2;
	wander_risk_i = 2;
	hole_risk_h = 2;
	feed_need_g = 1;
	draft_f = draft_item(game_obj_b);
	has_item_n = MsgControllerPartyHasItem(self, IGRP_ANIMAL_FEED);
	if (draft_f) {
		heat_risk_i = DraftAnimalItemGetHeatRisk(draft_f);
		cold_risk_f = DraftAnimalItemGetColdRisk(draft_f);
		pace_risk_i = DraftAnimalItemGetPaceRisk(draft_f);
		storm_risk_l = DraftAnimalItemGetStormRisk(draft_f);
		wander_risk_i = DraftAnimalItemGetWanderRisk(draft_f);
		hole_risk_h = DraftAnimalItemGetHoleRisk(draft_f);
		feed_need_g = DraftAnimalItemGetFeedNeed(draft_f);
	}

	chance_b = 0;
	if (MsgQueueFindByCmd(mq_f, MSG_THUNDERSTORM) || MsgQueueFindByCmd(mq_f, MSG_BLIZZARD)
		|| MsgQueueFindByCmd(mq_f, MSG_DUSTSTORM) || MsgQueueFindByCmd(mq_f, MSG_HEAVY_FOG)
		|| MsgQueueFindByCmd(mq_f, MSG_HAILSTORM)) {
		chance_b = 100 * storm_risk_l;

	} else {
		if (NodeBaseGetId(game_obj_b->cur_node) != NODE_KIND_TRAIL || MsgQueueFindByCmd(mq_f, MSG_STRANGERS_AHEAD)
			|| MsgQueueFindByCmd(mq_f, MSG_INDIANS_AHEAD))
			chance_b = 25 * wander_risk_i;
		else
			chance_b = 3 * storm_risk_l;
	}
	if (MsgControllerPartyHasSkill(self, SKILL_FARMING))
		chance_b /= 10;
	if (MsgControllerGetInventoryQty(self, g_item_spare_horse_or_mule_harn) || MsgControllerGetInventoryQty(self, g_item_bridle))
		chance_b /= 2;
	if (RandRangeAvg(1, 10000, 1) <= chance_b) {
		MsgControllerKillDraftAnimals(self, 1, MSG_MISSING_ANIMAL);
		done_l = true;

	} else {
		chance_b /= 5;
		if (RandRangeAvg(1, 10000, 1) <= chance_b) {
			MsgControllerKillDraftAnimals(self, RandRangeAvg(2, 5, 1), 12);
			done_l = true;
		}
	}

	if (!done_l) {
		chance_b = 0;
		if (PartySumSpares(game_obj_b->party))
			++chance_b;
		if (terrain_p == TERRAIN_ROUGH || terrain_p == TERRAIN_MOUNTAIN || terrain_p == TERRAIN_SAND
			|| terrain_p == TERRAIN_UPHILL)
			++chance_b;
		if (feed_need_g == 3 && !has_item_n)
			chance_b += 10;
		if (feed_need_g == 2 && !has_item_n)
			chance_b += 5;
		if (GameIsEventAllowed(game_obj_b, NODEFEAT_NO_GRASS) && !MsgControllerPartyHasItem(self, IGRP_ANIMAL_FEED))
			chance_b += 20;
		if (WeatherGetHighTemp(weather_n, 0) > 95)
			chance_b += 3 * heat_risk_i;
		if (WeatherGetLowTemp(weather_n, 0) < 10)
			chance_b += 3 * cold_risk_f;
		if (game_obj_b->travel_pace)
			chance_b += 5 * pace_risk_i;
		chance_b += game_obj_b->days_traveled / 30;
		if (MsgControllerPartyHasSkill(self, SKILL_FARMING))
			chance_b /= 2;
		if (RandRangeAvg(1, 1000, 1) <= chance_b) {
			MsgQueueEnqueue(mq_f, MSG_ANIMALS_EXHAUSTED, NULL, 0, 0, NULL, -1);
			done_l = true;
		}
	}

	chance_b = 5 * hole_risk_h;
	if (MsgControllerPartyHasSkill(self, SKILL_FARMING))
		chance_b /= 2;
	if (!done_l && RandRangeAvg(1, 10000, 1) <= chance_b) {
		MsgControllerKillDraftAnimals(self, 1, MSG_ANIMAL_IN_HOLE);
		done_l = true;
	}

	if (!done_l) {
		chance_b = 10 * (game_obj_b->days_traveled / 30) + 1;
		chance_b += 10;
		if (game_obj_b->party->pull_mode == PULL_SINGLE_STRAINED || game_obj_b->party->pull_mode == PULL_TEAM_STRAINED)
			chance_b += 15;
		if (MsgControllerPartyHasSkill(self, SKILL_FARMING))
			chance_b /= 2;
		if (RandRangeAvg(1, 10000, 1) < chance_b)
			MsgControllerKillDraftAnimals(self, 1, MSG_INJURED_LIVESTOCK);
	}

	if (self->wagon_dust_cooldown) {
		++self->wagon_dust_cooldown;
		self->wagon_dust_cooldown %= 10;
	}
	if (!weather_n->snow_depth && !self->trail_muddy && !self->wagon_dust_cooldown) {
		chance_b = 0;
		if ((terrain_p == TERRAIN_DESERT || terrain_p == TERRAIN_SAND) && !WeatherSumPrecip(weather_n, 5))
			chance_b = 40;
		else if (terrain_p != TERRAIN_MOUNTAIN && !WeatherSumPrecip(weather_n, 10))
			chance_b = 10;
		if (RandRangeAvg(1, 100, 1) <= chance_b) {
			self->wagon_dust_cooldown = 1;
			MsgQueueEnqueue(mq_f, MSG_THICK_DUST, NULL, 0, 0, NULL, -1);
		}
	}

	if (month_j == MONTH_JULY && day == 4)
		MsgQueueEnqueue(mq_f, MSG_INDEPENDENCE_DAY, NULL, 0, 0, NULL, -1);

	struct cstr *ox_c = CStrCreateFrom("ox");
	if (InvListFindByName(&game_obj_b->party->animals, ox_c)) {
		chance_b = game_obj_b->event_days / 30 + 1;
		pullcap2_i = PartyTotalWagonCapacity(game_obj_b->party);
		if (!pullcap2_i)
			pullcap2_i = 1;
		loadpct_i = (int16_t) (100 * (PartyGetLoadUnits(game_obj_b->party) / pullcap2_i) - 80);
		if (loadpct_i > 0)
			chance_b += loadpct_i / 5 + 1;
		if (terrain_p && terrain_p != TERRAIN_RAFT)
			++chance_b;
		if (RandRangeAvg(1, 1000, 1) <= chance_b)
			MsgQueueEnqueue(mq_f, MSG_BROKEN_YOKE, NULL, 0, 0, NULL, -1);
	}

	if (!greenhorn_allowance(game_obj_b) && game_obj_b->party->reputation < 25 && !self->low_morale_warned) {
		self->low_morale_warned = true;
		MsgQueueEnqueue(mq_f, MSG_LOW_MORALE, NULL, 0, 0, NULL, -1);

	} else {
		if (game_obj_b->party->reputation > 34)
			self->low_morale_warned = false;
	}
	if (!greenhorn_allowance(game_obj_b) && self->low_morale_warned
		&& (game_obj_b->party->reputation < 10 || RandRangeAvg(1, 100, 1) <= 5)) {
		if (game_obj_b->player_level == LEVEL_TRAIL_GUIDE) {
			MsgQueueEnqueue(mq_f, MSG_FIRED, NULL, 0, 0, NULL, -1);

		} else {
			MsgQueueEnqueue(mq_f, MSG_DISMISSED, NULL, 0, 0, NULL, -1);
			self->days_since_dismissal = 1;
		}
	}
	if (self->days_since_dismissal > 0)
		++self->days_since_dismissal;
	if (self->days_since_dismissal > 15 && RandRangeAvg(1, 100, 1) <= 3) {
		MsgQueueEnqueue(mq_f, MSG_REELECTED, NULL, 0, 0, NULL, -1);
		self->days_since_dismissal = -1;
	}

	if (self->mosquito_cooldown) {
		++self->mosquito_cooldown;
		self->mosquito_cooldown %= 8;

	} else {
		if (!weather_n->snow_depth && month_j >= MONTH_JUNE && month_j <= MONTH_SEPTEMBER
			&& !WeatherCountFreezingDays(weather_n) && RandRangeAvg(1, 100, 1) <= 3) {
			MsgQueueEnqueue(mq_f, MSG_MOSQUITOES, NULL, 0, 0, NULL, -1);
			++self->mosquito_cooldown;
		}
	}

	CStrDestroy(&ox_c);
}

bool MsgControllerRollRandomAccident(struct msg_controller *self, int32_t chance)
{
	int32_t roll;
	int32_t ailment_id;

	if (RandRangeAvg(1, 100, 1) <= chance) {
		roll = RandRangeAvg(1, 100, 1);
		if (roll <= 45)
			ailment_id = AILMENT_CUTS_AND_ABRASIONS;
		else if (roll <= 67)
			ailment_id = MsgControllerPickRandomSprain(self);
		else if (roll <= 90)
			ailment_id = MsgControllerPickRandomBrokenBone(self);
		else
			ailment_id = AILMENT_CONCUSSION;
		MemberAddAilment(MsgControllerPickRandomMember(self), ailment_id, STAGE_INCUBATION);

		return false;
	}

	return true;
}

bool MsgControllerRollRandomMisfortune(struct msg_controller *self, int32_t chance)
{
	int32_t cmd_w;
	int32_t roll_bh;

	if (RandRangeAvg(1, 100, 1) <= chance) {
		roll_bh = RandRangeAvg(1, 100, 1);
		if (roll_bh <= 25)
			cmd_w = MSG_BROKEN_WHEEL;
		else if (roll_bh <= 50)
			cmd_w = MSG_BROKEN_AXLE;
		else if (roll_bh <= 75)
			cmd_w = MSG_BROKEN_TONGUE;
		else
			cmd_w = MSG_TIPPED_WAGON;

		if (cmd_w == MSG_TIPPED_WAGON)
			MsgControllerReportSupplyLoss(self, MSG_TIPPED_WAGON, 20, false, false);
		else
			MsgQueueEnqueue(GetMessageQueue(), cmd_w, NULL, 0, 0, NULL, -1);

		if (RandRangeAvg(1, 100, 1) <= 2)
			MsgControllerKillDraftAnimals(self, 1, MSG_INJURED_LIVESTOCK);

		return false;
	}

	return true;
}

void MsgControllerRollDailyConditions(struct msg_controller *self)
{
	int32_t day;
	struct party *party_o;
	int32_t month_j;
	int32_t chance_c;
	struct msg_queue *mq_g;
	struct weather *weather_o;
	int32_t amt_h;
	int32_t year_a;
	int32_t precip_type_c;
	int32_t roll_c;
	int32_t snow_b;
	struct game *game = GetGameObj();

	weather_o = game->weather;
	mq_g = GetMessageQueue();
	party_o = game->party;
	roll_c = RandRangeAvg(1, 100, 1);
	precip_type_c = WeatherGetPrecipType(weather_o, &amt_h, 0);
	month_j = game->date->month;
	day = game->date->day;
	year_a = game->date->year;

	switch (amt_h) {
	case 2:
		if (weather_o->snow_fall > 50) {
			MsgQueueEnqueue(mq_g, MSG_BLIZZARD, NULL, 0, 0, NULL, -1);
			self->fallen_timbers_armed = true;
			self->fallen_rocks_armed = true;
		}
		break;
	case 1:
		chance_c = 0;
		if (month_j < MONTH_APRIL || month_j > MONTH_OCTOBER || (month_j == MONTH_OCTOBER && day > 15)) {
			chance_c = 0;

		} else {
			if (precip_type_c > 2)
				chance_c = 40;
			if (precip_type_c > 5)
				chance_c = 65;
			if (WeatherGetHighTemp(weather_o, 0) < 75)
				chance_c = (int32_t) ((double) chance_c * 0.25);
			if (year_a == 1849 && month_j == MONTH_JUNE && day > 8 && day < 12
				&& weather_o->region == REGION_NORTH_PLATTE)
				chance_c = 200;
		}
		if (chance_c > roll_c) {
			roll_c = RandRangeAvg(1, 100, 1);
			if (month_j < MONTH_MAY || (month_j == MONTH_MAY && day < 15) || month_j > MONTH_AUGUST)
				chance_c = 0;
			else
				chance_c = 10;
			if (chance_c > roll_c)
				MsgQueueEnqueue(mq_g, MSG_HAILSTORM, NULL, 0, 0, NULL, -1);
			else
				MsgQueueEnqueue(mq_g, MSG_THUNDERSTORM, NULL, 0, 0, NULL, -1);
			self->fallen_timbers_armed = true;
			self->fallen_rocks_armed = true;
		}
		break;
	case 0:
		if (!WeatherSumPrecip(weather_o, 2) && WeatherGetLowTemp(weather_o, 0) >= 25
			&& WeatherGetLowTemp(weather_o, 0) <= 50 && RandRangeAvg(1, 100, 1) <= 15)
			MsgQueueEnqueue(mq_g, MSG_HEAVY_FOG, NULL, 0, 0, NULL, -1);
		break;
	default:
		break;
	}

	chance_c = 0;
	if (weather_o->snow_depth > 150)
		chance_c = 5;
	if (weather_o->snow_depth > 200)
		chance_c = 30;
	if (weather_o->snow_depth > 250)
		chance_c = 50;
	if (weather_o->snow_depth > 300)
		chance_c = 100;
	if (chance_c) {
		if (party_o->pull_mode == PULL_SINGLE_STRAINED || party_o->pull_mode == PULL_TEAM_STRAINED)
			chance_c += 5;
		if (RandRangeAvg(1, 100, 1) <= chance_c)
			MsgQueueEnqueue(mq_g, MSG_SNOWBOUND, NULL, weather_o->snow_depth, 0, NULL, -1);
	}

	if (!weather_o->snow_depth) {
		snow_b = WeatherSumPrecip(weather_o, 3);
		chance_c = 0;
		if (snow_b > 30)
			chance_c = 25;
		else if (snow_b > 20)
			chance_c = 10;
		else if (snow_b > 10)
			chance_c = 2;
		if (RandRangeAvg(1, 100, 1) < chance_c)
			MsgQueueEnqueue(mq_g, MSG_FLOODED_TRAIL, NULL, 0, 0, NULL, -1);
	}

	self->trail_muddy = false;
	if (NodeBaseGetId(game->cur_node) == NODE_KIND_TRAIL
		&& NodeGetTerrain((struct node *) game->cur_node) != TERRAIN_ROUGH
		&& NodeGetTerrain((struct node *) game->cur_node) != TERRAIN_SAND
		&& !weather_o->snow_depth
		&& (WeatherNormalAverage(weather_o) < WeatherSumPrecip(weather_o, 30) || WeatherSumPrecip(weather_o, 2) > 5)) {
		self->trail_muddy = true;
		MsgControllerAdjustMorale(self, -1);
		MsgControllerSetPacePenalty(self, 50);
	}
	if (self->trail_muddy)
		MainUiSetCatFlag(CAT_MUDDY_TRAIL);
	else
		MainUiClearCatFlag(CAT_MUDDY_TRAIL);

	if (WeatherGetHighTemp(weather_o, 0) > 95)
		MsgQueueEnqueue(mq_g, MSG_EXTREME_HEAT, NULL, 0, 0, NULL, -1);
	if (WeatherGetLowTemp(weather_o, 0) < 10)
		MsgQueueEnqueue(mq_g, MSG_EXTREME_COLD, NULL, 0, 0, NULL, -1);
}

int32_t MsgControllerWeatherHazardChance(struct msg_controller *self, int32_t terrain_type, int32_t month,
	int32_t day)
{
	int32_t chance = 0;

	if (terrain_type != TERRAIN_DESERT && terrain_type != TERRAIN_ROUGH && terrain_type != TERRAIN_MOUNTAIN
		&& terrain_type != TERRAIN_SAND) {
		if (month == MONTH_AUGUST)
			chance = 3;
		else if ((month > MONTH_MAY && month < MONTH_OCTOBER) || (month == MONTH_OCTOBER && day <= 15))
			chance = 1;
	}
	if (MsgControllerPartyHasSkill(self, SKILL_BOTANY))
		chance *= 2;

	return chance;
}

void MsgControllerStartHunt(struct msg_controller *self, struct msg *node)
{
	struct event spec_ag = EventMake(EVCLS_ANY, -1, -1);

	EventSetId(&spec_ag, 5300);

	struct cmd_event *cmd = CmdEventCreateSpecs(CMD_CONTROL_ACTION, &spec_ag, &g_app->base.eh_event);
	if (node->number_index == MSG_BUFFALO_STAMPEDE)
		cmd->param1 = 1;
	DispatchGlobalEvent(cmd);
}

void MsgControllerRollDailyHazards(struct msg_controller *self)
{
	int32_t chance_p;
	int32_t terrain_l;

	if (MsgControllerPartyHasSkill(self, SKILL_SHARPSHOOTING))
		chance_p = 10;
	else
		chance_p = 500;
	if (MsgControllerGetInventoryQty(self, g_item_rifle_or_shotgun_sheath))
		chance_p /= 10;
	if (RandRangeAvg(1, 10000, 1) <= chance_p)
		MemberAddAilment(PartyGetMember(GetGameObj()->party, 0), AILMENT_ACCIDENTAL_GUNSHOT, STAGE_INCUBATION);
	if (GameIsEventAllowed(GetGameObj(), MSG_ANIMAL_BITE) && RandRangeAvg(1, 100, 1) <= 3)
		MemberAddAilment(PartyGetMember(GetGameObj()->party, 0), AILMENT_ANIMAL_BITE, STAGE_INCUBATION);
	if (GameIsEventAllowed(GetGameObj(), MSG_ANIMAL_MAULING) && RandRangeAvg(1, 100, 1) <= 1)
		MemberAddAilment(PartyGetMember(GetGameObj()->party, 0), AILMENT_ANIMAL_MAULING, STAGE_INCUBATION);

	if (NodeBaseGetId(GetGameObj()->cur_node) == NODE_KIND_TRAIL)
		terrain_l = NodeGetTerrain((struct node *) GetGameObj()->cur_node);
	else
		terrain_l = 0;

	chance_p = MsgControllerWeatherHazardChance(self, terrain_l, GetGameObj()->date->month, GetGameObj()->date->day);
	chance_p *= 2;
	if (RandRangeAvg(0, 100, 1) < chance_p)
		MsgQueueEnqueue(GetMessageQueue(), MSG_WILD_FRUIT, NULL, 0, 0, NULL, -1);
	else if (RandRangeAvg(0, 100, 1) < chance_p)
		MsgQueueEnqueue(GetMessageQueue(), MSG_WILD_VEGETABLES, NULL, 0, 0, NULL, -1);
}

void MsgControllerReportSupplyLoss(struct msg_controller *self, int32_t event_code, int32_t amount, bool flag,
	bool flag2)
{
	struct inv_list *inv;
	int32_t count_j;
	struct cstr *msg;

	if (event_code != MSG_TIPPED_WAGON && event_code != MSG_SWAMPED_WAGON && event_code != MSG_FELL_THROUGH_ICE
		&& event_code != MSG_RAFTING_LOSSES)
		return;

	msg = CStrCreateFrom("You lost the following supplies: ");
	inv = InvListLoseRandom(&GetGameObj()->party->inventory, (int16_t) amount, flag);
	if (inv->base.count > 0) {
		struct inv_item *first_page_o = InvItemFromNode(inv->base.head);
		struct cstr *lost_text_o = InvListFormatContentsPage(inv, &first_page_o, true);
		CStrAppendStr(msg, lost_text_o);
		CStrDestroy(&lost_text_o);

	} else {
		CStrAppendBuf(msg, "nothing.");
		InvListDestroy(&inv);
	}

	count_j = 0;
	if (flag)
		++count_j;
	if (flag2)
		++count_j;
	if (!MsgQueuePost(GetMessageQueue(), event_code, msg, (intptr_t) inv, 0, NULL, count_j))
		InvListDestroy(&inv);

	if (flag && (event_code == 35 || event_code == 99) && RandRangeAvg(1, 100, 1) < 30)
		MemberAddAilment(MsgControllerPickRandomMember(self), AILMENT_DROWNING, STAGE_INCUBATION);
}

void MsgControllerRollFoodSpoilage(struct msg_controller *self)
{
	struct game *game_obj_d = GetGameObj();
	int32_t amount_a;
	int32_t month_n = game_obj_d->date->month;
	int32_t chance_f = 0;

	if (MsgControllerPartyHasItem(self, IGRP_FISHING_GEAR)) {
		if (month_n > MONTH_FEBRUARY && month_n < MONTH_JUNE)
			chance_f = 15;
		else if (month_n > MONTH_MAY && month_n < MONTH_SEPTEMBER)
			chance_f = 8;
		else if (month_n == MONTH_SEPTEMBER || month_n == MONTH_OCTOBER)
			chance_f = 2;
		else
			chance_f = 0;

		if (RandRangeAvg(1, 100, 1) <= chance_f) {
			amount_a = RandRangeAvg(2, chance_f * 3, 1);
			MsgQueuePost(GetMessageQueue(), MSG_CAUGHT_FISH, NULL, amount_a, 0, NULL, -1);
		}
	}
}

void InventoryAddMeat(int32_t pounds)
{
	struct inv_list *inv_j;
	int32_t salted_lbs_h;
	int32_t salt_units;
	int32_t unsalted;
	int32_t qty_h;
	struct inv_item *item_b;

	pounds = (int16_t) pounds;

	struct cstr *meat_n = CStrCreateFrom("meat [from hunting]");
	struct cstr *salted_f = CStrCreateFrom("salted meat [from hunting]");
	inv_j = &GetGameObj()->party->inventory;
	struct cstr *salt_d = CStrCreateFrom("salt");
	salt_units = (pounds + 7) / 8;
	item_b = InvListFindByName(inv_j, salt_d);
	if (item_b) {
		qty_h = (int16_t) InvItemTotalPortions(item_b);
		if (qty_h >= salt_units) {
			salted_lbs_h = pounds;
			unsalted = 0;

		} else {
			salted_lbs_h = 8 * qty_h;
			salt_units = qty_h;
			unsalted = pounds - salted_lbs_h;
		}
		if (salted_lbs_h) {
			InvListAdd(&GetGameObj()->party->inventory, salted_f, salted_lbs_h, 0.0);
			InvItemConsumePortions(item_b, salt_units);
		}
		if (unsalted)
			InvListAdd(&GetGameObj()->party->inventory, meat_n, unsalted, 0.0);

	} else {
		InvListAdd(&GetGameObj()->party->inventory, meat_n, pounds, 0.0);
	}

	CStrDestroy(&salt_d);
	CStrDestroy(&salted_f);
	CStrDestroy(&meat_n);
}

static bool event_roll_hill(int32_t code, bool uphill)
{
	struct game *g;
	int32_t pull_mode_o;
	int32_t hill_o;
	bool flag_g;
	int32_t chance_v = 0;
	int32_t amount_q;
	int32_t roll_bx;
	int32_t fired_g = 0;

	g = GetGameObj();
	pull_mode_o = g->party->pull_mode;
	g = GetGameObj();
	hill_o = g->cur_node->steepness;
	flag_g = MainUiGetCatFlag(CAT_MUDDY_TRAIL);
	{
		struct hill_situation hill_s;

		hill_s.method = code;
		hill_s.uphill = uphill;
		hill_s.steepness = hill_o;
		hill_s.conditions_checked = g_conditions_checked;
		hill_s.strained = pull_mode_o == PULL_SINGLE_STRAINED || pull_mode_o == PULL_TEAM_STRAINED;
		hill_s.muddy_or_snow = flag_g || GetGameObj()->weather->snow_depth;
		hill_s.conestoga = InvListFindByType(&GetGameObj()->party->wagons, g_item_conestoga) != NULL;
		chance_v = HillSuccessChance(&hill_s);
	}

	roll_bx = RandRangeAvg(1, 100, 1);
	if (roll_bx > chance_v) {
		fired_g = 1;
		amount_q = RandRangeAvg(0, 20, 1) + 5 * hill_o;
		MsgControllerReportSupplyLoss(GetAuxMsgqueue(), MSG_TIPPED_WAGON, amount_q, false, false);
	}

	return fired_g == 0;
}

bool EventRollAilment2(int32_t code, int32_t node_id)
{
	return event_roll_hill(code, true);
}

bool EventRollAilment(int32_t code, int32_t node_id)
{
	return event_roll_hill(code, false);
}
