#include "endframe.h"

#include <stddef.h>

#include "../app/app.h"
#include "../ui/button.h"
#include "../ui/cliplabelview.h"
#include "../base/cstr.h"
#include "../ui/cursormgr.h"
#include "../base/date.h"
#include "../game/endgame.h"
#include "../app/event.h"
#include "../ui/framewin.h"
#include "../game/game.h"
#include "../game/gamecmd.h"
#include "../data/gfxdev.h"
#include "../game/journal.h"
#include "../base/list.h"
#include "../base/log.h"
#include "../base/memory.h"
#include "../dialogs/modaldialog.h"
#include "../game/msg.h"
#include "../dialogs/msgbox.h"
#include "../trail/node.h"
#include "../app/otiiapp.h"
#include "../game/party.h"
#include "../ui/picview.h"
#include "../base/str.h"
#include "../data/strrsrc.h"
#include "../ui/textview.h"
#include "../features/twitchcontrol.h"

bool g_journal_logged_land_deed = false;
bool g_journal_logged_gold_claim = false;
bool g_journal_logged_arrival = false;
bool g_journal_logged_final_score = false;
bool g_journal_logged_epilogue = false;

const struct end_frame_ops END_FRAME_OPS_TABLE = {
	{
		{
			{end_frame_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			end_frame_on_event,
			view_on_idle,
			view_on_idle_spare,
		},
		view_add_child,
		view_get_bounds,
		view_get_clip_region,
		view_hide,
		view_remove_child,
		view_remove_from_parent,
		view_set_cursor_id,
		view_set_device,
		view_show,
		view_draw_children,
		view_move_children,
	},
};

static struct journal *app_journal(void)
{
	return &((struct otii_app *) g_app)->journal;
}

static bool greenhorn_allowance(struct game *game)
{
	bool arrived = game->player_level == LEVEL_GREENHORN;

	if (arrived && GameIsEventAllowed(game, NODEFEAT_NO_GREENHORN_ALLOWANCE))
		arrived = false;

	return arrived;
}

void EndFrameInit(struct end_frame *self, int32_t view_id, const struct rect *rect, struct end_game *result,
	struct game *game, int32_t end_type)
{
	ViewInitId(&self->base, view_id, rect);
	self->base.base.base.ops = &END_FRAME_OPS_TABLE.base.base.base;

	int32_t handlers[2];
	handlers[0] = CMD_CONTROL_ACTION;
	handlers[1] = CMD_MOUSE;
	self->result = result;
	self->game = game;
	self->state = 0;

	switch (end_type) {
		case END_FINAL_SCORE:
			if (!greenhorn_allowance(self->game)) {
				if (self->game->destination != self->result->destination) {
					struct cstr *warning = CStrCreateFrom("Because you reached a destination different from the one "
						"you originally chose, you will not receive extra points for reaching your goal.");

					if (self->game->player_level == LEVEL_TRAIL_GUIDE)
						CStrAppendBuf(warning, "  Also, you will not recieve your final payment of $1000.00");

					struct msg_box box;
					MsgBoxInit(&box, warning, false);
					ModalDialogRun(&box.base, 0, &g_parent_win->base);
					msg_box_destroy((struct object *) &box);
				}
			}

			EndFrameAdvanceState(self, 6);
			break;

		case END_LAND_CLAIM:
			self->result->result_mode = 1;
			EndFrameAdvanceState(self, 1);
			break;

		case END_GOLD_CLAIM:
			self->result->result_mode = 2;
			EndFrameAdvanceState(self, 3);
			break;

		case END_EPILOGUE:
			EndFrameAdvanceState(self, 7);
			break;
	}

	EvtHandlerBindHandler(&self->base.base, 2, handlers);
}

struct end_frame *EndFrameCreate(int32_t view_id, const struct rect *rect, struct end_game *result, struct game *game,
	int32_t end_type)
{
	struct end_frame *self = MemAlloc(1, sizeof(struct end_frame));
	EndFrameInit(self, view_id, rect, result, game, end_type);

	return self;
}

void EndFrameDestroy(struct end_frame **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void end_frame_destroy(struct object *obj)
{
	obj->ops = &VIEW_OPS_TABLE.base.base;
	view_destroy(obj);
}

void end_frame_on_event(struct evt_handler *handler, struct cmd_event *evt)
{
	struct end_frame *self = (struct end_frame *) handler;

	if (evt->command == CMD_MOUSE) {
		if (((struct mouse_event *) evt)->kind != MOUSE_DOWN)
			return;
	}

	evt->handled = true;

	switch (self->state) {
		case 0:
			LogError(LOG_ERROR, "Got MOUSEDOWN in state \"NONE\"");
			break;

		case 1:
			EndFrameAdvanceState(self, 2);
			break;

		case 3:
			EndFrameAdvanceState(self, 4);
			break;

		case 4:
			EndFrameAdvanceState(self, 5);
			break;

		case 2:
		case 5:
		case 6:
		case 7:
			EvtHandlerUnbindHandler(handler);
			DispatchGlobalEvent(CmdEventCreate(GCMD_NODE_CONTINUE));
			break;
	}
}

static struct rect page_rect(int32_t top, int32_t left, int32_t bottom, int32_t right, struct point origin)
{
	struct rect r;
	r.top = top;
	r.left = left;
	r.right = right;
	r.bottom = bottom;
	r.left += origin.h;
	r.right += origin.h;
	r.top += origin.v;
	r.bottom += origin.v;

	return r;
}

static void add_child(struct view *parent, struct view *child)
{
	VIEW_OPS(parent)->AddChild(parent, child);
}

static void replace_int(struct cstr *line, const char *tag, int32_t value)
{
	struct cstr *value_str = CStrCreateInt(value, true);
	CStrReplaceStr(line, tag, value_str);
	CStrDestroy(&value_str);
}

static void replace_person_word(struct cstr *line, const char *tag, struct strrsrc *person_words, int32_t count)
{
	struct cstr *value_str;

	if (count == 1)
		value_str = StrRsrcGetCStr(person_words, 0);
	else
		value_str = StrRsrcGetCStr(person_words, 1);

	CStrReplaceStr(line, tag, value_str);
	CStrDestroy(&value_str);
}

static void build_final_score(struct end_frame *self, struct view *picture, struct point origin)
{
	char multiplier_text[12];
	char text_buf[200];
	struct cstr *line;
	struct cstr *value;
	struct rect rect;
	struct view *text_view;

	struct strrsrc *template_lines = StrRsrcLoad(g_text_lib, 962, true);
	struct strrsrc *person_words = StrRsrcLoad(g_text_lib, 967, true);
	struct strrsrc *page_lines = StrRsrcCreate();

	for (int32_t line_index = 0; StrRsrcCount(template_lines) > line_index; ++line_index) {
		line = StrRsrcGetCStr(template_lines, line_index);
		replace_int(line, "[1]", self->result->n_healthy);
		replace_person_word(line, "[11]", person_words, self->result->n_healthy);
		replace_int(line, "[2]", self->result->n_fair);
		replace_person_word(line, "[12]", person_words, self->result->n_fair);
		replace_int(line, "[3]", self->result->n_poor);
		replace_person_word(line, "[13]", person_words, self->result->n_poor);
		replace_int(line, "[4]", (int32_t) (int64_t) self->game->party->cash);
		replace_int(line, "[5]", self->result->pts_wagons / 50);

		if (self->result->pts_wagons == 50)
			CStrReplaceAll(line, "[6]", "");
		else
			CStrReplaceAll(line, "[6]", "s");

		replace_int(line, "[9]", self->result->pts_animals / 10);

		if (self->result->pts_animals == 10)
			CStrReplaceAll(line, "[10]", "");
		else
			CStrReplaceAll(line, "[10]", "s");

		StrRsrcAppend(page_lines, CStrGet(line));
		CStrDestroy(&line);
	}

	StrRsrcDestroy(&template_lines);
	StrRsrcDestroy(&person_words);
	rect = page_rect(50, 40, 280, 240, origin);
	text_view = (struct view *) TextViewCreateStrs(-2, &rect, page_lines, 3, 0, 0, 0, 3, 0, 0, 255, 0, 1);
	add_child(picture, text_view);

	template_lines = StrRsrcLoad(g_text_lib, 963, true);
	page_lines = StrRsrcCreate();

	for (int32_t line_index = 0; StrRsrcCount(template_lines) > line_index; ++line_index) {
		line = StrRsrcGetCStr(template_lines, line_index);
		replace_int(line, "[1]", self->result->pts_healthy);
		replace_int(line, "[2]", self->result->pts_fair);
		replace_int(line, "[3]", self->result->pts_poor);
		replace_int(line, "[4]", self->result->pts_cash);
		replace_int(line, "[5]", self->result->pts_handcarts + self->result->pts_wagons);
		replace_int(line, "[6]", self->result->pts_animals);
		replace_int(line, "[7]", self->result->pts_goods);
		replace_int(line, "[8]", self->result->pts_unused_skill_points);
		StrRsrcAppend(page_lines, CStrGet(line));
		CStrDestroy(&line);
	}

	StrRsrcDestroy(&template_lines);
	rect = page_rect(50, 40, 280, 240, origin);
	text_view = (struct view *) TextViewCreateStrs(-2, &rect, page_lines, 3, 2, 0, 0, 3, 0, 0, 255, 0, 1);
	add_child(picture, text_view);

	template_lines = StrRsrcLoad(g_text_lib, 964, true);
	struct strrsrc *occupation_names = StrRsrcLoad(g_text_lib, 2200, true);
	struct strrsrc *node_names = StrRsrcLoad(g_text_lib, 968, true);
	page_lines = StrRsrcCreate();

	for (int32_t line_index = 0; StrRsrcCount(template_lines) > line_index; ++line_index) {
		line = StrRsrcGetCStr(template_lines, line_index);
		value = CStrCreateInt(self->game->start_date->year, false);
		CStrReplaceStr(line, "[1]", value);
		CStrDestroy(&value);

		switch (self->game->end_node_id) {
			case 1:
				value = StrRsrcGetCStr(node_names, 0);
				break;

			case 267:
				value = StrRsrcGetCStr(node_names, 1);
				break;

			case 289:
				value = StrRsrcGetCStr(node_names, 4);
				break;

			case 312:
			case 313:
				value = StrRsrcGetCStr(node_names, 2);
				break;

			case 315:
				value = StrRsrcGetCStr(node_names, 3);
				break;

			default:
				value = StrRsrcGetCStr(node_names, 0);
				break;
		}

		CStrReplaceStr(line, "[2]", value);
		CStrDestroy(&value);

		switch (self->game->cur_node->node_id) {
			case 265:
				value = StrRsrcGetCStr(node_names, 5);
				break;

			case 266:
				value = StrRsrcGetCStr(node_names, 6);
				break;

			case 619:
				value = StrRsrcGetCStr(node_names, 7);
				break;

			case 620:
				value = StrRsrcGetCStr(node_names, 8);
				break;

			case 506:
				value = StrRsrcGetCStr(node_names, 9);
				break;

			case 507:
				value = StrRsrcGetCStr(node_names, 10);
				break;

			case 753:
				value = StrRsrcGetCStr(node_names, 11);
				break;

			default:
				value = StrRsrcGetCStr(node_names, 5);
				break;
		}

		if (!g_journal_logged_arrival) {
			g_journal_logged_arrival = true;
			struct cstr *journal_line = CStrCreateFrom("Arrived today at ");
			CStrAppendStr(journal_line, value);

			if (self->game->destination != self->result->destination)
				CStrAppendBuf(journal_line, ". While it is not where we originally wished to settle, we cannot bear "
					"the prospect of continuing any further.  Here we are, and here we stay!");
			else
				CStrAppendBuf(journal_line, "!! At last we have reached our destination! Truly this is the land of "
					"opportunity!");

			JournalSetDate(app_journal(), GetGameObj()->date);
			JournalAddLine(app_journal(), journal_line);
		}

		CStrReplaceStr(line, "[3]", value);
		CStrDestroy(&value);
		value = StrRsrcGetCStr(occupation_names, self->game->party->occupation);
		CStrReplaceStr(line, "[4]", value);
		CStrDestroy(&value);
		StrRsrcAppend(page_lines, CStrGet(line));
		CStrDestroy(&line);
	}

	StrRsrcDestroy(&template_lines);
	StrRsrcDestroy(&occupation_names);
	StrRsrcDestroy(&node_names);
	rect = page_rect(50, 340, 280, 540, origin);
	text_view = (struct view *) TextViewCreateStrs(-2, &rect, page_lines, 3, 0, 0, 0, 3, 0, 0, 255, 0, 1);
	add_child(picture, text_view);

	template_lines = StrRsrcLoad(g_text_lib, 965, true);
	page_lines = StrRsrcCreate();

	for (int32_t line_index = 0; StrRsrcCount(template_lines) > line_index; ++line_index) {
		line = StrRsrcGetCStr(template_lines, line_index);
		replace_int(line, "[1]", self->result->pts_year);
		replace_int(line, "[2]", self->result->pts_route);
		replace_int(line, "[3]", self->result->pts_speed);
		replace_int(line, "[4]", self->result->pts_misc);
		replace_int(line, "[5]", self->result->raw_total);
		StrPrintf(multiplier_text, "%.1f", self->result->multiplier);
		CStrReplaceAll(line, "[6]", multiplier_text);
		StrRsrcAppend(page_lines, CStrGet(line));
		CStrDestroy(&line);
	}

	StrRsrcDestroy(&template_lines);
	rect = page_rect(50, 340, 280, 540, origin);
	text_view = (struct view *) TextViewCreateStrs(-2, &rect, page_lines, 3, 2, 0, 0, 3, 0, 0, 255, 0, 1);
	add_child(picture, text_view);

	template_lines = StrRsrcLoad(g_text_lib, 966, true);
	page_lines = StrRsrcCreate();

	if (!g_journal_logged_final_score) {
		g_journal_logged_final_score = true;
		StrPrintf(text_buf, "Final Score: %d", self->result->final_total);
		JournalSetDate(app_journal(), self->result->arrival_date);
		JournalAddLine(app_journal(), CStrCreateFrom(text_buf));
	}

	for (int32_t line_index = 0; StrRsrcCount(template_lines) > line_index; ++line_index) {
		line = StrRsrcGetCStr(template_lines, line_index);
		replace_int(line, "[1]", self->result->final_total);
		StrRsrcAppend(page_lines, CStrGet(line));
		CStrDestroy(&line);
	}

	StrRsrcDestroy(&template_lines);
	rect = page_rect(280, 100, 310, 500, origin);
	text_view = (struct view *) TextViewCreateStrs(-2, &rect, page_lines, 6, 1, 0, 0, 0, 0, 0, 255, 0, 1);
	add_child(picture, text_view);
}

void EndFrameAdvanceState(struct end_frame *self, int32_t state)
{
	struct view *view = &self->base;
	struct point origin = PointMake(22, 14);
	struct rect rect;
	struct pic_view *picture = NULL;
	struct cstr *text;
	struct cstr *journal_line;
	char text_buf[200];

	if (view->has_child_list) {
		struct obarray *list = view->child_list;
		ObarrayDestroy(&list);
		view->has_child_list = false;
		view->child_list = NULL;
	}

	struct rect bounds = VIEW_OPS(view)->GetBounds(view);
	struct clip_button *click_button = ClipButtonCreate(-2, &view->base.eh_event, &bounds, CMD_CONTROL_ACTION, 0, 0);
	VIEW_OPS(click_button)->SetCursorId((struct view *) click_button, CURSOR_PAGE);

#ifdef OTII_TWITCH
	if (state == 6)
		TwitchControlScoreReached();
#endif

	switch (state) {
		case 0:
			LogError(LOG_ERROR, "Attempt to set state to \"NONE\"");
			break;

		case 1:
			picture = PicViewCreatePoint(-2, &origin, 22040);
			add_child(&picture->base, (struct view *) click_button);
			break;

		case 2:
			picture = PicViewCreatePoint(-2, &origin, 22020);
			add_child(&picture->base, (struct view *) click_button);
			rect = page_rect(255, 125, 285, 200, origin);
			text = CStrCreateInt(self->result->acres, true);
			add_child(&picture->base, (struct view *) ClipLabelViewCreate(-2, &rect, text, 6, 0, 1, 0, 0, 255, 0, 1));
			rect = page_rect(300, 100, 320, 250, origin);
			text = CStrCreateCopy(self->result->arrival_text);
			add_child(&picture->base, (struct view *) ClipLabelViewCreate(-2, &rect, text, 3, 0, 1, 0, 0, 255, 0, 1));

			if (!g_journal_logged_land_deed) {
				g_journal_logged_land_deed = true;
				StrPrintf(text_buf, "Obtained a land deed today from the county for %d acres. Now I can begin to "
					"build my new life!", self->result->acres);
				journal_line = CStrCreateFrom(text_buf);
				JournalSetDate(app_journal(), self->result->arrival_date);
				JournalAddLine(app_journal(), journal_line);
			}

			break;

		case 3:
			picture = PicViewCreatePoint(-2, &origin, 22001);
			add_child(&picture->base, (struct view *) click_button);
			break;

		case 4:
			picture = PicViewCreatePoint(-2, &origin, 22041);
			add_child(&picture->base, (struct view *) click_button);
			break;

		case 5:
			picture = PicViewCreatePoint(-2, &origin, 22021);
			add_child(&picture->base, (struct view *) click_button);
			rect = page_rect(203, 420, 228, 520, origin);
			text = CStrCreateInt(self->result->score_per_day, true);
			add_child(&picture->base, (struct view *) ClipLabelViewCreate(-2, &rect, text, 2, 0, 1, 0, 0, 255, 0, 1));
			rect = page_rect(235, 330, 260, 450, origin);
			text = CStrCreateInt(self->result->dollars, true);
			CStrAppendBuf(text, ".00");

			if (!g_journal_logged_gold_claim) {
				g_journal_logged_gold_claim = true;
				StrPrintf(text_buf, "Got claim certificate #%d from assay office today, for %d ounces of gold worth $",
					self->result->lucky_number, self->result->score_per_day);
				journal_line = CStrCreateFrom(text_buf);
				CStrAppendStr(journal_line, text);
				CStrAppendBuf(journal_line, "!");

				if (self->result->score_per_day > 24)
					CStrAppendBuf(journal_line, " I'm rich!");

				JournalSetDate(app_journal(), self->result->arrival_date);
				JournalAddLine(app_journal(), journal_line);
			}

			add_child(&picture->base, (struct view *) ClipLabelViewCreate(-2, &rect, text, 2, 0, 1, 0, 0, 255, 0, 1));
			rect = page_rect(278, 100, 298, 150, origin);
			text = CStrCreateInt(self->result->lucky_number, true);
			add_child(&picture->base, (struct view *) ClipLabelViewCreate(-2, &rect, text, 3, 0, 1, 0, 0, 255, 0, 1));
			rect = page_rect(278, 200, 298, 350, origin);
			text = CStrCreateCopy(self->result->arrival_text);
			add_child(&picture->base, (struct view *) ClipLabelViewCreate(-2, &rect, text, 3, 0, 1, 0, 0, 255, 0, 1));
			break;

		case 6:
			picture = PicViewCreatePoint(-2, &origin, 22031);
			add_child(&picture->base, (struct view *) click_button);
			build_final_score(self, &picture->base, origin);
			break;

		case 7: {
			picture = PicViewCreatePoint(-2, &origin, 22030);
			add_child(&picture->base, (struct view *) click_button);
			struct strrsrc *epilogue = StrRsrcCreateCopy(self->result->epilogue);
			struct date journal_date = *self->result->arrival_date;
			DateSetDay(&journal_date, journal_date.day + 1);
			rect = page_rect(90, 40, 400, 570, origin);
			add_child(&picture->base, (struct view *) TextViewCreateStrs(-2, &rect, epilogue, 3, 0, 0, 1, 5, 0, 0, 255,
				0, 1));

			if (!g_journal_logged_epilogue) {
				g_journal_logged_epilogue = true;
				JournalSetDate(app_journal(), &journal_date);

				for (int32_t entry_index = 0; StrRsrcCount(epilogue) > entry_index; ++entry_index) {
					journal_line = StrRsrcGetCStr(epilogue, entry_index);
					CStrAppendBuf(journal_line, "\n\n");
					JournalAddLine(app_journal(), journal_line);
				}
			}

			break;
		}
	}

	if (picture)
		add_child(view, &picture->base);

	self->state = state;
}
