#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "event.h"
#include "../base/object.h"

struct evt_handler;
struct obarray;

struct bindings {
	bool match_all;
	struct obarray *ids;
};

struct evt_handler_ops {
	struct object_ops base;
	bool (*IsHandled)(struct evt_handler *self);
	void (*SetHandledFor)(struct evt_handler *self, struct event *event);
	void (*SetHandled)(struct evt_handler *self);
	void (*ClearHandledFor)(struct evt_handler *self, struct event *event);
	void (*ClearHandled)(struct evt_handler *self);
	void (*OnEvent)(struct evt_handler *self, struct cmd_event *event);
	void (*OnIdle)(struct evt_handler *self, void *task);
	void (*OnIdleSpare)(struct evt_handler *self, void *task);
};

struct evt_handler {
	struct object base;
	bool handled;
	struct event eh_event;
	struct bindings *bindings;
};

#define EVT_HANDLER_OPS(p) ((const struct evt_handler_ops *) ((const struct object *) (p))->ops)

extern const struct evt_handler_ops EVT_HANDLER_OPS_TABLE;

void EvtHandlerInit(struct evt_handler *self, int32_t classid, int32_t id);
void EvtHandlerInitEvent(struct evt_handler *self, const struct event *evt);
void EvtHandlerInitCopy(struct evt_handler *self, const struct evt_handler *other);
void evt_handler_destroy(struct object *self);

bool evt_handler_is_handled(struct evt_handler *self);
void evt_handler_set_handled_for(struct evt_handler *self, struct event *event);
void evt_handler_set_handled(struct evt_handler *self);
void evt_handler_clear_handled_for(struct evt_handler *self, struct event *event);
void evt_handler_clear_handled(struct evt_handler *self);
void evt_handler_on_event(struct evt_handler *self, struct cmd_event *event);
void evt_handler_on_idle(struct evt_handler *self, void *task);
void evt_handler_on_idle_spare(struct evt_handler *self, void *task);

struct evt_handler *EvtHandlerFindGlobalHandler(struct evt_handler *self, struct event *event);
bool EvtHandlerInvokeGlobalHandler(struct evt_handler *self, struct event *event);
void EvtHandlerBindHandler(struct evt_handler *self, int32_t count, const int32_t *event_ids);
void EvtHandlerUnbindHandler(struct evt_handler *self);
bool EvtHandlerRegisterResponder(struct evt_handler *self);
void EvtHandlerReleaseBinding(struct evt_handler *self, int32_t count, const int32_t *event_ids);
bool EvtHandlerAddIdleHandler(struct evt_handler *self, bool which, int32_t interval);
bool EvtHandlerRemoveIdleHandler(struct evt_handler *self, bool which);
struct timer_task *EvtHandlerFindIdleHandler(struct evt_handler *self, bool which);
void EvtHandlerSetIdleOffset(struct evt_handler *self, bool which, int32_t offset);

void DispatchGlobalEvent(struct cmd_event *ev);
void RunIdleProcessing(int32_t budget);
void EvtSetSuspendedQuery(bool (*is_suspended)(void));

struct timer_task {
	struct evt_handler *target;
	int32_t interval;
	int32_t next_fire;
	int32_t cur_fire;
	int32_t last_fire;
	bool done;
};
