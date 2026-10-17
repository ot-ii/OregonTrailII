#include "crowdcontrol.h"

#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../game/ailment.h"
#include "../app/app.h"
#include "../ui/combobox.h"
#include "crowdnet.h"
#include "crowdcontrolconfig.h"
#include "../base/cstr.h"
#include "../app/event.h"
#include "../app/evthandler.h"
#include "../base/file.h"
#include "../ui/framewin.h"
#include "../game/game.h"
#include "../game/gamecmd.h"
#include "../data/gfxdev.h"
#include "../screens/healthwin.h"
#include "../base/ini.h"
#include "../game/invitem.h"
#include "../game/invlist.h"
#include "../game/journal.h"
#include "../base/list.h"
#include "../screens/mainui.h"
#include "../game/member.h"
#include "../game/msg.h"
#include "../game/msgcontroller.h"
#include "../game/msgqueue.h"
#include "../trail/node.h"
#include "../app/otiiapp.h"
#include "../game/party.h"
#include "../app/replay.h"
#include "../base/rng.h"
#include "../base/str.h"
#include "../data/strrsrc.h"
#include "../app/version.h"
#include "../game/weather.h"
#include "../ui/window.h"
#include "../base/memory.h"
#include "../third_party/mty_json.h"


static const char *const OK = "success";
static const char *const RETRY = "failTemporary";
static const char *const FAIL = "failPermanent";

struct request {
	char id[128];
	char effect[64];
	char viewer[64];
	int32_t quantity;
	int32_t duration_ms;
	char p1[64];
	char p2[64];
};

struct outcome {
	const char *status;
	char message[256];
	char text[256];
	bool posted_event;
};


enum param_kind {
	P_NONE,
	P_MEMBER,
	P_DISEASE,
	P_INJURY,
	P_FOOD,
	P_PART,
	P_PACE,
	P_RATIONS,
	P_EVENT,
	P_CLOTHING,
	P_CREW,
	P_FIREARM,
	P_AMMO,
	P_MEDICINE,
	P_SPARE,
};

struct option {
	const char *key;
	const char *name;
	int32_t value;
};

static const char *const VARIABLES[6][2] = {
	{"leader_name", "The Wagon Leader"}, {"member2_name", "Second Person"}, {"member3_name", "Third Person"},
	{"member4_name", "Fourth Person"}, {"member5_name", "Fifth Person"}, {"member6_name", "Sixth Person"}
};
static const struct option MEMBERS[] = {
	{"random", "Random", -1}, {"leader", "{{leader_name}}", 0}, {"second", "{{member2_name}}", 1},
	{"third", "{{member3_name}}", 2}, {"fourth", "{{member4_name}}", 3}, {"fifth", "{{member5_name}}", 4},
	{"sixth", "{{member6_name}}", 5}, {NULL, NULL, 0}
};
static const struct option CREW[] = {
	{"random", "Random", -1}, {"second", "{{member2_name}}", 1}, {"third", "{{member3_name}}", 2},
	{"fourth", "{{member4_name}}", 3}, {"fifth", "{{member5_name}}", 4}, {"sixth", "{{member6_name}}", 5},
	{NULL, NULL, 0}
};

static const int32_t DISEASES[] = {
	AILMENT_ALKALI_SICKNESS, AILMENT_BAD_COLD, AILMENT_BERIBERI, AILMENT_CHOLERA, AILMENT_CONSUMPTION,
	AILMENT_DIPHTHERIA, AILMENT_DYSENTERY, AILMENT_FOOD_POISONING, AILMENT_FREEZING, AILMENT_FROSTBITE,
	AILMENT_GANGRENE, AILMENT_INFECTION, AILMENT_LOCKJAW, AILMENT_MALARIA, AILMENT_MEASLES, AILMENT_MOUNTAIN_FEVER,
	AILMENT_PNEUMONIA, AILMENT_RABIES, AILMENT_SCARLET_FEVER, AILMENT_SCURVY, AILMENT_SMALLPOX, AILMENT_STARVATION,
	AILMENT_THE_GRIPPE, AILMENT_THIRST, AILMENT_TYPHOID_FEVER, AILMENT_TYPHUS, AILMENT_WATER_POISONING, -1
};
static const int32_t INJURIES[] = {
	AILMENT_ACCIDENTAL_GUNSHOT, AILMENT_ANIMAL_BITE, AILMENT_ANIMAL_MAULING, AILMENT_BROKEN_ARM, AILMENT_BROKEN_LEG,
	AILMENT_BROKEN_FOOT, AILMENT_BROKEN_HIP, AILMENT_BROKEN_WRIST, AILMENT_BURNS, AILMENT_CONCUSSION,
	AILMENT_CUTS_AND_ABRASIONS, AILMENT_DROWNING, AILMENT_INTERNAL_INJURIES, AILMENT_SNAKEBITE,
	AILMENT_SPRAINED_WRIST, AILMENT_SPRAINED_ELBOW, AILMENT_SPRAINED_KNEE, AILMENT_SPRAINED_ANKLE,
	AILMENT_SPRAINED_SHOULDER, AILMENT_WOUND, -1
};

static const struct option FOODS[] = {
	{"salt pork", "Meat (salt pork)", 0}, {"flour", "Flour", 0}, {"dried beans", "Beans", 0},
	{"coffee beans", "Coffee", 0}, {NULL, NULL, 0}
};
static const struct option PARTS[] = {
	{"wheel", "A wheel", MSG_BROKEN_WHEEL}, {"axle", "An axle", MSG_BROKEN_AXLE},
	{"tongue", "The tongue", MSG_BROKEN_TONGUE}, {NULL, NULL, 0}
};
static const struct option PACES[] = {
	{"steady", "Steady (8 hours a day)", PACE_8_HOURS}, {"strenuous", "Strenuous (10 hours a day)", PACE_10_HOURS},
	{"grueling", "Grueling (exhausting)", PACE_EXHAUSTING}, {NULL, NULL, 0}
};
static const struct option RATIONS[] = {
	{"filling", "Filling", RATIONS_FILLING}, {"meager", "Meager", RATIONS_MEAGER},
	{"bare_bones", "Bare bones", RATIONS_BARE_BONES}, {NULL, NULL, 0}
};
static const struct option EVENTS[] = {
	{"thunderstorm", "Thunderstorm", MSG_THUNDERSTORM}, {"hailstorm", "Hailstorm", MSG_HAILSTORM},
	{"blizzard", "Blizzard", MSG_BLIZZARD}, {"heavy_fog", "Heavy fog", MSG_HEAVY_FOG},
	{"dust_storm", "Dust storm", MSG_DUSTSTORM}, {"extreme_heat", "Extreme heat", MSG_EXTREME_HEAT},
	{"extreme_cold", "Extreme cold", MSG_EXTREME_COLD}, {"prairie_fire", "Prairie fire", MSG_PRAIRIE_FIRE},
	{"buffalo_stampede", "Buffalo stampede", MSG_BUFFALO_STAMPEDE}, {"locusts", "Locusts", MSG_LOCUSTS},
	{"mosquitoes", "Mosquitoes", MSG_MOSQUITOES},
	{"fallen_timbers", "Fallen timbers on the trail", MSG_FALLEN_TIMBERS},
	{"fallen_rocks", "Fallen rocks on the trail", MSG_FALLEN_ROCKS},
	{"flooded_trail", "Flooded trail", MSG_FLOODED_TRAIL},
	{"stuck_in_mud", "Stuck in the mud", MSG_STUCK_IN_MUD}, {"wagon_fire", "Wagon fire", MSG_WAGON_FIRE},
	{"wild_fruit", "Wild fruit", MSG_WILD_FRUIT}, {"wild_vegetables", "Wild vegetables", MSG_WILD_VEGETABLES},
	{"abandoned_wagon", "An abandoned wagon", MSG_ABANDONED_WAGON},
	{"strangers", "Strangers ahead", MSG_STRANGERS_AHEAD},
	{"stuck_in_sand", "Stuck in the sand", MSG_STUCK_IN_SAND}, {"thick_dust", "Thick dust", MSG_THICK_DUST},
	{"broken_yoke", "A broken ox yoke", MSG_BROKEN_YOKE},
	{"independence_day", "Independence Day", MSG_INDEPENDENCE_DAY},
	{"snowbound", "Snowbound", MSG_SNOWBOUND}, {"caught_fish", "Fish", MSG_CAUGHT_FISH},
	{NULL, NULL, 0}
};
static const int32_t GOOD_EVENTS[] = {MSG_WILD_FRUIT, MSG_WILD_VEGETABLES, MSG_ABANDONED_WAGON, MSG_CAUGHT_FISH,
	MSG_INDEPENDENCE_DAY, MSG_STRANGERS_AHEAD, -1};
static const int32_t BAD_EVENTS[] = {MSG_THUNDERSTORM, MSG_HAILSTORM, MSG_HEAVY_FOG, MSG_DUSTSTORM,
	MSG_EXTREME_HEAT, MSG_EXTREME_COLD, MSG_MOSQUITOES, MSG_FALLEN_TIMBERS, MSG_FALLEN_ROCKS, MSG_FLOODED_TRAIL,
	MSG_STUCK_IN_MUD, MSG_STUCK_IN_SAND, MSG_THICK_DUST, MSG_BROKEN_YOKE, MSG_SNOWBOUND, -1};

struct event_effect {
	const char *effect;
	int32_t msg;
};

static const struct event_effect EVENT_EFFECTS[] = {
	{"find_wild_fruit", MSG_WILD_FRUIT}, {"find_wild_vegetables", MSG_WILD_VEGETABLES},
	{"find_abandoned_wagon", MSG_ABANDONED_WAGON}, {"blizzard", MSG_BLIZZARD}, {"prairie_fire", MSG_PRAIRIE_FIRE},
	{"buffalo_stampede", MSG_BUFFALO_STAMPEDE}, {"wagon_fire", MSG_WAGON_FIRE}, {"locusts", MSG_LOCUSTS},
	{"random_good_event", -1}, {"random_bad_event", -2}, {NULL, 0}
};
static const struct option FIREARMS[] = {
	{"rifle", "Rifle", 0}, {"shotgun", "Shotgun", 0}, {"pistol", "Pistol", 0}, {NULL, NULL, 0}
};
static const struct option AMMO[] = {
	{"bullets", "Bullets (Boxes of 20)", 0}, {"shot", "Shot (10-lb. Sacks)", 0},
	{"gunpowder", "Gunpowder (25-lb. Kegs)", 0}, {NULL, NULL, 0}
};
static const struct option MEDICINE[] = {
	{"laudanum", "Laudanum", 0}, {"peppermint", "Peppermint", 0}, {"quinine", "Quinine", 0},
	{"iodine", "Iodine", 0}, {"castor oil", "Castor oil", 0}, {NULL, NULL, 0}
};
static const struct option SPARES[] = {
	{"spare wagon wheel", "Wagon wheel", 0}, {"spare wagon axle", "Wagon axle", 0},
	{"spare wagon tongue", "Wagon tongue", 0}, {"spare ox yoke", "Ox yoke", 0}, {NULL, NULL, 0}
};
static const struct option TOOLS[] = {
	{"wagon cover", "Wagon cover", 0}, {"rope", "Rope", 0}, {"ax", "Ax", 0}, {"shovel", "Shovel", 0},
	{"hammer", "Hammer", 0}, {"tent", "Tent", 0}, {"soap", "Soap", 0}, {"matches", "Matches", 0},
	{"lantern", "Lantern", 0}, {"fishing pole", "Fishing pole", 0}, {"spare horseshoe", "Spare horseshoe", 0},
	{"epsom salts", "Epsom salts", 0}, {"magnesia", "Magnesia", 0}, {"hydrogen peroxide", "Hydrogen peroxide", 0},
	{NULL, NULL, 0}
};
static const struct option CLOTHING[] = {
	{"boots", "Boots", 0}, {"pants", "Pants", 0}, {"hat", "Hat", 0}, {"mittens", "Mittens", 0},
	{"long underwear", "Long underwear", 0}, {"raincoat", "Raincoat", 0}, {"blanket", "Blanket", 0},
	{"winter coat", "Winter coat", 0}, {"set of clothing", "Set of clothing", 0}, {NULL, NULL, 0}
};

struct ailment_effect {
	const char *effect;
	int32_t ailment;
};

static const struct ailment_effect AILMENT_EFFECTS[] = {
	{"bad_cold", AILMENT_BAD_COLD}, {"dysentery", AILMENT_DYSENTERY}, {"measles", AILMENT_MEASLES},
	{"typhoid_fever", AILMENT_TYPHOID_FEVER}, {"cholera", AILMENT_CHOLERA}, {"smallpox", AILMENT_SMALLPOX},
	{"rabies", AILMENT_RABIES}, {"frostbite", AILMENT_FROSTBITE}, {"broken_arm", AILMENT_BROKEN_ARM},
	{"snakebite", AILMENT_SNAKEBITE}, {"broken_leg", AILMENT_BROKEN_LEG}, {"drowning", AILMENT_DROWNING},
	{NULL, 0}
};


typedef void (*apply_func)(const struct request *r, struct outcome *o);
typedef bool (*possible_func)(void);

struct effect {
	const char *id;
	const char *name;
	const char *category;
	const char *description;
	int32_t price;
	int32_t qmin;
	int32_t qmax;
	int32_t duration;
	enum param_kind p1;
	enum param_kind p2;
	float orderliness;
	float morality;
	const char *tag;
	apply_func apply;
	possible_func possible;
};

static void apply_illness(const struct request *r, struct outcome *o);
static void apply_heal(const struct request *r, struct outcome *o);
static void apply_heal_all(const struct request *r, struct outcome *o);
static void apply_kill(const struct request *r, struct outcome *o);
static void apply_give_money(const struct request *r, struct outcome *o);
static void apply_take_money(const struct request *r, struct outcome *o);
static void apply_add_food(const struct request *r, struct outcome *o);
static void apply_remove_food(const struct request *r, struct outcome *o);
static void apply_theft(const struct request *r, struct outcome *o);
static void apply_break_part(const struct request *r, struct outcome *o);
static void apply_ox_missing(const struct request *r, struct outcome *o);
static void apply_ox_dies(const struct request *r, struct outcome *o);
static void apply_give_ox(const struct request *r, struct outcome *o);
static void apply_force_pace(const struct request *r, struct outcome *o);
static void apply_force_rations(const struct request *r, struct outcome *o);
static void apply_rest(const struct request *r, struct outcome *o);
static void apply_trail_event(const struct request *r, struct outcome *o);
static void apply_random_illness(const struct request *r, struct outcome *o);
static void apply_give_random_item(const struct request *r, struct outcome *o);
static void apply_take_random_item(const struct request *r, struct outcome *o);
static void apply_give_item(const struct request *r, struct outcome *o);
static void apply_take_item(const struct request *r, struct outcome *o);
static void apply_tipped_wagon(const struct request *r, struct outcome *o);
static void apply_member_missing(const struct request *r, struct outcome *o);

static bool can_have_money(void);
static bool can_have_food(void);
static bool can_have_oxen(void);
static bool can_crew(void);
static bool can_have_firearms(void);
static bool can_have_ammo(void);
static bool can_have_medicine(void);
static bool can_have_clothing(void);
static bool can_have_spares(void);
static bool can_have_items(void);

static const struct effect EFFECTS[] = {
	{"bad_cold", "Bad Cold", "Illnesses", "A crew member catches a bad cold.", 50, 0, 0, 0, P_CREW, P_NONE, 0.2f, -0.3f, NULL, apply_illness, can_crew},
	{"dysentery", "Dysentery", "Illnesses", "A crew member comes down with dysentery.", 150, 0, 0, 0, P_CREW, P_NONE, 0.2f, -0.6f, NULL, apply_illness, can_crew},
	{"measles", "Measles", "Illnesses", "A crew member catches the measles.", 150, 0, 0, 0, P_CREW, P_NONE, 0.2f, -0.6f, NULL, apply_illness, can_crew},
	{"typhoid_fever", "Typhoid Fever", "Illnesses", "A crew member comes down with typhoid fever.", 250, 0, 0, 0, P_CREW, P_NONE, 0.2f, -0.7f, NULL, apply_illness, can_crew},
	{"cholera", "Cholera", "Illnesses", "A crew member catches cholera.", 300, 0, 0, 0, P_CREW, P_NONE, 0.2f, -0.8f, NULL, apply_illness, can_crew},
	{"smallpox", "Smallpox", "Illnesses", "A crew member comes down with smallpox.", 300, 0, 0, 0, P_CREW, P_NONE, 0.2f, -0.8f, NULL, apply_illness, can_crew},
	{"rabies", "Rabies", "Illnesses", "A crew member has rabies.", 400, 0, 0, 0, P_CREW, P_NONE, 0.2f, -0.9f, NULL, apply_illness, can_crew},
	{"frostbite", "Frostbite", "Injuries", "A crew member gets frostbite.", 100, 0, 0, 0, P_CREW, P_NONE, 0.2f, -0.5f, NULL, apply_illness, can_crew},
	{"broken_arm", "Broken Arm", "Injuries", "A crew member breaks an arm.", 100, 0, 0, 0, P_CREW, P_NONE, 0.2f, -0.5f, NULL, apply_illness, can_crew},
	{"snakebite", "Snakebite", "Injuries", "A rattlesnake bites a crew member.", 150, 0, 0, 0, P_CREW, P_NONE, 0.2f, -0.6f, NULL, apply_illness, can_crew},
	{"broken_leg", "Broken Leg", "Injuries", "A crew member breaks a leg.", 200, 0, 0, 0, P_CREW, P_NONE, 0.2f, -0.7f, NULL, apply_illness, can_crew},
	{"drowning", "Nearly Drowns", "Injuries", "A crew member nearly drowns.", 200, 0, 0, 0, P_CREW, P_NONE, 0.2f, -0.7f, NULL, apply_illness, can_crew},
	{"random_disease", "Random Disease", "Illnesses", "A crew member falls ill with one of the trail's diseases, chosen at random.", 150, 0, 0, 0, P_CREW, P_NONE, 0.2f, -0.6f, NULL, apply_random_illness, can_crew},
	{"random_injury", "Random Injury", "Injuries", "A crew member is hurt: a sprain, a broken bone, burns... chosen at random.", 150, 0, 0, 0, P_CREW, P_NONE, 0.2f, -0.6f, NULL, apply_random_illness, can_crew},
	{"heal_member", "Heal Someone", "Health", "Cures every illness of one person and restores their health.", 200, 0, 0, 0, P_MEMBER, P_NONE, 0.6f, 0.8f, "__cc_heal", apply_heal, NULL},
	{"heal_all", "Heal Everyone", "Health", "Cures the whole party and restores everyone's health.", 500, 0, 0, 0, P_NONE, P_NONE, 0.7f, 0.9f, "__cc_heal", apply_heal_all, NULL},
	{"kill", "Kill a Crew Member", "Health", "A crew member (not the wagon leader) dies on the spot.", 2000, 0, 0, 0, P_CREW, P_NONE, 0.9f, -1.0f, "__cc_kill", apply_kill, can_crew},
	{"member_missing", "A Crew Member Goes Missing", "Health", "A crew member (not the wagon leader) wanders off: search for them, or go on without them.", 300, 0, 0, 0, P_CREW, P_NONE, 0.2f, -0.6f, NULL, apply_member_missing, can_crew},
	{"give_money", "Give Money", "Money", "Dollars for the party's purse.", 1, 1, 1000, 0, P_NONE, P_NONE, 0.5f, 0.5f, NULL, apply_give_money, NULL},
	{"take_money", "Take Money", "Money", "Dollars out of the party's purse.", 1, 1, 1000, 0, P_NONE, P_NONE, 0.5f, -0.5f, NULL, apply_take_money, can_have_money},
	{"add_food", "Give Food", "Food", "Pounds of the food of your choice.", 1, 5, 500, 0, P_FOOD, P_NONE, 0.5f, 0.5f, NULL, apply_add_food, NULL},
	{"remove_food", "Take Food", "Food", "Pounds of food go missing from the wagon.", 1, 5, 500, 0, P_NONE, P_NONE, 0.4f, -0.5f, NULL, apply_remove_food, can_have_food},
	{"give_firearm", "Give a Firearm", "Supplies", "A rifle, shotgun or pistol.", 100, 1, 3, 0, P_FIREARM, P_NONE, 0.5f, 0.4f, NULL, apply_give_item, NULL},
	{"take_firearm", "Take a Firearm", "Supplies", "A rifle, shotgun or pistol disappears.", 150, 1, 3, 0, P_FIREARM, P_NONE, 0.4f, -0.5f, NULL, apply_take_item, can_have_firearms},
	{"give_ammo", "Give Ammunition", "Supplies", "Bullets, shot or gunpowder.", 30, 1, 10, 0, P_AMMO, P_NONE, 0.5f, 0.4f, NULL, apply_give_item, NULL},
	{"take_ammo", "Take Ammunition", "Supplies", "Bullets, shot or gunpowder disappear.", 40, 1, 10, 0, P_AMMO, P_NONE, 0.4f, -0.4f, NULL, apply_take_item, can_have_ammo},
	{"give_medicine", "Give Medicine", "Supplies", "A medicine of your choice.", 30, 1, 5, 0, P_MEDICINE, P_NONE, 0.5f, 0.5f, NULL, apply_give_item, NULL},
	{"take_medicine", "Take Medicine", "Supplies", "A medicine disappears from the wagon.", 40, 1, 5, 0, P_MEDICINE, P_NONE, 0.4f, -0.5f, NULL, apply_take_item, can_have_medicine},
	{"give_clothing", "Give Clothing", "Supplies", "Clothing of your choice.", 15, 1, 10, 0, P_CLOTHING, P_NONE, 0.5f, 0.4f, NULL, apply_give_item, NULL},
	{"take_clothing", "Take Clothing", "Supplies", "Clothing disappears from the wagon.", 25, 1, 10, 0, P_CLOTHING, P_NONE, 0.4f, -0.4f, NULL, apply_take_item, can_have_clothing},
	{"give_spare_part", "Give a Spare Part", "Supplies", "A spare wagon wheel, axle, tongue or ox yoke.", 50, 1, 3, 0, P_SPARE, P_NONE, 0.5f, 0.6f, NULL, apply_give_item, NULL},
	{"take_spare_part", "Take a Spare Part", "Supplies", "A spare wagon part disappears.", 75, 1, 3, 0, P_SPARE, P_NONE, 0.4f, -0.5f, NULL, apply_take_item, can_have_spares},
	{"give_random_item", "Give a Random Item", "Supplies", "Something from the store: food, clothes, a gun, a tool...", 25, 0, 0, 0, P_NONE, P_NONE, 0.0f, 0.4f, NULL, apply_give_random_item, NULL},
	{"take_random_item", "Take a Random Item", "Supplies", "Something the party carries disappears.", 40, 0, 0, 0, P_NONE, P_NONE, 0.0f, -0.4f, NULL, apply_take_random_item, can_have_items},
	{"theft", "Theft", "Supplies", "A thief makes off with some of the party's goods.", 200, 0, 0, 0, P_NONE, P_NONE, 0.2f, -0.6f, NULL, apply_theft, can_have_food},
	{"break_part", "Break the Wagon", "Wagon & Animals", "A wheel, axle or tongue of the wagon breaks.", 200, 0, 0, 0, P_PART, P_NONE, 0.3f, -0.6f, NULL, apply_break_part, NULL},
	{"tipped_wagon", "Tip Over the Wagon", "Wagon & Animals", "The wagon tips over and some supplies are lost.", 250, 0, 0, 0, P_NONE, P_NONE, 0.2f, -0.5f, NULL, apply_tipped_wagon, NULL},
	{"ox_missing", "An Animal Wanders Off", "Wagon & Animals", "One of the draft animals goes missing.", 250, 0, 0, 0, P_NONE, P_NONE, 0.3f, -0.6f, NULL, apply_ox_missing, can_have_oxen},
	{"ox_dies", "An Animal Dies", "Wagon & Animals", "One of the draft animals dies.", 400, 0, 0, 0, P_NONE, P_NONE, 0.5f, -0.8f, NULL, apply_ox_dies, can_have_oxen},
	{"give_ox", "Give an Ox", "Wagon & Animals", "An ox joins the team.", 250, 0, 0, 0, P_NONE, P_NONE, 0.5f, 0.6f, NULL, apply_give_ox, NULL},
	{"force_pace", "Force the Pace", "Travel", "The party has to travel at the pace of your choice for a while.", 100, 0, 0, 60, P_PACE, P_NONE, 0.3f, -0.2f, NULL, apply_force_pace, NULL},
	{"force_rations", "Force the Rations", "Travel", "The party has to eat the rations of your choice for a while.", 100, 0, 0, 60, P_RATIONS, P_NONE, 0.3f, -0.2f, NULL, apply_force_rations, NULL},
	{"rest", "Rest", "Travel", "The party stops to rest for some days.", 50, 1, 7, 0, P_NONE, P_NONE, 0.4f, 0.0f, NULL, apply_rest, NULL},
	{"find_wild_fruit", "Find Wild Fruit", "Trail Events", "The party comes upon wild fruit.", 50, 0, 0, 0, P_NONE, P_NONE, 0.0f, 0.5f, NULL, apply_trail_event, NULL},
	{"find_wild_vegetables", "Find Wild Vegetables", "Trail Events", "The party comes upon wild vegetables.", 50, 0, 0, 0, P_NONE, P_NONE, 0.0f, 0.5f, NULL, apply_trail_event, NULL},
	{"find_abandoned_wagon", "Find an Abandoned Wagon", "Trail Events", "An abandoned wagon by the trail: search it?", 100, 0, 0, 0, P_NONE, P_NONE, 0.0f, 0.4f, NULL, apply_trail_event, NULL},
	{"random_good_event", "Random Good Event", "Trail Events", "Something good happens on the trail: fish, fruit, a celebration...", 75, 0, 0, 0, P_NONE, P_NONE, 0.0f, 0.5f, NULL, apply_trail_event, NULL},
	{"blizzard", "Blizzard", "Trail Events", "A blizzard hits the party.", 250, 0, 0, 0, P_NONE, P_NONE, 0.0f, -0.5f, NULL, apply_trail_event, NULL},
	{"prairie_fire", "Prairie Fire", "Trail Events", "A prairie fire comes toward the party.", 250, 0, 0, 0, P_NONE, P_NONE, 0.0f, -0.6f, NULL, apply_trail_event, NULL},
	{"buffalo_stampede", "Buffalo Stampede", "Trail Events", "A buffalo herd stampedes toward the wagon.", 250, 0, 0, 0, P_NONE, P_NONE, 0.0f, -0.6f, NULL, apply_trail_event, NULL},
	{"wagon_fire", "Wagon Fire", "Trail Events", "The wagon catches fire.", 300, 0, 0, 0, P_NONE, P_NONE, 0.0f, -0.7f, NULL, apply_trail_event, NULL},
	{"locusts", "Locusts", "Trail Events", "A swarm of locusts descends on the party.", 200, 0, 0, 0, P_NONE, P_NONE, 0.0f, -0.5f, NULL, apply_trail_event, NULL},
	{"random_bad_event", "Random Bad Event", "Trail Events", "Something goes wrong on the trail: a storm, fog, mud, a broken yoke...", 150, 0, 0, 0, P_NONE, P_NONE, 0.0f, -0.4f, NULL, apply_trail_event, NULL},
};

#define EFFECT_COUNT ((int32_t) (sizeof(EFFECTS) / sizeof(EFFECTS[0])))


static uintptr_t g_timer;
static int32_t g_busy;
static struct crowd_net *g_net;

struct log_entry {
	char text[200];
	int32_t viewer_len;
	bool failed;
};

static struct log_entry g_log[20];
static int32_t g_log_count;
static int32_t g_log_version;
static int32_t g_available[64];
static uint32_t g_last_report;
static char g_sent_variables[1024];
static bool g_quiet;

struct timed {
	struct request r;
	enum param_kind kind;
	int32_t value;
	int32_t saved;
	int32_t remaining_ms;
	bool paused;
};

static struct timed g_timed[8];
static int32_t g_timed_count;
static uint32_t g_last_tick;


static struct game *game(void)
{
	return GetGameObj();
}

static struct otii_app *otii_app(void)
{
	return (struct otii_app *) g_app;
}

static void ailment_name(int32_t id, char *out, int32_t size)
{
	struct strrsrc *names = StrRsrcLoad(g_text_lib, 15000 + id, true);
	const char *name = names ? StrRsrcGetAt(names, 0) : NULL;

	out[0] = 0;
	if (name)
		StrCopyN(out, (size_t) size, name);
	StrRsrcDestroy(&names);
	if (!out[0])
		StrPrintfN(out, (size_t) size, "ailment %d", id);
}

static void key_of(const char *name, char *out, int32_t size)
{
	int32_t n = 0;

	for (; *name && n < size - 1; ++name)
		out[n++] = isalnum((uint8_t) *name) ? (char) tolower((uint8_t) *name) : '_';
	out[n] = 0;
}

static const char *member_name(struct member *m)
{
	static char name[64];

	name[0] = 0;
	if (m && m->name)
		StrCopy(name, CStrGet(m->name));

	return name;
}

static int32_t pick_member(const char *option, bool (*usable)(struct member *))
{
	struct party *p = game()->party;
	int32_t alive[6];
	int32_t n = 0;
	int32_t i;

	for (i = 0; i < p->member_count && i < 6; ++i)
		if (p->members[i] && p->members[i]->state == MEMBER_ALIVE && (!usable || usable(p->members[i])))
			alive[n++] = i;
	if (!n)
		return -1;

	for (i = 0; MEMBERS[i].key; ++i) {
		if (!strcmp(option, MEMBERS[i].key) && MEMBERS[i].value >= 0) {
			int32_t k = MEMBERS[i].value;

			for (int32_t j = 0; j < n; ++j)
				if (alive[j] == k)
					return k;

			return -1;
		}
	}

	return alive[n > 1 ? RandRangeAvg(0, n - 1, 1) : 0];
}

static const struct option *find_option(const struct option *list, const char *key)
{
	for (int32_t i = 0; list[i].key; ++i)
		if (!strcmp(list[i].key, key))
			return &list[i];

	return NULL;
}

static void refresh(void)
{
	struct otii_app *app = otii_app();

	if (app && app->ui)
		MainUiRefresh(app->ui);
	if (g_health_window)
		HealthWinUpdate(g_health_window, false);
}

static void diary(const char *line)
{
	struct otii_app *app = otii_app();

	if (!app || !game() || !game()->date)
		return;

	JournalSetDate(&app->journal, game()->date);
	JournalAddLine(&app->journal, CStrCreateFrom(line));
}

static void box(const char *text)
{
	(void) text;
}

static bool post_event(int32_t msg_id, struct cstr *payload, intptr_t member, intptr_t subject)
{
	return MsgQueuePost(GetMessageQueue(), msg_id, payload, member, subject, NULL, -1) != NULL;
}

static void fail(struct outcome *o, const char *status, const char *message)
{
	o->status = status;
	StrCopy(o->message, message);
}

static int32_t clamp(int32_t v, int32_t lo, int32_t hi)
{
	return v < lo ? lo : v > hi ? hi : v;
}

static const double CASH_MAX = 32767.99;


static int32_t alive_count(void)
{
	struct party *p = game()->party;
	int32_t n = 0;

	for (int32_t i = 0; i < p->member_count && i < 6; ++i)
		if (p->members[i] && p->members[i]->state == MEMBER_ALIVE)
			++n;

	return n;
}

static bool can_have_money(void)
{
	return game()->party->cash >= 1.0;
}

static bool can_have_food(void)
{
	return PartyGetFoodPounds(game()->party) > 0;
}

static bool can_have_oxen(void)
{
	return game()->party->animals.base.head != NULL;
}

static bool can_crew(void)
{
	struct party *p = game()->party;

	for (int32_t i = 1; i < p->member_count && i < 6; ++i)
		if (p->members[i] && p->members[i]->state == MEMBER_ALIVE)
			return true;

	return false;
}

static const char *not_now(void)
{
	struct otii_app *app = otii_app();
	struct game *g = game();

	if (!app || !g || !g->party || !g->cur_node || !g->date)
		return "No game is being played";
	if (app->app_state != APP_STATE_TRAVELING && app->app_state != APP_STATE_AT_NODE)
		return "The party is busy (hunting, shopping, the diary...)";
	if (!CrowdControlMainWindowEnabled())
		return "A window is open in the game";
	if (g->event_node)
		return "The game is showing an event";
	if (g->travel_state == TRAVEL_RESTING)
		return "The party is resting";
	if (g->crossing_river)
		return "The party is crossing a river";
	if (NodeBaseGetId(g->cur_node) == NODE_KIND_TRAIL && NodeGetTerrain((struct node *) g->cur_node) == TERRAIN_RAFT)
		return "The party is rafting";

	return NULL;
}


static int32_t g_ailment_for_pick;

static bool can_catch(struct member *m)
{
	return !MemberHasAilment(m, g_ailment_for_pick);
}

static bool (*g_crew_usable)(struct member *);

static bool crew_usable(struct member *m)
{
	return m != game()->party->members[0] && (!g_crew_usable || g_crew_usable(m));
}

static int32_t pick_target(const char *option, bool crew_only, bool (*usable)(struct member *), struct outcome *o,
	const char *none)
{
	int32_t who;

	if (option[0] && !find_option(crew_only ? CREW : MEMBERS, option)) {
		fail(o, FAIL, crew_only ? "No such crew member (the wagon leader cannot be chosen)" : "No such person");
		return -1;
	}
	if (crew_only) {
		g_crew_usable = usable;
		who = pick_member(option[0] ? option : "random", crew_usable);
		if (who < 0 && option[0] && strcmp(option, "random"))
			who = pick_member("random", crew_usable);
		g_crew_usable = NULL;

	} else {
		who = pick_member(option[0] ? option : "random", usable);
		if (who < 0 && option[0] && strcmp(option, "random"))
			who = pick_member("random", usable);
	}
	if (who < 0)
		fail(o, RETRY, none);

	return who;
}

static void apply_illness(const struct request *r, struct outcome *o)
{
	char name[64];
	char text[256];
	int32_t id = -1;

	for (int32_t i = 0; AILMENT_EFFECTS[i].effect && id < 0; ++i)
		if (!strcmp(AILMENT_EFFECTS[i].effect, r->effect))
			id = AILMENT_EFFECTS[i].ailment;
	if (id < 0) {
		fail(o, FAIL, "No such illness");
		return;
	}
	ailment_name(id, name, sizeof(name));
	g_ailment_for_pick = id;

	int32_t who = pick_target(r->p1, true, can_catch, o, "Nobody can catch that right now");

	if (who < 0)
		return;

	struct member *m = game()->party->members[who];

	StrPrintf(text, "%s gives %s %s.", r->viewer, member_name(m), name);
	box(text);
	if (!MemberAddAilment(m, id, STAGE_ACUTE)) {
		fail(o, RETRY, "The illness could not take hold (already has one like it)");
		return;
	}
	StrCopy(o->text, text);
	o->posted_event = true;
	refresh();
}

static void apply_random_illness(const struct request *r, struct outcome *o)
{
	const int32_t *list = !strcmp(r->effect, "random_disease") ? DISEASES : INJURIES;
	char name[64];
	char text[256];
	int32_t n;

	int32_t who = pick_target(r->p1, true, NULL, o, "Nobody alive");

	if (who < 0)
		return;

	struct member *m = game()->party->members[who];

	for (n = 0; list[n] >= 0; ++n)
		continue;
	for (int32_t tries = 0; tries < 20; ++tries) {
		int32_t id = list[RandRangeAvg(0, n - 1, 1)];

		if (MemberHasAilment(m, id))
			continue;
		ailment_name(id, name, sizeof(name));
		StrPrintf(text, "%s gives %s %s.", r->viewer, member_name(m), name);
		if (!MemberAddAilment(m, id, STAGE_ACUTE))
			continue;
		box(text);
		StrCopy(o->text, text);
		o->posted_event = true;
		refresh();
		return;
	}
	fail(o, RETRY, "Nothing would take hold right now");
}

static void cure(struct member *m)
{
	struct ailment *a;

	for (int32_t i = 0; i < 64 && (a = OblistDataAt(m->nodes, i, false)) != NULL; ++i) {
		if (a->def)
			a->def->flags &= ~HPROB_FLAG_RELAPSE;
		a->stage = STAGE_NONE;
		a->days_left = 0;
	}
	MemberPurgeExpiredAilments(m);
	m->health = m->max_health;
}

static void apply_heal(const struct request *r, struct outcome *o)
{
	int32_t who = pick_target(r->p1, false, NULL, o, "Nobody alive to heal");

	if (who < 0)
		return;

	struct member *m = game()->party->members[who];

	cure(m);
	StrPrintf(o->text, "%s heals %s.", r->viewer, member_name(m));
	refresh();
}

static void apply_heal_all(const struct request *r, struct outcome *o)
{
	struct party *p = game()->party;

	if (!alive_count()) {
		fail(o, RETRY, "Nobody alive to heal");
		return;
	}
	for (int32_t i = 0; i < p->member_count && i < 6; ++i)
		if (p->members[i] && p->members[i]->state == MEMBER_ALIVE)
			cure(p->members[i]);
	StrPrintf(o->text, "%s heals the whole party.", r->viewer);
	refresh();
}

static void apply_kill(const struct request *r, struct outcome *o)
{
	char line[256];

	int32_t who = pick_target(r->p1, true, NULL, o, "Nobody alive");

	if (who < 0)
		return;

	struct member *m = game()->party->members[who];

	StrPrintf(line, "%s strikes down %s.", r->viewer, member_name(m));
	box(line);

	struct cstr *text = CStrCreateFrom(member_name(m));

	CStrAppendBuf(text, " died.");
	m->health = 0;
	m->state = MEMBER_DEAD;

	struct cmd_event *death = CmdEventCreate(GCMD_MEMBER_DIED);

	death->param1 = (intptr_t) m;
	death->param2 = (intptr_t) text;
	DispatchGlobalEvent(death);
	StrCopy(o->text, line);
	o->posted_event = true;
	refresh();
}

static void apply_give_money(const struct request *r, struct outcome *o)
{
	struct party *p = game()->party;
	int32_t dollars = clamp(r->quantity, 1, 1000);

	p->cash = p->cash + dollars > CASH_MAX ? CASH_MAX : p->cash + dollars;
	StrPrintf(o->text, "%s gives the party $%d.", r->viewer, dollars);
	refresh();
}

static void apply_take_money(const struct request *r, struct outcome *o)
{
	struct party *p = game()->party;
	int32_t dollars = clamp(r->quantity, 1, 1000);

	if (p->cash < 1.0) {
		fail(o, RETRY, "The party has no money");
		return;
	}
	p->cash = p->cash > dollars ? p->cash - dollars : 0.0;
	StrPrintf(o->text, "%s takes $%d from the party.", r->viewer, dollars);
	refresh();
}

static void give_item(const char *key, int32_t units)
{
	struct cstr *name = CStrCreateFrom(key);

	InvListAdd(&game()->party->inventory, name, units, 0.0);
	CStrDestroy(&name);
}

static int32_t units_for(const char *key, int32_t pounds)
{
	struct cstr *name = CStrCreateFrom(key);
	struct inv_item *probe = InvItemCreate(name, 1, 0.0);
	int32_t x16 = INV_ITEM_OPS(probe)->GetUnitWeightX16(probe);

	InvItemDestroy(&probe);
	CStrDestroy(&name);

	return x16 > 0 ? (pounds * 16 + x16 - 1) / x16 : pounds;
}

static void apply_add_food(const struct request *r, struct outcome *o)
{
	const struct option *food = find_option(FOODS, r->p1[0] ? r->p1 : "flour");
	int32_t pounds = clamp(r->quantity, 1, 500);

	if (!food) {
		fail(o, FAIL, "No such food");
		return;
	}
	give_item(food->key, units_for(food->key, pounds));
	StrPrintf(o->text, "%s gives the party %d lb of %s.", r->viewer, pounds, food->name);
	refresh();
}

static bool is_food(struct inv_item *it)
{
	return InvItemInGroup(it, IGRP_FOOD) && (InvItemGetCount(it, true) > 0 || InvItemGetPartialPortions(it) > 0);
}

static int32_t remove_food(int32_t pounds)
{
	struct party *p = game()->party;
	int32_t removed = 0;

	for (int32_t guard = 0; removed < pounds && guard < 1000; ++guard) {
		struct inv_item *best = NULL;
		int32_t best_weight = 0;
		int32_t w;

		for (struct list_node *node = p->inventory.base.head; node; node = node->next) {
			struct inv_item *it = InvItemFromNode(node);

			if (is_food(it) && InvItemGetCount(it, true) > 0 &&
					(w = InvItemGetCount(it, true) * INV_ITEM_OPS(it)->GetUnitWeightX16(it)) > best_weight) {
				best_weight = w;
				best = it;
			}
		}
		if (!best)
			break;

		int32_t unit = INV_ITEM_OPS(best)->GetUnitWeightX16(best) / 16;

		removed += unit > 0 ? unit : 1;
		InvItemRemoveUnits(best, 1);
	}
	InvListPurgeEmpty(&p->inventory);

	return removed;
}

static void apply_remove_food(const struct request *r, struct outcome *o)
{
	int32_t pounds = clamp(r->quantity, 1, 500);

	if (!can_have_food()) {
		fail(o, RETRY, "The party has no food");
		return;
	}

	int32_t removed = remove_food(pounds);

	StrPrintf(o->text, "%s takes %d lb of the party's food.", r->viewer, removed);
	refresh();
}

static void apply_theft(const struct request *r, struct outcome *o)
{
	struct inv_list *lost = InvListLoseRandom(&game()->party->inventory, RandRangeAvg(4, 30, 1), false);
	char line[256];

	if (!lost || !lost->base.head) {
		InvListDestroy(&lost);
		fail(o, RETRY, "There is nothing to steal");
		return;
	}
	StrPrintf(line, "%s sends a thief.", r->viewer);
	box(line);

	struct inv_item *first_page = InvItemFromNode(lost->base.head);

	post_event(MSG_THEFT, InvListFormatContentsPage(lost, &first_page, true), (intptr_t) lost, 0);
	StrCopy(o->text, line);
	o->posted_event = true;
}

static void apply_break_part(const struct request *r, struct outcome *o)
{
	const struct option *part = find_option(PARTS, r->p1[0] ? r->p1 : "wheel");
	char line[256];

	if (!part) {
		fail(o, FAIL, "No such part");
		return;
	}
	StrPrintf(line, "%s breaks the wagon: %s.", r->viewer, part->name);
	box(line);
	if (!post_event(part->value, NULL, 0, 0)) {
		fail(o, RETRY, "The game cannot show that right now");
		return;
	}
	StrCopy(o->text, line);
	o->posted_event = true;
}

static void animal_event(const struct request *r, struct outcome *o, int32_t msg_id, const char *what)
{
	char line[256];

	if (!can_have_oxen()) {
		fail(o, RETRY, "The party has no draft animals");
		return;
	}
	StrPrintf(line, "%s: %s", r->viewer, what);
	box(line);
	MsgControllerKillDraftAnimals(GetAuxMsgqueue(), 1, msg_id);
	MsgQueuePump(GetMessageQueue(), NULL);
	PartyCalcPullingMode(game()->party);
	StrCopy(o->text, line);
	o->posted_event = true;
	refresh();
}

static void apply_ox_missing(const struct request *r, struct outcome *o)
{
	animal_event(r, o, MSG_MISSING_ANIMAL, "an animal wanders off.");
}

static void apply_ox_dies(const struct request *r, struct outcome *o)
{
	animal_event(r, o, MSG_ANIMAL_DIED_ACCIDENT, "an animal dies.");
}

static void apply_give_ox(const struct request *r, struct outcome *o)
{
	struct party *p = game()->party;
	struct cstr *ox = CStrCreateFrom("ox");

	PartyAddDraftAnimal(p, &p->animals, ox, 1);
	PartyCalcPullingMode(p);
	StrPrintf(o->text, "%s gives the party an ox.", r->viewer);
	refresh();
	CStrDestroy(&ox);
}

static void start_timed(const struct request *r, struct outcome *o, enum param_kind kind, const struct option *list,
	const char *what)
{
	const struct option *choice = find_option(list, r->p1);

	if (!choice) {
		fail(o, FAIL, "No such choice");
		return;
	}
	for (int32_t i = 0; i < g_timed_count; ++i) {
		if (g_timed[i].kind == kind) {
			fail(o, RETRY, "Already forced: wait until it ends");
			return;
		}
	}
	if (g_timed_count >= 8) {
		fail(o, RETRY, "Too many at once");
		return;
	}

	struct timed *t = &g_timed[g_timed_count++];

	t->r = *r;
	t->kind = kind;
	t->value = choice->value;
	t->saved = kind == P_PACE ? game()->travel_pace : game()->party->ration_level;
	t->remaining_ms = r->duration_ms > 0 ? r->duration_ms : 60000;
	t->paused = false;
	if (kind == P_PACE) {
		game()->travel_pace = choice->value;

	} else {
		game()->party->ration_level = choice->value;
	}
	StrPrintf(o->text, "%s forces %s: %s for %d seconds.", r->viewer, what, choice->name, t->remaining_ms / 1000);
	refresh();
}

static void apply_force_pace(const struct request *r, struct outcome *o)
{
	start_timed(r, o, P_PACE, PACES, "the pace");
}

static void apply_force_rations(const struct request *r, struct outcome *o)
{
	start_timed(r, o, P_RATIONS, RATIONS, "the rations");
}

static void apply_rest(const struct request *r, struct outcome *o)
{
	int32_t days = clamp(r->quantity, 1, 7);
	char line[256];

	StrPrintf(line, "%s makes the party rest %d day%s.", r->viewer, days, days > 1 ? "s" : "");
	box(line);
	GameRest(game(), 24 * days, 1);
	StrCopy(o->text, line);
	o->posted_event = true;
	refresh();
}

static bool try_event(int32_t msg, const struct request *r, struct outcome *o)
{
	const char *name = "an event";
	char line[256];

	if (!GameIsEventAllowed(game(), msg) || (msg == MSG_SNOWBOUND && game()->weather->snow_depth <= 0))
		return false;

	for (int32_t i = 0; EVENTS[i].key; ++i)
		if (EVENTS[i].value == msg)
			name = EVENTS[i].name;
	StrPrintf(line, "%s sends: %s.", r->viewer, name);
	box(line);

	intptr_t member = msg == MSG_SNOWBOUND ? game()->weather->snow_depth : msg == MSG_CAUGHT_FISH ?
		RandRangeAvg(5, 30, 1) : 0;

	if (!post_event(msg, NULL, member, 0)) {
		fail(o, RETRY, "The game cannot show that right now");
		return true;
	}
	StrCopy(o->text, line);
	o->posted_event = true;

	return true;
}

static void apply_trail_event(const struct request *r, struct outcome *o)
{
	int32_t msg = 0;
	bool found = false;
	int32_t possible[32];
	int32_t n = 0;
	int32_t i;

	for (i = 0; EVENT_EFFECTS[i].effect && !found; ++i) {
		if (!strcmp(EVENT_EFFECTS[i].effect, r->effect)) {
			msg = EVENT_EFFECTS[i].msg;
			found = true;
		}
	}
	if (!found) {
		fail(o, FAIL, "No such event");
		return;
	}
	if (msg >= 0) {
		if (!try_event(msg, r, o))
			fail(o, RETRY, "That cannot happen here right now");
		return;
	}

	const int32_t *pool = msg == -1 ? GOOD_EVENTS : BAD_EVENTS;

	for (i = 0; pool[i] >= 0; ++i)
		if (GameIsEventAllowed(game(), pool[i]) && !(pool[i] == MSG_SNOWBOUND && game()->weather->snow_depth <= 0))
			possible[n++] = pool[i];
	if (!n || !try_event(possible[n > 1 ? RandRangeAvg(0, n - 1, 1) : 0], r, o))
		fail(o, RETRY, "Nothing like that can happen here right now");
}

static const char *item_word(const struct option *item)
{
	if (!strncmp(item->name, "A ", 2))
		return item->name + 2;
	if (!strncmp(item->name, "An ", 3))
		return item->name + 3;

	return item->name;
}

static const struct option *group_of(const char *effect)
{
	if (strstr(effect, "firearm"))
		return FIREARMS;
	if (strstr(effect, "ammo"))
		return AMMO;
	if (strstr(effect, "medicine"))
		return MEDICINE;
	if (strstr(effect, "spare_part"))
		return SPARES;

	return CLOTHING;
}

static void apply_give_item(const struct request *r, struct outcome *o)
{
	const struct option *item = find_option(group_of(r->effect), r->p1);
	int32_t n = clamp(r->quantity, 1, 10);

	if (!item) {
		fail(o, FAIL, "No such item");
		return;
	}
	give_item(item->key, n);
	StrPrintf(o->text, "%s gives the party %d x %s.", r->viewer, n, item_word(item));
	refresh();
}

static int32_t stacked_count(const char *key)
{
	int32_t n = 0;

	for (struct list_node *node = game()->party->inventory.base.head; node; node = node->next) {
		struct inv_item *it = InvItemFromNode(node);

		if (CStrEqualBuf(it->name, key))
			n += InvItemGetCount(it, true);
	}

	return n;
}

static bool has_any(const struct option *list)
{
	for (int32_t i = 0; list[i].key; ++i)
		if (stacked_count(list[i].key) > 0)
			return true;

	return false;
}

static bool can_have_firearms(void)
{
	return has_any(FIREARMS);
}

static bool can_have_ammo(void)
{
	return has_any(AMMO);
}

static bool can_have_medicine(void)
{
	return has_any(MEDICINE);
}

static bool can_have_clothing(void)
{
	return has_any(CLOTHING);
}

static bool can_have_spares(void)
{
	return has_any(SPARES);
}

static bool can_have_items(void)
{
	return game()->party->inventory.base.head != NULL;
}

static void apply_give_random_item(const struct request *r, struct outcome *o)
{
	static const struct option *const lists[] = {FOODS, FIREARMS, AMMO, MEDICINE, CLOTHING, SPARES, TOOLS};
	const struct option *pick = NULL;
	int32_t l;
	int32_t i;
	int32_t n = 0;

	for (l = 0; l < 7; ++l)
		for (i = 0; lists[l][i].key; ++i)
			++n;

	int32_t k = RandRangeAvg(0, n - 1, 1);

	for (l = 0; l < 7 && !pick; ++l)
		for (i = 0; lists[l][i].key && !pick; ++i)
			if (k-- == 0)
				pick = &lists[l][i];
	give_item(pick->key, 1);
	StrPrintf(o->text, "%s gives the party: %s.", r->viewer, pick->name);
	refresh();
}

static void apply_take_random_item(const struct request *r, struct outcome *o)
{
	struct list_node *node;
	struct inv_item *it = NULL;
	int32_t n = 0;
	char name[64];

	for (node = game()->party->inventory.base.head; node; node = node->next)
		if (InvItemGetCount(InvItemFromNode(node), true) > 0)
			++n;
	if (!n) {
		fail(o, RETRY, "The party has nothing to take");
		return;
	}

	int32_t k = RandRangeAvg(0, n - 1, 1);

	for (node = game()->party->inventory.base.head; node && !it; node = node->next)
		if (InvItemGetCount(InvItemFromNode(node), true) > 0 && k-- == 0)
			it = InvItemFromNode(node);
	StrCopy(name, CStrGet(it->name));
	InvItemRemoveUnits(it, 1);
	InvListPurgeEmpty(&game()->party->inventory);
	StrPrintf(o->text, "%s takes: %s.", r->viewer, name);
	refresh();
}

static void apply_take_item(const struct request *r, struct outcome *o)
{
	const struct option *item = find_option(group_of(r->effect), r->p1);
	int32_t n = clamp(r->quantity, 1, 10);

	if (!item) {
		fail(o, FAIL, "No such item");
		return;
	}

	int32_t had = stacked_count(item->key);

	if (had <= 0) {
		fail(o, RETRY, "The party has none of that");
		return;
	}
	if (n > had)
		n = had;

	int32_t left = n;

	for (struct list_node *node = game()->party->inventory.base.head; node && left > 0; node = node->next) {
		struct inv_item *it = InvItemFromNode(node);

		if (!CStrEqualBuf(it->name, item->key))
			continue;

		int32_t take = InvItemGetCount(it, true) < left ? InvItemGetCount(it, true) : left;

		InvItemRemoveUnits(it, take);
		left -= take;
	}
	InvListPurgeEmpty(&game()->party->inventory);
	StrPrintf(o->text, "%s takes %d x %s.", r->viewer, n, item_word(item));
	refresh();
}

static void apply_tipped_wagon(const struct request *r, struct outcome *o)
{
	char line[256];

	if (!GameIsEventAllowed(game(), MSG_TIPPED_WAGON)) {
		fail(o, RETRY, "That cannot happen here right now");
		return;
	}
	StrPrintf(line, "%s tips over the wagon.", r->viewer);
	box(line);
	MsgControllerReportSupplyLoss(GetAuxMsgqueue(), MSG_TIPPED_WAGON, RandRangeAvg(2, 40, 1), false, false);
	StrCopy(o->text, line);
	o->posted_event = true;
	refresh();
}

static bool can_go_missing(struct member *m)
{
	return m != game()->party->members[0];
}

static void apply_member_missing(const struct request *r, struct outcome *o)
{
	struct party *p = game()->party;
	char line[256];

	if (r->p1[0] && !find_option(CREW, r->p1)) {
		fail(o, FAIL, "The wagon leader cannot go missing");
		return;
	}
	if (!GameIsEventAllowed(game(), MSG_MISSING_PERSON)) {
		fail(o, RETRY, "That cannot happen here right now");
		return;
	}

	int32_t k = pick_target(r->p1, true, can_go_missing, o, "Nobody but the wagon leader is there to go missing");

	if (k < 0)
		return;

	struct member *m = p->members[k];

	StrPrintf(line, "%s: %s goes missing.", r->viewer, member_name(m));

	struct cstr *text = CStrCreateCopy(m->name);

	CStrAppendBuf(text, " is missing.");
	m->state = MEMBER_MISSING;
	if (!post_event(MSG_MISSING_PERSON, text, (intptr_t) m, 0)) {
		m->state = MEMBER_ALIVE;
		fail(o, RETRY, "The game cannot show that right now");
		return;
	}
	box(line);
	StrCopy(o->text, line);
	o->posted_event = true;
	refresh();
}


static void copy_string(const MTY_JSON *item, char *out, int32_t size)
{
	const char *s = MTY_JSONStringPtr(item);

	out[0] = 0;
	if (s)
		StrCopyN(out, (size_t) size, s);
}

static int32_t number_int(double d)
{
	if (d >= (double) INT32_MAX)
		return INT32_MAX;
	if (d <= (double) INT32_MIN)
		return INT32_MIN;

	return (int32_t) d;
}

static void param_value(const MTY_JSON *params, const char *key, char *out, int32_t size)
{
	const MTY_JSON *p = MTY_JSONObjGetItem(params, key);

	out[0] = 0;
	if (!p)
		return;
	if (MTY_JSONGetType(p) == MTY_JSON_STRING) {
		copy_string(p, out, size);
		return;
	}
	copy_string(MTY_JSONObjGetItem(p, "value"), out, size);
	if (!out[0])
		copy_string(MTY_JSONObjGetItem(p, "name"), out, size);
}

static const char *param_key(enum param_kind kind)
{
	switch (kind) {
		case P_MEMBER: return "member";
		case P_DISEASE: return "disease";
		case P_INJURY: return "injury";
		case P_FOOD: return "food";
		case P_PART: return "part";
		case P_PACE: return "pace";
		case P_RATIONS: return "rations";
		case P_EVENT: return "event";
		case P_CLOTHING: return "clothing";
		case P_CREW: return "member";
		case P_FIREARM: case P_AMMO: case P_MEDICINE: case P_SPARE: return "item";
		default: return "";
	}
}

static bool parse_request(const char *json, struct request *r, const struct effect **effect)
{
	MTY_JSON *root = MTY_JSONParse(json);
	double d;

	memset(r, 0, sizeof(*r));
	*effect = NULL;
	if (!root)
		return false;

	copy_string(MTY_JSONObjGetItem(root, "requestID"), r->id, sizeof(r->id));

	const MTY_JSON *ef = MTY_JSONObjGetItem(root, "effect");

	copy_string(MTY_JSONObjGetItem(ef, "effectID"), r->effect, sizeof(r->effect));
	if (MTY_JSONNumber(MTY_JSONObjGetItem(ef, "duration"), &d))
		r->duration_ms = (int32_t) (d * 1000);
	r->quantity = MTY_JSONNumber(MTY_JSONObjGetItem(root, "quantity"), &d) ? number_int(d) : 1;
	copy_string(MTY_JSONObjGetItem(MTY_JSONObjGetItem(root, "requester"), "name"), r->viewer, sizeof(r->viewer));
	if (!r->viewer[0])
		StrCopy(r->viewer, "A viewer");

	const MTY_JSON *params = MTY_JSONObjGetItem(root, "parameters");

	for (int32_t i = 0; i < EFFECT_COUNT; ++i) {
		if (!strcmp(EFFECTS[i].id, r->effect)) {
			*effect = &EFFECTS[i];
			param_value(params, param_key(EFFECTS[i].p1), r->p1, sizeof(r->p1));
			param_value(params, param_key(EFFECTS[i].p2), r->p2, sizeof(r->p2));
			break;
		}
	}
	MTY_JSONDestroy(&root);

	return r->id[0] != 0;
}


static void add_log(const char *viewer, bool failed, const char *line)
{
	size_t n = viewer ? strlen(viewer) : 0;

	if (g_log_count == 20) {
		memmove(&g_log[0], &g_log[1], sizeof(g_log[0]) * 19);
		--g_log_count;
	}

	struct log_entry *e = &g_log[g_log_count++];

	StrCopy(e->text, line);
	e->viewer_len = n && !strncmp(e->text, viewer, n) ? (int32_t) n : 0;
	e->failed = failed;
	++g_log_version;
}

void CrowdControlLog(const char *line)
{
	char text[200];

	StrCopy(text, line);
	add_log(NULL, false, text);
}

int32_t CrowdControlLogVersion(void)
{
	return g_log_version;
}

int32_t CrowdControlLogCount(void)
{
	return g_log_count;
}

const char *CrowdControlLogEntry(int32_t index, int32_t *viewer_len, bool *failed)
{
	if (index < 0 || index >= g_log_count)
		return NULL;

	*viewer_len = g_log[index].viewer_len;
	*failed = g_log[index].failed;

	return g_log[index].text;
}

static struct outcome handle_request(const char *json)
{
	struct request r;
	const struct effect *e;
	struct outcome o;
	char diary_line[300];

	memset(&o, 0, sizeof(o));
	o.status = OK;
	if (!parse_request(json, &r, &e)) {
		fail(&o, FAIL, "Unreadable request");
		return o;
	}
	if (!e) {
		fail(&o, FAIL, "Oregon Trail II has no such effect");
		CrowdNetRespond(g_net, r.id, o.status, o.message, 0);
		return o;
	}

	const char *why = not_now();

	if (why) {
		fail(&o, RETRY, why);

	} else if (e->possible && !e->possible()) {
		fail(&o, RETRY, "Not possible right now");

	} else {
		e->apply(&r, &o);
	}

	if (o.status == OK) {
		if (!o.posted_event && o.text[0]) {
			char box_text[300];

			StrPrintf(box_text, "Crowd Control: %s", o.text);
			box(box_text);
		}
		StrPrintf(diary_line, "Crowd Control: %s", o.text);
		diary(diary_line);
		add_log(r.viewer, false, o.text);

	} else {
		char line[200];

		StrPrintf(line, "%s %s: %s", r.viewer, e->name, o.message);
		add_log(r.viewer, true, line);
	}
	CrowdNetRespond(g_net, r.id, o.status, o.message, 0);
	if (o.status == OK && e->duration)
		CrowdNetRespond(g_net, r.id, "timedBegin", "", g_timed_count ? g_timed[g_timed_count - 1].remaining_ms : 0);

	return o;
}

static void sync_list(int32_t id, enum param_kind kind, int32_t value)
{
	struct control *c = g_parent_win ? WindowFindCtrlById((struct window *) g_parent_win, id) : NULL;
	bool forced = false;

	if (!c || c->control_type != CTRL_COMBOBOX)
		return;

	for (int32_t i = 0; i < g_timed_count; ++i)
		if (g_timed[i].kind == kind)
			forced = true;

	if (((struct combo_box *) c)->disabled != forced)
		ComboBoxSetEnabled((struct combo_box *) c, !forced);

	if (c->value != value)
		CONTROL_OPS(c)->SetValue(c, value, false);
}

static void step_timed(int32_t ms)
{
	bool was_forcing = g_timed_count > 0;

	for (int32_t i = 0; i < g_timed_count; ) {
		struct timed *t = &g_timed[i];
		bool now_paused = not_now() != NULL;

		if (now_paused != t->paused) {
			t->paused = now_paused;
			CrowdNetRespond(g_net, t->r.id, now_paused ? "timedPause" : "timedResume", "", t->remaining_ms);
		}
		if (game() && game()->party) {
			if (t->kind == P_PACE) {
				game()->travel_pace = t->value;

			} else {
				game()->party->ration_level = t->value;
			}
		}
		if (!t->paused)
			t->remaining_ms -= ms;
		if (t->remaining_ms <= 0) {
			char line[200];

			if (game() && game()->party) {
				if (t->kind == P_PACE) {
					game()->travel_pace = t->saved;

				} else {
					game()->party->ration_level = t->saved;
				}
			}
			CrowdNetRespond(g_net, t->r.id, "timedEnd", "", 0);
			StrPrintf(line, "%s ended", t->kind == P_PACE ? "Forced pace" : "Forced rations");
			CrowdControlLog(line);
			g_timed[i] = g_timed[--g_timed_count];
			refresh();
			continue;
		}
		++i;
	}

	if (was_forcing && game() && game()->party) {
		sync_list(9013, P_PACE, game()->travel_pace);
		sync_list(9014, P_RATIONS, game()->party->ration_level);
	}
}

static void report_ids(const char *const *ids, uint32_t count, const char *status)
{
	if (!count)
		return;

	MTY_JSON *list = MTY_JSONArrayCreate(count);

	for (uint32_t i = 0; i < count; ++i)
		MTY_JSONArraySetItem(list, i, MTY_JSONStringCreate(ids[i]));

	char *text = MTY_JSONSerialize(list);

	CrowdNetReport(g_net, text, status);
	MTY_Free(text);
	MTY_JSONDestroy(&list);
}

static void report_menu(void)
{
	const char *changed[2][64];
	uint32_t count[2] = {0, 0};
	bool have_game = game() && game()->party;

	for (int32_t i = 0; i < EFFECT_COUNT && i < 64; ++i) {
		int32_t can = have_game && (!EFFECTS[i].possible || EFFECTS[i].possible()) ? 1 : 0;

		if (g_available[i] == can)
			continue;
		g_available[i] = can;
		changed[can][count[can]++] = EFFECTS[i].id;
	}
	report_ids(changed[1], count[1], "menuAvailable");
	report_ids(changed[0], count[0], "menuUnavailable");
}

static bool pack_variables(char *out, int32_t size)
{
	out[0] = 0;
	if (!game() || !game()->party)
		return false;

	struct party *p = game()->party;
	MTY_JSON *values = MTY_JSONObjCreate();
	const char *off[6];
	uint32_t off_count = 0;

	for (int32_t i = 0; i < 6; ++i) {
		struct member *m = i < p->member_count ? p->members[i] : NULL;

		if (m && m->state == MEMBER_ALIVE && member_name(m)[0]) {
			MTY_JSONObjSetString(values, VARIABLES[i][0], member_name(m));

		} else {
			off[off_count++] = VARIABLES[i][0];
		}
	}

	MTY_JSON *disabled = MTY_JSONArrayCreate(off_count);

	for (uint32_t i = 0; i < off_count; ++i)
		MTY_JSONArraySetString(disabled, i, off[i]);
	MTY_JSONObjSetItem(values, "__disabledVariables", disabled);

	char *text = MTY_JSONSerialize(values);

	StrCopyN(out, (size_t) size, text);
	MTY_Free(text);
	MTY_JSONDestroy(&values);

	return out[0] != 0;
}

static void send_variables(void)
{
	char state[256];
	char now[1024];

	if (CrowdNetState(g_net, state, sizeof(state)) != CROWD_SESSION) {
		g_sent_variables[0] = 0;
		return;
	}
	if (pack_variables(now, sizeof(now)) && strcmp(now, g_sent_variables)) {
		CrowdNetPackMetadata(g_net, now);
		StrCopy(g_sent_variables, now);
	}
}

void CrowdControlTick(void)
{
	static char payload[8192];

	if (g_busy)
		return;

	++g_busy;

	uint32_t now = RpGetTickCount();

	if (g_last_tick)
		step_timed((int32_t) (now - g_last_tick));
	g_last_tick = now;
	memset(payload, 0, sizeof(payload));
	if (RpQueryBytes(RPQ_CROWD, payload, sizeof(payload), g_net ? CrowdNetPoll(g_net, payload, sizeof(payload)) :
			false))
		handle_request(payload);
	if (g_net && now - g_last_report >= 2000) {
		g_last_report = now;
		report_menu();
		send_variables();
	}
	--g_busy;
}

struct crowd_net *CrowdControlNet(void)
{
	return g_net;
}


static bool word_is(const char *word, int32_t len, const char *minor)
{
	if ((int32_t) strlen(minor) != len)
		return false;

	for (int32_t i = 0; i < len; ++i)
		if (tolower((uint8_t) word[i]) != tolower((uint8_t) minor[i]))
			return false;

	return true;
}

static void title_case(const char *in, char *out, int32_t size)
{
	static const char *const minor[] = {"a", "an", "the", "and", "or", "nor", "but", "of", "in", "on", "at", "to",
		"for", "by", "with", "x", NULL};
	int32_t o = 0;
	bool first = true;

	if (strstr(in, "{{")) {
		StrCopyN(out, (size_t) size, in);
		return;
	}
	while (*in && o < size - 1) {
		const char *end = in;
		bool is_minor = false;

		while (*end && *end != ' ')
			++end;

		int32_t len = (int32_t) (end - in);

		for (int32_t k = 0; minor[k]; ++k)
			if (word_is(in, len, minor[k]))
				is_minor = true;
		for (int32_t k = 0; k < len && o < size - 1; ++k) {
			char c = in[k];

			if (k == 0 || (k == 1 && in[0] == '('))
				c = (char) ((first || !is_minor) ? toupper((uint8_t) c) : c);
			out[o++] = c;
		}
		first = false;
		while (*end == ' ' && o < size - 1)
			out[o++] = *end++;
		in = end;
	}
	out[o] = 0;
}

#define PACK_DEPTH 16

struct pack_out {
	char *text;
	size_t len;
	size_t size;
	int32_t depth;
	bool empty[PACK_DEPTH];
};

static void pack_append(struct pack_out *o, const char *s)
{
	size_t n = strlen(s);

	if (o->len + n + 1 > o->size) {
		while (o->len + n + 1 > o->size)
			o->size = o->size ? o->size * 2 : 4096;
		o->text = MemRealloc(o->text, o->size, 1);
	}
	memcpy(o->text + o->len, s, n + 1);
	o->len += n;
}

static void pack_indent(struct pack_out *o)
{
	for (int32_t i = 0; i < o->depth; ++i)
		pack_append(o, "\t");
}

static void pack_value(struct pack_out *o, MTY_JSON *value)
{
	char *text = MTY_JSONSerialize(value);

	pack_append(o, text);
	MTY_Free(text);
	MTY_JSONDestroy(&value);
}

static void pack_key(struct pack_out *o, const char *key)
{
	if (!o->empty[o->depth])
		pack_append(o, ",\n");
	o->empty[o->depth] = false;
	pack_indent(o);
	pack_value(o, MTY_JSONStringCreate(key));
	pack_append(o, ":\t");
}

static void pack_open(struct pack_out *o, const char *key)
{
	if (key)
		pack_key(o, key);
	pack_append(o, "{\n");
	if (o->depth < PACK_DEPTH - 1)
		o->empty[++o->depth] = true;
}

static void pack_close(struct pack_out *o)
{
	if (!o->empty[o->depth])
		pack_append(o, "\n");
	--o->depth;
	pack_indent(o);
	pack_append(o, "}");
}

static void pack_string(struct pack_out *o, const char *key, const char *value)
{
	pack_key(o, key);
	pack_value(o, MTY_JSONStringCreate(value));
}

static void pack_list(struct pack_out *o, const char *key, const char *value)
{
	pack_key(o, key);
	pack_append(o, "[");
	pack_value(o, MTY_JSONStringCreate(value));
	pack_append(o, "]");
}

static void pack_number(struct pack_out *o, const char *key, double value)
{
	char text[32];

	pack_key(o, key);
	if (isnan(value) || isinf(value)) {
		StrCopy(text, "null");

	} else if (value == (double) number_int(value)) {
		StrPrintf(text, "%d", number_int(value));

	} else {
		StrPrintf(text, "%1.15g", value);

		double back = strtod(text, NULL);
		double most = fabs(back) > fabs(value) ? fabs(back) : fabs(value);

		if (!(fabs(back - value) <= most * DBL_EPSILON))
			StrPrintf(text, "%1.17g", value);
	}
	pack_append(o, text);
}

static void pack_options(struct pack_out *o, enum param_kind kind)
{
	const struct option *list = NULL;
	const int32_t *ids = NULL;
	char name[64];
	char key[64];
	int32_t i;

	switch (kind) {
		case P_MEMBER: list = MEMBERS; break;
		case P_FOOD: list = FOODS; break;
		case P_PART: list = PARTS; break;
		case P_PACE: list = PACES; break;
		case P_RATIONS: list = RATIONS; break;
		case P_EVENT: list = EVENTS; break;
		case P_CLOTHING: list = CLOTHING; break;
		case P_CREW: list = CREW; break;
		case P_FIREARM: list = FIREARMS; break;
		case P_AMMO: list = AMMO; break;
		case P_MEDICINE: list = MEDICINE; break;
		case P_SPARE: list = SPARES; break;
		case P_DISEASE: ids = DISEASES; break;
		case P_INJURY: ids = INJURIES; break;
		default: break;
	}
	for (i = 0; list && list[i].key; ++i) {
		title_case(list[i].name, name, sizeof(name));
		pack_open(o, list[i].key);
		pack_string(o, "name", name);
		pack_close(o);
	}
	for (i = 0; ids && ids[i] >= 0; ++i) {
		ailment_name(ids[i], name, sizeof(name));
		key_of(name, key, sizeof(key));
		pack_open(o, key);
		pack_string(o, "name", name);
		pack_close(o);
	}
}

static const char *param_title(enum param_kind kind)
{
	switch (kind) {
		case P_MEMBER: return "Who";
		case P_DISEASE: return "Disease";
		case P_INJURY: return "Injury";
		case P_FOOD: return "Food";
		case P_PART: return "Part";
		case P_PACE: return "Pace";
		case P_RATIONS: return "Rations";
		case P_EVENT: return "Event";
		case P_CLOTHING: return "Clothing";
		case P_CREW: return "Crew member";
		case P_FIREARM: return "Firearm";
		case P_AMMO: return "Ammunition";
		case P_MEDICINE: return "Medicine";
		case P_SPARE: return "Spare part";
		default: return "";
	}
}

bool CrowdControlWritePack(const char *path)
{
	struct pack_out out = {0};
	struct pack_out *o = &out;
	bool ok = false;

	pack_open(o, NULL);
	pack_open(o, "meta");
	pack_string(o, "name", "The Oregon Trail II");
	pack_string(o, "platform", "PC");
	pack_string(o, "releaseDate", "2026-10-10");
	pack_list(o, "connector", "ExternalUnity");
	pack_string(o, "description",
		"Built into the modern Windows build of The Oregon Trail II: open Crowd Control on the game's menu bar, "
		"connect and start the session.");
	pack_close(o);
	pack_open(o, "effects");
	pack_open(o, "meta");
	pack_open(o, "variables");
	for (int32_t i = 0; i < 6; ++i)
		pack_string(o, VARIABLES[i][0], VARIABLES[i][1]);
	pack_close(o);
	pack_close(o);
	pack_open(o, "game");
	for (int32_t i = 0; i < EFFECT_COUNT; ++i) {
		const struct effect *e = &EFFECTS[i];

		pack_open(o, e->id);
		pack_string(o, "name", e->name);
		pack_number(o, "price", e->price);
		pack_string(o, "description", e->description);
		pack_list(o, "category", e->category);
		pack_list(o, "group", "singleplayer");
		if (e->duration) {
			pack_open(o, "duration");
			pack_number(o, "value", e->duration);
			pack_close(o);
		}
		if (e->qmax) {
			pack_open(o, "quantity");
			pack_number(o, "min", e->qmin);
			pack_number(o, "max", e->qmax);
			pack_close(o);
		}
		if (e->p1 != P_NONE) {
			enum param_kind kinds[2] = {e->p1, e->p2};

			pack_open(o, "parameters");
			for (int32_t k = 0; k < 2; ++k) {
				if (kinds[k] != P_NONE) {
					pack_open(o, param_key(kinds[k]));
					pack_string(o, "type", "options");
					pack_string(o, "name", param_title(kinds[k]));
					pack_open(o, "options");
					pack_options(o, kinds[k]);
					pack_close(o);
					pack_close(o);
				}
			}
			pack_close(o);
		}
		pack_open(o, "alignment");
		pack_number(o, "orderliness", e->orderliness);
		pack_number(o, "morality", e->morality);
		pack_close(o);
		if (e->tag)
			pack_list(o, "tags", e->tag);
		pack_close(o);
	}
	pack_close(o);
	pack_close(o);
	pack_close(o);

	FILE *f = fopen(path, "wb");

	if (f) {
		ok = fwrite(out.text, 1, out.len, f) == out.len;
		fclose(f);
	}
	MemFree(out.text);

	return ok;
}


static void ini_setting(const char *key, const char *def, char *out, size_t size)
{
	IniGetString("crowdcontrol", key, def, out, size);
}

static void wipe(void *p, size_t size)
{
	volatile uint8_t *b = p;

	while (size--)
		*b++ = 0;
}

static void ini_private(const char *key, char *out, size_t size)
{
	char shown[8];

	IniReadLayered(IniDataPath(), IniPath(), "crowdcontrol", key, out, size);
	StrCopy(shown, out[0] ? "(set)" : "");
	RpIniString(shown, sizeof(shown), (uint32_t) strlen(shown));
}

static void config_paths(struct crowd_config *cfg, const char *token_name)
{
	StrPrintf(cfg->user_agent, "OregonTrailII-Modern/%s", GameVersionString());
	FileGetExeDir(cfg->token_file, sizeof(cfg->token_file));
	StrCat(cfg->token_file, token_name);
}

void CrowdControlStart(void)
{
	struct crowd_config cfg;
	char auto_session[8];

	for (int32_t i = 0; i < 64; ++i)
		g_available[i] = -1;
	memset(&cfg, 0, sizeof(cfg));
	ini_setting("server", "wss://pubsub.crowdcontrol.live/", cfg.server, sizeof(cfg.server));
	ini_setting("api", "https://openapi.crowdcontrol.live/", cfg.api, sizeof(cfg.api));
	ini_private("app_id", cfg.app_id, sizeof(cfg.app_id));
	ini_private("secret", cfg.secret, sizeof(cfg.secret));
	if (!cfg.app_id[0]) {
		StrCopy(cfg.app_id, CROWD_CONTROL_APP_ID);
		if (!cfg.secret[0])
			StrCopy(cfg.secret, CROWD_CONTROL_SECRET);
	}
	ini_setting("scopes", "", cfg.scopes, sizeof(cfg.scopes));
	ini_setting("pack_id", "OregonTrailII", cfg.pack_id, sizeof(cfg.pack_id));
	ini_setting("auto_session", "0", auto_session, sizeof(auto_session));
	cfg.auto_session = auto_session[0] == '1';
	config_paths(&cfg, "crowdcontrol.token");
#ifndef OTII_TEST
	if (RpMode() != RP_REPLAY && cfg.server[0] && cfg.api[0] && cfg.app_id[0])
		g_net = CrowdNetCreate(&cfg);
#endif
	wipe(&cfg, sizeof(cfg));
	g_timer = CrowdControlStartTimer();
}

void CrowdControlStop(void)
{
	if (g_timer)
		CrowdControlStopTimer(g_timer);
	g_timer = 0;
	CrowdControlCloseWindow();
	CrowdNetDestroy(&g_net);
}


const char *CrowdControlHandle(const char *payload, char *message, int32_t message_size)
{
	struct outcome o = handle_request(payload);

	StrCopyN(message, (size_t) message_size, o.status == OK ? o.text : o.message);

	return o.status;
}

int32_t CrowdControlTimedCount(void)
{
	return g_timed_count;
}

void CrowdControlAdvanceTimed(int32_t ms)
{
	step_timed(ms);
}

bool CrowdControlConnect(const struct crowd_config *test_cfg, bool sign_in)
{
	struct crowd_config cfg = *test_cfg;

	if (g_net)
		CrowdControlDisconnect();
	cfg.auto_session = true;
	cfg.no_browser = true;
	config_paths(&cfg, "crowdcontrol_test.token");
	if (sign_in)
		FileDelete(cfg.token_file);
	for (int32_t i = 0; i < 64; ++i)
		g_available[i] = -1;
	g_net = CrowdNetCreate(&cfg);
	wipe(&cfg, sizeof(cfg));
	if (sign_in)
		CrowdNetSignIn(g_net);

	return g_net != NULL;
}

int32_t CrowdControlState(char *text, int32_t size)
{
	return CrowdNetState(g_net, text, size);
}

void CrowdControlDisconnect(void)
{
	CrowdNetDestroy(&g_net);
}

void CrowdControlSetQuiet(bool quiet)
{
	g_quiet = quiet;
}

bool CrowdControlPackVariables(char *out, int32_t size)
{
	return pack_variables(out, size);
}

void CrowdControlResendVariables(void)
{
	g_sent_variables[0] = 0;
	send_variables();
}

const char *CrowdControlItemKey(int32_t index, const char **give_effect)
{
	static const struct option *const groups[] = {FIREARMS, AMMO, MEDICINE, CLOTHING, SPARES};
	static const char *const effects[] = {"give_firearm", "give_ammo", "give_medicine", "give_clothing",
		"give_spare_part"};

	for (int32_t g = 0; g < 5; ++g) {
		for (int32_t i = 0; groups[g][i].key; ++i) {
			if (index-- == 0) {
				*give_effect = effects[g];
				return groups[g][i].key;
			}
		}
	}

	return NULL;
}
