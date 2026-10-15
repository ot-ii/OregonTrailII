#include "event.h"

#include <string.h>

#include "../base/memory.h"
#include "replay.h"

static int32_t g_event_autoid = -3;

struct event EventMake(int32_t code, int32_t id, int32_t event_code)
{
	struct event ev;
	ev.event_code = event_code;
	ev.code = code;
	EventSetId(&ev, id);

	return ev;
}

void EventSetId(struct event *ev, int32_t id)
{
	if (id != -2) {
		ev->id = id;
		return;
	}

	ev->id = --g_event_autoid;

	if (g_event_autoid >= 0) {
		g_event_autoid = -3;
		ev->id = g_event_autoid;
	}
}

bool EventMatches(const struct event *a, const struct event *b)
{
	if (!(a->code == EVCLS_ANY || b->code == EVCLS_ANY || a->code == b->code))
		return false;

	if (!(a->id == -1 || b->id == -1 || a->id == b->id))
		return false;

	return a->event_code == -1 || b->event_code == -1 || a->event_code == b->event_code;
}

void CmdEventInit(struct cmd_event *ev, int32_t command)
{
	memset(ev, 0, sizeof(struct cmd_event));
	ev->spec_a = EventMake(EVCLS_ANY, -1, -1);
	ev->spec_b = EventMake(EVCLS_ANY, -1, -1);
	ev->command = command;
	ev->timestamp = GameTicks();
}

struct cmd_event *CmdEventCreate(int32_t command)
{
	struct cmd_event *ev = MemAlloc(1, sizeof(struct cmd_event));
	CmdEventInit(ev, command);

	return ev;
}

struct cmd_event *CmdEventCreateFlag(int32_t command, bool flag_15)
{
	struct cmd_event *ev = CmdEventCreate(command);
	ev->flag_15 = flag_15;

	return ev;
}

struct cmd_event *CmdEventCreateSpec(int32_t command, const struct event *spec_a)
{
	struct cmd_event *ev = MemAlloc(1, sizeof(struct cmd_event));
	ev->spec_a = *spec_a;
	ev->spec_b = EventMake(EVCLS_ANY, -1, -1);
	ev->command = command;
	ev->timestamp = GameTicks();

	return ev;
}

struct cmd_event *CmdEventCreateSpecFlag(int32_t command, const struct event *spec_a, bool flag_15)
{
	struct cmd_event *ev = CmdEventCreateSpec(command, spec_a);
	ev->flag_15 = flag_15;

	return ev;
}

struct cmd_event *CmdEventCreateSpecs(int32_t command, const struct event *spec_b, const struct event *spec_a)
{
	struct cmd_event *ev = MemAlloc(1, sizeof(struct cmd_event));
	ev->spec_a = *spec_a;
	ev->spec_b = *spec_b;
	ev->command = command;
	ev->timestamp = GameTicks();

	return ev;
}

struct cmd_event *CmdEventCreateSpecsFlag(int32_t command, const struct event *spec_b, const struct event *spec_a,
	bool flag_15)
{
	struct cmd_event *ev = CmdEventCreateSpecs(command, spec_b, spec_a);
	ev->flag_15 = flag_15;

	return ev;
}

void CmdEventDestroy(struct cmd_event **ev_mod)
{
	if (!ev_mod || !*ev_mod)
		return;

	struct cmd_event *ev = *ev_mod;
	*ev_mod = NULL;

	MemFree(ev);
}

void MouseEventInit(struct mouse_event *ev, int32_t kind, bool button_down, const struct point *pos,
	int32_t field_3A)
{
	memset(ev, 0, sizeof(struct mouse_event));
	CmdEventInit(&ev->base, CMD_MOUSE);
	ev->base.is_mouse = true;
	ev->kind = kind;
	ev->button_down = button_down;
	ev->field_3A = field_3A;

	if (pos)
		ev->mouse_pos = *pos;
}

struct mouse_event *MouseEventCreate(int32_t kind, bool button_down, const struct point *pos, int32_t field_3A)
{
	struct mouse_event *ev = MemAlloc(1, sizeof(struct mouse_event));
	MouseEventInit(ev, kind, button_down, pos, field_3A);

	return ev;
}

bool MouseEventIsButtonDownAndParam1(const struct mouse_event *ev)
{
	return ev->button_down && ev->base.param1;
}
