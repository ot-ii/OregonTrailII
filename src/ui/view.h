#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../app/event.h"
#include "ctrldesc.h"
#include "../app/evthandler.h"
#include "../base/geom.h"

struct doc_file;
struct obarray;
struct region;
struct screen_dc;
struct strrsrc;
struct view;
struct window;

typedef struct view *(*CtrlDescCreateFunc)(struct ctrl_desc *desc);

struct view_ops {
	struct evt_handler_ops base;
	void (*AddChild)(struct view *self, struct view *node);
	struct rect (*GetBounds)(struct view *self);
	struct region *(*GetClipRegion)(struct view *self);
	void (*Hide)(struct view *self, bool fill, bool invalidate);
	struct view *(*RemoveChild)(struct view *self, struct event *ev);
	struct view *(*RemoveFromParent)(struct view *self);
	void (*SetCursorId)(struct view *self, int32_t cursor_id);
	void (*SetDevice)(struct view *self, struct window *device);
	void (*Show)(struct view *self);
	void (*DrawChildren)(struct view *self, struct screen_dc *dc, const struct rect *bounds);
	void (*MoveChildren)(struct view *self, int32_t delta, const struct rect *reserved);
};

struct view {
	struct evt_handler base;
	bool enabled;
	struct doc_file *data_file;
	struct rect rect_a;
	struct rect rect_b;
	struct view *parent;
	bool has_child_list;
	struct obarray *child_list;
	struct region *region_bounds;
	struct region *region_clip;
	struct window *owner_window;
	int32_t cursor_id;
};

#define VIEW_OPS(p) ((const struct view_ops *) ((const struct object *) (p))->ops)

extern const struct view_ops VIEW_OPS_TABLE;

extern struct view *g_focus_object;

void ViewInit(struct view *self, int32_t classid, int32_t id, const struct rect *rect);
void ViewInitId(struct view *self, int32_t id, const struct rect *rect);
void ViewInitPoint(struct view *self, int32_t classid, int32_t id, const struct point *pos);
struct view *ViewCreate(int32_t classid, int32_t id, const struct rect *rect);
struct view *ViewCreateId(int32_t id, const struct rect *rect);
struct view *ViewCreatePoint(int32_t classid, int32_t id, const struct point *pos);
void ViewDestroy(struct view **ctx_mod);
void view_destroy(struct object *self);

void view_on_event(struct evt_handler *self, struct cmd_event *event);
void view_on_idle(struct evt_handler *self, void *task);
void view_on_idle_spare(struct evt_handler *self, void *task);
void view_add_child(struct view *self, struct view *node);
struct rect view_get_bounds(struct view *self);
struct region *view_get_clip_region(struct view *self);
void view_hide(struct view *self, bool fill, bool invalidate);
struct view *view_remove_child(struct view *self, struct event *ev);
struct view *view_remove_from_parent(struct view *self);
void view_set_cursor_id(struct view *self, int32_t cursor_id);
void view_set_device(struct view *self, struct window *device);
void view_show(struct view *self);
void view_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);
void view_move_children(struct view *self, int32_t delta, const struct rect *reserved);

void ViewComputeClip(struct view *self, struct screen_dc *dc);
void ViewSetInheritFlag(struct view *self, struct doc_file *file);
void ViewSetDataFlag(struct view *self);
struct screen_dc *ViewGetWinDc(struct view *self);
bool ViewIsVisible(struct view *self);
void ViewNotifyChanged(struct view *self);
void ViewSetVisible(struct view *self);
void ViewToLocal(struct view *self, struct point *pt);
bool ViewPointInBounds(struct view *self, const struct point *pt);
void ViewSubtractRegion(struct view *self, struct region *rgn);
struct view *ViewFindByName(struct view *self, struct event spec);
struct view *ViewFindById(struct view *self, int32_t id);
struct view *ViewFindByTypeId(struct view *self, int32_t code, int32_t id);
struct view *ViewFindChildAt(struct view *self, struct point pt);
void ViewMoveTo(struct view *self, const struct point *pos);
struct point ViewGetOrigin(struct view *self);
struct obarray *ViewGetChildList(struct view *self);

bool ViewHitTest(struct view *view, const struct rect *ir);

struct view *CreateGroupFromTemplate(struct ctrl_desc *desc);
