#include "evthandler.h"

#include <stddef.h>

#include "../base/list.h"
#include "../base/memory.h"
#include "replay.h"

static struct obarray *g_responder;
struct idle_list {
	struct obarray *tasks;
	int32_t cursor;
};

static struct idle_list g_idle_list;
static struct idle_list g_idle_list_b;
static int32_t g_evtsys_initialized;
static bool (*g_is_suspended)(void);

void EvtSetSuspendedQuery(bool (*is_suspended)(void))
{
	g_is_suspended = is_suspended;
}

static struct bindings *bindings_create(int32_t count, const int32_t *ids)
{
	struct bindings *b = MemAlloc(1, sizeof(struct bindings));
	b->ids = ObarrayCreate(NULL);

	b->match_all = true;

	if (ids && count) {
		b->match_all = false;

		for (int32_t i = 0; i < count; i++)
			ObarrayAppend(b->ids, (void *) (intptr_t) ids[i]);
	}

	return b;
}

static void bindings_destroy(struct bindings **b_mod)
{
	if (!b_mod || !*b_mod)
		return;

	struct bindings *b = *b_mod;
	*b_mod = NULL;

	ObarrayDestroy(&b->ids);
	MemFree(b);
}

static bool bindings_allow(const struct bindings *b, int32_t command)
{
	return b->match_all || ObarrayIndexOf(b->ids, (void *) (intptr_t) command) >= 0;
}

static void init_event_system(struct evt_handler *self)
{
	self->bindings = NULL;

	if (!g_evtsys_initialized) {
		g_responder = ObarrayCreate(NULL);
		g_idle_list.tasks = ObarrayCreate(NULL);
		g_idle_list.cursor = 0;
		g_idle_list_b.tasks = ObarrayCreate(NULL);
		g_idle_list_b.cursor = 0;
	}

	self->handled = true;
}

const struct evt_handler_ops EVT_HANDLER_OPS_TABLE = {
	{evt_handler_destroy},
	evt_handler_is_handled,
	evt_handler_set_handled_for,
	evt_handler_set_handled,
	evt_handler_clear_handled_for,
	evt_handler_clear_handled,
	evt_handler_on_event,
	evt_handler_on_idle,
	evt_handler_on_idle_spare,
};

void EvtHandlerInitEvent(struct evt_handler *self, const struct event *evt)
{
	ObjectInit(&self->base);
	self->base.ops = &EVT_HANDLER_OPS_TABLE.base;
	self->eh_event = *evt;
	init_event_system(self);
	g_evtsys_initialized++;
}

void EvtHandlerInitCopy(struct evt_handler *self, const struct evt_handler *other)
{
	ObjectInit(&self->base);
	self->base.ops = &EVT_HANDLER_OPS_TABLE.base;
	self->eh_event = other->eh_event;
	g_evtsys_initialized++;
}

void EvtHandlerInit(struct evt_handler *self, int32_t classid, int32_t id)
{
	struct event evt = EventMake(classid, id, -1);
	EvtHandlerInitEvent(self, &evt);
}

static void free_task(void *task)
{
	MemFree(task);
}

void evt_handler_destroy(struct object *obj)
{
	struct evt_handler *self = (struct evt_handler *) obj;

	EvtHandlerUnbindHandler(self);
	EvtHandlerRemoveIdleHandler(self, false);
	EvtHandlerRemoveIdleHandler(self, true);

	if (!--g_evtsys_initialized) {
		ObarrayDestroy(&g_responder);

		while (g_idle_list.tasks && ObarrayCount(g_idle_list.tasks)) {
			free_task(ObarrayGetAt(g_idle_list.tasks, 0));
			ObarrayRemoveAt(g_idle_list.tasks, 0);
		}

		while (g_idle_list_b.tasks && ObarrayCount(g_idle_list_b.tasks)) {
			free_task(ObarrayGetAt(g_idle_list_b.tasks, 0));
			ObarrayRemoveAt(g_idle_list_b.tasks, 0);
		}

		ObarrayDestroy(&g_idle_list.tasks);
		ObarrayDestroy(&g_idle_list_b.tasks);
	}

	object_destroy(obj);
}

static struct evt_handler *find_for_event(struct event *event)
{
	for (int32_t i = 0; i < ObarrayCount(g_responder); i++) {
		struct evt_handler *h = ObarrayGetAt(g_responder, i);

		if (EventMatches(&h->eh_event, event))
			return h;
	}

	return NULL;
}

static bool responder_add(struct evt_handler *h)
{
	if (ObarrayIndexOf(g_responder, h) < 0) {
		ObarrayAppend(g_responder, h);
		return true;
	}

	return false;
}

static void responder_remove(struct evt_handler *h)
{
	int32_t index = ObarrayIndexOf(g_responder, h);

	if (index >= 0)
		ObarrayRemoveAt(g_responder, index);
}

static bool responder_dispatch(struct cmd_event *ev)
{
	if (!ev)
		return false;

	for (int32_t i = 0; i < ObarrayCount(g_responder); i++) {
		struct evt_handler *h = ObarrayGetAt(g_responder, i);

		if (!h || !EventMatches(&h->eh_event, &ev->spec_a) || !bindings_allow(h->bindings, ev->command))
			continue;

		if (EVT_HANDLER_OPS(h)->IsHandled(h)) {
			EVT_HANDLER_OPS(h)->OnEvent(h, ev);

			if (ev->handled) {
				CmdEventDestroy(&ev);
				return true;
			}
		}
	}

	CmdEventDestroy(&ev);

	return false;
}

void DispatchGlobalEvent(struct cmd_event *ev)
{
	if (!g_evtsys_initialized) {
		CmdEventDestroy(&ev);
		return;
	}

	responder_dispatch(ev);
}

bool evt_handler_is_handled(struct evt_handler *self)
{
	return self->handled;
}

void evt_handler_set_handled(struct evt_handler *self)
{
	self->handled = true;
}

void evt_handler_set_handled_for(struct evt_handler *self, struct event *event)
{
	(void) self;
	struct evt_handler *h = find_for_event(event);

	if (h)
		EVT_HANDLER_OPS(h)->SetHandled(h);
}

void evt_handler_clear_handled(struct evt_handler *self)
{
	self->handled = false;
}

void evt_handler_clear_handled_for(struct evt_handler *self, struct event *event)
{
	(void) self;
	struct evt_handler *h = find_for_event(event);

	if (h)
		EVT_HANDLER_OPS(h)->ClearHandled(h);
}

void evt_handler_on_event(struct evt_handler *self, struct cmd_event *event)
{
	(void) self;
	(void) event;
}

void evt_handler_on_idle(struct evt_handler *self, void *task)
{
	(void) self;
	(void) task;
}

void evt_handler_on_idle_spare(struct evt_handler *self, void *task)
{
	(void) self;
	(void) task;
}

struct evt_handler *EvtHandlerFindGlobalHandler(struct evt_handler *self, struct event *event)
{
	(void) self;

	return find_for_event(event);
}

bool EvtHandlerInvokeGlobalHandler(struct evt_handler *self, struct event *event)
{
	(void) self;
	struct evt_handler *h = find_for_event(event);

	return h ? EVT_HANDLER_OPS(h)->IsHandled(h) : false;
}

void EvtHandlerBindHandler(struct evt_handler *self, int32_t count, const int32_t *event_ids)
{
	if (!self->bindings) {
		self->bindings = bindings_create(count, event_ids);
		responder_add(self);

	} else {
		for (int32_t i = 0; i < count; i++)
			ObarrayAppend(self->bindings->ids, (void *) (intptr_t) event_ids[i]);
	}
}

bool EvtHandlerRegisterResponder(struct evt_handler *self)
{
	bindings_destroy(&self->bindings);
	self->bindings = bindings_create(0, NULL);

	return responder_add(self);
}

void EvtHandlerReleaseBinding(struct evt_handler *self, int32_t count, const int32_t *event_ids)
{
	if (!self->bindings)
		return;

	for (int32_t i = 0; i < count; i++)
		ObarrayRemoveAt(self->bindings->ids, ObarrayIndexOf(self->bindings->ids, (void *) (intptr_t) event_ids[i]));

	if (ObarrayCount(self->bindings->ids) == 0) {
		responder_remove(self);
		bindings_destroy(&self->bindings);
	}
}

void EvtHandlerUnbindHandler(struct evt_handler *self)
{
	if (self->bindings) {
		responder_remove(self);
		bindings_destroy(&self->bindings);
	}
}

static struct idle_list *idle_list(bool which)
{
	return which ? &g_idle_list : &g_idle_list_b;
}

static struct timer_task *idle_find(struct idle_list *list, struct event *ev)
{
	for (int32_t i = 0; i < ObarrayCount(list->tasks); i++) {
		struct timer_task *task = ObarrayGetAt(list->tasks, i);

		if (EventMatches(ev, &task->target->eh_event))
			return task;
	}

	return NULL;
}

struct timer_task *EvtHandlerFindIdleHandler(struct evt_handler *self, bool which)
{
	return idle_find(idle_list(which), &self->eh_event);
}

bool EvtHandlerAddIdleHandler(struct evt_handler *self, bool which, int32_t interval)
{
	struct idle_list *list = idle_list(which);

	if (idle_find(list, &self->eh_event))
		return false;

	struct timer_task *task = MemAlloc(1, sizeof(struct timer_task));
	task->target = self;
	task->interval = interval;
	task->next_fire = GameTicks() + interval;

	ObarrayAppend(list->tasks, task);

	return true;
}

void EvtHandlerSetIdleOffset(struct evt_handler *self, bool which, int32_t offset)
{
	struct timer_task *task = EvtHandlerFindIdleHandler(self, which);

	if (task) {
		task->interval = offset;
		task->next_fire = task->last_fire + offset;
	}
}

bool EvtHandlerRemoveIdleHandler(struct evt_handler *self, bool which)
{
	struct idle_list *list = idle_list(which);

	if (!list->tasks)
		return false;

	for (int32_t i = 0; i < ObarrayCount(list->tasks); i++) {
		struct timer_task *task = ObarrayGetAt(list->tasks, i);

		if (EventMatches(&self->eh_event, &task->target->eh_event)) {
			ObarrayRemoveAt(list->tasks, i);
			return true;
		}
	}

	return false;
}

static void task_tick(struct timer_task *task, bool fire_now)
{
	if (task->done)
		return;

	int32_t ticks = GameTicks();

	if (task->next_fire > ticks || !EVT_HANDLER_OPS(task->target)->IsHandled(task->target))
		return;

	task->next_fire = ticks + task->interval;
	task->cur_fire = ticks;

	if (fire_now) {
		EVT_HANDLER_OPS(task->target)->OnIdle(task->target, task);

	} else {
		EVT_HANDLER_OPS(task->target)->OnIdleSpare(task->target, task);
	}

	task->last_fire = task->cur_fire;
}

static void idle_purge(struct idle_list *list)
{
	for (int32_t i = 0; i < ObarrayCount(list->tasks); i++) {
		struct timer_task *task = ObarrayGetAt(list->tasks, i);

		if (task->done) {
			if (list->cursor)
				list->cursor--;

			MemFree(task);
			ObarrayRemoveAt(list->tasks, i);
		}
	}
}

static int32_t idle_pump(struct idle_list *list, int32_t budget, bool force)
{
	int32_t deadline = GameTicks() + budget;

	idle_purge(list);

	int32_t count = ObarrayCount(list->tasks);
	int32_t start = list->cursor;

	if (count <= start)
		start = 0;

	int32_t next = start;

	if (count > 0 || !force) {
		do {
			int32_t slot = next++;
			struct timer_task *task = ObarrayGetAt(list->tasks, slot);

			if (task)
				task_tick(task, force);

			if (next == count)
				next = 0;

			if (next == start)
				break;

		} while (force || GameTicks() < deadline);
	}

	list->cursor = next;

	return GameTicks();
}

void RunIdleProcessing(int32_t budget)
{
	if (g_is_suspended && g_is_suspended())
		return;

	if (g_evtsys_initialized <= 0)
		return;

	int32_t ticks = GameTicks();
	int32_t deadline = budget + ticks + 1;

	if (ObarrayCount(g_idle_list.tasks))
		ticks = idle_pump(&g_idle_list, budget, true);

	if (ticks < deadline && ObarrayCount(g_idle_list_b.tasks))
		idle_pump(&g_idle_list_b, deadline - ticks, false);
}
