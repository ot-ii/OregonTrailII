#include "debugscreen.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../game/ailment.h"
#include "../app/app.h"
#include "../app/autosave.h"
#include "../base/cstr.h"
#include "../base/date.h"
#include "../game/game.h"
#include "../game/invitem.h"
#include "../game/invlist.h"
#include "../base/list.h"
#include "../screens/mainui.h"
#include "../game/member.h"
#include "../base/memory.h"
#include "../game/msg.h"
#include "../game/msgcontroller.h"
#include "../game/msgqueue.h"
#include "../trail/node.h"
#include "../app/otiiapp.h"
#include "../game/party.h"
#include "../base/rng.h"
#include "twitchcontrol.h"
#include "twitchirc.h"
#include "../app/version.h"
#include "../game/weather.h"

struct text {
	char *buf;
	size_t len;
	size_t cap;
};

static const char *const k_ailment[48] = {
	"(none)", "Accidental Gunshot", "Alkali Sickness", "Animal Bite", "Animal Mauling", "Bad Cold", "Beriberi",
	"Broken Arm", "Broken Leg", "Broken Foot", "Broken Hip", "Broken Wrist", "Burns", "Cholera", "Concussion",
	"Consumption", "Cuts and Abrasions", "Diphtheria", "Drowning", "Dysentery", "Food Poisoning", "Freezing",
	"Frostbite", "Gangrene", "Infection", "Internal Injuries", "Lockjaw", "Malaria", "Measles", "Mountain Fever",
	"Pneumonia", "Rabies", "Scarlet Fever", "Scurvy", "Smallpox", "Snakebite", "Sprained Wrist", "Sprained Elbow",
	"Sprained Knee", "Sprained Ankle", "Sprained Shoulder", "Starvation", "The Grippe", "Thirst", "Typhoid Fever",
	"Typhus", "Water Poisoning", "Wound"
};
static const char *const k_stage[4] = {"finished", "incubation", "ACUTE", "recovery"};
static const char *const k_treat[30] = {
	"continue", "rest", "increase rations", "clean and dress", "antiseptic", "astringent", "harmful remedy",
	"fluids", "peppermint", "laudanum", "slow down", "go faster", "vinegar", "olive oil", "camphor", "exercise",
	"magnesia", "lecithin", "quinine", "sassafras", "aloe", "control fever", "Duffy's Elixir", "Dover's Powder",
	"warm gradually", "warm quickly", "amputate", "sulfur", "tourniquet", "flour or grease"
};
static const char *const k_skill[13] = {
	"Medical", "Riverwork", "Sharpshooting", "Blacksmithing", "Carpentry", "Farming", "Tracking", "Botany",
	"Commerce", "Cooking", "Musical", "Sewing", "Spanish"
};
static const char *const k_occup[25] = {
	"banker", "doctor", "merchant", "pharmacist", "wainwright", "gunsmith", "mason", "blacksmith", "wheelwright",
	"carpenter", "saddlemaker", "brickmaker", "prospector", "trapper", "surveyor", "shoemaker", "journalist",
	"printer", "butcher", "baker", "tailor", "farmer", "pastor", "artist", "teacher"
};
static const char *const k_cat[28] = {
	"No food", "No wagon", "No draft animals", "Person near death", "No progress", "Low food", "No grass",
	"No water", "Alkali water", "Bad water", "Muddy trail", "Deep sand", "Someone's sick", "No meat", "No fruit",
	"No vegetables", "Desert", "Rough trail", "Snowing", "Rainy", "Poor health", "Morale low (26-40)",
	"(flag 22: travel state 2)", "Resting", "Morale very low (<=25)", "Morale fair (41-50)", "Morale high (>80)",
	"Slow progress (<3 mi)"
};
static const char *const k_terrain[8] = {"normal", "sand", "uphill", "downhill", "rough", "'m'", "raft", "desert"};

static const char *pick(const char *const *table, int32_t count, int32_t index)
{
	return (index >= 0 && index < count) ? table[index] : "?";
}

static void put(struct text *s, const char *str)
{
	size_t n = strlen(str);

	if (s->len + n + 1 > s->cap) {
		size_t cap = s->cap ? s->cap : 8192;
		while (s->len + n + 1 > cap)
			cap *= 2;
		s->buf = MemRealloc(s->buf, cap, 1);
		s->cap = cap;
	}
	memcpy(s->buf + s->len, str, n + 1);
	s->len += n;
}

static void add(struct text *s, const char *fmt, ...)
{
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	buf[sizeof(buf) - 1] = 0;
	put(s, buf);
}

static const char *cstr_text(const struct cstr *s)
{
	return s ? CStrGet(s) : "";
}

static const char *health_word(int32_t band)
{
	static const char *const words[5] = {"bad", "poor", "fair", "OK", "good"};
	return pick(words, 5, band);
}

static const char *member_state(int32_t state)
{
	return state == MEMBER_ALIVE ? "alive" : state == MEMBER_MISSING ? "MISSING" : "DEAD";
}

static void add_inventory(struct text *s, const char *title, struct inv_list *list)
{
	struct list_node *node;
	int32_t n = 0;

	add(s, "  %s:", title);
	for (node = list ? list->base.head : NULL; node; node = node->next) {
		struct inv_item *item = InvItemFromNode(node);
		add(s, "%s %s x%" PRId32, n++ ? "," : "", cstr_text(InvItemGetNameCStr(item)), InvItemGetCount(item, false));
		if (n % 4 == 0)
			put(s, "\r\n   ");
	}
	put(s, n ? "\r\n" : " (none)\r\n");
}

static void add_member(struct text *s, struct member *m, int32_t index)
{
	bool any = false;
	int32_t i, count;

	add(s, "  [%" PRId32 "] %-14s age %-3" PRId32 " %-7s health %4" PRId32 " / %-4" PRId32 " (%s)  stamina %" PRId32
		"  collapse-at %" PRId32 "%s\r\n",
		index, cstr_text(m->name), m->age, member_state(m->state), m->health, m->max_health,
		health_word(m->health_band), m->stamina, m->collapse_threshold, index == 0 ? "  [leader]" : "");
	add(s, "      immune (had it): %s%s%s%s%s%s%s\r\n",
		m->had_ailment_42 ? "Grippe " : "", m->had_ailment_28 ? "Measles " : "",
		m->had_ailment_29 ? "Mountain-Fever " : "", m->had_ailment_17 ? "Diphtheria " : "",
		m->had_ailment_34 ? "Smallpox " : "", m->had_ailment_32 ? "Scarlet-Fever " : "",
		(m->had_ailment_42 | m->had_ailment_28 | m->had_ailment_29 | m->had_ailment_17 | m->had_ailment_34 |
		 m->had_ailment_32) ? "" : "(none)");
	if (m->pending_ailment)
		add(s, "      pending: %s, starting %s\r\n", pick(k_ailment, 48, m->pending_ailment),
			pick(k_stage, 4, m->pending_stage));
	count = m->nodes ? OblistCount(m->nodes) : 0;
	for (i = 0; i < count; i++) {
		struct ailment *a = OblistDataAt(m->nodes, i, false);
		int32_t id;
		if (!a || !a->def)
			continue;
		id = a->def->id;
		any = true;
		add(s, "      * %-18s %-10s damage %-4" PRId32 " days-left %-3" PRId32 " day-in-stage %-3" PRId32
			" %s%s last-treatment: %s\r\n",
			pick(k_ailment, 48, id), pick(k_stage, 4, a->stage), a->severity, a->days_left, a->days_in_stage,
			a->is_known ? "" : "(unidentified) ", a->worsened ? "(worsened)" : "",
			pick(k_treat, 30, a->last_treatment));
		if (a->followup_chance_incubation || a->followup_chance_acute)
			add(s, "        follow-up/day: %s %" PRId32 "/1000 (incubation), %s %" PRId32 "/1000 (acute)\r\n",
				pick(k_ailment, 48, a->followup_id_incubation), a->followup_chance_incubation,
				pick(k_ailment, 48, a->followup_id_acute), a->followup_chance_acute);
		add(s, "        category %" PRId32 ", flags 0x%" PRIX32 ", damage %" PRId32 "-%" PRId32 ", identify %" PRId32 "%%\r\n",
			a->def->category, (uint32_t) (uint16_t) a->def->flags, a->def->severity_min, a->def->severity_max,
			a->def->identify_chance);
	}
	if (!any)
		put(s, "      no ailments\r\n");
}

static void build_text(struct text *s)
{
	struct game *g = GetGameObj();
	struct otii_app *app = (struct otii_app *) g_app;
	int32_t i;

	s->len = 0;
	put(s, "");
	add(s, "OREGON TRAIL II %s -- DEBUG STATE   (refreshes every second; read-only)\r\n\r\n", GameVersionString());
	if (app)
		add(s, "App state %" PRId32 ", previous %" PRId32 "\r\n", app->app_state, app->prev_app_state);
#ifdef OTII_TWITCH
	{
		static char recent[8][640];
		static char picks[5][256];
		int32_t recent_n = TwitchControlRecentChat(recent, 8);
		int32_t picks_n = TwitchControlLastPicks(picks, 5);
		struct twitch_message m;
		char status[160];
		int32_t k;

		TwitchStatus(status, sizeof(status));
		put(s, "== TWITCH ==\r\n");
		add(s, "  TWITCH CHAT CONTROL   (%s)\r\n", status);
		if (picks_n) {
			put(s, "  last picks:\r\n");
			for (k = 0; k < picks_n; ++k)
				add(s, "    %s\r\n", picks[k]);
		}

		add(s, "  party waiting list (%" PRId32 "):", TwitchPartyPending());
		for (k = 0; TwitchPartyPeek(k, &m); ++k)
			add(s, "%s %s (\"%s\")", k ? "," : "", m.display_name[0] ? m.display_name : m.login, m.text);

		put(s, "\r\n");
		for (k = recent_n - 1; k >= 0; --k)
			add(s, "  %s\r\n", recent[k]);
	}
#endif
	if (!g || !g->party) {
		put(s, "No game in progress.\r\n");
		return;
	}

	struct party *p = g->party;
	put(s, "== GAME ==\r\n");
	add(s, "  Random seed %" PRIu32 ", rand() calls since seeding %" PRIu64 "\r\n", RngSeed(), RngCalls());
	add(s, "  Autosave %s: %s\r\n", AutosaveEnabled() ? "on" : "off", AutosaveStatus());
	if (g->date)
		add(s, "  Date %" PRId32 "/%" PRId32 "/%" PRId32, g->date->month, g->date->day, g->date->year);
	if (g->start_date)
		add(s, "   started %" PRId32 "/%" PRId32 "/%" PRId32, g->start_date->month, g->start_date->day,
			g->start_date->year);
	add(s, "   day %" PRId32 " on the trail\r\n", g->days_traveled);
	add(s, "  Level %" PRId32 "  destination %" PRId32 "  start town %" PRId32 "  pace %" PRId32
		" (0=8h 1=10h 2=exhausting)  travel state %" PRId32 " (0 moving 1 resting 3 stopped)\r\n",
		g->player_level, g->destination, g->start_town, g->travel_pace, g->travel_state);
	add(s, "  Miles: total %" PRId32 ", today %" PRId32 ", to go %" PRId32 "   daily-mod %" PRId32 "%%   train %" PRId32
		" wagons / %" PRId32 " people   hired rafters %" PRId32 "\r\n",
		g->total_miles, g->miles_today, g->miles_to_go, g->daily_mod, g->train_wagons, g->train_people,
		(int32_t) (uint8_t) g->hired_rafters);
	if (g->cur_node) {
		int32_t kind = NodeBaseGetId(g->cur_node);
		add(s, "  Node %" PRId32 " (kind %" PRId32 ")", g->cur_node_id, kind);
		if (kind == NODE_KIND_TRAIL)
			add(s, "  terrain %s", pick(k_terrain, 8, NodeGetTerrain((struct node *) g->cur_node)));
		add(s, "   next %" PRId32 "  prev %" PRId32 "  end %" PRId32 "\r\n", g->next_node_id, g->prev_node_id,
			g->end_node_id);
	}

	put(s, "\r\n== PARTY ==\r\n");
	add(s, "  Cash $%.2f   morale %" PRId32 "   rations %" PRId32 " (0 bare 1 meager 2 filling)   occupation %s"
		"   pull mode %" PRId32 " (0/2 easy, 1/3 strained, 4 cannot)\r\n", p->cash, p->reputation, p->ration_level,
		pick(k_occup, 25, p->occupation), p->pull_mode);
	add(s, "  Food %" PRId32 " lb%s   days starving %" PRId32 "   scurvy risk %" PRId32 "   beriberi risk %" PRId32
		"   daily health drain %" PRId32 "\r\n",
		PartyGetFoodPounds(p), PartyHasPlentyOfFood(p) ? " (plenty)" : " (NOT plenty)", p->days_starving,
		p->scurvy_risk, p->beriberi_risk, p->daily_health_drain);
	put(s, "  Skills:");
	for (i = 0; i < 13; i++)
		if (p->skills[i])
			add(s, " %s", k_skill[i]);
	add(s, "   (unused points %" PRId32 ")\r\n", p->unused_skill_points);
	for (i = 0; i < p->member_count && i < 6; i++)
		if (p->members[i])
			add_member(s, p->members[i], i);

	if (g->weather) {
		struct weather *w = g->weather;
		put(s, "\r\n== WEATHER ==\r\n");
		add(s, "  Region %" PRId32 "  month %" PRId32 "  today high %" PRId32 " low %" PRId32 " precip %" PRId32
			"   snow depth %" PRId32 " (fell %" PRId32 ")\r\n", w->region, w->month + 1, w->day[0].high,
			w->day[0].low, w->day[0].precip, w->snow_depth, w->snow_fall);
		add(s, "  Month-to-date days %" PRId32 ", precip so far %" PRId32 "\r\n", w->day_count,
			WeatherSumPrecip(w, w->day_count));
	}

	struct msg_controller *mc = GetAuxMsgqueue();
	if (mc) {
		put(s, "\r\n== EVENT STATE ==\r\n");
		add(s, "  muddy %" PRId32 "  timbers armed %" PRId32 "  rocks armed %" PRId32 "  prairie-fire cooldown %" PRId32 "  mosquito cooldown %"
			PRId32 "  dust cooldown %" PRId32 "\r\n", (int32_t) mc->trail_muddy, (int32_t) mc->fallen_timbers_armed,
			(int32_t) mc->fallen_rocks_armed, mc->prairie_fire_cooldown, mc->mosquito_cooldown, mc->wagon_dust_cooldown);
		add(s, "  dry days %" PRId32 "  thirst warned %" PRId32 "  alkali exposure %" PRId32 "  low-morale warned %" PRId32
			"  days since dismissal %" PRId32 "\r\n", mc->dry_days, (int32_t) mc->thirst_warned, mc->alkali_exposure,
			(int32_t) mc->low_morale_warned, mc->days_since_dismissal);
	}
	struct msg_queue *q = GetMessageQueue();
	if (q) {
		struct list_node *node;
		int32_t n = 0;
		put(s, "  Event queue:");
		for (node = q->base.head; node; node = node->next) {
			add(s, " %" PRId32, MsgFromNode(node)->number_index);
			n++;
		}
		put(s, n ? "   (STRLST 11000+id)\r\n" : " (empty)\r\n");
	}
	{
		bool any = false;
		put(s, "  Trail warnings:");
		for (i = 0; i < CAT_COUNT; i++)
			if (g_main_ui_cat_flags[i]) {
				add(s, "%s %s", any ? "," : "", k_cat[i]);
				any = true;
			}
		put(s, any ? "\r\n" : " (none)\r\n");
	}

	put(s, "\r\n== INVENTORY ==\r\n");
	add_inventory(s, "Wagons", &p->wagons);
	add_inventory(s, "Hitched animals", &p->animals);
	add_inventory(s, "Spare animals", &p->spare_animals);
	add_inventory(s, "Supplies", &p->inventory);
}

const char *DebugScreenText(void)
{
	static struct text text;

	build_text(&text);
	return text.buf;
}
