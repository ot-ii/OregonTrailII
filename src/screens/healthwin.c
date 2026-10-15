#include "healthwin.h"

#include <stddef.h>

#include "../ui/button.h"
#include "../ui/cliplabelview.h"
#include "../ui/clipview.h"
#include "../base/cstr.h"
#include "../app/event.h"
#include "../ui/framewin.h"
#include "../game/game.h"
#include "../base/list.h"
#include "../game/member.h"
#include "../base/memory.h"
#include "../game/party.h"
#include "../ui/picview.h"
#include "../ui/textview.h"
#include "../ui/view.h"
#include "../ui/windowpos.h"

struct rect g_healthwin_rect = {0, 0, 300, 180};

const struct health_win_ops HEALTH_WIN_OPS_TABLE = {
	{
		{health_win_destroy},
		window_vf04,
		window_vf08,
		window_vf0c,
		window_post_close_event,
		window_vf14,
		window_draw,
		window_is_dialog,
		float_win_on_activate,
		float_win_on_deactivate,
		window_get_text,
		window_post_mouse_event,
		window_handle_key,
		window_handle_mouse,
	},
};

const struct evt_handler_ops HEALTH_WIN_HANDLER_OPS_TABLE = {
	{evt_handler_destroy},
	evt_handler_is_handled,
	evt_handler_set_handled_for,
	health_win_set_handled,
	evt_handler_clear_handled_for,
	evt_handler_clear_handled,
	health_win_on_event,
	evt_handler_on_idle,
	evt_handler_on_idle_spare,
};

static void add_child(struct view *content, void *child)
{
	VIEW_OPS(content)->AddChild(content, (struct view *) child);
}

void HealthWinInit(struct health_win *self, struct window *parent)
{
	struct pic_view *pic = PicViewCreate(0, &g_healthwin_rect, 2300);
	FloatWinInit(&self->base, CStrCreateFrom("Health Status"), (struct view *) pic, 180, 295, parent, 0);
	EvtHandlerInit(&self->handler, 3, 0);
	self->base.base.base.ops = &HEALTH_WIN_OPS_TABLE.base.base;
	self->handler.base.ops = &HEALTH_WIN_HANDLER_OPS_TABLE.base;

	if (g_health_window)
		ObjectDelete(g_health_window);

	g_health_window = self;

	struct view *content = self->base.base.root_view;
	struct rect rect;

	rect.top = 5;
	rect.left = 10;
	rect.right = 170;
	rect.bottom = 20;
	add_child(content, ClipLabelViewCreate(0, &rect, CStrCreateFrom("Party Health"), 4, 1, 1, 0, 0, 255, 0, 1));

	int32_t member_index;

	for (member_index = 0; member_index < 6; member_index++) {
		rect.top = 20 * member_index + 30;
		rect.left = 10;
		rect.right = 123;
		rect.bottom = 20 * member_index + 45;

		struct member *member = PartyGetMember(GetGameObj()->party, member_index);

		if (!member)
			break;

		add_child(content, ClipButtonCreate(member_index + 1, &self->handler.eh_event, &rect, CMD_CONTROL_ACTION,
			0, 0));

		struct clip_label_view *name_label = ClipLabelViewCreate(-2, &rect, CStrCreateCopy(member->name), 4, 1, 1,
			0, 0, 255, 1, 1);
		name_label->base.alt_draw = true;
		add_child(content, name_label);

		rect.top = 20 * member_index + 30;
		rect.left = 128;
		rect.right = 170;
		rect.bottom = 20 * member_index + 45;
		add_child(content, ClipLabelViewCreate(member_index + 11, &rect, NULL, 4, 0, 1, 0, 0, 255, 0, 1));
	}

	rect.top = 20 * member_index + 35;
	rect.left = 10;
	rect.right = 170;
	rect.bottom = 290;
	add_child(content, TextViewCreate(20, &rect, NULL, 4, 0, 0, 0, 0, 0, 0, 255, 0, 1));

	int32_t handler_ids[2] = {CMD_CONTROL_ACTION, CMD_CLOSE_WINDOW};
	EvtHandlerBindHandler(&self->handler, 2, handler_ids);
	WindowBringToFront(&self->base.base);
	WindowCenterMethod(&self->base.base);

	if (!WindowPosRestoreAs(self->base.base.hwnd, "health") && g_parent_win)
		WindowPlaceBeside(self->base.base.hwnd, g_parent_win->base.hwnd, true, false);

	self->selected_member = 0;
	HealthWinUpdate(self, true);
}

struct health_win *HealthWinCreate(struct window *parent)
{
	struct health_win *self = MemAlloc(1, sizeof(struct health_win));
	HealthWinInit(self, parent);

	return self;
}

void HealthWinDestroy(struct health_win **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void health_win_destroy(struct object *self)
{
	struct health_win *win = (struct health_win *) self;

	g_health_window = NULL;

	evt_handler_destroy(&win->handler.base);
	self->ops = &FLOAT_WIN_OPS_TABLE.base;
	float_win_destroy(self);
}

struct health_win *HealthWinFromHandler(struct evt_handler *handler)
{
	if (!handler)
		return NULL;

	return (struct health_win *) ((uint8_t *) handler - offsetof(struct health_win, handler));
}

void health_win_set_handled(struct evt_handler *self)
{
	WindowBringToFront(&HealthWinFromHandler(self)->base.base);
}

void health_win_on_event(struct evt_handler *self, struct cmd_event *evt)
{
	struct health_win *win = HealthWinFromHandler(self);

	if (evt->command == CMD_CONTROL_ACTION) {
		win->selected_member = evt->spec_b.id - 1;
		HealthWinUpdate(win, false);
		evt->handled = true;

	} else {
		if (evt->command == CMD_CLOSE_WINDOW)
			evt->handled = true;
	}
}

static bool child_matches(const void *item, const void *key)
{
	const struct evt_handler *h = item;

	return EventMatches(key, &h->eh_event);
}

static void *find_child(struct view *content, const struct event *key)
{
	struct obarray *children = ViewGetChildList(content);

	return children ? ObarrayFind(children, child_matches, key) : NULL;
}

void HealthWinUpdate(struct health_win *self, bool flag)
{
	struct cstr *status_text = NULL;
	struct cstr *detail_text = NULL;
	struct event find_key = EventMake(EVCLS_ANY, -1, -1);
	struct view *content = self->base.base.root_view;

	if (flag) {
		for (int32_t i = 0; i < 6; i++) {
			self->member_health[i].health = -1;
			self->member_health[i].state = -1;
		}
	}

	for (int32_t member_index = 0; member_index < 6; member_index++) {
		status_text = NULL;
		struct party *party = GetGameObj()->party;
		struct member *member = PartyGetMember(party, member_index);

		if (!member)
			break;

		if (self->selected_member == member_index) {
			struct cstr *member_status = MemberGetStatusText(member);
			detail_text = CStrCreateCopy(member->name);
			CStrAppendBuf(detail_text, ": ");
			CStrAppendStr(detail_text, member_status);
			CStrDestroy(&member_status);
		}

		int32_t state = member->state;
		int32_t health_band = MemberGetHealthBand(member);

		if (self->member_health[member_index].state != state ||
			self->member_health[member_index].health != health_band) {
			self->member_health[member_index].state = state;
			self->member_health[member_index].health = health_band;

			if (!state) {
				status_text = CStrCreateFrom("dead");

			} else if (state == 1) {
				status_text = CStrCreateFrom("missing");

			} else {
				switch (health_band) {
					case HEALTH_GOOD: status_text = CStrCreateFrom("good"); break;
					case HEALTH_OK: status_text = CStrCreateFrom("OK"); break;
					case HEALTH_FAIR: status_text = CStrCreateFrom("fair"); break;
					case HEALTH_POOR: status_text = CStrCreateFrom("poor"); break;
					case HEALTH_BAD: status_text = CStrCreateFrom("bad"); break;
				}
			}

		} else {
			if (flag)
				status_text = CStrCreate();
		}

		if (status_text) {
			EventSetId(&find_key, member_index + 11);
			struct clip_label_view *label = find_child(content, &find_key);

			if (label) {
				ClipLabelViewSetLabelAttr(label, status_text);

			} else {
				CStrDestroy(&status_text);
			}
		}
	}

	if (!detail_text)
		detail_text = CStrCreate();

	EventSetId(&find_key, 20);
	struct text_view *text_view = find_child(content, &find_key);

	if (text_view) {
		TextViewSetSource(text_view, detail_text);

	} else {
		CStrDestroy(&detail_text);
	}
}
