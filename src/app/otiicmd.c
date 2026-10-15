#include "otiiapp.h"

#include <inttypes.h>
#include <stddef.h>

#include "../dialogs/aboutotiidlg.h"
#include "../draw/canvas.h"
#include "../ui/clipview.h"
#include "../ui/combobox.h"
#include "../ui/control.h"
#include "../base/cstr.h"
#include "../ui/cursormgr.h"
#include "../data/dataversion.h"
#include "../data/docfile.h"
#include "docmanager.h"
#include "../draw/drawable.h"
#include "../dialogs/dropdlg.h"
#include "event.h"
#include "evthandler.h"
#include "../ui/framewin.h"
#include "../game/game.h"
#include "../game/gamecmd.h"
#include "../data/gfxdev.h"
#include "../screens/healthwin.h"
#include "../dialogs/hiscoredlg.h"
#include "../hunt/hunting.h"
#include "../dialogs/huntweapondlg.h"
#include "../screens/invgrid.h"
#include "../game/invitem.h"
#include "../game/invlist.h"
#include "license.h"
#include "../base/list.h"
#include "../screens/mainui.h"
#include "../dialogs/mapdlg.h"
#include "../draw/mediabase.h"
#include "../game/member.h"
#include "../base/memory.h"
#include "../ui/menubar.h"
#include "../dialogs/modaldialog.h"
#include "../dialogs/msgbox.h"
#include "../game/msgcontroller.h"
#include "../game/msgqueue.h"
#include "../trail/node.h"
#include "../draw/objbase.h"
#include "../dialogs/overviewmapdlg.h"
#include "../game/party.h"
#include "../base/path.h"
#include "../ui/pictctrl.h"
#include "../draw/pictserver.h"
#include "../ui/picview.h"
#include "../base/rng.h"
#include "../draw/server.h"
#include "settings.h"
#include "../draw/shape.h"
#include "../media/sndstream.h"
#include "../dialogs/speeddlg.h"
#include "../base/str.h"
#include "../media/strmdsnd.h"
#include "../data/strrsrc.h"
#include "../screens/titleanim.h"
#include "../dialogs/tradedlg.h"
#include "../ui/view.h"
#include "../ui/window.h"
#include "../ui/windowpos.h"

static struct inv_grid *g_inv_grid;
static const int32_t g_pool_tracks[26] = {
	2, 6, 9, 13, 20, 21, 24, 28, 30, 31, 36, 38, 39, 42, 43, 45, 46, 47, 55, 75, 77, 81, 84, 85, 100, 101
};
static const int32_t g_pool_count = 25;
bool g_about_open = false;
bool g_hunt_started = false;
struct game *g_game = NULL;

static struct app *as_app(struct otii_app *self)
{
	return &self->base.base;
}

static struct evt_handler *as_eh(struct otii_app *self)
{
	return &self->base.base.base;
}

static struct window *parent_win(void)
{
	return (struct window *) g_parent_win;
}

static void redraw_parent(void)
{
	WINDOW_OPS(g_parent_win)->Draw(parent_win(), NULL);
	WindowValidate(parent_win(), NULL);
}

static void set_capacity(struct inv_grid *grid, double cap)
{
	char a2[32];

	grid->capacity = cap;
	StrPrintf(a2, "%7.2f", grid->capacity);
	InvGridSetLabel(grid, 22, CStrCreateFrom(a2));
}

static void enter_state(struct otii_app *app, int32_t s)
{
	if (app->app_state != s) {
		app->saved_state = app->prev_app_state;
		app->prev_app_state = app->app_state;
		app->app_state = s;
	}
	APP_OPS(app)->UpdateEditMenuState(as_app(app), 0);
}

static struct control *find_tool(struct otii_app *self, struct event *query, int32_t id)
{
	EventSetId(query, id);
	return (struct control *) EvtHandlerFindGlobalHandler(as_eh(self), query);
}

static int32_t run_msg_box(struct cstr *text, bool two_buttons)
{
	struct msg_box box;
	int32_t result;

	MsgBoxInit(&box, text, two_buttons);
	result = ModalDialogRun(&box.base, 0, parent_win());
	msg_box_destroy((struct object *) &box);
	return result;
}

static void add_tool(struct view *panel, struct otii_app *self, int32_t id, const struct point *pos, int32_t img_id)
{
	struct pict_ctrl *control_f = PictCtrlCreate(id, &as_eh(self)->eh_event, pos, img_id, 0, 0, NULL, 0, 0, 0);
	VIEW_OPS(panel)->AddChild(panel, (struct view *) control_f);
}

static int32_t grid_right(void)
{
	struct rect r = VIEW_OPS(g_inv_grid)->GetBounds((struct view *) g_inv_grid);
	return r.right;
}

static void on_hunt_done(struct otii_app *self, struct cmd_event *ev)
{
	struct hunt_result aim_result_h;
	struct hunt_status st_c;
	int32_t meat_pounds_o = 0;
	char text_buf_h[32];
	char journal_buf_h[64];
	struct strrsrc *hunt_text_l = StrRsrcLoad(g_text_lib, 2500, true);
	struct cstr *result_text_n = StrRsrcGetCStr(hunt_text_l, 0);
	struct cstr *journal_text_k = CStrCreateFrom("We stopped along the trail to hunt.  ");

	GameUpdateMusic(g_game, 0);
	HuntingGetState3(g_hunt_state, &st_c);
	HuntingGetAim2(g_hunt_state, &aim_result_h);
	EvtHandlerRemoveIdleHandler((struct evt_handler *) g_hunt_state, true);
	EvtHandlerRemoveIdleHandler((struct evt_handler *) g_hunt_state, false);
	if (aim_result_h.shots_fired && aim_result_h.meat) {
		if (PartyMemberCount(GetGameObj()->party, true) > 1)
			meat_pounds_o = RandRangeAvg(160, 240, 2);
		else
			meat_pounds_o = RandRangeAvg(80, 120, 2);
		StrPrintf(text_buf_h, "%" PRId32, aim_result_h.shots_fired);
		CStrReplaceAll(result_text_n, "[shots]", text_buf_h);
		StrPrintf(text_buf_h, "%" PRId32, aim_result_h.meat);
		CStrReplaceAll(result_text_n, "[killed]", text_buf_h);
		if (aim_result_h.meat == 1)
			CStrReplaceAll(result_text_n, "pounds", "pound");
		if (aim_result_h.meat < meat_pounds_o) {
			CStrReplaceAll(result_text_n, "[all/some]", "all");
			meat_pounds_o = aim_result_h.meat;

		} else {
			StrPrintf(text_buf_h, "%" PRId32 " %s", meat_pounds_o, PoundsWord(meat_pounds_o));
			CStrReplaceAll(result_text_n, "[all/some]", text_buf_h);
		}
		InventoryAddHuntedMeat(meat_pounds_o);
		if (meat_pounds_o)
			StrPrintf(journal_buf_h, "We obtained %" PRId32 " %s of meat.", meat_pounds_o, PoundsWord(meat_pounds_o));
		else
			StrPrintf(journal_buf_h, "%s", "We did not have any luck.");
		CStrAppendBuf(journal_text_k, journal_buf_h);
		struct msg_box result_box_f;
		MsgBoxInit(&result_box_f, result_text_n, false);
		ModalDialogRun(&result_box_f.base, 0, parent_win());
		JournalSetDate(&self->journal, g_game->date);
		JournalAddLine(&self->journal, journal_text_k);
		msg_box_destroy((struct object *) &result_box_f);

	} else if (aim_result_h.shots_fired) {
		CStrAppendBuf(journal_text_k, "We did not have any luck.");
		JournalSetDate(&self->journal, g_game->date);
		JournalAddLine(&self->journal, journal_text_k);
		CStrDestroy(&result_text_n);

	} else {
		CStrDestroy(&result_text_n);
		CStrDestroy(&journal_text_k);
	}
	HuntUseAmmo(aim_result_h.shots_fired, st_c.weapon);
	GameRest(GetGameObj(), 12, 1);
	ev->handled = true;
	if (self->ui)
		MainUiShowTravelPanel(self->ui);
	OtiiAppApplyScreenMode(self);
	MsgControllerRollDailyHazards(GetAuxMsgqueue());
	StrRsrcDestroy(&hunt_text_l);
}

static void on_tool_hunt(struct otii_app *self, struct cmd_event *ev, struct event *root_query_e)
{
	struct rect hunt_rect_h = {14, 22, 373, 618};
	struct event hunt_query_i = EventMake(EVCLS_ANY, -1, -1);
	struct control *hunt_handler_o;
	int32_t aim_x_l = 0, aim_y_k = 0;
	struct view *content_view_d;

	ev->handled = true;
	if ((int16_t) GetGameObj()->cur_node->scenery_segment > 0) {
		if (APP_OPS(self)->GetMode(as_app(self)) == LICENSE_DEMO && g_hunt_started) {
			run_msg_box(CStrCreateFrom("Sorry. You can only go hunting once per game in the demo version. If this "
				"were the full product, you could hunt as often as you needed to."), false);
			return;
		}
		if (ChooseHuntingWeapon(&aim_y_k, &aim_x_l)) {
			g_hunt_started = true;
			OtiiAppStopSfx(self);
			OtiiAppStopMusic(self);
			OtiiAppPlayMusic(self, 0, false);
			struct view *doc = g_game->doc;
			content_view_d = VIEW_OPS(doc)->RemoveChild(doc, root_query_e);
			if (content_view_d)
				ObjectDelete(content_view_d);
			struct rect doc_bounds = VIEW_OPS(doc)->GetBounds(doc);
			content_view_d = ViewCreateId(100, &doc_bounds);
			enter_state(self, self->prev_app_state);
			if (self->app_state != APP_STATE_HUNTING) {
				self->saved_state = self->prev_app_state;
				self->prev_app_state = self->app_state;
				self->app_state = APP_STATE_HUNTING;
			}
			APP_OPS(self)->UpdateEditMenuState(as_app(self), 0);
			g_hunt_state = HuntingCreate(g_data_lib, 666, &hunt_rect_h, (struct node *) g_game->cur_node,
				ev->param1 == 1);
			VIEW_OPS(content_view_d)->AddChild(content_view_d, (struct view *) g_hunt_state);
			HuntingSetAimXY(g_hunt_state, aim_x_l, aim_y_k);
			VIEW_OPS(g_hunt_state)->SetCursorId((struct view *) g_hunt_state, CURSOR_CROSSHAIR);
			if (self->ui)
				MainUiShowHuntPanel(self->ui);
			VIEW_OPS(doc)->AddChild(doc, content_view_d);
			EventSetId(&hunt_query_i, TOOL_SUPPLIES);
			EVT_HANDLER_OPS(self)->SetHandledFor(as_eh(self), &hunt_query_i);
			hunt_handler_o = (struct control *) EvtHandlerFindGlobalHandler(as_eh(self), &hunt_query_i);
			if (hunt_handler_o)
				CONTROL_OPS(hunt_handler_o)->SetValue(hunt_handler_o, 0, true);
			hunt_handler_o = find_tool(self, &hunt_query_i, TOOL_HEALTH);
			if (hunt_handler_o)
				CONTROL_OPS(hunt_handler_o)->SetValue(hunt_handler_o, 1, true);
			hunt_handler_o = find_tool(self, &hunt_query_i, TOOL_JOURNAL);
			if (hunt_handler_o)
				CONTROL_OPS(hunt_handler_o)->SetValue(hunt_handler_o, 1, true);
			EVT_HANDLER_OPS(self)->ClearHandledFor(as_eh(self), &hunt_query_i);
			EventSetId(&hunt_query_i, TOOL_MAP);
			EVT_HANDLER_OPS(self)->ClearHandledFor(as_eh(self), &hunt_query_i);
			redraw_parent();

		} else if (aim_y_k != -1) {
			struct cstr *missing_text_u = HuntMissingText(HuntMissingItems());
			if (!missing_text_u)
				missing_text_u = CStrCreateFrom("You do not have a firearm or you do not have ammunition for your "
					"firearm to go hunting.");
			run_msg_box(missing_text_u, false);
		}

	} else {
		run_msg_box(CStrCreateFrom("There is no hunting allowed in this area."), false);
	}
}

static void on_tool_supplies(struct otii_app *self, struct cmd_event *ev, struct event *root_query_e)
{
	struct view *content_view_d;

	ev->handled = true;
	if (self->app_state == APP_STATE_HUNTING) {
		struct event resume_query_g = EventMake(EVCLS_ANY, -1, -1);
		struct control *resume_handler_d;
		resume_handler_d = find_tool(self, &resume_query_g, TOOL_SUPPLIES);
		if (resume_handler_d)
			CONTROL_OPS(resume_handler_d)->SetValue(resume_handler_d, 0, true);
		resume_handler_d = find_tool(self, &resume_query_g, TOOL_HEALTH);
		if (resume_handler_d)
			CONTROL_OPS(resume_handler_d)->SetValue(resume_handler_d, 0, true);
		resume_handler_d = find_tool(self, &resume_query_g, TOOL_JOURNAL);
		if (resume_handler_d)
			CONTROL_OPS(resume_handler_d)->SetValue(resume_handler_d, 0, true);
		EVT_HANDLER_OPS(self)->SetHandledFor(as_eh(self), &resume_query_g);
		EventSetId(&resume_query_g, TOOL_MAP);
		EVT_HANDLER_OPS(self)->SetHandledFor(as_eh(self), &resume_query_g);
		struct cmd_event *hunt_event_i = CmdEventCreate(GCMD_HUNT_DONE);
		DispatchGlobalEvent(hunt_event_i);
		return;
	}
	switch (self->app_state) {
		case APP_STATE_TRAVELING:
			if (NodeBaseGetId(g_game->cur_node) != NODE_KIND_TRAIL)
				break;
			// FALL THROUGH INTENTIONAL!
		case APP_STATE_AT_NODE:
		case APP_STATE_SCENE:
		case APP_STATE_HUNTING: {
			struct point layout_pos_d = {14, 22};
			struct rect combo_rect_e = {0, 0, 0, 0};
			struct view *doc = g_game->doc;
			OtiiAppStopSfx(self);
			EvtHandlerRemoveIdleHandler(&g_game->base, false);
			GameSetTravelState(g_game, 3);
			content_view_d = VIEW_OPS(doc)->RemoveChild(doc, root_query_e);
			if (content_view_d)
				ObjectDelete(content_view_d);
			struct rect doc_bounds = VIEW_OPS(doc)->GetBounds(doc);
			content_view_d = ViewCreateId(100, &doc_bounds);
			VIEW_OPS(doc)->AddChild(doc, content_view_d);
			struct inv_list *trail_inventory_n = PartyBuildInventoryList(GetGameObj()->party);
			g_inv_grid = InvGridCreate(0, &layout_pos_d, 2100, trail_inventory_n, 0, GetGameObj()->party->cash, false,
				true, 9015, false);
			if (self->app_state != APP_STATE_SUPPLIES) {
				self->saved_state = self->prev_app_state;
				self->prev_app_state = self->app_state;
				self->app_state = APP_STATE_SUPPLIES;
			}
			APP_OPS(self)->UpdateEditMenuState(as_app(self), 0);
			layout_pos_d.h = grid_right();
			struct view *pace_panel_i = (struct view *) PicViewCreatePoint(300, &layout_pos_d, 165);
			layout_pos_d.h = layout_pos_d.h + 28;
			layout_pos_d.v = layout_pos_d.v + 159;
			add_tool(pace_panel_i, self, TOOL_TURN, &layout_pos_d, 987);
			layout_pos_d.h = grid_right() + 111;
			add_tool(pace_panel_i, self, TOOL_DUMP, &layout_pos_d, 988);
			layout_pos_d.v = layout_pos_d.v + 92;
			add_tool(pace_panel_i, self, TOOL_TRADE, &layout_pos_d, 986);
			layout_pos_d.h = grid_right() + 28;
			add_tool(pace_panel_i, self, TOOL_HUNT, &layout_pos_d, 985);
			struct event combo_query_o = EventMake(EVCLS_ANY, -1, -1);
			struct control *control_f = find_tool(self, &combo_query_o, TOOL_SUPPLIES);
			if (control_f)
				CONTROL_OPS(control_f)->SetValue(control_f, 1, true);
			struct strrsrc *choice_list_i = StrRsrcCreate();
			StrRsrcAppend(choice_list_i, "8 hours per day");
			StrRsrcAppend(choice_list_i, "10 hours per day");
			StrRsrcAppend(choice_list_i, "12+ hours per day");
			layout_pos_d.h = 22;
			layout_pos_d.v = 14;
			combo_rect_e.top = layout_pos_d.v + 50;
			combo_rect_e.left = grid_right() + 22;
			combo_rect_e.right = grid_right() + 180;
			combo_rect_e.bottom = layout_pos_d.v + 70;
			struct combo_box *combo_c = ComboBoxCreate(9013, &GetGameObj()->base.eh_event, &combo_rect_e, 60,
				choice_list_i, GetGameObj()->travel_pace);
			combo_c->font_id = 3;
			ViewNotifyChanged((struct view *) combo_c);
			VIEW_OPS(pace_panel_i)->AddChild(pace_panel_i, (struct view *) combo_c);
			choice_list_i = StrRsrcCreate();
			StrRsrcAppend(choice_list_i, "Bare bones");
			StrRsrcAppend(choice_list_i, "Meager");
			StrRsrcAppend(choice_list_i, "Filling");
			layout_pos_d.h = 22;
			layout_pos_d.v = 14;
			combo_rect_e.top = layout_pos_d.v + 118;
			combo_rect_e.left = grid_right() + 22;
			combo_rect_e.right = grid_right() + 180;
			combo_rect_e.bottom = layout_pos_d.v + 138;
			combo_c = ComboBoxCreate(9014, &GetGameObj()->base.eh_event, &combo_rect_e, 60, choice_list_i,
				GetGameObj()->party->ration_level);
			combo_c->font_id = 3;
			ViewNotifyChanged((struct view *) combo_c);
			VIEW_OPS(pace_panel_i)->AddChild(pace_panel_i, (struct view *) combo_c);
			VIEW_OPS(content_view_d)->AddChild(content_view_d, pace_panel_i);
			VIEW_OPS(content_view_d)->AddChild(content_view_d, (struct view *) g_inv_grid);
			InvGridRefresh(g_inv_grid);
			break;
		}
		case APP_STATE_SUPPLIES:
			OtiiAppApplyScreenMode(self);
			break;
	}
}

static void on_control_action(struct otii_app *self, struct cmd_event *ev, int32_t menu_event_id_h,
	struct event *root_query_e)
{
	switch (menu_event_id_h - TOOL_MAP) {
		case TOOL_TRADE - TOOL_MAP: {
			ev->handled = true;
			struct cmd_event *event_l = CmdEventCreate(GCMD_OPEN_TRADE_DLG);
			event_l->param1 = -1;
			event_l->param2 = -1;
			DispatchGlobalEvent(event_l);
			struct inv_list *inventory_list_d = PartyBuildInventoryList(GetGameObj()->party);
			set_capacity(g_inv_grid, GetGameObj()->party->cash);
			InvGridSetList(g_inv_grid, inventory_list_d);
			break;
		}
		case TOOL_DUMP - TOOL_MAP: {
			ev->handled = true;
			struct cmd_event *cmd_event_b = CmdEventCreate(GCMD_OPEN_DROP_DLG);
			DispatchGlobalEvent(cmd_event_b);
			struct inv_list *new_inventory_c = PartyBuildInventoryList(GetGameObj()->party);
			InvGridSetList(g_inv_grid, new_inventory_c);
			break;
		}
		case TOOL_TURN - TOOL_MAP: {
			struct event next_query_k = EventMake(EVCLS_ANY, -1, -1);
			ev->handled = true;
			struct control *next_handler_m = find_tool(self, &next_query_k, TOOL_SUPPLIES);
			if (next_handler_m)
				CONTROL_OPS(next_handler_m)->SetValue(next_handler_m, 0, true);
			GameNextNode(GetGameObj());
			ViewNotifyChanged(GetGameObj()->doc);
			break;
		}
		case TOOL_HUNT - TOOL_MAP:
			on_tool_hunt(self, ev, root_query_e);
			break;
		case TOOL_MAP - TOOL_MAP: {
			ev->handled = true;
			if (self->app_state == APP_STATE_TRAVELING)
				EvtHandlerRemoveIdleHandler(&g_game->base, false);
			struct map_dlg map_dlg_j;
			MapDlgInit(&map_dlg_j, g_text_lib, 1, g_game->start_date->year);
			ModalDialogRun(&map_dlg_j.base, 0, parent_win());
			if (self->app_state == APP_STATE_TRAVELING)
				EvtHandlerAddIdleHandler(&g_game->base, false, 0);
			map_dlg_destroy((struct object *) &map_dlg_j);
			break;
		}
		case TOOL_JOURNAL - TOOL_MAP:
			ev->handled = true;
			if (self->app_state == APP_STATE_HUNTING) {
				struct event pause_query_l = EventMake(EVCLS_ANY, -1, -1);
				EventSetId(&pause_query_l, TOOL_JOURNAL);
				EVT_HANDLER_OPS(self)->ClearHandledFor(as_eh(self), &pause_query_l);
				struct cmd_event *pause_event_d = CmdEventCreate(GCMD_HUNT_RELOAD);
				DispatchGlobalEvent(pause_event_d);

			} else {
				int32_t saved_app_state_f = self->app_state;
				self->app_state = APP_STATE_DIARY;
				if (saved_app_state_f == 1)
					EvtHandlerRemoveIdleHandler(&g_game->base, false);
				JournalSetDate(&self->journal, g_game->date);
				JournalShowDialog(&self->journal, parent_win());
				self->app_state = saved_app_state_f;
				APP_OPS(self)->UpdateEditMenuState(as_app(self), 0);
				if (saved_app_state_f == 1)
					EvtHandlerAddIdleHandler(&g_game->base, false, 0);
			}
			break;
		case TOOL_HEALTH - TOOL_MAP:
			ev->handled = true;
			if (self->app_state == APP_STATE_HUNTING) {
				struct cmd_event *resume_event_e = CmdEventCreate(GCMD_HUNT_NEW_SCENE);
				DispatchGlobalEvent(resume_event_e);

			} else {
				if (g_health_window) {
					EVT_HANDLER_OPS(&g_health_window->handler)->SetHandled(&g_health_window->handler);

				} else {
					HealthWinCreate(parent_win());
				}
			}
			break;
		case TOOL_SUPPLIES - TOOL_MAP:
			on_tool_supplies(self, ev, root_query_e);
			break;
	}
}

static void on_media_done(struct otii_app *self)
{
	struct rect title_rect_o = {0, 0, 0, 0};
	struct event child_query_f = EventMake(EVCLS_ANY, 98, -1);
	struct point button_pos_k = {0, 0};
	struct point image_size_k = {0, 0};
	struct drawable *title_image_b;
	struct pict_ctrl *button_m;
	struct view *old_root_a;
	int32_t handler_ids_e = CMD_MEDIA_DONE;

	EvtHandlerReleaseBinding(as_eh(self), 1, &handler_ids_e);
	struct view *root = g_parent_win->base.root_view;
	struct view *old_child_h = VIEW_OPS(root)->RemoveChild(root, &child_query_f);
	ObjectDelete(old_child_h);
	TitleAnimStop();
	if (DataIs13()) {
		struct rect image_rect_s = {0, 0, 0, 0};
		struct canvas *image_canvas_q;
		struct clip_view *title_clip_h;

		title_image_b = ServerCreateVirtual((struct server *) g_pict_server, 222, true);
		OBJ_BASE_OPS(title_image_b)->GetSize((struct obj_base *) title_image_b, &image_size_k);
		image_canvas_q = OBJ_BASE_OPS(title_image_b)->GetRect((struct obj_base *) title_image_b, &image_rect_s);
		title_rect_o.top = 0;
		title_rect_o.left = 0;
		title_rect_o.right = image_size_k.h;
		title_rect_o.bottom = image_size_k.v;
		g_title_canvas = CanvasCreateRect(&title_rect_o, 8, 255);
		CANVAS_OPS(g_title_canvas)->StretchBlit(g_title_canvas, image_canvas_q, &image_rect_s, &title_rect_o, 0,
			&title_rect_o);
		ReleaseDrawable((struct media_base *) title_image_b, false);
		title_clip_h = ClipViewCreate(-2, &title_rect_o, (struct shape *) g_title_canvas, false);
		if (g_sound_server)
			SND_STREAM_OPS(g_sound_server)->Resume((struct snd_stream *) g_sound_server);
		OtiiAppLoadMusicCue(self, CUE_SHEET_TRAVEL);
		OtiiAppPlayMusicCue(self, CUE_TITLE);
		button_pos_k.h = 13;
		button_pos_k.v = 399;
		button_m = PictCtrlCreate(97, &as_eh(self)->eh_event, &button_pos_k, 610, 0, 0, NULL, 0, 0, 0);
		ControlSetCommand((struct control *) button_m, CMD_LOAD_GAME, 0, 0);
		VIEW_OPS(title_clip_h)->AddChild((struct view *) title_clip_h, (struct view *) button_m);
		button_pos_k.h = 221;
		button_m = PictCtrlCreate(96, &as_eh(self)->eh_event, &button_pos_k, 611, 0, 0, NULL, 0, 0, 0);
		ControlSetCommand((struct control *) button_m, CMD_NEW_GAME, 0, 0);
		VIEW_OPS(title_clip_h)->AddChild((struct view *) title_clip_h, (struct view *) button_m);
		button_pos_k.h = 432;
		button_m = PictCtrlCreate(95, &as_eh(self)->eh_event, &button_pos_k, 612, 0, 0, NULL, 0, 0, 0);
		ControlSetCommand((struct control *) button_m, CMD_NEW_GAME_OPTIONS, 0, 0);
		VIEW_OPS(title_clip_h)->AddChild((struct view *) title_clip_h, (struct view *) button_m);
		old_root_a = WindowSetRootView(parent_win(), (struct view *) title_clip_h);
		if (old_root_a)
			ObjectDelete(old_root_a);
		TitleAnimStart((struct view *) title_clip_h);
		redraw_parent();
		return;
	}
	title_image_b = ServerCreateVirtual((struct server *) g_pict_server, 222, true);
	OBJ_BASE_OPS(title_image_b)->GetSize((struct obj_base *) title_image_b, &image_size_k);
	title_rect_o.top = 0;
	title_rect_o.left = 0;
	title_rect_o.right = image_size_k.h;
	title_rect_o.bottom = image_size_k.v;
	struct pic_view *title_view_h = PicViewCreateImage(99, &title_rect_o, title_image_b);
	if (g_sound_server)
		SND_STREAM_OPS(g_sound_server)->Resume((struct snd_stream *) g_sound_server);
	OtiiAppLoadMusicCue(self, CUE_SHEET_TRAVEL);
	OtiiAppPlayMusicCue(self, CUE_TITLE);
	button_pos_k.h = 146;
	button_pos_k.v = 377;
	button_m = PictCtrlCreate(97, &as_eh(self)->eh_event, &button_pos_k, 610, 0, 0, NULL, 0, 0, 0);
	ControlSetCommand((struct control *) button_m, CMD_LOAD_GAME, 0, 0);
	VIEW_OPS(title_view_h)->AddChild((struct view *) title_view_h, (struct view *) button_m);
	button_pos_k.h = 354;
	button_m = PictCtrlCreate(96, &as_eh(self)->eh_event, &button_pos_k, 611, 0, 0, NULL, 0, 0, 0);
	ControlSetCommand((struct control *) button_m, CMD_NEW_GAME, 0, 0);
	VIEW_OPS(title_view_h)->AddChild((struct view *) title_view_h, (struct view *) button_m);
	old_root_a = WindowSetRootView(parent_win(), (struct view *) title_view_h);
	if (old_root_a)
		ObjectDelete(old_root_a);
	redraw_parent();
}

static void on_app_startup(struct otii_app *self, struct cmd_event *ev)
{
	struct rect window_rect_i = {0, 0, 440, 638};
	struct point window_pos_p = {0, 0};
	struct shape *frame_shape_k = ShapeCreateRect(&window_rect_i, 0, 0, 0, 1, 0);
	struct frame_win *frame_win_f;

	ev->handled = true;
	struct clip_view *clip_view_j = ClipViewCreate(-2, &window_rect_i, frame_shape_k, false);
	struct cstr *window_title_g = CStrCreateFrom("Oregon Trail II");
	frame_win_f = FrameWinCreate(window_title_g, (struct view *) clip_view_j, window_pos_p, true, false);
	g_parent_win = frame_win_f;

	MemFree(parent_win()->trigger_event);
	parent_win()->trigger_event = MemDup(&as_eh(self)->eh_event, sizeof(struct event));
	WindowBringToFront(&frame_win_f->base);
	if (!WindowPosRestore(frame_win_f->base.hwnd))
		WindowCenterMethod(&frame_win_f->base);

	WINDOW_OPS(frame_win_f)->Draw(&frame_win_f->base, NULL);
#ifndef OTII_TWITCH
	if (GetFirstCmdlineArg()) {
		struct cstr *arg_name_c = CStrCreateFrom(GetFirstCmdlineArg());
		struct path *doc_path_g = PathCreateStr(arg_name_c);
		OTII_APP_OPS(self)->OpenGameDocument(self, doc_path_g);
		CStrDestroy(&arg_name_c);

	} else
#endif
	{
		int32_t media_done_n = CMD_MEDIA_DONE;
		struct cmd_event *media_done_m = CmdEventCreate(CMD_MEDIA_DONE);
		EvtHandlerBindHandler(as_eh(self), 1, &media_done_n);
		EVT_HANDLER_OPS(self)->OnEvent(as_eh(self), media_done_m);
		CmdEventDestroy(&media_done_m);
	}
}

static void on_open_trade_dlg(struct cmd_event *ev)
{
	int32_t trade_param_i = (int32_t) ev->param1;
	int32_t store_track_p = (int16_t) trade_param_i;
	if (store_track_p < 0)
		store_track_p = g_pool_tracks[RandRangeAvg(0, g_pool_count, 1)] + 6000;
	int32_t store_h = trade_param_i >> 16;
	if (store_h == -1)
		store_h = 1;
	int32_t direction_i = (int16_t) ev->param2;
	if (direction_i == -1)
		direction_i = NodeBaseGetSeasonBackdrop(GetGameObj()->cur_node, GetGameObj()) + 5100;
	struct inv_list *store_inventory_a = PartyBuildInventoryList(GetGameObj()->party);
	double cash_b = GetGameObj()->party->cash;
	struct cstr *cash_name_g = CStrCreateFrom("cash");
	struct inv_item *cash_item_i = InvItemCreate(cash_name_g, (int32_t) cash_b, 1.0);
	CStrDestroy(&cash_name_g);
	ListInsertTail(&store_inventory_a->base, &cash_item_i->node);
	struct trade_dlg *trade_dlg_e = TradeDlgCreate(store_h, store_track_p, direction_i, store_inventory_a);
	ModalDialogRun((struct modal_dialog *) trade_dlg_e, 0, parent_win());
	ObjectDelete(trade_dlg_e);
	ev->handled = true;
}

void otii_app_on_event(struct evt_handler *eh, struct cmd_event *ev)
{
	struct otii_app *self = (struct otii_app *) eh;
	struct event root_query_e = EventMake(EVCLS_VIEW, -1, -1);
	struct menu_bar *menubar_c;

	struct doc_manager *doc_manager_j = APP_OPS(self)->GetDocManager(as_app(self));
	DocManagerGetSingleDoc(doc_manager_j);
	int32_t command_c = ev->command;
	switch (command_c) {
		case CMD_QUIT: {
			int32_t confirmed = 0;
			if (g_game)
				confirmed = run_msg_box(CStrCreateFrom("Are you sure you want to quit Oregon Trail II?"), true);
			if (!g_game || confirmed != DLG_CANCEL) {
				app_on_event(eh, ev);
				if (ev->handled)
					return;
			}
			break;
		}
		case CMD_ABOUT:
			if (!g_about_open) {
				g_about_open = true;
				struct about_otii_dlg *about_dlg_l = AboutOtiiDlgCreate(0);
				ev->handled = true;
				ModalDialogRun((struct modal_dialog *) about_dlg_l, 0, parent_win());
				ObjectDelete(about_dlg_l);
				g_about_open = false;
			}
			ev->handled = true;
			return;
		case MCMD_HELP_CONTENTS:
			OtiiAppWinHelp(self, 3);
			ev->handled = true;
			break;
		default:
			app_on_event(eh, ev);
			if (ev->handled)
				return;
			break;
	}
	int32_t menu_event_id_h = ev->spec_b.id;
	switch (command_c) {
		case GCMD_HUNT_SHOT: {
			struct event event_l = EventMake(EVCLS_ANY, -1, -1);
			if (self->ui)
				MainUiRefresh(self->ui);
			ev->handled = true;
			EventSetId(&event_l, TOOL_JOURNAL);
			EVT_HANDLER_OPS(self)->SetHandledFor(eh, &event_l);
			break;
		}
		case GCMD_HUNT_DONE:
			on_hunt_done(self, ev);
			break;
		case CMD_CONTROL_ACTION:
			on_control_action(self, ev, menu_event_id_h, &root_query_e);
			break;
		case CMD_END_GAME: {
			bool can_quit_e = true;
			if (g_game) {
				if (PartyGetMember(g_game->party, 0)->state != MEMBER_ALIVE)
					can_quit_e = false;
			}
			if (can_quit_e) {
				int32_t answer_k = run_msg_box(CStrCreateFrom("Are you sure you want to end your game?"), true);
				if (answer_k == DLG_CANCEL) {
					ev->handled = true;
					return;
				}
			}
		}
			// FALL THROUGH INTENTIONAL!
		case CMD_NEW_GAME_OPTIONS:
		case CMD_NEW_GAME:
		case CMD_LOAD_GAME:
		case CMD_SAVE_GAME:
		case CMD_SAVE_GAME_AS:
			ev->handled = true;
			if (g_parent_win)
				OtiiAppHandleDocCommand(self, command_c, g_parent_win->base.hwnd);
			break;
		case MCMD_EXPORT_DIARY:
			ev->handled = true;
			JournalExportWri(&self->journal);
			break;
		case GCMD_APP_STARTUP:
			on_app_startup(self, ev);
			break;
		case CMD_MEDIA_DONE:
			on_media_done(self);
			break;
		case CMD_CLOSE_WINDOW: {
			struct cmd_event *event_l = CmdEventCreateSpec(CMD_QUIT, &eh->eh_event);
			ev->handled = true;
			DispatchGlobalEvent(event_l);
			break;
		}
		case MCMD_SIM_SPEED: {
			struct speed_dlg *speed_dlg_g = SpeedDlgCreate();
			ev->handled = true;
			ModalDialogRun((struct modal_dialog *) speed_dlg_g, (intptr_t) self->settings, parent_win());
			ObjectDelete(speed_dlg_g);
			break;
		}
		case MCMD_LEGENDS: {
			ev->handled = true;
			struct hi_score_dlg hiscore_dlg_m;
			HiScoreDlgInit(&hiscore_dlg_m);
			ModalDialogRun(&hiscore_dlg_m.base, 0, parent_win());
			hi_score_dlg_destroy((struct object *) &hiscore_dlg_m);
			break;
		}
		case MCMD_TIME_OUT: {
			ev->handled = true;
			struct msg_box message_box_o;
			MsgBoxInit(&message_box_o, CStrCreateFrom("The game is paused.  To continue, press OK."), false);
			if (self->app_state == APP_STATE_TRAVELING)
				EvtHandlerRemoveIdleHandler(&g_game->base, false);
			ModalDialogRun(&message_box_o.base, 0, parent_win());
			if (self->app_state == APP_STATE_TRAVELING)
				EvtHandlerAddIdleHandler(&g_game->base, false, 0);
			msg_box_destroy((struct object *) &message_box_o);
			break;
		}
		case MCMD_TOGGLE_SFX:
			OtiiAppSetSfxEnabled(self, !SettingsSoundEffectsOn(self->settings));
			APP_OPS(self)->UpdateEditMenuState(as_app(self), MENU_ITEM_GAME);
			break;
		case MCMD_TOGGLE_MUSIC:
			OtiiAppSetMusicEnabled(self, !SettingsMusicOn(self->settings));
			APP_OPS(self)->UpdateEditMenuState(as_app(self), MENU_ITEM_GAME);
			break;
		case MCMD_TOGGLE_SPEECH:
			OtiiAppSetSpeechEnabled(self, !SettingsSpeechOn(self->settings));
			APP_OPS(self)->UpdateEditMenuState(as_app(self), MENU_ITEM_GAME);
			break;
		case MCMD_VIEW_MAP:
			if (!g_overviewmap_open) {
				int32_t node_id_k = 0;
				if (g_game)
					node_id_k = g_game->cur_node->map_id;
				struct overview_map_dlg *overview_dlg_n = OverviewMapDlgCreate(node_id_k);
				ev->handled = true;
				menubar_c = g_menubar;
				MenuBarEnableItem(menubar_c, MENU_ITEM_VIEW_MAP, false);
				ModalDialogRun((struct modal_dialog *) overview_dlg_n, 0, parent_win());
				MenuBarEnableItem(menubar_c, MENU_ITEM_VIEW_MAP, true);
				ObjectDelete(overview_dlg_n);
			}
			break;
		case MCMD_GLOSSARY:
			if (self->app_state == APP_STATE_TRAVELING)
				EvtHandlerRemoveIdleHandler(&g_game->base, false);
			if (!g_glossary_open) {
				struct map_dlg trail_map_dlg_a;
				MapDlgInit(&trail_map_dlg_a, g_text_lib, 1, 0);
				ev->handled = true;
				menubar_c = g_menubar;
				MenuBarEnableItem(menubar_c, MENU_ITEM_GLOSSARY, false);
				ModalDialogRun(&trail_map_dlg_a.base, 0, parent_win());
				MenuBarEnableItem(menubar_c, MENU_ITEM_GLOSSARY, true);
				map_dlg_destroy((struct object *) &trail_map_dlg_a);
			}
			if (self->app_state == APP_STATE_TRAVELING)
				EvtHandlerAddIdleHandler(&g_game->base, false, 0);
			break;
		case GCMD_OPEN_DROP_DLG: {
			struct drop_dlg *drop_dlg_d = DropDlgCreate(&GetGameObj()->party->inventory, 0);
			ModalDialogRun((struct modal_dialog *) drop_dlg_d, 0, parent_win());
			ObjectDelete(drop_dlg_d);
			ev->handled = true;
			break;
		}
		case GCMD_OPEN_TRADE_DLG:
			on_open_trade_dlg(ev);
			break;
	}
}
