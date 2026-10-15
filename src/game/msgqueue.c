#include "msgqueue.h"

#include "../app/app.h"
#include "../base/cstr.h"
#include "game.h"
#include "../base/memory.h"
#include "msg.h"
#include "msgcontroller.h"
#include "../trail/node.h"
#include "../app/otiiapp.h"
#include "../data/strrsrc.h"

struct msg_queue *g_msgq_main_a = NULL;
struct msg_queue *g_msgq_main_b = NULL;
struct msg_queue *g_msgq_current = NULL;
struct msg_controller *g_msgq_aux = NULL;

void MsgQueueInit(struct msg_queue *self)
{
	ListInit(&self->base);
}

struct msg_queue *MsgQueueCreate(void)
{
	struct msg_queue *self = MemAlloc(1, sizeof(struct msg_queue));
	MsgQueueInit(self);

	return self;
}

void msg_queue_destroy(struct msg_queue *self)
{
	(void) self;
}

void MsgQueueDestroy(struct msg_queue **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct msg_queue *ctx = *ctx_mod;
	*ctx_mod = NULL;

	msg_queue_destroy(ctx);

	MemFree(ctx);
}

void MsgReplacePlace(struct msg *self, struct cstr *text)
{
	(void) self;

	struct game *game = GetGameObj();
	struct cstr *place_name = StrRsrcGetCStr(game->node_names, game->cur_node->node_id);

	if (!place_name)
		place_name = CStrCreate();

	CStrReplaceStr(text, "[place]", place_name);
	CStrDestroy(&place_name);
}

struct msg_queue *GetMessageQueue(void)
{
	if (!g_msgq_current) {
		g_msgq_main_a = MsgQueueCreate();
		g_msgq_main_b = MsgQueueCreate();
		g_msgq_current = g_msgq_main_a;
	}

	return g_msgq_current;
}

struct msg_controller *GetAuxMsgqueue(void)
{
	if (!g_msgq_aux)
		g_msgq_aux = MsgControllerCreate();

	return g_msgq_aux;
}

void ReleaseMessageQueues(void)
{
	if (g_msgq_current) {
		MsgQueueDestroy(&g_msgq_main_a);
		MsgQueueDestroy(&g_msgq_main_b);
		g_msgq_current = NULL;
	}

	g_msgq_main_a = NULL;
	g_msgq_main_b = NULL;

	if (g_msgq_aux)
		MsgControllerDestroy(&g_msgq_aux);

	if (g_numberwords_rsrc)
		StrRsrcDestroy(&g_numberwords_rsrc);
}

static struct msg *queue_new(struct msg_queue *self, int32_t cmd, struct cstr *payload, intptr_t member,
	intptr_t subject, struct cstr *title, int32_t scene_variant)
{
	struct msg *node = MsgCreate(cmd, payload, member, subject);
	node->title = title;

	if (scene_variant != -1)
		node->scene_variant = scene_variant;

	ListInsertTail(&self->base, &node->node);

	return node;
}

struct msg *MsgQueueEnqueue(struct msg_queue *self, int32_t cmd, struct cstr *payload, intptr_t member,
	intptr_t subject, struct cstr *title, int32_t scene_variant)
{
	struct msg *node = NULL;

	if (g_msgq_main_b != self) {
		if (!GameIsEventAllowed(GetGameObj(), cmd))
			return NULL;
	}

	switch (cmd) {
		case MSG_MISSING_PERSON:
		case MSG_SICK_LIVESTOCK:
		case MSG_INJURED_LIVESTOCK:
		case MSG_CUTS_AND_ABRASIONS:
		case MSG_ALKALI_SICKNESS:
		case MSG_ANIMAL_ALKALI_SICKNESS:
		case MSG_BROKEN_BONE:
		case MSG_SPRAIN:
		case MSG_CHOLERA:
		case MSG_BAD_COLD:
		case MSG_CONCUSSION:
		case MSG_DYSENTERY:
		case MSG_THE_GRIPPE:
		case MSG_MALARIA:
		case MSG_MEASLES:
		case MSG_MOUNTAIN_FEVER:
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
		case MSG_MEMBER_DIED:
		case MSG_ANIMAL_IN_HOLE:
		case MSG_FREEZING:
		case MSG_LOCKJAW:
		case MSG_INTERNAL_INJURIES:
		case MSG_WOUND:
		case MSG_WATER_POISONING:
		case MSG_THIRST:
		case MSG_CAUGHT_FISH:
		case 1003:
		case MSG_UNKNOWN_AILMENT:
			node = queue_new(self, cmd, payload, member, subject, title, scene_variant);
			break;
		default:
			node = MsgQueueFindByCmd(self, cmd);

			if (node) {
				node->choice_state = 1;

			} else {
				node = queue_new(self, cmd, payload, member, subject, title, scene_variant);
			}
			break;
	}

	return node;
}

struct msg *MsgQueuePost(struct msg_queue *self, int32_t cmd, struct cstr *payload, intptr_t member,
	intptr_t subject, struct cstr *title, int32_t scene_variant)
{
	bool switched = false;
	struct game *game = GetGameObj();

	if (game->travel_state == TRAVEL_RESTING)
		return MsgQueueEnqueue(self, cmd, payload, member, subject, title, scene_variant);

	struct game *game2 = GetGameObj();

	if (!game2->event_node && g_msgq_current != g_msgq_main_b) {
		g_msgq_current = g_msgq_main_b;
		switched = true;
	}

	struct msg *result = MsgQueueEnqueue(GetMessageQueue(), cmd, payload, member, subject, title, scene_variant);

	if (switched)
		MsgQueuePump(GetMessageQueue(), NULL);

	return result;
}

static void remove_if_done(struct msg_queue *self, struct msg *cur)
{
	if (cur->choice_state == 2) {
		ListRemove(&self->base, &cur->node);
		ObjectDelete(cur);
	}
}

void MsgQueuePump(struct msg_queue *self, struct msg *node)
{
	if (GetGameObj()->event_node)
		return;

	if (!node) {
		node = MsgFromNode(self->base.head);

	} else {
		struct msg *cur = node;
		node = MsgFromNode(node->node.next);
		remove_if_done(self, cur);
	}

	while (node) {
		if (node->choice_state == 0 && node->number_index < 1000) {
			bool invert = false;
			int32_t number_value = MsgGetNumberValue(node);
			node->rsrc_index = -1;

			if (number_value < 0) {
				number_value = -number_value;
				invert = true;
			}

			OtiiAppPlaySfx((struct otii_app *) g_app, number_value, invert);
			GameRun(GetGameObj(), node);
			break;

		} else if (node->choice_state == 1 || node->number_index >= 1000) {
			MsgControllerHandleEvent(GetAuxMsgqueue(), node);
			struct msg *cur = node;
			node = MsgFromNode(node->node.next);
			remove_if_done(self, cur);
		}
	}

	if (!node)
		g_msgq_current = g_msgq_main_a;
}

struct msg *MsgQueueFindByCmd(struct msg_queue *self, int32_t cmd)
{
	for (struct msg *msg = MsgFromNode(self->base.head); msg; msg = MsgFromNode(msg->node.next)) {
		if (msg->number_index == cmd)
			return msg;
	}

	return NULL;
}
