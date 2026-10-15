#include "titleanim.h"

#include <stddef.h>

#include "../data/assets.h"
#include "../draw/canvas.h"
#include "../ui/clipview.h"
#include "../app/evthandler.h"
#include "../ui/framewin.h"
#include "../data/gfxdev.h"
#include "../data/image.h"
#include "../base/memory.h"
#include "../app/replay.h"
#include "../data/rsrc.h"
#include "../ui/view.h"
#include "../ui/window.h"

struct canvas *g_title_canvas;

enum {
	LAYER_BIRDS,
	LAYER_RIVER,
	LAYER_RABBIT,
	LAYERS,
};

static const struct {
	int32_t id;
	int32_t x, y;
	int32_t step;
} g_layers[LAYERS] = {
	{22201, 0, 45, 7},
	{22202, 230, 259, 8},
	{22203, 0, 315, 4},
};

struct title_anim {
	struct evt_handler base;
	struct rsrc *rsrc[LAYERS];
	struct cbmp *image[LAYERS];
	int32_t frames[LAYERS];
	int32_t frame[LAYERS];
	uint32_t next[LAYERS];
	struct view *root;
	struct canvas *canvas;
	uint32_t seed;
	int32_t drawn;
};

static struct title_anim *g_title_anim;

static void title_anim_destroy(struct object *self);
static void title_anim_on_idle(struct evt_handler *self, void *param);

static const struct evt_handler_ops TITLE_ANIM_OPS_TABLE = {
	{title_anim_destroy},
	evt_handler_is_handled,
	evt_handler_set_handled_for,
	evt_handler_set_handled,
	evt_handler_clear_handled_for,
	evt_handler_clear_handled,
	evt_handler_on_event,
	title_anim_on_idle,
	evt_handler_on_idle_spare,
};

static int32_t title_anim_roll(struct title_anim *self, int32_t lo, int32_t hi)
{
	self->seed = self->seed * 1103515245u + 12345u;
	return lo + (int32_t) ((self->seed >> 16) % (uint32_t) (hi - lo + 1));
}

static struct title_anim *title_anim_create(struct view *root_view)
{
	struct title_anim *self = MemAlloc(1, sizeof(struct title_anim));
	int32_t i;

	EvtHandlerInit(&self->base, EVCLS_APP + 97, 7097);
	self->base.base.ops = &TITLE_ANIM_OPS_TABLE.base;

	uint32_t now = (uint32_t) GameTicks();
	self->root = root_view;
	self->canvas = g_title_canvas;
	self->seed = RpTime() * 2654435761u + 1;
	self->drawn = 0;
	for (i = 0; i < LAYERS; i++) {
		self->rsrc[i] = g_data_lib ? AssetsGetElement(g_data_lib, "CBMP", g_layers[i].id, false) : NULL;
		self->image[i] = self->rsrc[i] ? CbmpCreate(self->rsrc[i]) : NULL;
		self->frames[i] = self->image[i] ? CbmpFrameCount(self->image[i]) : 0;
		self->frame[i] = 1;
	}
	self->next[LAYER_BIRDS] = now + (uint32_t) title_anim_roll(self, 60, 240);
	self->next[LAYER_RABBIT] = self->next[LAYER_BIRDS] + 240 + (uint32_t) title_anim_roll(self, 20, 120);
	self->next[LAYER_RIVER] = now;

	return self;
}

static void title_anim_destroy(struct object *obj)
{
	struct title_anim *self = (struct title_anim *) obj;
	int32_t i;

	for (i = 0; i < LAYERS; i++) {
		CbmpDestroy(&self->image[i]);
		RsrcDestroy(&self->rsrc[i]);
	}
	obj->ops = &EVT_HANDLER_OPS_TABLE.base;
	evt_handler_destroy(obj);
}

static void title_anim_on_idle(struct evt_handler *eh, void *param)
{
	struct title_anim *self = (struct title_anim *) eh;
	uint32_t now;
	int32_t i;

	(void) param;
	if (!g_parent_win || g_parent_win->base.root_view != self->root || g_title_canvas != self->canvas ||
		((struct clip_view *) self->root)->shape != (struct shape *) self->canvas || ViewFindById(self->root, 98))
	{
		return;
	}
	now = (uint32_t) GameTicks();
	for (i = 0; i < LAYERS; i++) {
		struct rect dirty = {0, 0, 0, 0};
		if (!self->frames[i] || now < self->next[i])
			continue;
		CanvasDrawFrameAt(self->canvas, self->image[i], self->frame[i]++, &dirty, g_layers[i].x, g_layers[i].y);
		if (dirty.bottom > dirty.top && dirty.right > dirty.left) {
			WindowInvalidate((struct window *) g_parent_win, &dirty);
			++self->drawn;
		}
		self->next[i] = now + (uint32_t) g_layers[i].step;
	}
	if (self->frame[LAYER_BIRDS] == self->frames[LAYER_BIRDS]) {
		self->frame[LAYER_BIRDS] = 1;
		self->next[LAYER_BIRDS] += (uint32_t) title_anim_roll(self, 100, 1000) + 120;
	}
	if (self->frame[LAYER_RABBIT] == self->frames[LAYER_RABBIT]) {
		self->frame[LAYER_RABBIT] = 1;
		self->next[LAYER_RABBIT] += (uint32_t) title_anim_roll(self, 100, 1000) + 120;
	}
	if (self->frame[LAYER_RIVER] == self->frames[LAYER_RIVER])
		self->frame[LAYER_RIVER] = 1;
}

static void title_anim_free(void)
{
	if (!g_title_anim)
		return;
	EvtHandlerRemoveIdleHandler(&g_title_anim->base, true);
	ObjectDestroy(&g_title_anim);
}

void TitleAnimStart(struct view *root)
{
	title_anim_free();
	if (!g_title_canvas || !root)
		return;
	g_title_anim = title_anim_create(root);
	EvtHandlerAddIdleHandler(&g_title_anim->base, true, 1);
}

void TitleAnimStop(void)
{
	title_anim_free();
	g_title_canvas = NULL;
}

bool TitleAnimRunning(void)
{
	return g_title_anim != NULL;
}

int32_t TitleAnimFramesDrawn(void)
{
	return g_title_anim ? g_title_anim->drawn : 0;
}
