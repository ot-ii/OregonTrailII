#pragma once

#include <stdint.h>

#include "../base/list.h"

struct cstr;
struct msg;
struct msg_controller;

struct msg_queue {
	struct list base;
};

extern struct msg_queue *g_msgq_main_a;
extern struct msg_queue *g_msgq_main_b;
extern struct msg_queue *g_msgq_current;
extern struct msg_controller *g_msgq_aux;

void MsgQueueInit(struct msg_queue *self);
struct msg_queue *MsgQueueCreate(void);
void msg_queue_destroy(struct msg_queue *self);
void MsgQueueDestroy(struct msg_queue **ctx_mod);

struct msg *MsgQueueFindByCmd(struct msg_queue *self, int32_t cmd);
struct msg *MsgQueueEnqueue(struct msg_queue *self, int32_t cmd, struct cstr *payload, intptr_t member,
	intptr_t subject, struct cstr *title, int32_t scene_variant);
struct msg *MsgQueuePost(struct msg_queue *self, int32_t cmd, struct cstr *payload, intptr_t member,
	intptr_t subject, struct cstr *title, int32_t scene_variant);
void MsgQueuePump(struct msg_queue *self, struct msg *node);

struct msg_queue *GetMessageQueue(void);
struct msg_controller *GetAuxMsgqueue(void);
void ReleaseMessageQueues(void);
