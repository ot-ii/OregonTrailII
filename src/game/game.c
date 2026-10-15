#include "game.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ailment.h"
#include "../app/app.h"
#include "../base/byteorder.h"
#include "../draw/canvas.h"
#include "../ui/clipview.h"
#include "../ui/control.h"
#include "../dialogs/convdlg.h"
#include "../base/cstr.h"
#include "../base/date.h"
#include "../data/docfile.h"
#include "../draw/drawable.h"
#include "../screens/endframe.h"
#include "endgame.h"
#include "../app/event.h"
#include "../ui/framewin.h"
#include "gamecmd.h"
#include "../data/gfxdev.h"
#include "../screens/healthwin.h"
#include "invitem.h"
#include "invlist.h"
#include "journal.h"
#include "../app/license.h"
#include "../screens/mainui.h"
#include "../trail/mapview.h"
#include "../media/mcivideo.h"
#include "../draw/mediabase.h"
#include "member.h"
#include "../base/memory.h"
#include "../trail/meter.h"
#include "../dialogs/modaldialog.h"
#include "msg.h"
#include "../dialogs/msgbox.h"
#include "msgcontroller.h"
#include "msgqueue.h"
#include "newgamescenario.h"
#include "../trail/node.h"
#include "../draw/objbase.h"
#include "occupmgr.h"
#include "../app/otiiapp.h"
#include "../draw/palette.h"
#include "party.h"
#include "../dialogs/pickerdlg.h"
#include "../ui/pictctrl.h"
#include "../draw/pictserver.h"
#include "../ui/picview.h"
#include "../dialogs/prologuedlg.h"
#include "../dialogs/quickstartdlg.h"
#include "../trail/raftframe.h"
#include "../draw/region.h"
#include "../app/replay.h"
#include "../dialogs/restdlg.h"
#include "../trail/riverrules.h"
#include "../base/rng.h"
#include "../data/saveformat.h"
#include "../ui/scaler.h"
#include "../screens/sceneassets.h"
#include "../screens/sceneframe.h"
#include "../draw/server.h"
#include "../app/settings.h"
#include "../draw/shape.h"
#include "../media/sndstream.h"
#include "../base/str.h"
#include "../media/strmdsnd.h"
#include "../data/strrsrc.h"
#include "../draw/textlabel.h"
#include "../ui/textview.h"
#include "../dialogs/tradedlg.h"
#include "../ui/view.h"
#include "weather.h"
#include "../ui/window.h"
#include "../draw/windc.h"

static bool game_check_event_condition(int32_t condition, struct party *caller_party);
static int32_t node_speaker_conv(struct node_base *node, int32_t index);

static bool g_window_dirty = true;
static int32_t g_cached_ration = 9999;
static int32_t g_cached_pace = -1;
static int32_t g_cached_min_health = HEALTH_GOOD;
static int32_t g_cached_food_consume = 0;
static int32_t g_idle_interval = 6;
static int32_t g_travel_health_scratch;
static int32_t g_day_length;

int32_t g_item_chicken;
static struct cstr *g_str_egg;
int32_t g_item_egg;
int32_t g_item_milk_cow;
static struct cstr *g_str_milk;
int32_t g_item_milk;
static struct cstr *g_str_butter;
int32_t g_item_butter_churn;
int32_t g_item_conestoga;
int32_t g_item_rifle;
int32_t g_item_pistol;
int32_t g_item_shotgun;
static bool g_cond_flag_bc;
static bool g_cond_flag_c0;

static int32_t g_ferry_wait_days = 0;

static uint8_t g_logged_100_miles;
static uint8_t g_logged_500_miles;
static uint8_t g_logged_1000_miles;
static uint8_t g_logged_100_days;
static uint8_t g_logged_200_days;

static struct view *g_active_scene;
static int32_t g_crossing_outcome;
static int32_t g_river_method;

static struct canvas *g_talk_canvas = NULL;
static int32_t g_conv_npc_idx = 0;
static int32_t g_node_scene_pic = 0;
static struct clip_view *g_crossing_view;

static struct text_view *g_trail_text;
static bool g_ferry_wait_pending = false;

static const int32_t g_conv_ids[16] = {
	675, 891, 892, 893, 894, 895, 896, 85, 759, 760, 762, 765,
	770, 774, 757, 776,
};
static const int32_t g_conv_id_max = 0xF;

static struct otii_app *get_app(void)
{
	return (struct otii_app *) g_app;
}

static void redraw_parent(void)
{
	WINDOW_OPS(&g_parent_win->base)->Draw(&g_parent_win->base, NULL);
	WindowValidate(&g_parent_win->base, NULL);
}

static void add_journal_line(struct game *self, struct cstr *line)
{
	JournalSetDate(&get_app()->journal, self->date);
	JournalAddLine(&get_app()->journal, line);
}

static void enter_state(struct otii_app *app, int32_t st)
{
	if (app->app_state != st) {
		app->saved_state = app->prev_app_state;
		app->prev_app_state = app->app_state;
		app->app_state = st;
	}

	struct app *base = (struct app *) app;
	APP_OPS(base)->UpdateEditMenuState(base, 0);
}

static void adjust_reputation(struct party *p, int32_t d)
{
	p->reputation += d;
	if (p->reputation > 100)
		p->reputation = 100;
	if (p->reputation < -50)
		p->reputation = -50;
}

static void apply_scenery_switch(struct game *g, struct node_base *n)
{
	if (n->has_scenery_switch)
		g->scenery_switch = n->scenery_switch_on;
}

static void reset_thirst(struct msg_controller *q)
{
	q->dry_days = 0;
	q->thirst_warned = false;
}

static void find_prev_node(struct game *g)
{
	for (int32_t i = 0; g->map_view->node_count > i; ++i) {
		if (g->map_view->point_node[i] == g->prev_node_id) {
			g->path_nodes[0] = i;
			return;
		}
	}

	g->path_nodes[0] = -1;
}

static int32_t map_find_cur(struct game *g)
{
	for (int32_t i = 0; g->map_view->node_count > i; ++i) {
		if (g->map_view->point_node[i] == g->cur_node_id)
			return i;
	}

	return -1;
}

static void path_append(struct game *g, int32_t v)
{
	if (g->path_len >= 0 && g->path_len < GAME_PATH_MAX)
		g->path_nodes[g->path_len] = v;
	++g->path_len;
}

static void dispatch_cmd(int32_t command)
{
	DispatchGlobalEvent(CmdEventCreate(command));
}

static void replace_node_view(struct game *self, struct view **out_view)
{
	struct event remove_ev = EventMake(EVCLS_VIEW, -1, -1);
	struct view *view = VIEW_OPS(self->doc)->RemoveChild(self->doc, &remove_ev);

	if (view)
		ObjectDelete(view);
	struct rect bounds = VIEW_OPS(self->doc)->GetBounds(self->doc);
	view = ViewCreateId(-2, &bounds);
	VIEW_OPS(self->doc)->AddChild(self->doc, view);
	*out_view = view;
}

static struct node_base *new_lnode(int32_t node_id)
{
	return &LNodeCreate(node_id, true)->base;
}

static struct node_base *new_node(int32_t node_id)
{
	return &NodeCreate(node_id, true)->base;
}

static void delete_node(struct node_base **node)
{
	NodeBaseDestroy(node);
}

static int32_t node_terrain(struct game *self)
{
	return NodeGetTerrain((struct node *) self->cur_node);
}

static void sound_server_pause(void)
{
	if (g_sound_server)
		SND_STREAM_OPS(g_sound_server)->Pause(&g_sound_server->base);
}

static void sound_server_resume(void)
{
	if (g_sound_server)
		SND_STREAM_OPS(g_sound_server)->Resume(&g_sound_server->base);
}

static void kill_member(struct member *m)
{
	m->state = MEMBER_DEAD;
	if (!m->state)
		m->health = 0;
}

static void style_choice(struct pict_ctrl *ctrl)
{
	((struct text_label *) ctrl->base.data_src)->color_mode2 = 82;
}

static void rect_offset(struct rect *r, int32_t dh, int32_t dv)
{
	r->left += dh;
	r->right += dh;
	r->top += dv;
	r->bottom += dv;
}

static void rect_offset_v(struct rect *r, int32_t dv)
{
	r->top += dv;
	r->bottom += dv;
}

static void rect_from_size(struct rect *dst, const struct point *sz)
{
	dst->top = 0;
	dst->left = 0;
	dst->right = sz->h;
	dst->bottom = sz->v;
}

static bool greenhorn_gate(struct game *g)
{
	bool gate = !g->player_level;

	if (gate && GameIsEventAllowed(g, NODEFEAT_NO_GREENHORN_ALLOWANCE))
		gate = false;
	return gate;
}

static void *create_virtual(int32_t key)
{
	return ServerCreateVirtual((struct server *) g_pict_server, key, true);
}

static void *create_virtual_packed(int32_t type_id, int32_t index)
{
	return ServerCreateVirtualPacked((struct server *) g_pict_server, type_id, index, true);
}

static void release_drawable(struct drawable *obj)
{
	ReleaseDrawable((struct media_base *) obj, false);
}

static struct pict_ctrl *new_choice(int32_t choice_id, struct event *ev, struct rect *rect, struct cstr *text)
{
	struct point pos = RectTopLeft(rect);

	return PictCtrlCreate(choice_id + 10000, ev, &pos, 901, 0, 0, text, 3, 1, 1);
}

const struct game_ops GAME_OPS_TABLE = {
	{
		{game_destroy},
		evt_handler_is_handled,
		evt_handler_set_handled_for,
		evt_handler_set_handled,
		evt_handler_clear_handled_for,
		evt_handler_clear_handled,
		game_on_event,
		evt_handler_on_idle,
		game_on_idle_spare,
	},
	game_node_action,
	game_build_main_screen,
	game_river_crossing,
	game_run_npc_conv,
	game_execute_command,
};

void GameInit(struct game *self, int32_t game_mode, struct view *doc)
{
	EvtHandlerInit(&self->base, 2, game_mode);
	self->base.base.ops = &GAME_OPS_TABLE.base.base;

	int32_t handlers_o[7];
	handlers_o[0] = GCMD_NODE_CONTINUE;
	handlers_o[1] = CMD_CONTROL_ACTION;
	handlers_o[2] = GCMD_PLAY_SFX;
	handlers_o[3] = GCMD_TALK;
	handlers_o[4] = GCMD_BUILD_MAIN_SCREEN;
	handlers_o[5] = GCMD_CHOICE;
	handlers_o[6] = GCMD_FERRY_WAIT_DONE;
	struct cstr *scratch_m = CStrCreateFrom("chicken");

	for (int32_t slot = 0; slot < 1200; slot++)
		g_slot_flags[slot] = 0;
	struct inv_list spare;
	InvListInit(&spare);

	g_item_chicken = LookupItemId(scratch_m);
	g_str_egg = CStrCreateFrom("egg [from chickens]");
	g_item_egg = LookupItemId(g_str_egg);
	CStrAssignBuf(scratch_m, "milk cow");
	g_item_milk_cow = LookupItemId(scratch_m);
	g_str_milk = CStrCreateFrom("milk [from milk cow]");
	g_item_milk = LookupItemId(g_str_milk);
	g_str_butter = CStrCreateFrom("fresh butter");
	CStrAssignBuf(scratch_m, "butter churn");
	g_item_butter_churn = LookupItemId(scratch_m);
	CStrAssignBuf(scratch_m, "Conestoga wagon");
	g_item_conestoga = LookupItemId(scratch_m);
	CStrAssignBuf(scratch_m, "rifle");
	g_item_rifle = LookupItemId(scratch_m);
	CStrAssignBuf(scratch_m, "pistol");
	g_item_pistol = LookupItemId(scratch_m);
	CStrAssignBuf(scratch_m, "shotgun");
	g_item_shotgun = LookupItemId(scratch_m);

	g_window_dirty = true;
	g_cached_ration = 9999;
	g_cached_pace = -1;
	EvtHandlerBindHandler(&self->base, 7, handlers_o);

	struct assets *data_lib_l = g_text_lib;
	for (int32_t slot = 0; slot < 650; slot++) {
		self->portrait_anchors[slot].v = 0;
		self->portrait_anchors[slot].h = self->portrait_anchors[slot].v;
	}

	struct strrsrc *table_c = StrRsrcLoad(data_lib_l, 977, true);
	int32_t slot_a = 1;
	int32_t lo_i = 0;
	int32_t hi_h = 0;
	int32_t key_a = 0;
	do {
		const char *line_j = StrRsrcGetAt(table_c, slot_a++);
		if (!line_j)
			break;
		(void) sscanf(line_j, "%" SCNd32 " %" SCNd32 " %" SCNd32, &lo_i, &hi_h, &key_a);
		if (lo_i > 0 && lo_i < 650) {
			self->portrait_anchors[lo_i].h = hi_h;
			self->portrait_anchors[lo_i].v = key_a;
		}
	} while (lo_i > 0);
	StrRsrcDestroy(&table_c);

	self->end_score = NULL;
	self->mile_frac = 0;
	self->day_tick = 0;
	self->travel_state = TRAVEL_STOPPED;
	self->region = RegionCreate();
	self->doc = doc;
	self->map_view = NULL;
	self->node_names = StrRsrcLoad(data_lib_l, 900, true);
	StrRsrcRemapHighChars(self->node_names, 0);
	self->arrival_journal_lines = StrRsrcLoad(data_lib_l, 970, true);
	StrRsrcRemapHighChars(self->arrival_journal_lines, 0);
	self->game_mode = game_mode;
	self->cur_node = NULL;
	self->days_traveled = 0;
	self->event_days = self->days_traveled;
	self->miles_today = 0;
	self->total_miles = 0;
	self->miles_to_go = 0;
	self->prev_node_id = 0;
	self->cur_node_id = 0;
	self->next_node_id = 0;
	self->map_id = -1;
	self->shown_map_id = 0;
	self->path_len = 0;
	self->travel_pace = PACE_8_HOURS;
	self->travel_mod = 100;
	self->daily_mod = 100;
	self->train_wagons = -1;
	self->train_people = -1;
	self->party = NULL;
	self->weather = WeatherCreate();
	self->start_date = NULL;
	self->date = self->start_date;
	self->event_node = NULL;
	self->unused_C1E = 0;
	self->hired_rafters = 0;

	inv_list_destroy(&spare);
	CStrDestroy(&scratch_m);
}

struct game *GameCreate(int32_t game_mode, struct view *doc)
{
	struct game *self = MemAlloc(1, sizeof(struct game));
	GameInit(self, game_mode, doc);

	return self;
}

void GameDestroy(struct game **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void game_destroy(struct object *obj)
{
	struct game *self = (struct game *) obj;

	if (g_health_window)
		ObjectDelete(g_health_window);
	ObjectDelete(self->map_view);
	ObjectDelete(self->region);
	StrRsrcDestroy(&self->node_names);
	StrRsrcDestroy(&self->arrival_journal_lines);
	PartyDestroy(&self->party);
	WeatherDestroy(&self->weather);
	MemFree(self->date);
	MemFree(self->start_date);
	EndGameDestroy(&self->end_score);
	g_msgq_ready = false;
	ReleaseMessageQueues();
	CStrDestroy(&g_str_butter);
	CStrDestroy(&g_str_milk);
	CStrDestroy(&g_str_egg);

	self->base.base.ops = &EVT_HANDLER_OPS_TABLE.base;
	evt_handler_destroy(obj);
}

int32_t GameRest(struct game *self, int32_t hours, int32_t flag)
{
	(void) flag;

	int32_t cadence_a = 18;
	int32_t saved_state_d = self->travel_state;

	if (get_app()->ui)
		MainUiShowTravelPanel(get_app()->ui);
	if (!hours) {
		struct rest_dlg dlg_a;
		RestDlgInit(&dlg_a);
		ModalDialogRun((struct modal_dialog *) &dlg_a, (intptr_t) &hours, &g_parent_win->base);
		rest_dlg_destroy((struct object *) &dlg_a);
	}
	if (hours) {
		self->travel_state = TRAVEL_RESTING;
		int32_t days_l = hours / 24;
		struct main_ui *main_ui_c = get_app()->ui;
		if (hours > 24 && main_ui_c) {
			struct view *v = (struct view *) main_ui_c;
			struct rect bounds = VIEW_OPS(v)->GetBounds(v);
			VIEW_OPS(v)->DrawChildren(v, (struct screen_dc *) g_parent_win->base.windc, &bounds);
		}
		switch (self->travel_pace) {
			case PACE_8_HOURS:
				cadence_a = 13;
				break;
			case PACE_10_HOURS:
				cadence_a = 18;
				break;
			case PACE_EXHAUSTING:
				cadence_a = 24;
				break;
		}
		int32_t total_g = cadence_a * days_l;
		int32_t count = 1;
		for (int32_t hour_c = 0; total_g > hour_c; ++hour_c) {
			GameTravelTick(self, false);
			if (!--count) {
				count = cadence_a;
				if (hours > 24 && main_ui_c) {
					MainUiRefresh(main_ui_c);
					struct view *v = (struct view *) main_ui_c;
					struct rect bounds = VIEW_OPS(v)->GetBounds(v);
					VIEW_OPS(v)->DrawChildren(v, (struct screen_dc *) g_parent_win->base.windc, &bounds);
					ScalerPresent(g_parent_win->base.hwnd, false);
					int32_t deadline_j = GameTicks() + 60;
					do {
					} while (GameTicks() < deadline_j);
				}
			}
		}
		total_g = 2 * cadence_a * (hours % 24) / 48;
		for (int32_t hour_c = 0; total_g > hour_c; ++hour_c)
			GameTravelTick(self, false);
		self->travel_state = saved_state_d;
	}
	return hours;
}

static void log_two(struct game *self, uint8_t *logged, const char *a, const char *b)
{
	struct cstr *s = NULL;

	*logged = 1;
	switch (RandRangeAvg(0, 1, 1)) {
	case 0:
		s = CStrCreateFrom(a);
		break;
	case 1:
		s = CStrCreateFrom(b);
		break;
	}
	add_journal_line(self, s);
}

void GameTravelTick(struct game *self, bool flag)
{
	struct otii_app *app_f = get_app();
	int32_t food_m = 0;
	int32_t pace_i = SettingsGetSpeed(app_f->settings);
	struct inv_list *inventory_p = &self->party->inventory;
	int32_t start_frac_g = self->mile_frac;
	int32_t roll_flag_l;
	struct inv_item *item_p;

	if (MainUiGetCatFlag(CAT_RESTING)) {
		switch (g_cached_min_health) {
		case HEALTH_OK:
		case HEALTH_GOOD:
			GameUpdateMusic(self, CUE_RESTING_GOOD);
			break;
		case HEALTH_FAIR:
			GameUpdateMusic(self, CUE_RESTING_FAIR);
			break;
		case HEALTH_BAD:
		case HEALTH_POOR:
			GameUpdateMusic(self, CUE_RESTING_POOR);
			break;
		}
	}
	if (!self->map_view)
		flag = false;
	if (flag)
		roll_flag_l = self->travel_state;
	else
		roll_flag_l = 3;
	++self->day_tick;
	if (self->travel_pace != g_cached_ration || pace_i != g_cached_pace) {
		g_cached_ration = self->travel_pace;
		g_cached_pace = pace_i;
		switch (self->travel_pace) {
		case PACE_8_HOURS:
			switch (pace_i) {
			case SIM_SPEED_SLOW:
				g_idle_interval = 24;
				break;
			case SIM_SPEED_MEDIUM:
				g_idle_interval = 8;
				break;
			case SIM_SPEED_FAST:
				g_idle_interval = 3;
				break;
			default:
				g_idle_interval = 8;
				break;
			}
			g_day_length = 14;
			break;
		case PACE_10_HOURS:
			switch (pace_i) {
			case SIM_SPEED_SLOW:
				g_idle_interval = 18;
				break;
			case SIM_SPEED_MEDIUM:
				g_idle_interval = 6;
				break;
			case SIM_SPEED_FAST:
				g_idle_interval = 2;
				break;
			default:
				g_idle_interval = 6;
				break;
			}
			g_day_length = 19;
			break;
		case PACE_EXHAUSTING:
			switch (pace_i) {
			case SIM_SPEED_SLOW:
				g_idle_interval = 12;
				break;
			case SIM_SPEED_MEDIUM:
				g_idle_interval = 4;
				break;
			case SIM_SPEED_FAST:
				g_idle_interval = 1;
				break;
			default:
				g_idle_interval = 4;
				break;
			}
			g_day_length = 24;
			break;
		}
		EvtHandlerSetIdleOffset(&self->base, false, g_idle_interval);
	}
	if (!roll_flag_l) {
		if (!self->travel_mod) {
			food_m = self->mile_frac + 25;

		} else {
			int32_t health_pct_o = 0;
			int32_t snow_i = self->weather->snow_depth;
			int32_t party_field9c_l = self->party->days_starving;

			if (!g_cached_food_consume) {
				g_cached_min_health = HEALTH_GOOD;
				for (int32_t index_n = 0; PartyMemberCount(self->party, false) > index_n; ++index_n) {
					struct member *member_d = PartyGetMember(self->party, index_n);
					if (member_d->state == MEMBER_ALIVE) {
						g_travel_health_scratch = MemberGetHealthBand(member_d);
						if (g_travel_health_scratch < g_cached_min_health)
							g_cached_min_health = g_travel_health_scratch;
					}
				}
				food_m = PartyCalcPullingMode(self->party);
				if (food_m) {
					if (snow_i) {
						int32_t snow_pct_g;
						if (snow_i > 300)
							snow_pct_g = 25;
						else if (snow_i > 240)
							snow_pct_g = 35;
						else if (snow_i > 180)
							snow_pct_g = 50;
						else if (snow_i > 120)
							snow_pct_g = 75;
						else
							snow_pct_g = 85;
						food_m = snow_pct_g * food_m;
						food_m /= 100;
					}
					if (party_field9c_l) {
						int32_t field9c_pct_k;
						if (party_field9c_l > 14)
							field9c_pct_k = 25;
						else if (party_field9c_l > 7)
							field9c_pct_k = 50;
						else
							field9c_pct_k = 75;
						food_m = field9c_pct_k * food_m;
						food_m /= 100;
					}
					if (g_cached_min_health < HEALTH_OK) {
						switch (g_cached_min_health) {
						case HEALTH_FAIR:
							health_pct_o = 90;
							break;
						case HEALTH_POOR:
							health_pct_o = 70;
							break;
						case HEALTH_BAD:
							health_pct_o = 60;
							break;
						}
						food_m = health_pct_o * food_m;
						food_m /= 100;
					}
					if (self->daily_mod != 100) {
						food_m = self->daily_mod * food_m;
						food_m /= 100;
					}
				}
				g_cached_food_consume = food_m;

			} else {
				food_m = g_cached_food_consume;
			}
			int32_t daily_variance_pct_d = RandRangeAvg(80, 125, 2);
			food_m = daily_variance_pct_d * food_m;
			food_m /= 100;
			if (self->travel_mod != 100) {
				food_m = self->travel_mod * food_m;
				food_m /= 100;
			}
			food_m = self->mile_frac + food_m;
		}
		self->mile_frac = food_m % 20;
		food_m /= 20;
		if (self->miles_to_go < food_m)
			food_m = self->miles_to_go;
		self->miles_today += food_m;
		if (self->miles_to_go) {
			self->total_miles += food_m;
			self->miles_to_go -= food_m;
		}
		if (self->map_view)
			MapViewUpdateMarker(self->map_view, MAP_MARKER_REDRAW);
	}
	if (self->day_tick >= g_day_length) {
		g_cached_food_consume = 0;
		PartyDailyHealthUpdate(self->party, 24);
		DateSetDay(self->date, self->date->day + 1);
		++self->days_traveled;
		if (self->days_traveled > 400)
			self->party->daily_health_drain += self->days_traveled - 400;
		++self->event_days;
		if (self->days_traveled % 7 == 0) {
			if (self->party->skills[SKILL_COOKING])
				adjust_reputation(self->party, 1);
			if (self->party->skills[SKILL_MUSICAL])
				adjust_reputation(self->party, 2);
			if (self->party->occupation == OCCUP_PASTOR)
				adjust_reputation(self->party, 2);
		}
		if (MainUiGetCatFlag(CAT_LOW_FOOD))
			adjust_reputation(self->party, -2);
		if (MainUiGetCatFlag(CAT_NO_FOOD))
			adjust_reputation(self->party, -8);
		if (MainUiGetCatFlag(CAT_NO_FRUIT))
			adjust_reputation(self->party, -1);
		if (MainUiGetCatFlag(CAT_NO_WATER))
			adjust_reputation(self->party, -2);
		if (MainUiGetCatFlag(CAT_POOR_HEALTH))
			adjust_reputation(self->party, -2);
		if (self->days_traveled % 30 == 0) {
			int32_t chance_p = 50;
			if (self->party->skills[SKILL_SEWING])
				chance_p /= 2;
			if (InvListHasGroup(inventory_p, IGRP_SEWING))
				chance_p /= 2;
			if (RandRangeAvg(0, 99, 1) < chance_p) {
				int32_t out_d = 0;
				struct inv_item **table_p = InvListCollectGroup(inventory_p, IGRP_CLOTHING, &out_d, 0);
				if (out_d) {
					int32_t half2_l = RandRangeAvg(0, out_d - 1, 1);
					InvItemRemoveUnits(table_p[half2_l], 1);
				}
				MemFree(table_p);
			}
		}
		if (!MainUiGetCatFlag(CAT_NO_WATER)) {
			item_p = InvListFindByType(inventory_p, g_item_chicken);
			if (item_p) {
				int32_t half_a = InvItemGetCount(item_p, false) / 2;
				int32_t qty_b = InvItemGetCount(item_p, false) - half_a + RandRangeAvg(0, half_a, 1);
				struct inv_item *node_m = InvItemCreate(g_str_egg, qty_b, 0.0);
				ListInsertTail(&inventory_p->base, &node_m->node);
				if (RandRangeAvg(0, 99, 1) < 3) {
					struct cstr *str1_e = NULL;
					InvItemRemoveUnits(item_p, 1);
					switch (RandRangeAvg(0, 1, 1)) {
					case 0:
						str1_e = CStrCreateFrom("A chicken died today.");
						break;
					case 1:
						str1_e = CStrCreateFrom("We lost a chicken today.");
						break;
					}
					add_journal_line(self, str1_e);
				}
			}
		}
		if (!self->travel_pace && !MainUiGetCatFlag(CAT_NO_WATER) && RandRangeAvg(0, 500, 2) > self->days_traveled) {
			item_p = InvListFindByType(inventory_p, g_item_milk_cow);
			if (item_p) {
				int32_t count_g;
				if (self->days_traveled < 100)
					count_g = (RandRangeAvg(1, 3, 1) + 0) * InvItemGetCount(item_p, false);
				else
					count_g = InvItemGetCount(item_p, false);
				struct inv_item *node2_i = InvItemCreate(g_str_milk, count_g, 0.0);
				ListInsertTail(&inventory_p->base, &node2_i->node);
				if (RandRangeAvg(0, 99, 1) < 2) {
					struct cstr *str2_d = NULL;
					InvItemRemoveUnits(item_p, 1);
					switch (RandRangeAvg(0, 1, 1)) {
					case 0:
						str2_d = CStrCreateFrom("A milk cow died today.");
						break;
					case 1:
						str2_d = CStrCreateFrom("We lost a milk cow today.");
						break;
					}
					add_journal_line(self, str2_d);
				}
			}
		}
		if (!roll_flag_l) {
			MainUiClearCatFlag(CAT_NO_PROGRESS);
			MainUiClearCatFlag(CAT_SLOW_PROGRESS);
			if (!self->miles_today && self->mile_frac == start_frac_g)
				MainUiSetCatFlag(CAT_NO_PROGRESS);
			else if (self->miles_today < 3)
				MainUiSetCatFlag(CAT_SLOW_PROGRESS);
		}
		self->miles_today = 0;
		WeatherSetMonth(self->weather, self->date->month);
		WeatherSimulateDay(self->weather);
		self->daily_mod = 100;
		MsgControllerRunDailySim(GetAuxMsgqueue(), flag);
		if (!MainUiGetCatFlag(CAT_NO_WATER)) {
			item_p = InvListFindByType(inventory_p, g_item_milk);
			if (item_p && InvListFindByType(inventory_p, g_item_butter_churn)) {
				int32_t butter_units_b = InvItemGetCount(item_p, true) / 2;
				if (butter_units_b) {
					struct inv_item *node3_j = InvItemCreate(g_str_butter, butter_units_b, 0.0);
					InvItemRemoveUnits(item_p, 2 * butter_units_b);
					ListInsertTail(&inventory_p->base, &node3_j->node);
				}
			}
		}
		if (get_app()->ui)
			MainUiRefresh(get_app()->ui);
		if (!g_logged_100_miles && self->total_miles >= 100)
			log_two(self, &g_logged_100_miles, "Passed the hundred mile mark today.",
				"Today we have traveled 100 miles.");
		if (!g_logged_500_miles && self->total_miles >= 500)
			log_two(self, &g_logged_500_miles, "Passed the five hundred mile mark today.",
				"Today we have traveled 500 miles.");
		if (!g_logged_1000_miles && self->total_miles >= 1000)
			log_two(self, &g_logged_1000_miles, "Passed the thousand mile mark today!",
				"Today we have traveled 1,000 miles!");
		if (!g_logged_100_days && self->days_traveled >= 100)
			log_two(self, &g_logged_100_days, "We have been traveling for 100 days, today.",
				"Today we have traveled for 100 days.");
		if (!g_logged_200_days && self->days_traveled >= 200)
			log_two(self, &g_logged_200_days, "We have been traveling for 200 days, today. Will we never arrive?",
				"Will this journey never end! Today we have traveled for 200 days.");
		if (PartyGetMember(self->party, 0)->state != MEMBER_ALIVE)
			roll_flag_l = 0;
		if (!roll_flag_l)
			MsgQueuePump(GetMessageQueue(), NULL);
		self->day_tick -= g_day_length;
	}
}

void game_on_idle_spare(struct evt_handler *handler, void *param)
{
	struct game *self = (struct game *) handler;
	struct timer_task *task = param;
	int32_t range_out_i[8];
	int32_t marker_id_i;
	bool flag_g;

	int32_t node_kind_j = NodeBaseGetId(self->cur_node);
	switch (node_kind_j) {
	case NODE_KIND_START:
	case NODE_KIND_F:
	case NODE_KIND_O:
	case NODE_KIND_END:
	case NODE_KIND_M:
	case NODE_KIND_N:
		NodeBaseListActiveLinks(self->cur_node, self->date, self->prev_node_id, range_out_i);
		if (self->map_view) {
			marker_id_i = map_find_cur(self);
			if (marker_id_i > -1) {
				if (node_kind_j == NODE_KIND_N || node_kind_j == NODE_KIND_M)
					marker_id_i += 1000;
				path_append(self, marker_id_i);
			}
			MapViewUpdateMarker(self->map_view, MAP_MARKER_AT_NODE);
		}
		self->route_index = 0;
		flag_g = false;
		if (node_kind_j != NODE_KIND_N && node_kind_j != NODE_KIND_M) {
			flag_g = true;
			task->done = true;
			self->travel_state = TRAVEL_STOPPED;
			struct cmd_event *cmd_event_b = CmdEventCreate(GCMD_NODE_CONTINUE);

			EVT_HANDLER_OPS(self)->OnEvent(&self->base, cmd_event_b);
			CmdEventDestroy(&cmd_event_b);
		}
		if (!flag_g) {
			if (self->map_view && node_kind_j == NODE_KIND_N)
				MapViewUpdateMarker(self->map_view, MAP_MARKER_REDRAW);
			self->prev_node_id = self->cur_node_id;
			self->cur_node_id = range_out_i[self->route_index];
			g_cond_flag_c0 = g_cond_flag_bc = true;
			delete_node(&self->cur_node);
			self->cur_node = new_node(self->cur_node_id);
			apply_scenery_switch(self, self->cur_node);
			if (!GameIsEventAllowed(self, MSG_THIRST))
				reset_thirst(GetAuxMsgqueue());
			WeatherSetRegion(self->weather, self->cur_node->weather_region);
			GameUpdateMusic(self, 0);
			int32_t terrain_k = node_terrain(self);
			if (terrain_k == TERRAIN_UPHILL || terrain_k == TERRAIN_DOWNHILL || terrain_k == TERRAIN_DESERT) {
				self->miles_to_go = -1;

			} else {
				struct point pseg_a = PointMake(22, 14);

				self->miles_to_go = NodeBaseGetSegCount(self->cur_node);
				self->map_id = self->cur_node->map_id;
				if (self->map_id != self->shown_map_id || !self->map_view) {
					struct view *view_a;

					replace_node_view(self, &view_a);
					self->shown_map_id = self->map_id;
					self->path_len = 1;
					ObjectDelete(self->region);
					self->region = RegionCreate();
					if (self->map_view)
						ObjectDelete(self->map_view);
					self->map_view = MapViewCreate(102, &pseg_a, self->map_id, self);
					find_prev_node(self);
					VIEW_OPS(view_a)->AddChild(view_a, (struct view *) self->map_view);
					if (g_window_dirty)
						redraw_parent();
				}
				MapViewUpdateMarker(self->map_view, MAP_MARKER_AT_NODE);
				MapViewUpdateMarker(self->map_view, self->miles_to_go);
			}
			NodeBaseListActiveLinks(self->cur_node, self->date, self->prev_node_id, range_out_i);
			self->route_index = 0;
			self->next_node_id = range_out_i[self->route_index];
		}
		break;
	case NODE_KIND_TRAIL:
		if (node_terrain(self) == TERRAIN_RAFT && self->miles_to_go > 0 && self->hired_rafters) {
			self->travel_state = TRAVEL_STOPPED;
			self->total_miles += self->miles_to_go;
			self->miles_to_go = 0;
			MapViewUpdateMarker(self->map_view, MAP_MARKER_REDRAW);
			delete_node(&self->cur_node);
			self->cur_node = new_lnode(self->next_node_id);
			g_conditions_checked = false;
			apply_scenery_switch(self, self->cur_node);
			self->prev_node_id = self->cur_node_id;
			self->cur_node_id = self->next_node_id;
			if (self->map_view) {
				marker_id_i = map_find_cur(self);
				if (marker_id_i > -1)
					path_append(self, marker_id_i);
				MapViewUpdateMarker(self->map_view, MAP_MARKER_AT_NODE);
			}
			return;

		} else if (node_terrain(self) == TERRAIN_RAFT && self->miles_to_go > 0 && !self->hired_rafters) {
			int32_t raft_rsrc_id_e;
			struct point pt2_c = PointMake(0, 0);
			struct rect r_h = RectMake(0, 0, 442, 640);

			GameUpdateMusic(self, 0);
			switch (self->cur_node->node_id) {
			case 556:
				raft_rsrc_id_e = 1000;
				break;
			case 560:
				raft_rsrc_id_e = 1002;
				break;
			case 562:
				raft_rsrc_id_e = 1003;
				break;
			case 695:
				raft_rsrc_id_e = 1001;
				break;
			default:
				raft_rsrc_id_e = 1000;
				break;
			}
			task->done = true;
			self->travel_state = TRAVEL_STOPPED;
			self->total_miles += self->miles_to_go;
			self->miles_to_go = 0;
			MapViewUpdateMarker(self->map_view, MAP_MARKER_REDRAW);
			delete_node(&self->cur_node);
			self->cur_node = new_lnode(self->next_node_id);
			g_conditions_checked = false;
			apply_scenery_switch(self, self->cur_node);
			if (self->cur_node->depth || !GameIsEventAllowed(self, MSG_THIRST))
				reset_thirst(GetAuxMsgqueue());
			self->prev_node_id = self->cur_node_id;
			self->cur_node_id = self->next_node_id;
			if (self->map_view) {
				int32_t found_node_id_f = map_find_cur(self);
				if (found_node_id_f > -1)
					path_append(self, found_node_id_f);
				MapViewUpdateMarker(self->map_view, MAP_MARKER_AT_NODE);
			}
			struct view *view2_d = ViewCreateId(-2, &r_h);
			struct view *child_a = (struct view *) PicViewCreatePoint(-2, &pt2_c, 125);
			VIEW_OPS(view2_d)->AddChild(view2_d, child_a);
			pt2_c.h = 0;
			pt2_c.v = 20;
			child_a = (struct view *) PicViewCreatePoint(-2, &pt2_c, 126);
			VIEW_OPS(view2_d)->AddChild(view2_d, child_a);
			pt2_c.h = 0;
			pt2_c.v = 420;
			child_a = (struct view *) PicViewCreatePoint(-2, &pt2_c, 127);
			VIEW_OPS(view2_d)->AddChild(view2_d, child_a);
			r_h.top = 20;
			r_h.left = 20;
			r_h.right = 420;
			r_h.bottom = 420;
			struct raft_frame *raft_p = RaftFrameCreate(-2, &r_h, g_palette, g_data_lib, raft_rsrc_id_e, 1100, 0, 0);
			VIEW_OPS(view2_d)->AddChild(view2_d, (struct view *) raft_p);
			pt2_c.h = 420;
			pt2_c.v = 0;
			struct meter *meter_b = MeterCreate(-2, &pt2_c, 128, raft_rsrc_id_e, raft_p);
			VIEW_OPS(view2_d)->AddChild(view2_d, (struct view *) meter_b);
			EvtHandlerAddIdleHandler((struct evt_handler *) meter_b, false, 45);
			struct view *old_g = WindowSetRootView(&g_parent_win->base, view2_d);
			if (old_g)
				ObjectDelete(old_g);
			if (g_window_dirty)
				redraw_parent();
			return;
		}
		if (self->miles_to_go == -1) {
			task->done = true;
			self->travel_state = TRAVEL_STOPPED;
			struct cmd_event *e2_j = CmdEventCreate(GCMD_NODE_CONTINUE);

			EVT_HANDLER_OPS(self)->OnEvent(&self->base, e2_j);
			CmdEventDestroy(&e2_j);
		}
		GameTravelTick(self, true);
		if (!self->miles_to_go) {
			bool ok_d;

			path_append(self, -self->cur_node_id);
			delete_node(&self->cur_node);
			self->cur_node = new_lnode(self->next_node_id);
			g_conditions_checked = false;
			apply_scenery_switch(self, self->cur_node);
			if (self->cur_node->depth || !GameIsEventAllowed(self, MSG_THIRST))
				reset_thirst(GetAuxMsgqueue());
			int32_t type2_f = NodeBaseGetId(self->cur_node);
			if (type2_f == 6 || type2_f == 4)
				ok_d = false;
			else
				ok_d = true;
			if (ok_d) {
				struct cstr *line_j = StrRsrcGetCStr(self->arrival_journal_lines,
					RandRangeAvg(0, StrRsrcCount(self->arrival_journal_lines) - 1, 1));
				struct cstr *name_i = StrRsrcGetCStr(self->node_names, self->next_node_id);
				CStrReplaceAll(name_i, "\r", " ");
				CStrReplaceStr(line_j, "@", name_i);
				CStrDestroy(&name_i);
				add_journal_line(self, line_j);
			}
			WeatherSetRegion(self->weather, self->cur_node->weather_region);
			GameUpdateMusic(self, 0);
			self->prev_node_id = self->cur_node_id;
			self->cur_node_id = self->next_node_id;
		}
		break;
	}
}

int32_t GameGetSeason(struct game *self)
{
	if (self->weather->snow_depth)
		return SEASON_SNOW;
	int32_t idx = SEASON_SUMMER;
	switch (self->date->month) {
		case MONTH_SEPTEMBER:
			if (self->date->day > 19)
				idx = SEASON_AUTUMN;
			break;
		case MONTH_OCTOBER:
			idx = SEASON_AUTUMN;
			break;
		case MONTH_NOVEMBER:
			if (self->date->day < 15)
				idx = SEASON_AUTUMN;
			else
				idx = SEASON_WINTER;
			break;
		case MONTH_JANUARY:
		case MONTH_FEBRUARY:
		case MONTH_DECEMBER:
			idx = SEASON_WINTER;
			break;
		case MONTH_MARCH:
			if (self->date->day < 26)
				idx = SEASON_WINTER;
			break;
	}
	return idx;
}

void GameUpdateMusic(struct game *self, int32_t cue)
{
	int32_t cue_sheet_a = CUE_SHEET_OTHER;
	int32_t healthy_o = 1;
	int32_t members_m = PartyMemberCount(self->party, false);
	int32_t min_band_g = HEALTH_GOOD;
	int32_t cue_entry_n;

	if (!cue && self->event_node) {
		if (self->event_node->number_index == MSG_MEMBER_DIED || self->event_node->number_index == MSG_LEADER_DIED)
			cue = CUE_DESERT_OR_DEATH;
	}
	if (!cue) {
		if (NodeBaseGetId(self->cur_node) == NODE_KIND_END) {
			cue_entry_n = CUE_JOURNEY_END;

		} else if (NodeBaseGetId(self->cur_node) == NODE_KIND_TRAIL) {
			int32_t type = node_terrain(self);
			switch (type) {
				case TERRAIN_UPHILL:
				case TERRAIN_DOWNHILL:
					cue_entry_n = CUE_HILLS;
					break;
				case TERRAIN_DESERT:
					cue_entry_n = CUE_DESERT_OR_DEATH;
					break;
				case TERRAIN_RAFT:
					cue_entry_n = CUE_RAFTING;
					break;
				default:
					cue_entry_n = NodeBaseGetTravelMusic(self->cur_node, self);
					break;
			}

		} else {
			cue_entry_n = NodeBaseGetTravelMusic(self->cur_node, self);
		}
		if (cue_entry_n < 7) {
			cue_sheet_a = CUE_SHEET_TRAVEL;
			--cue_entry_n;
			cue_entry_n *= 3;
			for (int32_t member_idx_a = 0; member_idx_a < members_m; ++member_idx_a) {
				struct member *member_c = PartyGetMember(self->party, member_idx_a);
				if (member_c->state == MEMBER_ALIVE) {
					int32_t band_l = MemberGetHealthBand(member_c);
					if (band_l < min_band_g)
						min_band_g = band_l;
					++healthy_o;
				}
			}
			if (min_band_g < HEALTH_OK || members_m > healthy_o)
				++cue_entry_n;
			if (min_band_g < HEALTH_FAIR)
				++cue_entry_n;
			++cue_entry_n;
		}

	} else {
		cue_entry_n = cue;
	}
	OtiiAppLoadMusicCue(get_app(), cue_sheet_a);
	OtiiAppPlayMusicCue(get_app(), cue_entry_n);
}

void game_on_event(struct evt_handler *handler, struct cmd_event *evt)
{
	struct game *self = (struct game *) handler;

	switch (evt->command) {
	case CMD_CONTROL_ACTION:
		if (evt->spec_b.id == 9013) {
			evt->handled = true;
			int32_t new_pace_k = (int32_t) evt->param1;
			if (self->travel_pace != new_pace_k) {
				struct cstr *ration_line_c = NULL;

				JournalSetDate(&get_app()->journal, self->date);
				self->travel_pace = new_pace_k;
				switch (self->travel_pace) {
				case PACE_8_HOURS:
					switch (RandRangeAvg(0, 1, 1)) {
					case 0:
						ration_line_c = CStrCreateFrom("We will travel eight hours/day from now on.");
						break;
					case 1:
						ration_line_c = CStrCreateFrom("From now on, eight hours of travel per day.");
						break;
					}
					break;
				case PACE_10_HOURS:
					switch (RandRangeAvg(0, 1, 1)) {
					case 0:
						ration_line_c = CStrCreateFrom("We will push on for ten hours/day from now on.");
						break;
					case 1:
						ration_line_c = CStrCreateFrom("From now on, ten hours of travel per day.");
						break;
					}
					break;
				case PACE_EXHAUSTING:
					switch (RandRangeAvg(0, 1, 1)) {
					case 0:
						ration_line_c = CStrCreateFrom("We will press on until we are exhausted each day.");
						break;
					case 1:
						ration_line_c = CStrCreateFrom(
							"From now on, no stopping until we are ready to drop in our tracks.");
						break;
					}
					break;
				}
				JournalAddLine(&get_app()->journal, ration_line_c);
			}
			return;

		} else if (evt->spec_b.id == 9014) {
			evt->handled = true;
			int32_t new_ration_p = (int32_t) evt->param1;
			struct cstr *pace_line_a = NULL;
			if (self->party->ration_level > new_ration_p) {
				JournalSetDate(&get_app()->journal, self->date);
				switch (RandRangeAvg(0, 2, 1)) {
				case 0:
					pace_line_a = CStrCreateFrom("We will have to eat less each day.");
					break;
				case 1:
					pace_line_a = CStrCreateFrom("We will eat fewer meals.");
					break;
				case 2:
					pace_line_a = CStrCreateFrom("We will be more frugal with our food.");
					break;
				}
				JournalAddLine(&get_app()->journal, pace_line_a);

			} else {
				if (self->party->ration_level < new_ration_p) {
					JournalSetDate(&get_app()->journal, self->date);
					switch (RandRangeAvg(0, 2, 1)) {
					case 0:
						pace_line_a = CStrCreateFrom("We will increase our rations.");
						break;
					case 1:
						pace_line_a = CStrCreateFrom("We will eat better from now on.");
						break;
					case 2:
						pace_line_a = CStrCreateFrom("We will have heartier meals.");
						break;
					}
					JournalAddLine(&get_app()->journal, pace_line_a);
				}
			}
			self->party->ration_level = new_ration_p;
			return;
		}
		break;
	case GCMD_NODE_CONTINUE:
	case GCMD_FERRY_WAIT_DONE:
		GAME_OPS(self)->NodeAction(self, evt);
		break;
	case CMD_MEDIA_DONE:
		GAME_OPS(self)->RiverCrossing(self, evt);
		break;
	case GCMD_BUILD_MAIN_SCREEN:
		GAME_OPS(self)->BuildMainScreen(self, evt);
		break;
	case GCMD_TALK:
		GAME_OPS(self)->RunNpcConv(self, evt);
		break;
	case GCMD_CHOICE:
		GAME_OPS(self)->ExecuteCommand(self, evt);
		break;
	case GCMD_PLAY_SFX:
		OtiiAppPlaySfx(get_app(), (int32_t) evt->param2, false);
		evt->handled = true;
		break;
	default:
		break;
	}
}

static void retry_segment(struct game *self)
{
	self->route_index = 9999;
	self->miles_to_go = NodeBaseGetSegCount(self->cur_node);
	if (self->map_view)
		MapViewUpdateMarker(self->map_view, self->miles_to_go);
}

static void say_too_heavy(void)
{
	struct msg_box box;

	MsgBoxInit(&box, CStrCreateFrom("You can't leave yet because your wagon load is too heavy and/or you don't have "
		"enough draft animals."), false);
	ModalDialogRun(&box.base, 0, &g_parent_win->base);
	msg_box_destroy((struct object *) &box);
}

static void show_end_frame(struct game *self, int32_t end_type)
{
	struct rect rect = RectMake(14, 22, 373, 618);
	struct view *old;

	if (!self->end_score)
		self->end_score = EndGameCreate(self);
	replace_node_view(self, &old);
	struct end_frame *endframe = EndFrameCreate(-2, &rect, self->end_score, self, end_type);
	VIEW_OPS(old)->AddChild(old, (struct view *) endframe);
}

void game_execute_command(struct game *self, struct cmd_event *cmd)
{
	int32_t node_ids_h[8];
	int32_t found;

	if (self->event_node) {
		self->game_mode = (int32_t) cmd->param1;
		if (self->game_mode == NODE_CMD_SAVE_DIARY) {
			JournalExportWri(&get_app()->journal);

		} else if (self->game_mode == NODE_CMD_EVENT_END_GAME) {
			cmd->handled = true;
			dispatch_cmd(CMD_END_GAME);
			return;

		} else if (self->game_mode > NODE_CMD_EVENT_END_GAME) {
			int32_t conv_id_g;

			cmd->handled = true;
			switch (self->event_node->number_index) {
				case MSG_HEAVY_FOG:  conv_id_g = 5685; break;
				case MSG_DUSTSTORM: conv_id_g = 5686; break;
				case MSG_THICK_DUST: conv_id_g = 5687; break;
				default: conv_id_g = NodeBaseGetSeasonBackdrop(self->cur_node, self) + 5100; break;
			}
			struct conv_dlg *conv_dlg_a = ConvDlgCreate(self->game_mode, 3, conv_id_g);
			ModalDialogRun((struct modal_dialog *) conv_dlg_a, 0, &g_parent_win->base);
			ObjectDelete(conv_dlg_a);

		} else {
			self->event_node->rsrc_index = self->game_mode;
			cmd->handled = true;
			MsgControllerHandleEvent(GetAuxMsgqueue(), self->event_node);
			if (self->event_node->choice_state) {
				struct msg *node_d = self->event_node;
				self->event_node = NULL;
				if (get_app()->app_state == APP_STATE_HUNTING) {
					ListRemove(&GetMessageQueue()->base, &node_d->node);
					ObjectDelete(node_d);

				} else {
					if (get_app()->ui)
						MainUiShowTravelPanel(get_app()->ui);
					OtiiAppApplyScreenMode(get_app());
					MsgQueuePump(GetMessageQueue(), node_d);
				}

			} else {
				self->event_node->rsrc_index = -1;
			}
		}

	} else {
		int32_t p2_ = (int32_t) cmd->param2;
		if (p2_)
			game_check_event_condition(p2_, self->party);
		int32_t sub_cmd_n = (int32_t) cmd->param1;
		cmd->handled = true;
		self->route_index = -1;
		if (sub_cmd_n < 3) {
			self->route_index = sub_cmd_n;

		} else {
		switch (sub_cmd_n) {
			case NODE_CMD_CAULK_AND_FLOAT:
				GameRiverCrossingAttempt(self, NODE_CMD_CAULK_AND_FLOAT,self->cur_node_id, g_crossing_view, cmd);
				break;
			case NODE_CMD_CHECK_RUINS: {
				g_cond_flag_bc = false;
				struct inv_list *loot_n = GenRandomFoundItems(RandRangeAvg(1, 100, 1));
				if (loot_n->base.count) {
					ShowItemListBox("You found:", loot_n);
					PartyDistributeLoot(GetGameObj()->party, loot_n);

				} else {
					struct msg_box box4;
					MsgBoxInit(&box4, CStrCreateFrom("You didn't find anything useful."), false);

					ModalDialogRun(&box4.base, 0, &g_parent_win->base);
					msg_box_destroy((struct object *) &box4);
				}
				InvListDestroy(&loot_n);
				dispatch_cmd(GCMD_NODE_CONTINUE);
				break;
			}
			case NODE_CMD_CHECK_CONDITIONS:
				g_conditions_checked = true;
				if (NodeBaseGetId(self->cur_node) == NODE_KIND_TRAIL) {
					if (get_app()->ui)
						MainUiShowHillPanel(get_app()->ui);

				} else {
					if (get_app()->ui)
						MainUiShowRiverPanel(get_app()->ui);
				}
				break;
			case NODE_CMD_GO_DOWN:
				if (self->scenery_switch) {
					if (EventRollAilment(NODE_CMD_GO_DOWN,self->cur_node_id))
						retry_segment(self);

				} else if (EventRollAilment2(NODE_CMD_GO_UP,self->cur_node_id)) {
					retry_segment(self);
				}
				break;
			case NODE_CMD_CONTINUE_RAFTING:
				if (self->cur_node->node_id == 557)
					self->route_index = 1;
				else
					self->route_index = 0;
				break;
			case NODE_CMD_GO_UP:
				if (self->scenery_switch) {
					if (EventRollAilment2(NODE_CMD_GO_UP,self->cur_node_id))
						retry_segment(self);

				} else if (EventRollAilment(NODE_CMD_GO_DOWN,self->cur_node_id)) {
					retry_segment(self);
				}
				break;
			case NODE_CMD_CROSS_ICE:
				GameRiverCrossingAttempt(self, NODE_CMD_CROSS_ICE,self->cur_node_id, g_crossing_view, cmd);
				break;
			case NODE_CMD_DOUBLE_TEAM:
				if (EventRollAilment2(NODE_CMD_DOUBLE_TEAM,self->cur_node_id))
					retry_segment(self);
				break;
			case NODE_CMD_FORD:
				GameRiverCrossingAttempt(self, NODE_CMD_FORD,self->cur_node_id, g_crossing_view, cmd);
				break;
			case NODE_CMD_GATHER_HAY: {
				int32_t bales_c;
				char buf12_n[80];

				g_cond_flag_c0 = false;
				struct cstr *hay_l = CStrCreateFrom("hay");

				if (PartyCountFood(self->party) > 1)
					bales_c = PartyCountFood(self->party);
				else
					bales_c = 1;
				bales_c = RandRangeAvg(bales_c - 1, bales_c + 1, 1);
				InvListAdd(&self->party->inventory, hay_l, bales_c, 0.0);
				StrPrintf(buf12_n, "%" PRId32 " bales of hay have been added to your inventory!", bales_c);
				struct msg_box box12_j;
				MsgBoxInit(&box12_j, CStrCreateFrom(buf12_n), false);

				ModalDialogRun(&box12_j.base, 0, &g_parent_win->base);
				dispatch_cmd(GCMD_NODE_CONTINUE);
				msg_box_destroy((struct object *) &box12_j);
				CStrDestroy(&hay_l);
				break;
			}
			case NODE_CMD_GO_INTO_TOWN: {
				struct view *view_a;

				replace_node_view(self, &view_a);
				{
					struct point pt13_p = PointMake(22, 14);

					int32_t conv = LNodeGetLandmarkConv((struct lnode *) self->cur_node, self);
					struct view *scene = (struct view *) SceneFrameCreate(200, &pt13_p, conv);
					enter_state(get_app(), 4);
					if (get_app()->ui)
						MainUiShowNodeDescPanel(get_app()->ui);
					VIEW_OPS(view_a)->AddChild(view_a, scene);
				}
				if (g_window_dirty)
					redraw_parent();
				break;
			}
			case NODE_CMD_HIRE_INDIANS: {
				struct lnode *ln_b = (struct lnode *) self->cur_node;
				struct cstr *offer_a = CStrCreateFrom("We will guide you across the river in return for");
				struct inv_list *items_b = PartyBuildInventoryList(self->party);
				int32_t backdrop_e = NodeBaseGetSeasonBackdrop(self->cur_node, self);
				int32_t offer = RandRangeAvg(800, 1000, 1);
				struct trade_dlg *trade_k = TradeDlgCreateOffer(offer_a, offer, 2, ln_b->guide_conv + 6000,
					backdrop_e + 5100, items_b);
				int32_t res_h = ModalDialogRun((struct modal_dialog *) trade_k, 0, &g_parent_win->base);
				ObjectDelete(trade_k);
				if (res_h != DLG_CANCEL)
					GameRiverCrossingAttempt(self, NODE_CMD_HIRE_INDIANS,self->cur_node_id, g_crossing_view, cmd);
				break;
			}
			case NODE_CMD_HIRE_RAFTERS:
				self->hired_rafters = 1;
				found = NodeBaseListActiveLinks(self->cur_node, self->date, self->prev_node_id, node_ids_h);
				self->route_index = 0;
				for (int32_t i = 0; i < found; i++) {
					if (node_ids_h[i] == 556) {
						self->route_index = i;
						i = found;
					}
				}
				break;
			case NODE_CMD_PORTAGE:
				self->route_index = 0;
				break;
			case NODE_CMD_LIGHTEN_LOAD:
				dispatch_cmd(GCMD_OPEN_DROP_DLG);
				break;
			case NODE_CMD_LOCK_WHEELS:
				if (EventRollAilment(NODE_CMD_LOCK_WHEELS,self->cur_node_id))
					retry_segment(self);
				break;
			case NODE_CMD_PASS_QUARANTINE:
				self->route_index = 0;
				break;
			case NODE_CMD_PAY_TOLL:
				self->route_index = 0;
				break;
			case NODE_CMD_REST: {
				int32_t rested_g = GameRest(self, 0, 1);
				if (rested_g) {
					delete_node(&self->cur_node);
					self->cur_node = new_lnode(self->cur_node_id);
					if (NodeBaseGetId(self->cur_node) == NODE_KIND_NONE) {
						delete_node(&self->cur_node);
						self->cur_node = new_node(self->cur_node_id);
					}
					dispatch_cmd(GCMD_NODE_CONTINUE);
				}
				break;
			}
			case NODE_CMD_WAIT_FOR_SPRING: {
				struct date spring_l = DateMake(3, 2, 1847);
				struct date april_g = DateMake(4, 1, 1847);
				struct date today_i = DateMake(self->date->month, self->date->day, self->date->year);
				struct date *pick_n;

				struct cstr *msg_b = CStrCreateFrom("The pioneers who first set out from Nauvoo and Kanesville "
					"reached Winter Quarters in mid-1846. They waited until the Spring of 1847 to continue west.");
				struct msg_box box22;
				MsgBoxInit(&box22, msg_b, false);

				struct cstr *note_b = CStrCreateFrom(
					"Arrived at Winter Quarters.  Will stay here until spring of 1847.");
				JournalSetDate(&get_app()->journal, self->date);
				JournalAddLine(&get_app()->journal, note_b);
				ModalDialogRun(&box22.base, 0, &g_parent_win->base);
				if (DateGreater(&today_i, &spring_l))
					pick_n = &today_i;
				else
					pick_n = &spring_l;
				DateSetYear(self->date, pick_n->year);
				DateSetMonth(self->date, pick_n->month);
				DateSetDay(self->date, pick_n->day);
				WeatherSetMonth(self->weather, self->date->month);
				WeatherSimulateDay(self->weather);
				do
					GameRest(self, 25, 1);
				while (DateLess(self->date, &april_g));
				self->cur_node_id = 322;
				self->prev_node_id = 320;
				delete_node(&self->cur_node);
				self->cur_node = new_lnode(self->cur_node_id);
				apply_scenery_switch(self, self->cur_node);
				g_conditions_checked = false;
				if (self->cur_node->depth || !GameIsEventAllowed(self, MSG_THIRST))
					reset_thirst(GetAuxMsgqueue());
				dispatch_cmd(GCMD_NODE_CONTINUE);
				msg_box_destroy((struct object *) &box22);
				return;
			}
			case NODE_CMD_FERRY: {
				char buf23[32];

				if (NodeBaseGetId(self->cur_node) != NODE_KIND_TRAIL
					&& ((struct lnode *) self->cur_node)->ferry_wait_days) {
					g_ferry_wait_days = ((struct lnode *) self->cur_node)->ferry_wait_days;
					if (g_ferry_wait_days == 1)
						StrCopy(buf23, "The wait is only a day!");
					else
						StrPrintf(buf23, "The wait is %" PRId32 " days!", g_ferry_wait_days);
					MsgQueuePost(GetMessageQueue(), MSG_FERRY_DELAY, CStrCreateFrom(buf23), 0, 0, NULL, -1);
					return;

				} else {
					GameRiverCrossingAttempt(self, NODE_CMD_FERRY,self->cur_node_id, g_crossing_view, cmd);
				}
				break;
			}
			case NODE_CMD_TOLL_BRIDGE:
				self->route_index = 0;
				break;
			case NODE_CMD_LOOK_AROUND: {
				struct cmd_event *e25 = CmdEventCreate(GCMD_NODE_CONTINUE);
				g_conv_npc_idx = 0;
				e25->param2 = 1;
				DispatchGlobalEvent(e25);
				break;
			}
			case NODE_CMD_ROPES_AND_CHAINS:
				if (node_terrain(self) == TERRAIN_UPHILL) {
					if (EventRollAilment2(NODE_CMD_ROPES_AND_CHAINS,self->cur_node_id))
						retry_segment(self);

				} else if (EventRollAilment(NODE_CMD_ROPES_AND_CHAINS,self->cur_node_id)) {
					retry_segment(self);
				}
				break;
			case NODE_CMD_WAIT_FOR_SUNSET:
				self->party->daily_health_drain = self->party->daily_health_drain - 100;
				self->miles_to_go = NodeBaseGetSegCount(self->cur_node);
				if (self->map_view)
					MapViewUpdateMarker(self->map_view, self->miles_to_go);
				self->route_index = 0;
				break;
			case NODE_CMD_ANCHOR_WAGON:
				if (EventRollAilment(NODE_CMD_ANCHOR_WAGON,self->cur_node_id))
					retry_segment(self);
				break;
			case NODE_CMD_READY_TO_GO: {
				int32_t want_a = LNodeSelectNextRoute((struct lnode *) self->cur_node, self);
				found = NodeBaseListActiveLinks(self->cur_node, self->date, self->prev_node_id, node_ids_h);
				self->route_index = 0;
				for (int32_t j_p = 0; j_p < found; j_p++) {
					if (node_ids_h[j_p] == want_a) {
						self->route_index = j_p;
						j_p = found;
					}
				}
				break;
			}
			case NODE_CMD_TALK: {
				int32_t conv_t = node_speaker_conv(self->cur_node, g_conv_npc_idx);

				if (!conv_t)
					break;
				if (self->cur_node->conv_count > 0)
					g_conv_npc_idx = g_conv_npc_idx % self->cur_node->conv_count;
				struct cmd_event *e32 = CmdEventCreate(GCMD_TALK);
				e32->param1 = conv_t;
				e32->param2 = NodeBaseGetSeasonBackdrop(self->cur_node, self);
				DispatchGlobalEvent(e32);
				break;
			}
			case NODE_CMD_LOOK_FOR_SOMEONE_ELSE: {
				struct point size;
				struct rect r1_e;
				struct rect r2_a;
				struct canvas *img_j;

				int32_t backdrop_g = NodeBaseGetSeasonBackdrop(self->cur_node, self);
				g_conv_npc_idx++;
				if (self->cur_node->conv_count <= g_conv_npc_idx)
					g_conv_npc_idx = 0;
				int32_t node_data_n = self->cur_node->conv_table[g_conv_npc_idx];
				struct drawable *obj = create_virtual_packed(RegionalAssetId1000(), g_node_scene_pic);
				if (obj) {
					OBJ_BASE_OPS(obj)->GetSize((struct obj_base *) obj, &size);
					img_j = OBJ_BASE_OPS(obj)->GetRect((struct obj_base *) obj, &r2_a);
					rect_from_size(&r1_e, &size);
					CANVAS_OPS(g_talk_canvas)->StretchBlit(g_talk_canvas, img_j, &r2_a, &r1_e, 0, &r1_e);
					release_drawable(obj);
				}
				obj = create_virtual(ConvGetSpeaker(node_data_n) + 6200);
				if (!obj)
					obj = create_virtual(6201);
				if (obj) {
					img_j = DRAWABLE_OPS(obj)->GetRectAt(obj, 0, &r1_e);
					struct rect src_l = r1_e;

					rect_offset(&r1_e, self->portrait_anchors[backdrop_g].h - (r1_e.right - r1_e.left) / 2,
						self->portrait_anchors[backdrop_g].v - (r1_e.bottom - r1_e.top));
					struct point at = {r1_e.top, r1_e.left};
					CANVAS_OPS(g_talk_canvas)->BlitImage(g_talk_canvas, img_j, &src_l, &at, &r1_e, false);
					release_drawable(obj);
				}
				ViewNotifyChanged((struct view *) g_crossing_view);
				break;
			}
			case NODE_CMD_DONE_LOOKING:
				dispatch_cmd(GCMD_NODE_CONTINUE);
				break;
			case NODE_CMD_STAKE_LAND_CLAIM:
				show_end_frame(self, END_LAND_CLAIM);
				break;
			case NODE_CMD_PROSPECT_FOR_GOLD:
				show_end_frame(self, END_GOLD_CLAIM);
				break;
			case NODE_CMD_FINAL_SCORE:
				show_end_frame(self, END_FINAL_SCORE);
				break;
			case NODE_CMD_SEE_WHAT_LIES_AHEAD:
				show_end_frame(self, END_EPILOGUE);
				break;
			case NODE_CMD_END_GAME:
				dispatch_cmd(CMD_END_GAME);
				return;
			case NODE_CMD_SETTLE_HERE:
				self->prev_node_id = self->cur_node_id;
				self->cur_node_id = 753;
				delete_node(&self->cur_node);
				self->cur_node = new_lnode(self->cur_node_id);
				dispatch_cmd(GCMD_NODE_CONTINUE);
				return;
			case NODE_CMD_SAVE_DIARY:
				JournalExportWri(&get_app()->journal);
				break;
			case NODE_CMD_TRAVEL_BY_DAY:
				self->party->daily_health_drain = self->party->daily_health_drain + 50;
				self->miles_to_go = NodeBaseGetSegCount(self->cur_node);
				if (self->map_view)
					MapViewUpdateMarker(self->map_view, self->miles_to_go);
				self->route_index = 0;
				break;
			default:
				break;
		}
		}
		if (self->route_index >= 0) {
			if (!self->total_miles && NodeBaseGetId(self->cur_node) != NODE_KIND_TRAIL &&
				!PartyHasDraftAnimals(self->party)) {
				self->route_index = -1;
				say_too_heavy();
				return;
			}
			OtiiAppStopSfx(get_app());
			GameEnterNode(self, self->route_index);
		}
	}
}

void game_build_main_screen(struct game *self, struct cmd_event *evt)
{
	struct rect screen_rect_d;
	struct point child_pos_c;
	struct point panel_pos_p = PointMake(0, 38);
	struct view *child_b;

	evt->handled = true;
	screen_rect_d.top = 0;
	screen_rect_d.left = 0;
	screen_rect_d.right = 640;
	screen_rect_d.bottom = 442;
	struct view *view_g = ViewCreateId(-2, &screen_rect_d);
	struct view *prev_root_i = WindowSetRootView(&g_parent_win->base, view_g);
	if (prev_root_i)
		ObjectDelete(prev_root_i);
	static const int32_t pics[9][3] = {
		{0, 0, 130}, {0, 14, 131}, {618, 0, 132}, {377, 373, 133}, {377, 436, 134},
		{433, 380, 135}, {495, 380, 136}, {557, 380, 137}, {619, 380, 138},
	};
	for (int32_t i = 0; i < 9; i++) {
		child_pos_c.h = pics[i][0];
		child_pos_c.v = pics[i][1];
		child_b = (struct view *) PicViewCreatePoint(-2, &child_pos_c, pics[i][2]);
		VIEW_OPS(view_g)->AddChild(view_g, child_b);
	}
	panel_pos_p.v = 380;
	panel_pos_p.h = 377;
	for (int32_t row_f = 0; row_f < 4; ++row_f) {
		struct view *child2_c = (struct view *) PictCtrlCreate(row_f + TOOL_MAP, &g_app->base.eh_event, &panel_pos_p,
			row_f + 981, 0, 0, NULL, 0, 0, 0);
		VIEW_OPS(view_g)->AddChild(view_g, child2_c);
		panel_pos_p.h = panel_pos_p.h + 62;
	}
	child_pos_c.h = 22;
	child_pos_c.v = 373;
	get_app()->ui = MainUiCreate(-2, &child_pos_c, 149);
	VIEW_OPS(view_g)->AddChild(view_g, (struct view *) get_app()->ui);
	self->doc = view_g;
	int32_t amount_m = (int32_t) evt->param1;
	if (amount_m) {
		if (amount_m > 99) {
			struct member *member_a = PartyGetMember(self->party, 0);
			member_a->state = MEMBER_DEAD;
			if (!member_a->state)
				member_a->health = 0;
			struct cstr *drowned_p = CStrCreateCopy(member_a->name);
			CStrAppendBuf(drowned_p, " drowned");
			struct cstr *died_l = CStrCreateCopy(member_a->name);
			CStrAppendBuf(died_l, " died");
			struct cstr *note_j = CStrCreate();
			CStrAssign(note_j, died_l);
			add_journal_line(self, note_j);
			MsgQueuePost(GetMessageQueue(), MSG_LEADER_DIED, drowned_p, (intptr_t) member_a, 0, died_l, -1);
			MsgControllerRunDailySim(GetAuxMsgqueue(), false);
			g_window_dirty = false;

		} else {
			dispatch_cmd(GCMD_NODE_CONTINUE);
			MsgControllerReportSupplyLoss(GetAuxMsgqueue(), MSG_RAFTING_LOSSES, amount_m, true, true);
		}

	} else {
		dispatch_cmd(GCMD_NODE_CONTINUE);
	}
	GameUpdateMusic(self, 0);
}

void game_river_crossing(struct game *self, struct cmd_event *evt)
{
	int32_t amount_l;
	int32_t swiftness_f = self->cur_node->swiftness;
	int32_t handler_j = CMD_MEDIA_DONE;
	struct event handled_ev_m = EventMake(EVCLS_ANY, -1, -1);

	EventSetId(&handled_ev_m, TOOL_SUPPLIES);
	EVT_HANDLER_OPS(self)->SetHandledFor(&self->base, &handled_ev_m);
	VIEW_OPS(g_active_scene)->RemoveFromParent(g_active_scene);
	ObjectDelete(g_active_scene);
	sound_server_resume();
	GameUpdateMusic(self, 0);
	EvtHandlerReleaseBinding(&self->base, 1, &handler_j);
	evt->handled = true;
	enter_state(get_app(), APP_STATE_TRAVELING);
	switch (g_crossing_outcome) {
		case CROSS_OUTCOME_SUCCESS:
			GameEnterNode(self, 0);
			break;
		case CROSS_OUTCOME_STUCK:
			MsgQueuePost(GetMessageQueue(), MSG_STUCK_IN_MUD, NULL, 0, 0, NULL, -1);
			break;
		case CROSS_OUTCOME_TIPPED_DEEP:
		case CROSS_OUTCOME_TIPPED:
			amount_l = RandRangeAvg(0, 20, 1) + 2 * swiftness_f;
			MsgControllerReportSupplyLoss(GetAuxMsgqueue(), MSG_TIPPED_WAGON, amount_l, true,
				g_river_method != NODE_CMD_FORD);
			break;
		case CROSS_OUTCOME_SWAMPED_DEEP:
		case CROSS_OUTCOME_SWAMPED:
			amount_l = RandRangeAvg(0, 30, 1) + 2 * swiftness_f;
			MsgControllerReportSupplyLoss(GetAuxMsgqueue(), MSG_SWAMPED_WAGON, amount_l, true,
				g_river_method != NODE_CMD_FORD);
			break;
		case CROSS_OUTCOME_THROUGH_ICE:
			amount_l = RandRangeAvg(0, 30, 1) + 5 * swiftness_f;
			MsgControllerReportSupplyLoss(GetAuxMsgqueue(), MSG_FELL_THROUGH_ICE, amount_l, true, false);
			break;
	}
}

static void scan_choice(const char *line, int32_t *cmd, int32_t *arg)
{
	(void) sscanf(line, "%" SCNd32 " %" SCNd32, cmd, arg);
}

void game_node_action(struct game *self, struct cmd_event *evt)
{
	struct canvas *frame = NULL;
	struct rect canvas_rect_f;
	struct point size;
	struct rect src_rect_k;
	struct point frame_pos_i;
	struct view *view_b;
	struct rect ctrl_rect_e = RectMake(90, 390, 110, 595);
	struct rect label_rect_k = RectMake(30, 380, 80, 615);
	struct event handled_ev_m = EventMake(EVCLS_ANY, -1, -1);
	int32_t cmdset_n;
	struct cstr *item_n;
	struct view *frame_view_m;
	struct cstr *line_str_n;
	int32_t node_ids_n[8];
	struct drawable *obj_;
	struct view *picview_f;

	bool at_endgame_n = false;
	if (APP_OPS(g_app)->GetMode(g_app) == LICENSE_DEMO)
		at_endgame_n = true;
	if (at_endgame_n && self->cur_node->node_id == 38) {
		struct msg_box box1_j;
		MsgBoxInit(&box1_j, CStrCreateFrom("You have reached the end of the demo version of Oregon Trail II. To "
			"order this product, call 1-800-685-MECC, ext. 269 or visit your local retailer."), false);

		ModalDialogRun(&box1_j.base, 0, &g_parent_win->base);
		evt->handled = true;
		kill_member(PartyGetMember(self->party, 0));
		dispatch_cmd(CMD_END_GAME);
		msg_box_destroy((struct object *) &box1_j);
		return;
	}
	int32_t param2_e = (int32_t) evt->param2;
	if (evt->command == GCMD_NODE_CONTINUE && self->train_wagons == -1 && self->train_people == -1) {
		int32_t picks_o[2] = {0, 0};

		if (at_endgame_n && self->start_town > START_ST_JOSEPH) {
			evt->handled = true;
			struct msg_box box2_k;
			MsgBoxInit(&box2_k, CStrCreateFrom("Sorry, you cannot start your journey from here in the demo "
				"version.  You can only depart from Independence or St. Joseph. Do you want to stop exploring this "
				"town?"), true);

			int32_t choice_k = ModalDialogRun(&box2_k.base, 0, &g_parent_win->base);
			if (choice_k != DLG_CANCEL) {
				kill_member(PartyGetMember(self->party, 0));
				dispatch_cmd(CMD_END_GAME);
			}
			msg_box_destroy((struct object *) &box2_k);
			return;
		}
		if (!self->party->animals.base.count) {
			struct msg_box box3;
			MsgBoxInit(&box3, CStrCreateFrom("You're unable to leave town because you don't have any draft "
				"animals to pull your wagon.  You can buy draft animals at the livery stable."), false);

			ModalDialogRun(&box3.base, 0, &g_parent_win->base);
			evt->handled = true;
			msg_box_destroy((struct object *) &box3);
			return;
		}
		if (!PartyHasDraftAnimals(self->party)) {
			say_too_heavy();
			evt->handled = true;
			return;
		}
		struct picker_dlg *dlg_a = PickerDlgCreate();
		ModalDialogRun((struct modal_dialog *) dlg_a, (intptr_t) picks_o, &g_parent_win->base);
		ObjectDelete(dlg_a);
		self->train_wagons = picks_o[0];
		self->train_people = picks_o[1];
		self->train_wagons_start = self->train_wagons;
		self->train_people_start = self->train_people;
	}
	if (evt->command == GCMD_FERRY_WAIT_DONE) {
		g_ferry_wait_pending = true;
		return;
	}
	if (evt->command == GCMD_NODE_CONTINUE) {
		evt->handled = true;
		if (self->event_node) {
			GameRun(self, self->event_node);
			return;
		}
		if (g_health_window)
			HealthWinUpdate(g_health_window, false);
		EventSetId(&handled_ev_m, TOOL_SUPPLIES);
		EVT_HANDLER_OPS(self)->SetHandledFor(&self->base, &handled_ev_m);
		int32_t arrival_sound_m = NodeBaseGetArrivalSound(self->cur_node, self);
		if (arrival_sound_m < -1)
			OtiiAppPlaySfx(get_app(), -arrival_sound_m, false);
		else if (arrival_sound_m > 0)
			OtiiAppPlaySfx(get_app(), arrival_sound_m, true);
		enter_state(get_app(), APP_STATE_AT_NODE);
		if (get_app()->ui)
			MainUiShowTravelPanel(get_app()->ui);
		GameUpdateMusic(self, 0);
		if (NodeBaseGetId(self->cur_node) != NODE_KIND_TRAIL && ((struct lnode *) self->cur_node)->arrival_wait_days) {
			char buf_a[100];

			int32_t days_l = ((struct lnode *) self->cur_node)->arrival_wait_days;
			if (days_l == 1)
				StrCopy(buf_a, "The wait is only a day!");
			else
				StrPrintf(buf_a, "The wait is %" PRId32 " days!", days_l);
			((struct lnode *) self->cur_node)->arrival_wait_days = 0;
			MsgQueuePost(GetMessageQueue(), MSG_CROWDED_RIVER, CStrCreateFrom(buf_a), 0, 0, NULL, -1);
			GameRest(self, 24 * days_l, 1);
			return;
		}
		replace_node_view(self, &view_b);
		int32_t found_b = NodeBaseListActiveLinks(self->cur_node, self->date, self->prev_node_id, node_ids_n);
		if (!found_b) {
			self->prev_node_id = self->cur_node_id;
			self->shown_map_id = -1;
			NodeBaseListActiveLinks(self->cur_node, self->date, self->prev_node_id, node_ids_n);
		}
		obj_ = NULL;
		if (param2_e || !self->cur_node->scene_pic_by_season[GameGetSeason(self)]) {
			NodeBaseSelectConvNpcs(self->cur_node);
			g_node_scene_pic = NodeBaseGetSeasonBackdrop(self->cur_node, self) + 4100;
			obj_ = create_virtual_packed(RegionalAssetId1000(), g_node_scene_pic);

		} else {
			g_node_scene_pic = self->cur_node->scene_pic_by_season[GameGetSeason(self)];
			if (g_node_scene_pic)
				obj_ = create_virtual_packed(RegionalAssetId1000(), g_node_scene_pic);
		}
		if (!obj_)
			obj_ = create_virtual(2000);
		if (obj_) {
			OBJ_BASE_OPS(obj_)->GetSize((struct obj_base *) obj_, &size);
			frame = OBJ_BASE_OPS(obj_)->GetRect((struct obj_base *) obj_, &src_rect_k);
			rect_from_size(&canvas_rect_f, &size);
			g_talk_canvas = CanvasCreateRect(&canvas_rect_f, 8, 255);
			CANVAS_OPS(g_talk_canvas)->StretchBlit(g_talk_canvas, frame, &src_rect_k, &canvas_rect_f, 0,
				&canvas_rect_f);
			release_drawable(obj_);
			rect_offset(&canvas_rect_f, 52, 34);
			SHAPE_OPS(g_talk_canvas)->SetRect((struct shape *) g_talk_canvas, &canvas_rect_f);
			g_crossing_view = ClipViewCreate(-2, &canvas_rect_f, (struct shape *) g_talk_canvas, false);
			VIEW_OPS(view_b)->AddChild(view_b, (struct view *) g_crossing_view);
		}
		frame_pos_i.h = 22;
		frame_pos_i.v = 14;
		frame_view_m = (struct view *) PicViewCreatePoint(-2, &frame_pos_i, 140);
		VIEW_OPS(view_b)->AddChild(view_b, frame_view_m);
		frame_pos_i.h = 22;
		frame_pos_i.v = 34;
		frame_view_m = (struct view *) PicViewCreatePoint(-2, &frame_pos_i, 141);
		VIEW_OPS(view_b)->AddChild(view_b, frame_view_m);
		frame_pos_i.h = 22;
		frame_pos_i.v = 354;
		frame_view_m = (struct view *) PicViewCreatePoint(-2, &frame_pos_i, 142);
		VIEW_OPS(view_b)->AddChild(view_b, frame_view_m);
		frame_pos_i.h = 372;
		frame_pos_i.v = 14;
		picview_f = (struct view *) PicViewCreatePoint(-2, &frame_pos_i, 143);
		VIEW_OPS(view_b)->AddChild(view_b, picview_f);
		if (!self->cur_node->scene_pic_by_season[GameGetSeason(self)])
			line_str_n = CStrCreateFrom("On the Trail");
		else
			line_str_n = StrRsrcGetCStr(self->node_names, self->cur_node_id);
		g_trail_text = TextViewCreate(-2, &label_rect_k, line_str_n, 2, 1, 1, 0, 0, 0, 0, 255, 0, 1);
		VIEW_OPS(view_b)->AddChild(view_b, (struct view *) g_trail_text);
		bool rebuild_n = false;
		if (NodeBaseGetId(self->cur_node) == NODE_KIND_TRAIL) {
			int32_t terrain = node_terrain(self);
			if ((terrain == TERRAIN_UPHILL || terrain == TERRAIN_DOWNHILL || terrain == TERRAIN_DESERT)
				&& self->miles_to_go != -1)
				rebuild_n = true;
		}
		if (param2_e || !self->cur_node->scene_pic_by_season[GameGetSeason(self)] || rebuild_n) {
			int32_t conv_i = node_speaker_conv(self->cur_node, g_conv_npc_idx);
			struct rect r_e;

			int32_t backdrop_f = NodeBaseGetSeasonBackdrop(self->cur_node, self);
			if (self->cur_node->scene_pic_by_season[GameGetSeason(self)]) {
				cmdset_n = 1;

			} else {
				cmdset_n = self->cur_node->cmd_set_id;
				if (cmdset_n == 2)
					cmdset_n = 100;
			}
			if (rebuild_n)
				cmdset_n = 100;
			struct drawable *obj2_k = conv_i ? create_virtual(ConvGetSpeaker(conv_i) + 6200) : NULL;
			if (!obj2_k)
				obj2_k = create_virtual(6201);
			if (obj2_k) {
				struct canvas *frame2 = DRAWABLE_OPS(obj2_k)->GetRectAt(obj2_k, 0, &r_e);
				struct rect src_e = r_e;

				rect_offset(&r_e, self->portrait_anchors[backdrop_f].h - (r_e.right - r_e.left) / 2,
					self->portrait_anchors[backdrop_f].v - (r_e.bottom - r_e.top));
				struct point at = {r_e.top, r_e.left};
				CANVAS_OPS(g_talk_canvas)->BlitImage(g_talk_canvas, frame2, &src_e, &at, &r_e, false);
				release_drawable(obj2_k);
			}

		} else {
			cmdset_n = self->cur_node->cmd_set_id;
		}
		if (!cmdset_n)
			cmdset_n = 2;
		if (!self->scenery_switch) {
			switch (cmdset_n) {
				case 12: cmdset_n = 13; break;
				case 13: cmdset_n = 12; break;
				case 32: cmdset_n = 50; break;
				case 39: cmdset_n = 51; break;
				case 50: cmdset_n = 32; break;
				case 51: cmdset_n = 39; break;
			}
		}
		if (cmdset_n) {
			int32_t count_i = 0;
			int32_t cmd_f = 0;
			int32_t cmd_arg_i = 0;
			bool pass_f;
			struct lnode *node = NodeBaseGetId(self->cur_node) != NODE_KIND_TRAIL ? (struct lnode *) self->cur_node
				: NULL;
			int32_t node_guide_conv = node ? node->guide_conv : 0;
			bool node_ferry_available = node ? node->ferry_available : false;
			int32_t node_ferry_price = node ? node->ferry_price : 0;
			int32_t node_toll_bridge_price = node ? node->toll_bridge_price : 0;

			struct strrsrc *cmds_m = StrRsrcLoad(g_text_lib, cmdset_n + 10000, true);
			int32_t total_h = StrRsrcCount(cmds_m);
			if (total_h) {
				StrRsrcRemapHighChars(cmds_m, 0);
				for (int32_t row_f = 0; total_h > row_f; row_f++) {
					const char *line_k = StrRsrcGetAt(cmds_m, row_f);
					scan_choice(line_k, &cmd_f, &cmd_arg_i);
					if (greenhorn_gate(self)) {
						if (cmdset_n == 92) {
							if (cmd_f > 2)
								pass_f = true;
							else
								pass_f = false;

						} else {
							if (cmd_f != 2)
								pass_f = true;
							else
								pass_f = false;
						}

					} else {
						pass_f = true;
					}
					if (pass_f) {
						switch (cmd_f) {
							case NODE_CMD_FORD:
								if (!self->cur_node->swiftness)
									pass_f = false;
								break;
							case NODE_CMD_HIRE_INDIANS:
								if (!node_guide_conv)
									pass_f = false;
								break;
							case NODE_CMD_FERRY:
								if (!node_ferry_available)
									pass_f = false;
								break;
						}
					}
					if (pass_f)
						count_i++;
				}
			}
			int32_t off_l = 10 * (8 - count_i);
			rect_offset_v(&ctrl_rect_e, off_l);
			for (int32_t row_f = 0; StrRsrcCount(cmds_m) > row_f; row_f++) {
				line_str_n = StrRsrcGetCStr(cmds_m, row_f);
				const char *line_k = CStrGet(line_str_n);
				scan_choice(line_k, &cmd_f, &cmd_arg_i);
				int32_t n_ = 0;
				do
					;
				while (line_k[n_++] != 9);
				do
					;
				while (line_k[n_++] != 9);
				item_n = CStrCreate();
				CStrSubstr2(line_str_n, n_ + 1, 999, item_n);
				CStrDestroy(&line_str_n);
				pass_f = false;
				if (greenhorn_gate(self)) {
					if (cmd_f < 2 && cmdset_n != 92) {
						CStrDestroy(&item_n);
						item_n = CStrCreateFrom("Ready to go");
						if (cmd_f)
							cmd_f = NODE_CMD_READY_TO_GO;
					}
					if (cmdset_n == 92) {
						if (cmd_f > 2)
							pass_f = true;

					} else if (cmd_f != 2) {
						pass_f = true;
					}

				} else {
					if (cmd_f == 1 || cmd_f == 2) {
						cmd_f--;
						CStrDestroy(&item_n);
						item_n = StrRsrcGetCStr(self->node_names, node_ids_n[cmd_f]);
					}
					pass_f = true;
				}
				if (pass_f) {
					switch (cmd_f) {
						case NODE_CMD_FORD:
							if (!self->cur_node->swiftness)
								pass_f = false;
							break;
						case NODE_CMD_CAULK_AND_FLOAT:
							if (!self->cur_node->swiftness) {
								cmd_f = 9;
								CStrDestroy(&item_n);
								item_n = CStrCreateFrom("Cross on the ice");
							}
							break;
						case NODE_CMD_HIRE_INDIANS:
							if (!node_guide_conv)
								pass_f = false;
							break;
						case NODE_CMD_FERRY:
							if (!node_ferry_available) {
								pass_f = false;

							} else {
								char buf2_m[32];

								int32_t price_i = node_ferry_price;
								int32_t dollars = price_i / 100;
								int32_t cents_b = price_i % 100;
								if (price_i) {
									StrPrintf(buf2_m, " ($%" PRId32 ".%02" PRId32 ")", dollars, cents_b);
									CStrAppendBuf(item_n, buf2_m);

								} else {
									CStrAppendBuf(item_n, " (Free)");
								}
							}
							break;
						case NODE_CMD_PAY_TOLL:
						case NODE_CMD_TOLL_BRIDGE: {
							char buf3_m[32];

							int32_t price2_c = node_toll_bridge_price;
							int32_t dollars2 = price2_c / 100;
							int32_t cents2_d = price2_c % 100;
							if (price2_c) {
								StrPrintf(buf3_m, " ($%" PRId32 ".%02" PRId32 ")", dollars2, cents2_d);
								CStrAppendBuf(item_n, buf3_m);

							} else {
								CStrAppendBuf(item_n, " (Free)");
							}
							break;
						}
					}
				}
				if (pass_f) {
					struct pict_ctrl *ctrl_g = new_choice(cmd_f, &self->base.eh_event, &ctrl_rect_e, item_n);
					ControlSetCommand((struct control *) ctrl_g, GCMD_CHOICE, cmd_f, cmd_arg_i);
					style_choice(ctrl_g);
					VIEW_OPS(picview_f)->AddChild(picview_f, (struct view *) ctrl_g);
					if (!GameEvalChoiceCondition(cmd_arg_i, self->party))
						EVT_HANDLER_OPS(ctrl_g)->ClearHandled((struct evt_handler *) ctrl_g);
					rect_offset_v(&ctrl_rect_e, 30);

				} else {
					CStrDestroy(&item_n);
				}
			}
			StrRsrcDestroy(&cmds_m);
		}
		if (g_ferry_wait_pending) {
			g_ferry_wait_pending = false;
			GameRest(self, 24 * g_ferry_wait_days, 1);
			g_ferry_wait_days = 0;
			GameRiverCrossingAttempt(self, NODE_CMD_FERRY,self->cur_node_id, g_crossing_view, evt);

		} else if (g_window_dirty) {
			redraw_parent();
		}
	}
}

static void enter_node_map(struct game *self)
{
	struct point pt_b = PointMake(22, 14);
	struct view *view_b;

	replace_node_view(self, &view_b);
	self->map_id = self->cur_node->map_id;
	if (self->map_id != self->shown_map_id) {
		self->shown_map_id = self->map_id;
		self->path_len = 1;
		ObjectDelete(self->region);
		self->region = RegionCreate();
		if (self->map_view)
			ObjectDelete(self->map_view);
		self->map_view = MapViewCreate(102, &pt_b, self->map_id, self);
		find_prev_node(self);

	} else {
		if (self->map_view)
			ObjectDelete(self->map_view);
		self->map_view = MapViewCreate(102, &pt_b, self->map_id, self);
	}
	VIEW_OPS(view_b)->AddChild(view_b, (struct view *) self->map_view);
}

void GameEnterNode(struct game *self, int32_t node_id)
{
	struct event handled_ev_m = EventMake(EVCLS_ANY, -1, -1);
	int32_t range_out_h[8];
	int32_t seg_count_c;

	EventSetId(&handled_ev_m, TOOL_SUPPLIES);
	EVT_HANDLER_OPS(self)->SetHandledFor(&self->base, &handled_ev_m);
	if (node_id == 9999)
		node_id = 0;
	enter_state(get_app(), APP_STATE_TRAVELING);
	if (get_app()->ui)
		MainUiShowTravelPanel(get_app()->ui);
	if (NodeBaseGetId(self->cur_node) != NODE_KIND_TRAIL) {
		int32_t range_count_l = NodeBaseListActiveLinks(self->cur_node, self->date, self->prev_node_id,
			range_out_h);
		self->prev_node_id = self->cur_node_id;
		if (range_count_l == 1)
			node_id = 0;
		self->cur_node_id = range_out_h[node_id];
		delete_node(&self->cur_node);
		self->cur_node = new_node(self->cur_node_id);
		apply_scenery_switch(self, self->cur_node);
		if (!GameIsEventAllowed(self, MSG_THIRST))
			reset_thirst(GetAuxMsgqueue());
		g_cond_flag_c0 = g_cond_flag_bc = true;
		WeatherSetRegion(self->weather, self->cur_node->weather_region);
		GameUpdateMusic(self, 0);
		int32_t node_type_i = node_terrain(self);
		if (node_type_i == 2 || node_type_i == 3 || node_type_i == 7)
			self->miles_to_go = -1;
		else
			self->miles_to_go = NodeBaseGetSegCount(self->cur_node);
		seg_count_c = self->miles_to_go;
		NodeBaseListActiveLinks(self->cur_node, self->date, self->prev_node_id, range_out_h);
		node_id = 0;
		self->next_node_id = range_out_h[node_id];

	} else {
		seg_count_c = NodeBaseGetSegCount(self->cur_node);
	}
	if (seg_count_c > 0) {
		EvtHandlerAddIdleHandler(&self->base, false, g_idle_interval);
		self->travel_state = TRAVEL_MOVING;
		enter_node_map(self);
		MapViewUpdateMarker(self->map_view, MAP_MARKER_AT_NODE);
		MapViewUpdateMarker(self->map_view, seg_count_c);
		if (g_window_dirty)
			redraw_parent();

	} else if (!seg_count_c) {
		EvtHandlerAddIdleHandler(&self->base, false, 0);
		self->travel_state = TRAVEL_MOVING;
		enter_node_map(self);
		if (g_window_dirty)
			redraw_parent();
		struct timer_task *idle_m = EvtHandlerFindIdleHandler(&self->base, false);
		EVT_HANDLER_OPS(self)->OnIdleSpare(&self->base, idle_m);

	} else {
		EvtHandlerAddIdleHandler(&self->base, false, 0);
		self->travel_state = TRAVEL_MOVING;
		struct timer_task *idle_m = EvtHandlerFindIdleHandler(&self->base, false);
		EVT_HANDLER_OPS(self)->OnIdleSpare(&self->base, idle_m);
	}
	GameUpdateMusic(self, 0);
}

static bool no_losses(struct game *g)
{
	bool r = g->player_level == LEVEL_GREENHORN;

	if (r && GameIsEventAllowed(g, NODEFEAT_NO_GREENHORN_ALLOWANCE))
		r = false;
	return r;
}

static void log_choice(struct game *self, const char *a, const char *b)
{
	struct cstr *line = NULL;

	switch (RandRangeAvg(0, 1, 1)) {
	case 0:
		line = CStrCreateFrom(a);
		break;
	case 1:
		line = CStrCreateFrom(b);
		break;
	}
	add_journal_line(self, line);
}

void GameRiverCrossingAttempt(struct game *self, int32_t method, int32_t node_id, struct clip_view *crossing_view,
	struct cmd_event *evt)
{
	(void) node_id;

	int32_t outcome_b_j = 0;
	int32_t outcome_a_k = 0;
	int32_t loss_pct_m;
	struct river_situation situation;

	memset(&situation, 0, sizeof(situation));
	int32_t depth_e = self->cur_node->depth;
	int32_t width_o = self->cur_node->width;
	int32_t swiftness_f = self->cur_node->swiftness;
	loss_pct_m = 100;
	bool is_auto_pick_n = false;
	struct event ev_spec_k = evt->spec_b;
	struct event state_ev_p = EventMake(EVCLS_ANY, -1, -1);
	enter_state(get_app(), APP_STATE_TRAVELING);
	EventSetId(&state_ev_p, TOOL_SUPPLIES);
	EVT_HANDLER_OPS(self)->ClearHandledFor(&self->base, &state_ev_p);
	OtiiAppStopSfx(get_app());
	g_crossing_outcome = CROSS_OUTCOME_SUCCESS;
	g_river_method = method;
	for (int32_t ev_idx_e = 0; ev_idx_e < 43; ++ev_idx_e) {
		EventSetId(&ev_spec_k, ev_idx_e + 10000);
		EVT_HANDLER_OPS(self)->ClearHandledFor(&self->base, &ev_spec_k);
	}
	if (g_river_method == NODE_CMD_HIRE_INDIANS) {
		is_auto_pick_n = true;
		g_river_method = RiverGuideMethod(depth_e);
	}
	switch (g_river_method) {
	case NODE_CMD_CAULK_AND_FLOAT:
		log_choice(self, "Decided to caulk the wagon and float it across.",
			"Caulk the wagon and float it across, that is the way to go.");
		break;
	case NODE_CMD_FORD:
		log_choice(self, "Decided to ford the river.", "Fording the river looks to be our best option.");
		break;
	case NODE_CMD_FERRY: {
		double ferry_cost_e = (double) ((struct lnode *) self->cur_node)->ferry_price / 100.0;
		self->party->cash += -ferry_cost_e;
		log_choice(self, "Decided to take advantage of the ferry.", "Given the price, we will take the ferry.");
		break;
	}
	case NODE_CMD_CROSS_ICE: {
		struct inv_list *inventory_i = PartyBuildInventoryList(self->party);
		int32_t total_weight_a = InvListTotalWeight(inventory_i);
		int32_t high_temp_i = WeatherGetHighTemp(self->weather, 0);
		log_choice(self, "Not much else to do, but cross our fingers, and hope the ice holds.",
			"The ice looks like it will hold the wagon... So across we go.");
		situation.ice_weight = total_weight_a;
		situation.ice_long_freeze = WeatherCountFreezingDays(self->weather) > 20;
		situation.ice_high_temp = high_temp_i;
		InvListDestroy(&inventory_i);
		break;
	}
	}
	situation.method = g_river_method;
	situation.depth = depth_e;
	situation.width = width_o;
	situation.swiftness = swiftness_f;
	situation.auto_pick = is_auto_pick_n;
	situation.conditions_checked = g_conditions_checked;
	if (g_river_method == NODE_CMD_FORD || g_river_method == NODE_CMD_CAULK_AND_FLOAT) {
		situation.raining = WeatherSumPrecip(self->weather, 0) != 0;
		int32_t pull_f = self->party->pull_mode;
		situation.strained = pull_f == PULL_SINGLE_STRAINED || pull_f == PULL_TEAM_STRAINED;
		situation.conestoga = InvListFindByType(&self->party->wagons, g_item_conestoga) != NULL;
	}
	situation.riverwork = self->party->skills[SKILL_RIVERWORK];
	int32_t success_chance_i = RiverSuccessChance(&situation);
	int32_t roll_k = RandRangeAvg(1, 100, 1);
	g_crossing_outcome = CROSS_OUTCOME_SUCCESS;
	if (roll_k > success_chance_i) {
		roll_k = RandRangeAvg(1, 100, 1);
		RiverFailure(g_river_method, depth_e, swiftness_f, &outcome_b_j, &outcome_a_k, &loss_pct_m);
		g_crossing_outcome = outcome_b_j;
		if (roll_k > loss_pct_m)
			g_crossing_outcome = outcome_a_k;
	}
	int32_t scene_id_h = LNodeSelectCrossingPicture((struct lnode *) self->cur_node, g_river_method,
		g_crossing_outcome, self->scenery_switch != 0);
	sound_server_pause();
	redraw_parent();
	struct view *cv = (struct view *) crossing_view;
	struct rect bounds = VIEW_OPS(cv)->GetBounds(cv);
	struct mci_video *video = MciVideoCreate(-2, &self->base.eh_event, &bounds, scene_id_h, false, false);
	g_active_scene = (struct view *) video;
	ControlSetCommand(&video->base, CMD_MEDIA_DONE, 0, 0);
	VIEW_OPS(cv)->AddChild(cv, g_active_scene);
	if (!SettingsSoundEffectsOn(get_app()->settings))
		MCI_VIDEO_OPS(video)->SetVolume(video, 0);
	MCI_VIDEO_OPS(video)->Play(video);
	int32_t h = CMD_MEDIA_DONE;
	EvtHandlerBindHandler(&self->base, 1, &h);
	if (!no_losses(self) && g_crossing_outcome && RandRangeAvg(0, 1, 1)) {
		int32_t wagons_lost_o = 0;
		int32_t people_lost_d = 0;
		int32_t lost_min_j;
		int32_t lost_max_j;
		if (self->train_wagons > 1) {
			lost_min_j = 1;
			lost_max_j = (self->train_wagons + 4) / 5;
			if (lost_max_j > 15)
				lost_max_j = 15;
			wagons_lost_o = RandRangeAvg(lost_min_j, lost_max_j, 2);
		}
		if (self->train_people > wagons_lost_o && self->train_people > 8) {
			lost_min_j = wagons_lost_o;
			lost_max_j = 3 * wagons_lost_o;
			people_lost_d = RandRangeAvg(lost_min_j, lost_max_j, 2);
		}
		if (wagons_lost_o && people_lost_d) {
			char msg_a[100];

			StrPrintf(msg_a, "Our train lost %" PRId32 " wagons and %" PRId32 " people crossing the river today.",
				wagons_lost_o, people_lost_d);
			self->train_wagons -= wagons_lost_o;
			self->train_people -= people_lost_d;
			add_journal_line(self, CStrCreateFrom(msg_a));
		}
	}
}

void GameRun(struct game *self, struct msg *msg)
{
	struct event handled_ev_m = EventMake(EVCLS_ANY, -1, -1);
	int32_t asset_id_j = 0;

	if (!msg)
		msg = self->event_node;
	if (!msg)
		return;
	EventSetId(&handled_ev_m, TOOL_SUPPLIES);
	EVT_HANDLER_OPS(self)->ClearHandledFor(&self->base, &handled_ev_m);
	int32_t code = msg->number_index;
	enter_state(get_app(), APP_STATE_EVENT);
	struct strrsrc *rsrc_k = StrRsrcLoad(g_text_lib, code + 11000, true);
	StrRsrcRemapHighChars(rsrc_k, 0);
	struct cstr *line_str_l = StrRsrcGetCStr(rsrc_k, 0);
	const char *line_k = CStrGet(line_str_l);
	(void) sscanf(line_k, "%" SCNd32, &asset_id_j);
	int32_t scan_j = 0;
	do
		;
	while (line_k[scan_j++] != 9);
	CStrSubstr(line_str_l, scan_j + 1, 999);
	if (asset_id_j == -1) {
		int32_t slots_c[10] = {0};

		const char *row_a = StrRsrcGetAt(rsrc_k, 1);
		int32_t count_g = 0;
		(void) sscanf(row_a, "%" SCNd32 " %" SCNd32 " %" SCNd32 " %" SCNd32 " %" SCNd32 " %" SCNd32 " %" SCNd32
			" %" SCNd32 " %" SCNd32 " %" SCNd32, &slots_c[0], &slots_c[1], &slots_c[2], &slots_c[3], &slots_c[4],
			&slots_c[5], &slots_c[6], &slots_c[7], &slots_c[8], &slots_c[9]);
		for (int32_t slot_o = 0; slot_o < 10 && slots_c[slot_o] > 0; ++slot_o) {
			if (slots_c[slot_o] != -1)
				++count_g;
		}
		if (count_g) {
			int32_t slot_o = msg->scene_variant % count_g;
			asset_id_j = slots_c[slot_o];
		}
	}
	StrRsrcDestroy(&rsrc_k);
	self->event_node = msg;
	GameUpdateMusic(self, 0);
	EvtHandlerRemoveIdleHandler(&self->base, false);
	self->travel_state = TRAVEL_STOPPED;
	if (NodeBaseGetId(self->cur_node) == NODE_KIND_TRAIL && !self->miles_to_go)
		self->miles_to_go = 1;
	if (msg->title) {
		CStrDestroy(&line_str_l);
		line_str_l = CStrCreateCopy(msg->title);
	}
	if (asset_id_j >= 0) {
		if (GameGetSeason(self) == SEASON_SNOW)
			asset_id_j += 500;
		GameShowEventDialog(self, code + 11000, RegionalAssetId1000(), asset_id_j + 20000, line_str_l, self);

	} else {
		GameShowEventDialog(self, code + 11000, asset_id_j, 0, line_str_l, self);
	}
	if (msg->payload) {
		if (get_app()->ui)
			MainUiShowEventPanel(get_app()->ui);
	}
}

struct view *GameShowEventDialog(struct game *self, int32_t rsrc_id, int32_t asset_id, int32_t sub_id,
	struct cstr *line_str_m, struct game *game)
{
	int32_t condition_id_p = 0;
	int32_t choice_id_l = 0;
	struct view *view_c;
	struct view *frame_view_l;
	bool disabled_g;
	struct rect choice_rect_i = RectMake(90, 390, 110, 595);
	struct rect text_rect_d = RectMake(30, 380, 80, 615);
	struct rect pic_rect_j;
	struct point frame_pos_j;
	struct point pic_size_l;

	replace_node_view(self, &view_c);
	struct drawable *picture_m = NULL;
	if (sub_id) {
		picture_m = create_virtual_packed(asset_id, sub_id);

	} else if (asset_id > 0) {
		picture_m = create_virtual(asset_id);

	} else if (asset_id == -2) {
		pic_rect_j = RectMake(0, 0, 320, 320);
		rect_offset(&pic_rect_j, 52, 34);
		struct mci_video *video = MciVideoCreate(-2, &self->base.eh_event, &pic_rect_j, rsrc_id - 8000, false, true);
		g_active_scene = (struct view *) video;
		VIEW_OPS(view_c)->AddChild(view_c, g_active_scene);
		if (!SettingsSoundEffectsOn(get_app()->settings))
			MCI_VIDEO_OPS(video)->SetVolume(video, 0);
		MCI_VIDEO_OPS(video)->Play(video);
	}
	if (picture_m) {
		OBJ_BASE_OPS(picture_m)->GetSize((struct obj_base *) picture_m, &pic_size_l);
		pic_rect_j = RectMake(0, 0, pic_size_l.v, pic_size_l.h);
		rect_offset(&pic_rect_j, 52, 34);
		frame_view_l = (struct view *) PicViewCreateImage(-2, &pic_rect_j, picture_m);
		VIEW_OPS(view_c)->AddChild(view_c, frame_view_l);
	}
	frame_pos_j.h = 22;
	frame_pos_j.v = 14;
	frame_view_l = (struct view *) PicViewCreatePoint(-2, &frame_pos_j, 140);
	VIEW_OPS(view_c)->AddChild(view_c, frame_view_l);
	frame_pos_j.h = 22;
	frame_pos_j.v = 34;
	frame_view_l = (struct view *) PicViewCreatePoint(-2, &frame_pos_j, 141);
	VIEW_OPS(view_c)->AddChild(view_c, frame_view_l);
	frame_pos_j.h = 22;
	frame_pos_j.v = 354;
	frame_view_l = (struct view *) PicViewCreatePoint(-2, &frame_pos_j, 142);
	VIEW_OPS(view_c)->AddChild(view_c, frame_view_l);
	frame_pos_j.h = 372;
	frame_pos_j.v = 14;
	struct view *choice_view_n = (struct view *) PicViewCreatePoint(-2, &frame_pos_j, 143);
	VIEW_OPS(view_c)->AddChild(view_c, choice_view_n);
	struct view *text_view_k = (struct view *) TextViewCreate(-2, &text_rect_d, line_str_m, 2, 1, 1, 0, 0, 0, 0,
		255, 0, 1);
	VIEW_OPS(view_c)->AddChild(view_c, text_view_k);
	struct strrsrc *event_rsrc_a = StrRsrcLoad(g_text_lib, rsrc_id, true);
	StrRsrcRemapHighChars(event_rsrc_a, 0);
	int32_t title_shift_l = 10 * (10 - StrRsrcCount(event_rsrc_a));
	choice_rect_i.top += title_shift_l;
	choice_rect_i.bottom += title_shift_l;
	for (int32_t row_f = 2; StrRsrcCount(event_rsrc_a) > row_f; ++row_f) {
		line_str_m = StrRsrcGetCStr(event_rsrc_a, row_f);
		int32_t scan_k = 0;
		const char *line_k = CStrGet(line_str_m);
		do
			;
		while (line_k[scan_k++] != 9);
		do
			;
		while (line_k[scan_k++] != 9);
		struct cstr *choice_text_n = CStrCreate();
		CStrSubstr2(line_str_m, scan_k + 1, 999, choice_text_n);
		line_k = CStrGet(line_str_m);
		scan_choice(line_k, &choice_id_l, &condition_id_p);
		CStrDestroy(&line_str_m);
		struct pict_ctrl *choice_ctrl_g = new_choice(choice_id_l, &game->base.eh_event, &choice_rect_i,
			choice_text_n);
		ControlSetCommand((struct control *) choice_ctrl_g, GCMD_CHOICE, choice_id_l, condition_id_p);
		style_choice(choice_ctrl_g);
		VIEW_OPS(choice_view_n)->AddChild(choice_view_n, (struct view *) choice_ctrl_g);
		if (choice_id_l < 1000) {
			if (choice_id_l < 9)
				disabled_g = !(choice_id_l >= 0 && self->event_node->flags[choice_id_l]);
			else
				disabled_g = false;
			if (!GameEvalChoiceCondition(condition_id_p, self->party))
				disabled_g = true;

		} else {
			disabled_g = false;
		}
		if (disabled_g)
			EVT_HANDLER_OPS(choice_ctrl_g)->ClearHandled((struct evt_handler *) choice_ctrl_g);
		choice_rect_i.top += 30;
		choice_rect_i.bottom += 30;
	}
	StrRsrcDestroy(&event_rsrc_a);
	if (g_window_dirty)
		redraw_parent();
	return view_c;
}

bool GameIsEventAllowed(struct game *self, int32_t cmd)
{
	if (NodeBaseGetId(self->cur_node) == NODE_KIND_TRAIL) {
		if (node_terrain(self) == TERRAIN_RAFT)
			return false;
	}
	if (get_app()->app_state == APP_STATE_SUPPLIES)
		return false;
	int32_t food = PartyCountFood(self->party);
	if (self->travel_state == TRAVEL_RESTING || self->travel_state == 2) {
		switch (cmd) {
			case MSG_CUTS_AND_ABRASIONS:
			case MSG_BROKEN_BONE:
			case MSG_SPRAIN:
			case MSG_CHOLERA:
			case MSG_BAD_COLD:
			case MSG_CONCUSSION:
			case MSG_DYSENTERY:
			case MSG_THE_GRIPPE:
			case MSG_MALARIA:
			case MSG_MEASLES:
			case MSG_PNEUMONIA:
			case MSG_BERIBERI:
			case MSG_DIPHTHERIA:
			case MSG_FROSTBITE:
			case MSG_GANGRENE:
			case MSG_SCURVY:
			case MSG_SMALLPOX:
			case MSG_SNAKEBITE:
			case MSG_TYPHOID_FEVER:
			case MSG_RABIES:
			case MSG_TYPHUS:
			case MSG_SCARLET_FEVER:
			case MSG_FOOD_POISONING:
			case MSG_INFECTION:
			case MSG_CONSUMPTION:
			case MSG_BURNS:
			case MSG_LOW_MORALE:
			case MSG_DISMISSED:
			case MSG_VERY_LOW_MORALE:
			case MSG_FIRED:
			case MSG_MEMBER_DIED:
			case MSG_LOCKJAW:
			case MSG_INTERNAL_INJURIES:
			case MSG_WOUND:
			case MSG_LEADER_DIED:
			case MSG_RAFTING_LOSSES:
			case 1010:
				return true;
			case MSG_CAUGHT_FISH:
				if (g_ferry_wait_days)
					return false;
				for (int32_t j = 0; self->cur_node->event_count > j; ++j) {
					if (self->cur_node->events[j] == cmd)
						return true;
				}
				return false;
		}
		return false;
	}
	switch (cmd) {
		case MSG_BROKEN_WHEEL:
		case MSG_BROKEN_AXLE:
		case MSG_BROKEN_TONGUE:
		case MSG_MISSING_ANIMAL:
		case MSG_MISSING_ANIMALS:
		case MSG_SICK_LIVESTOCK:
		case MSG_INJURED_LIVESTOCK:
		case MSG_ABANDONED_WAGON:
		case MSG_TIPPED_WAGON:
		case MSG_ANIMAL_DIED_ACCIDENT:
		case MSG_ANIMAL_DIED_ILLNESS:
		case MSG_STUCK_IN_MUD:
		case MSG_STUCK_IN_SAND:
		case MSG_HILL_TOO_STEEP:
		case MSG_ANIMALS_EXHAUSTED:
		case MSG_THICK_DUST:
		case MSG_ANIMAL_IN_HOLE:
		case MSG_BROKEN_YOKE:
		case 1000:
		case 1001:
		case 1005:
		case 1009:
			if (food)
				return true;
			else
				return false;
		case MSG_BLIZZARD:
		case MSG_SNOWBOUND:
		case MSG_THUNDERSTORM:
		case MSG_HAILSTORM:
		case MSG_HEAVY_FOG:
		case MSG_FLOODED_TRAIL:
		case MSG_MISSING_PERSON:
		case MSG_WILD_FRUIT:
		case MSG_LOCUSTS:
		case MSG_EXTREME_HEAT:
		case MSG_EXTREME_COLD:
		case MSG_ACCIDENTAL_GUNSHOT:
		case MSG_WAGON_FIRE:
		case MSG_THEFT:
		case MSG_CANNOT_PAY_TOLL:
		case MSG_INDEPENDENCE_DAY:
		case MSG_SUNDAY:
		case MSG_CUTS_AND_ABRASIONS:
		case MSG_BROKEN_BONE:
		case MSG_SPRAIN:
		case MSG_LOST_SUPPLIES:
		case MSG_CHOLERA:
		case MSG_BAD_COLD:
		case MSG_CONCUSSION:
		case MSG_DYSENTERY:
		case MSG_THE_GRIPPE:
		case MSG_MALARIA:
		case MSG_MEASLES:
		case MSG_PNEUMONIA:
		case MSG_BERIBERI:
		case MSG_DIPHTHERIA:
		case MSG_FROSTBITE:
		case MSG_GANGRENE:
		case MSG_SCURVY:
		case MSG_SMALLPOX:
		case MSG_SNAKEBITE:
		case MSG_TYPHOID_FEVER:
		case MSG_RABIES:
		case MSG_TYPHUS:
		case MSG_SCARLET_FEVER:
		case MSG_FOOD_POISONING:
		case MSG_INFECTION:
		case MSG_CONSUMPTION:
		case MSG_BURNS:
		case MSG_DROWNING:
		case MSG_ELECTED_CAPTAIN:
		case MSG_LOW_MORALE:
		case MSG_DISMISSED:
		case MSG_REELECTED:
		case MSG_HIRED_AS_GUIDE:
		case MSG_VERY_LOW_MORALE:
		case MSG_FIRED:
		case MSG_TRAIN_SPLITTING:
		case MSG_WAGONS_JOIN:
		case MSG_MEMBER_DIED:
		case MSG_FREEZING:
		case MSG_STARVATION:
		case MSG_LOCKJAW:
		case MSG_INTERNAL_INJURIES:
		case MSG_WOUND:
		case MSG_WILD_VEGETABLES:
		case MSG_RAFTING_LOSSES:
		case 1002:
		case 1003:
		case MSG_UNKNOWN_AILMENT:
		case 1008:
		case 1010:
			return true;
	}
	for (int32_t i = 0; self->cur_node->event_count > i; ++i) {
		if (self->cur_node->events[i] == cmd)
			return true;
	}
	return false;
}

static int32_t node_speaker_conv(struct node_base *node, int32_t index)
{
	if (!node || index < 0)
		return 0;
	int32_t count = node->conv_count < 70 ? node->conv_count : 70;
	if (count <= 0)
		return 0;
	index %= count;
	return node->conv_table[index] > 0 ? node->conv_table[index] : 0;
}

#ifdef OTII_TEST
int32_t GameTestNodeSpeakerConv(struct node_base *node, int32_t index)
{
	return node_speaker_conv(node, index);
}
#endif

static bool has_contagion_free_party(void)
{
	bool ok_c = true;
	struct party *party_a = GetGameObj()->party;

	for (int32_t index_k = 0; ok_c && PartyMemberCount(party_a, false) > index_k; ++index_k) {
		struct member *member = PartyGetMember(party_a, index_k);
		if (member) {
			if (member->state == MEMBER_ALIVE) {
				if (MemberHasAilment(member, AILMENT_CHOLERA))
					ok_c = false;
				else if (MemberHasAilment(member, AILMENT_DIPHTHERIA))
					ok_c = false;
				else if (MemberHasAilment(member, AILMENT_DYSENTERY))
					ok_c = false;
				else if (MemberHasAilment(member, AILMENT_MEASLES))
					ok_c = false;
				else if (MemberHasAilment(member, AILMENT_SCARLET_FEVER))
					ok_c = false;
				else if (MemberHasAilment(member, AILMENT_SMALLPOX))
					ok_c = false;
				else if (MemberHasAilment(member, AILMENT_TYPHOID_FEVER))
					ok_c = false;
				else if (MemberHasAilment(member, AILMENT_TYPHUS))
					ok_c = false;
				if (!ok_c) {
					struct cstr *status_m = MemberGetStatusText(member);
					if (!strncmp(CStrGet(status_m), "feeling", 7))
						ok_c = true;
					CStrDestroy(&status_m);
				}
			}
		}
	}
	return ok_c;
}

static bool has_gun(void)
{
	if (InvListFindByType(&GetGameObj()->party->inventory, g_item_rifle))
		return true;
	if (InvListFindByType(&GetGameObj()->party->inventory, g_item_shotgun))
		return true;
	if (InvListFindByType(&GetGameObj()->party->inventory, g_item_pistol))
		return true;
	return false;
}

bool GameEvalChoiceCondition(int32_t condition, struct party *caller_party)
{
	double price_b_k;
	double price_a_l;

	if (!condition || !caller_party)
		return true;
	if (NodeBaseGetId(GetGameObj()->cur_node) != NODE_KIND_TRAIL) {
		price_a_l = (double) ((struct lnode *) GetGameObj()->cur_node)->ferry_price / 100.0;
		price_b_k = (double) ((struct lnode *) GetGameObj()->cur_node)->toll_bridge_price / 100.0;

	} else {
		price_a_l = price_b_k = 0.0;
	}
	if (condition < 0) {
		struct end_game *end_score;

		switch (condition) {
		case COND_ALWAYS:
			return true;
		case COND_CAN_AFFORD_FERRY:
			if (caller_party->cash >= price_a_l)
				return true;
			else
				return false;
		case COND_CAN_AFFORD_TOLL:
			if (caller_party->cash >= price_b_k)
				return true;
			else
				return false;
		case COND_CAN_AFFORD_RAFTERS:
			if (caller_party->cash >= 80.0)
				return true;
			else
				return false;
		case COND_HAS_GUN:
			return has_gun();
		case COND_NO_LAND_CLAIM:
		case COND_NO_GOLD_CLAIM:
			end_score = GetGameObj()->end_score;
			if (!end_score)
				return true;
			else if (!END_GAME_OPS(end_score)->GetResultMode(end_score))
				return true;
			else
				return false;
		case COND_NO_CONTAGION:
			return has_contagion_free_party();
		case COND_NO_RAFTERS_HIRED:
			return GetGameObj()->hired_rafters == 0;
		case COND_HILL_BELOW_4:
			return GetGameObj()->cur_node->steepness < 4;
		case COND_HILL_BELOW_5:
			return GetGameObj()->cur_node->steepness < 5;
		case COND_HAY_AVAILABLE:
			if (g_cond_flag_c0)
				return GetGameObj()->weather->snow_depth == 0;
			else
				return false;
		case COND_RUINS_UNCHECKED:
			return g_cond_flag_bc;
		}

	} else {
		struct strrsrc *rsrc_h = StrRsrcLoad(g_text_lib, condition + 10200, true);
		struct inv_list *inv_j = &caller_party->inventory;
		if (!rsrc_h || !inv_j)
			return false;
		StrRsrcRemapHighChars(rsrc_h, 0);
		for (int32_t i_c = 0; StrRsrcCount(rsrc_h) > i_c; ++i_c) {
			struct cstr *name_k = StrRsrcGetCStr(rsrc_h, i_c);
			if (InvListFindByName(inv_j, name_k)) {
				StrRsrcDestroy(&rsrc_h);
				CStrDestroy(&name_k);
				return true;
			}
			CStrDestroy(&name_k);
		}
		StrRsrcDestroy(&rsrc_h);
		return false;
	}
	return true;
}

static bool game_check_event_condition(int32_t condition, struct party *caller_party)
{
	double price_b_k;
	double price_a_l;

	if (!condition || !caller_party)
		return true;
	if (NodeBaseGetId(GetGameObj()->cur_node) != NODE_KIND_TRAIL) {
		price_a_l = (double) ((struct lnode *) GetGameObj()->cur_node)->ferry_price / 100.0;
		price_b_k = (double) ((struct lnode *) GetGameObj()->cur_node)->toll_bridge_price / 100.0;

	} else {
		price_a_l = price_b_k = 0.0;
	}
	if (condition < 0) {
		switch (condition) {
		case COND_ALWAYS:
			return true;
		case COND_CAN_AFFORD_FERRY:
			if (caller_party->cash >= price_a_l)
				return true;
			else
				return false;
		case COND_CAN_AFFORD_TOLL:
			if (caller_party->cash >= price_b_k) {
				caller_party->cash += -price_b_k;
				return true;

			} else {
				return false;
			}
		case COND_CAN_AFFORD_RAFTERS:
			if (caller_party->cash >= 80.0) {
				caller_party->cash += -80.0;
				return true;

			} else {
				return false;
			}
		case COND_HAS_GUN:
			return has_gun();
		case COND_NO_LAND_CLAIM:
			return true;
		case COND_NO_GOLD_CLAIM:
			return true;
		case COND_NO_CONTAGION:
			return has_contagion_free_party();
		case COND_NO_RAFTERS_HIRED:
			return GetGameObj()->hired_rafters == 0;
		case COND_HILL_BELOW_4:
			return GetGameObj()->cur_node->steepness < 4;
		case COND_HILL_BELOW_5:
			return GetGameObj()->cur_node->steepness < 5;
		case COND_HAY_AVAILABLE:
			if (g_cond_flag_c0)
				return GetGameObj()->weather->snow_depth == 0;
			else
				return false;
		case COND_RUINS_UNCHECKED:
			return g_cond_flag_bc;
		}

	} else {
		struct strrsrc *rsrc_h = StrRsrcLoad(g_text_lib, condition + 10200, true);
		struct inv_list *inv_j = &caller_party->inventory;
		if (!rsrc_h || !inv_j)
			return false;
		StrRsrcRemapHighChars(rsrc_h, 0);
		for (int32_t i_c = 0; StrRsrcCount(rsrc_h) > i_c; ++i_c) {
			struct cstr *name_k = StrRsrcGetCStr(rsrc_h, i_c);
			struct inv_item *item_n = InvListFindByName(inv_j, name_k);
			if (item_n) {
				InvItemConsumePortions(item_n, 1);
				CStrDestroy(&name_k);
				StrRsrcDestroy(&rsrc_h);
				return true;
			}
			CStrDestroy(&name_k);
		}
		StrRsrcDestroy(&rsrc_h);
		return false;
	}
	return true;
}

void GameUpdateCmdFlags(struct game *self)
{
	int32_t precip_amount_k;
	int32_t precip_rate_h = 0;

	int32_t precip_type_i = WeatherGetPrecipType(self->weather, &precip_rate_h, 0);
	if (!precip_type_i)
		precip_type_i = self->weather->snow_depth;
	if (GameIsEventAllowed(self, NODEFEAT_NO_GRASS))
		MainUiSetCatFlag(CAT_NO_GRASS);
	else
		MainUiClearCatFlag(CAT_NO_GRASS);

	if (!precip_type_i && GameIsEventAllowed(self, MSG_THIRST) && get_app()->app_state != APP_STATE_SCENE)
		MainUiSetCatFlag(CAT_NO_WATER);
	else
		MainUiClearCatFlag(CAT_NO_WATER);
	if (GameIsEventAllowed(self, MSG_ALKALI_SICKNESS))
		MainUiSetCatFlag(CAT_ALKALI_WATER);
	else
		MainUiClearCatFlag(CAT_ALKALI_WATER);
	if (GameIsEventAllowed(self, MSG_WATER_POISONING))
		MainUiSetCatFlag(CAT_BAD_WATER);
	else
		MainUiClearCatFlag(CAT_BAD_WATER);
	if (NodeBaseGetId(self->cur_node) == NODE_KIND_TRAIL)
		precip_amount_k = node_terrain(self);
	else
		precip_amount_k = 0;
	if (precip_amount_k == 1)
		MainUiSetCatFlag(CAT_DEEP_SAND);
	else
		MainUiClearCatFlag(CAT_DEEP_SAND);
	if (precip_amount_k == 7)
		MainUiSetCatFlag(CAT_DESERT);
	else
		MainUiClearCatFlag(CAT_DESERT);
	if (precip_amount_k == 4)
		MainUiSetCatFlag(CAT_ROUGH_TRAIL);
	else
		MainUiClearCatFlag(CAT_ROUGH_TRAIL);
	if (self->travel_state == 2)
		MainUiSetCatFlag(22);
	else
		MainUiClearCatFlag(22);
	if (self->travel_state == TRAVEL_RESTING)
		MainUiSetCatFlag(CAT_RESTING);
	else
		MainUiClearCatFlag(CAT_RESTING);
}

void GameSetTravelState(struct game *self, int32_t state)
{
	self->travel_state = state;
}

void GameNextNode(struct game *self)
{
	int32_t nodes_k[8];

	if (PartyCalcPullingMode(self->party)) {
		if (NodeBaseGetId(self->cur_node) == NODE_KIND_TRAIL) {
			int32_t prev_node_m = self->prev_node_id;
			int32_t saved_n = self->miles_to_go;
			self->prev_node_id = self->next_node_id;
			self->next_node_id = prev_node_m;
			self->shown_map_id = -1;
			GameEnterNode(self, 0);
			self->miles_to_go = self->miles_to_go - saved_n;

		} else {
			int32_t want_node_g = self->prev_node_id;
			self->prev_node_id = self->cur_node_id;
			int32_t count_k = NodeBaseListActiveLinks(self->cur_node, self->date, self->prev_node_id, nodes_k);
			int32_t found_h = 0;
			for (int32_t slot_l = 0; count_k > slot_l; ++slot_l) {
				if (nodes_k[slot_l] == want_node_g) {
					found_h = slot_l;
					slot_l = count_k;
				}
			}
			self->shown_map_id = -1;
			GameEnterNode(self, found_h);
		}
	}
}

#define RNG_SAVE_OFFSET 480
#define RNG_SAVE_BLOCK  (RNG_SAVE_OFFSET + 16)
#define GAME_SAVE_WORDS 42
#define RNG_BLOCK_SIZE  12

static bool rng_read_tail(const uint8_t *block, int32_t size, uint32_t *seed, uint64_t *calls)
{
	if (size < RNG_SAVE_BLOCK || memcmp(block + RNG_SAVE_OFFSET, "RNG1", 4) != 0)
		return false;
	*seed = ReadLeU32(block + RNG_SAVE_OFFSET + 4);
	*calls = ReadLeU64(block + RNG_SAVE_OFFSET + 8);
	return true;
}

void GameSaveState(struct game *self, struct doc_file *file)
{
	int32_t path_words = self->path_len > 0 ? self->path_len : 0;
	if (path_words > GAME_PATH_MAX)
		path_words = GAME_PATH_MAX;
	int32_t size = 2 * (GAME_SAVE_WORDS + (self->path_len > 0 ? self->path_len : 0));
	uint8_t *blk_a = MemAlloc(size, 1);
	int32_t wp_m = 0;

	const int32_t words[GAME_SAVE_WORDS] = {
		self->player_level,
		self->days_traveled,
		self->event_days,
		self->miles_today,
		self->total_miles,
		self->miles_to_go,
		self->end_node_id,
		self->start_town,
		self->prev_node_id,
		self->cur_node_id,
		self->next_node_id,
		self->destination,
		self->route_index,
		self->map_id,
		self->shown_map_id,
		self->travel_pace,
		self->travel_mod,
		self->daily_mod,
		self->train_wagons,
		self->train_people,
		self->train_wagons_start,
		self->train_people_start,
		(uint8_t) self->unused_C1E,
		(uint8_t) self->hired_rafters,
		self->date->year,
		self->date->month,
		self->date->day,
		self->start_date->year,
		self->start_date->month,
		self->start_date->day,
		self->travel_state,
		self->day_tick,
		self->score_multiplier,
		(uint8_t) self->scenery_switch,
		g_logged_100_miles,
		g_logged_500_miles,
		g_logged_1000_miles,
		g_logged_100_days,
		g_logged_200_days,
		(uint8_t) g_msgq_ready,
		self->mile_frac,
		self->path_len,
	};
	for (int32_t i = 0; i < GAME_SAVE_WORDS; i++, wp_m += 2)
		WriteLe16(blk_a + wp_m, words[i]);
	for (int32_t idx_h = 0; path_words > idx_h; ++idx_h, wp_m += 2)
		WriteLe16(blk_a + wp_m, self->path_nodes[idx_h]);
	DocFileWriteObject(file, blk_a, size);
	MemFree(blk_a);
	{
		uint8_t rng_blk[RNG_BLOCK_SIZE];
		uint32_t seed = RngSeed();
		uint64_t calls = RngCalls();

		WriteLe32(rng_blk, seed);
		WriteLeU64(rng_blk + 4, calls);
		DocFileWriteObject(file, rng_blk, RNG_BLOCK_SIZE);
	}
	uint8_t *flags_blk = MemAlloc(1250, 1);
	for (int32_t idx_h = 0; idx_h < 1200; ++idx_h)
		flags_blk[idx_h] = g_slot_flags[idx_h];
	DocFileWriteObject(file, flags_blk, 1250);
	MemFree(flags_blk);
	WeatherSaveState(self->weather, file);
	PartySaveState(self->party, file);
	RegionOnSave(self->region, file->stream);
}

static bool g_loaded_rng_pending;
static uint32_t g_loaded_rng_seed;
static uint64_t g_loaded_rng_calls;

static int32_t block_word(const uint8_t *blk, int32_t size, int32_t index)
{
	if (2 * index + 2 > size)
		return 0;
	return ReadLe16(blk + 2 * index);
}

static uint8_t block_byte(const uint8_t *blk, int32_t size, int32_t index)
{
	if (2 * index + 1 > size)
		return 0;
	return blk[2 * index];
}

void GameLoadState(struct game *self, struct doc_file *file)
{
	void *mem = NULL;
	int32_t size = 0;
	uint32_t rng_seed_l = 0;
	uint64_t rng_calls_l = 0;
	bool rng_saved_l;
	int32_t w = 0;

	g_ferry_wait_pending = false;
	g_ferry_wait_days = 0;
	DocFileReadObject2(file, &mem, &size);
	const uint8_t *blk = mem;
	if ((uint16_t) file->version < SAVE_FORMAT_2_0)
		rng_saved_l = rng_read_tail(blk, size, &rng_seed_l, &rng_calls_l);
	else
		rng_saved_l = false;
	self->player_level = block_word(blk, size, w++);
	self->days_traveled = block_word(blk, size, w++);
	self->event_days = block_word(blk, size, w++);
	self->miles_today = block_word(blk, size, w++);
	self->total_miles = block_word(blk, size, w++);
	self->miles_to_go = block_word(blk, size, w++);
	self->end_node_id = block_word(blk, size, w++);
	self->start_town = block_word(blk, size, w++);
	self->prev_node_id = block_word(blk, size, w++);
	self->cur_node_id = block_word(blk, size, w++);
	self->next_node_id = block_word(blk, size, w++);
	self->destination = block_word(blk, size, w++);
	self->route_index = block_word(blk, size, w++);
	self->map_id = block_word(blk, size, w++);
	self->shown_map_id = block_word(blk, size, w++);
	self->travel_pace = block_word(blk, size, w++);
	self->travel_mod = block_word(blk, size, w++);
	self->daily_mod = block_word(blk, size, w++);
	self->train_wagons = block_word(blk, size, w++);
	self->train_people = block_word(blk, size, w++);
	self->train_wagons_start = block_word(blk, size, w++);
	self->train_people_start = block_word(blk, size, w++);
	self->unused_C1E = block_byte(blk, size, w++);
	self->hired_rafters = block_byte(blk, size, w++);
	self->date = MemAlloc(1, sizeof(struct date));
	DateInit(self->date);
	DateSetYear(self->date, block_word(blk, size, w++));
	DateSetMonth(self->date, block_word(blk, size, w++));
	DateSetDay(self->date, block_word(blk, size, w++));
	self->start_date = MemAlloc(1, sizeof(struct date));
	DateInit(self->start_date);
	DateSetYear(self->start_date, block_word(blk, size, w++));
	DateSetMonth(self->start_date, block_word(blk, size, w++));
	DateSetDay(self->start_date, block_word(blk, size, w++));
	self->travel_state = block_word(blk, size, w++);
	self->day_tick = block_word(blk, size, w++);
	self->score_multiplier = block_word(blk, size, w++);
	self->scenery_switch = block_byte(blk, size, w++);
	g_logged_100_miles = block_byte(blk, size, w++);
	g_logged_500_miles = block_byte(blk, size, w++);
	g_logged_1000_miles = block_byte(blk, size, w++);
	g_logged_100_days = block_byte(blk, size, w++);
	g_logged_200_days = block_byte(blk, size, w++);
	g_msgq_ready = block_byte(blk, size, w++) != 0;
	self->mile_frac = block_word(blk, size, w++);
	self->path_len = block_word(blk, size, w++);
	if (self->path_len < 0)
		self->path_len = 0;
	if (self->path_len > 200)
		self->path_len = 200;
	if (self->path_len > size / 2 - GAME_SAVE_WORDS)
		self->path_len = size / 2 - GAME_SAVE_WORDS > 0 ? size / 2 - GAME_SAVE_WORDS : 0;
	for (int32_t idx_m = 0; self->path_len > idx_m; ++idx_m)
		self->path_nodes[idx_m] = block_word(blk, size, w++);
	if ((uint16_t) file->version >= SAVE_FORMAT_2_0) {
		DocFileReadObject2(file, &mem, &size);
		if (size >= RNG_BLOCK_SIZE) {
			const uint8_t *rng_blk = mem;
			rng_seed_l = ReadLeU32(rng_blk);
			rng_calls_l = ReadLeU64(rng_blk + 4);
			rng_saved_l = true;
		}
	}
	DocFileReadObject2(file, &mem, &size);
	const uint8_t *flags_j = mem;
	for (int32_t idx_m = 0; idx_m < 1200; ++idx_m)
		g_slot_flags[idx_m] = idx_m < size ? flags_j[idx_m] : 0;
	MemFree(mem);
	WeatherLoadState(self->weather, file);
	self->party = PartyCreate();
	PartyLoadState(self->party, file);
	g_conditions_checked = false;
	self->cur_node = new_lnode(self->cur_node_id);
	if (NodeBaseGetId(self->cur_node) == NODE_KIND_NONE) {
		delete_node(&self->cur_node);
		self->cur_node = new_node(self->cur_node_id);
	}
	WeatherSetRegion(self->weather, self->cur_node->weather_region);
	self->map_id = self->cur_node->map_id;
	RegionOnLoad(self->region, file->stream);
	g_loaded_rng_pending = rng_saved_l;
	g_loaded_rng_seed = rng_seed_l;
	g_loaded_rng_calls = rng_calls_l;
}

void GameRestoreLoadedRng(void)
{
	if (!g_loaded_rng_pending)
		return;
	g_loaded_rng_pending = false;
	RngRestore(g_loaded_rng_seed, g_loaded_rng_calls);
}

static void set_destination(struct game *self, int32_t destination)
{
	switch (destination) {
		case 0:
			self->destination = DEST_WILLAMETTE;
			self->next_node_id = 265;
			break;
		case 1:
			self->destination = DEST_WILLAMETTE;
			self->next_node_id = 266;
			break;
		case 2:
			self->destination = DEST_SACRAMENTO;
			self->next_node_id = 506;
			break;
		case 3:
			self->destination = DEST_SACRAMENTO;
			self->next_node_id = 507;
			break;
		case 4:
			self->destination = DEST_SALT_LAKE;
			self->next_node_id = 415;
			break;
		case 5:
			self->destination = DEST_ROGUE_RIVER;
			self->next_node_id = 619;
			break;
		case 6:
			self->destination = DEST_ROGUE_RIVER;
			self->next_node_id = 620;
			break;
	}
}

static void set_start_town(struct game *self)
{
	switch (self->end_node_id) {
		case 1:
			self->start_town = START_INDEPENDENCE;
			break;
		case 267:
			self->start_town = START_ST_JOSEPH;
			break;
		case 289:
			self->start_town = START_NAUVOO;
			break;
		case 312:
		case 313:
		case 315:
			self->start_town = START_KANESVILLE;
			break;
		default:
			self->start_town = START_INDEPENDENCE;
			break;
	}
}

static struct date *new_date(int32_t month, int32_t day, int32_t year)
{
	struct date *d = MemAlloc(1, sizeof(struct date));
	*d = DateMake(month, day, year);

	return d;
}

void GameStartNewGame(struct game *self, struct new_game_scenario *scenario)
{
	g_logged_100_miles = g_logged_500_miles = g_logged_1000_miles = g_logged_100_days = g_logged_200_days = 0;
	g_ferry_wait_pending = false;
	g_ferry_wait_days = 0;
	self->party = PartyCreateScenario(scenario);
	struct assets *data_lib_j = g_data_lib;
	if (!data_lib_j)
		return;
	self->scenery_switch = 1;
	self->score_multiplier = scenario->score_multiplier;
	self->date = new_date(scenario->start_month, 1, scenario->start_year);
	self->start_date = new_date(scenario->start_month, 1, scenario->start_year);
	self->cur_node = new_lnode(scenario->start_node);
	WeatherSetRegion(self->weather, self->cur_node->weather_region);
	self->player_level = scenario->player_level;
	self->next_node_id = self->prev_node_id = self->cur_node_id = scenario->start_node;
	self->map_id = self->cur_node->map_id;
	set_destination(self, scenario->destination);
	self->end_node_id = scenario->start_node;
	set_start_town(self);
	WeatherInit(self->weather, self->cur_node->weather_region, self->date->month);
	self->miles_today = self->total_miles = self->miles_to_go = 0;
	{
		struct point pt_b = PointMake(22, 14);

		struct rect bounds = VIEW_OPS(self->doc)->GetBounds(self->doc);
		struct view *view_b = ViewCreateId(-2, &bounds);
		VIEW_OPS(self->doc)->AddChild(self->doc, view_b);
		int32_t conv = LNodeGetLandmarkConv((struct lnode *) self->cur_node, self);
		struct view *scene_g = (struct view *) SceneFrameCreate(200, &pt_b, conv);
		enter_state(get_app(), APP_STATE_SCENE);
		if (get_app()->ui)
			MainUiShowNodeDescPanel(get_app()->ui);
		VIEW_OPS(view_b)->AddChild(view_b, scene_g);
		redraw_parent();
	}
	struct prologue_dlg *new_game_dlg_h = PrologueDlgCreate();
	ModalDialogRun((struct modal_dialog *) new_game_dlg_h, (intptr_t) self->party, &g_parent_win->base);
	ObjectDelete(new_game_dlg_h);
	if (self->player_level == LEVEL_ADVENTURER)
		MsgQueueEnqueue(GetMessageQueue(), MSG_ELECTED_CAPTAIN, NULL, 0, 0, NULL, -1);
	else if (self->player_level == LEVEL_TRAIL_GUIDE)
		MsgQueueEnqueue(GetMessageQueue(), MSG_HIRED_AS_GUIDE, NULL, 0, 0, NULL, -1);
}

void GameStartQuickGame(struct game *self, struct quick_start_choices *choices)
{
	struct occup_mgr *occupations_k = OccupMgrCreate();
	g_logged_100_miles = g_logged_500_miles = g_logged_1000_miles = g_logged_100_days = g_logged_200_days = 0;
	g_ferry_wait_pending = false;
	g_ferry_wait_days = 0;
	self->party = choices->party;
	choices->party = NULL;
	self->scenery_switch = 1;
	self->score_multiplier = OccupMgrGetScoreMultX10(occupations_k, self->party->occupation);
	self->date = new_date(choices->month, 15, choices->year);
	self->start_date = new_date(choices->month, 15, choices->year);
	int32_t start_node_f = choices->start_node;
	self->cur_node = new_lnode(start_node_f);
	WeatherSetRegion(self->weather, self->cur_node->weather_region);
	self->player_level = LEVEL_ADVENTURER;
	self->next_node_id = self->prev_node_id = self->cur_node_id = start_node_f;
	self->map_id = self->cur_node->map_id;
	set_destination(self, choices->destination);
	self->end_node_id = start_node_f;
	set_start_town(self);
	WeatherInit(self->weather, self->cur_node->weather_region, self->date->month);
	self->miles_today = self->total_miles = self->miles_to_go = 0;
	self->train_wagons = choices->wagons;
	self->train_people = choices->people;
	self->train_wagons_start = self->train_wagons;
	self->train_people_start = self->train_people;

	struct cstr *leader = CStrCreateCopy(PartyGetMember(self->party, 0)->name);
	struct strrsrc *skill_names_l = occupations_k->skill_names;
	struct date date_g = *self->date;
	int32_t skill_count_l = 0;
	struct strrsrc *rsrc_i = StrRsrcLoad(g_text_lib, 2200, true);
	struct cstr *occupation_text_a = StrRsrcGetCStr(rsrc_i, self->party->occupation);
	StrRsrcDestroy(&rsrc_i);
	rsrc_i = StrRsrcLoad(g_text_lib, 2212, true);
	struct cstr *article_text_d = StrRsrcGetCStr(rsrc_i, self->party->occupation);
	StrRsrcDestroy(&rsrc_i);
	rsrc_i = StrRsrcLoad(g_text_lib, 901, true);
	struct cstr *town_text_e = StrRsrcGetCStr(rsrc_i, self->cur_node_id);
	struct cstr *destination_text_b = StrRsrcGetCStr(rsrc_i, self->next_node_id);
	StrRsrcDestroy(&rsrc_i);
	DateSetDay(&date_g, date_g.day - 1);
	JournalSetDate(&get_app()->journal, &date_g);

	struct cstr *str_i = CStrCreateFrom("Here begins the journal of [NAME], formerly [A/AN] [OCCUPATION]. Tomorrow "
		"we leave [TOWN] behind to begin our journey west to [DESTINATION].  We have purchased the supplies we need "
		"to begin our long trek.");
	CStrReplaceStr(str_i, "[NAME]", leader);
	CStrReplaceStr(str_i, "[A/AN]", article_text_d);
	CStrReplaceStr(str_i, "[OCCUPATION]", occupation_text_a);
	CStrReplaceStr(str_i, "[TOWN]", town_text_e);
	CStrReplaceStr(str_i, "[DESTINATION]", destination_text_b);
	JournalAddLine(&get_app()->journal, str_i);
	CStrDestroy(&occupation_text_a);
	CStrDestroy(&article_text_d);
	CStrDestroy(&town_text_e);
	CStrDestroy(&destination_text_b);

	for (int32_t index_a = 0; index_a <= 12; index_a++) {
		if (self->party->skills[index_a])
			++skill_count_l;
	}
	if (skill_count_l) {
		if (skill_count_l > 1)
			str_i = CStrCreateFrom("I hope that the following skills will prove of some use along the trail: ");
		else
			str_i = CStrCreateFrom("I hope that my skill in ");
		skill_count_l = 0;
		for (int32_t index_a = 0; index_a <= 12; index_a++) {
			if (self->party->skills[index_a]) {
				struct cstr *skill_name_g = StrRsrcGetCStr(skill_names_l, index_a);
				if (skill_count_l)
					CStrAppendBuf(str_i, ", ");
				CStrAppendStr(str_i, skill_name_g);
				CStrDestroy(&skill_name_g);
				++skill_count_l;
			}
		}
		if (skill_count_l == 1)
			CStrAppendBuf(str_i, " will prove of some value.");
		else
			CStrAppendBuf(str_i, ".");
		JournalAddLine(&get_app()->journal, str_i);
	}
	OccupMgrDestroy(&occupations_k);
	dispatch_cmd(GCMD_NODE_CONTINUE);
	redraw_parent();
	CStrDestroy(&leader);
}

void game_run_npc_conv(struct game *self, struct cmd_event *arg0)
{
	int32_t conv_id_a = (int32_t) arg0->param1;
	int32_t dir_value_m = (int32_t) arg0->param2;

	if (conv_id_a == -1)
		conv_id_a = g_conv_ids[RandRangeAvg(0, g_conv_id_max, 1)];
	if (dir_value_m == -1)
		dir_value_m = NodeBaseGetSeasonBackdrop(self->cur_node, self);
	struct conv_dlg *conv_dlg_m = ConvDlgCreate(conv_id_a, 0, dir_value_m + 5100);
	int32_t conv_result_j = ModalDialogRun((struct modal_dialog *) conv_dlg_m, 0, &g_parent_win->base);
	int32_t conv_rsrc_id_n = ConvDlgGetRsrcId(conv_dlg_m);
	ObjectDelete(conv_dlg_m);
	arg0->handled = true;
	if (conv_result_j > 0) {
		if (conv_id_a == 349 || conv_id_a == 893) {
			struct cstr *request_text_f;

			if (conv_id_a == 349)
				request_text_f = CStrCreateFrom("My chief is interested in");
			else
				request_text_f = CStrCreateFrom("We would be grateful if you could spare");
			struct inv_list *trade_inventory_m = PartyBuildInventoryList(self->party);
			struct trade_dlg *trade_dlg_k = TradeDlgCreateOffer(request_text_f, 1000, conv_result_j, conv_rsrc_id_n,
				dir_value_m + 5100, trade_inventory_m);
			int32_t trade_result_j = ModalDialogRun((struct modal_dialog *) trade_dlg_k, 0, &g_parent_win->base);
			ObjectDelete(trade_dlg_k);
			if (trade_result_j == DLG_CANCEL)
				adjust_reputation(self->party, -10);
			else
				adjust_reputation(self->party, 10);

		} else {
			struct cmd_event *result_event_h = CmdEventCreate(GCMD_OPEN_TRADE_DLG);
			result_event_h->param1 = conv_rsrc_id_n | (conv_result_j << 16);
			result_event_h->param2 = dir_value_m + 5100;
			DispatchGlobalEvent(result_event_h);
		}
	}
}

#ifdef OTII_TEST
void GameTestSetFerryWait(bool pending, int32_t days)
{
	g_ferry_wait_pending = pending;
	g_ferry_wait_days = days;
}

bool GameTestFerryWaitPending(void)
{
	return g_ferry_wait_pending;
}
#endif
