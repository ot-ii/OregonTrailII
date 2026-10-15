#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"

enum evt_class {
	EVCLS_ANY = 20000,
	EVCLS_APP = 20002,
	EVCLS_CONTROL = 20003,
	EVCLS_VIEW = 20006,
	EVCLS_PICVIEW = 20008,
	EVCLS_LABELED_CONTROL = 20013,
	EVCLS_SCROLLVIEW = 20014,
};

enum cmd_id {
	CMD_EDIT_COMMITTED = 11,
	CMD_MOUSE = 30000,
	CMD_KEY_DOWN = 30002,
	CMD_BUTTON_HILITE = 30006,
	CMD_QUIT = 30007,
	CMD_CLOSE_WINDOW = 30008,
	CMD_ABOUT = 30009,
	CMD_CONTROL_ACTION = 30012,
	CMD_PICVIEW_FRAME = 30013,
	CMD_BUTTON_TOGGLE = 30014,
	CMD_FOCUS_GAINED = 30015,
	CMD_FOCUS_LOST = 30016,
	CMD_FOCUS_KEPT = 30019,
	CMD_SCROLLVIEW_CTRL = 30020,
	CMD_MEDIA_DONE = 30021,
	CMD_VIDEO_CTRL = 30022,
	CMD_NEW_GAME = 31000,
	CMD_LOAD_GAME = 31001,
	CMD_END_GAME = 31002,
	CMD_SAVE_GAME = 31003,
	CMD_SAVE_GAME_AS = 31004,
	CMD_UNDO = 31005,
	CMD_CUT = 31006,
	CMD_COPY = 31007,
	CMD_PASTE = 31008,
	CMD_CLEAR = 31009,
	CMD_SELECT_ALL = 31011,
};

enum mouse_kind {
	MOUSE_DOWN = 1,
	MOUSE_UP = 2,
	MOUSE_MOVE = 3,
};

struct event {
	int32_t code;
	int32_t id;
	int32_t event_code;
};

struct event EventMake(int32_t code, int32_t id, int32_t event_code);
void EventSetId(struct event *ev, int32_t id);
bool EventMatches(const struct event *a, const struct event *b);

struct cmd_event {
	int32_t command;
	int32_t timestamp;
	intptr_t param1;
	intptr_t param2;
	bool handled;
	bool flag_15;
	struct event spec_a;
	struct event spec_b;
	bool is_mouse;
};

struct mouse_event {
	struct cmd_event base;
	int32_t kind;
	struct point mouse_pos;
	int32_t field_3A;
	bool button_down;
};

struct cmd_event *CmdEventCreate(int32_t command);
struct cmd_event *CmdEventCreateFlag(int32_t command, bool flag_15);
struct cmd_event *CmdEventCreateSpec(int32_t command, const struct event *spec_a);
struct cmd_event *CmdEventCreateSpecFlag(int32_t command, const struct event *spec_a, bool flag_15);
struct cmd_event *CmdEventCreateSpecs(int32_t command, const struct event *spec_b, const struct event *spec_a);
struct cmd_event *CmdEventCreateSpecsFlag(int32_t command, const struct event *spec_b, const struct event *spec_a,
	bool flag_15);
void CmdEventInit(struct cmd_event *ev, int32_t command);
void CmdEventDestroy(struct cmd_event **ev_mod);

struct mouse_event *MouseEventCreate(int32_t kind, bool button_down, const struct point *pos, int32_t field_3A);
void MouseEventInit(struct mouse_event *ev, int32_t kind, bool button_down, const struct point *pos,
	int32_t field_3A);
bool MouseEventIsButtonDownAndParam1(const struct mouse_event *ev);
