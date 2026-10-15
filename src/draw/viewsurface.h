#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "../base/object.h"

struct canvas;
struct drawable;
struct list;
struct palette;
struct rect_rgn_list;
struct screen_dc;
struct spr_obj;
struct view_surface;
struct win_dc;

struct view_surface_ops {
	struct object_ops base;
	void (*RenderPipeline)(struct view_surface *self, struct win_dc *window, bool flush);
	void (*SetBackgroundSource)(struct view_surface *self, struct drawable *source, int32_t index);
	void (*SetCoverMask)(struct view_surface *self, struct drawable *source, int32_t index);
	struct spr_obj *(*Head)(struct view_surface *self);
	struct spr_obj *(*FindById)(struct view_surface *self, int32_t id);
	struct spr_obj *(*HitTest)(struct view_surface *self, struct point *pt);
	void (*AddChild)(struct view_surface *self, struct spr_obj *node);
	void (*RemoveChild)(struct view_surface *self, struct spr_obj *node);
	void (*MarkAllDirty)(struct view_surface *self);
	void (*RemoveAllChildren)(struct view_surface *self);
	void (*PurgeDead)(struct view_surface *self);
	void (*DrawRegion)(struct view_surface *self, struct rect_rgn_list *region);
	void (*MarkHit)(struct view_surface *self, struct rect_rgn_list *region);
	void (*FlushToWindow)(struct view_surface *self, struct win_dc *windc, struct rect_rgn_list *region);
	void (*RedrawDirty)(struct view_surface *self, struct rect_rgn_list *region);
	void (*Blit)(struct view_surface *self, struct rect *rect);
	void (*ForEachChild)(struct view_surface *self);
};

struct view_surface {
	struct object base;
	struct canvas *main_canvas;
	struct canvas *cover_mask;
	struct canvas *background;
	struct drawable *blit_source;
	struct drawable *cover_source;
	struct point origin;
	struct rect rect_a;
	struct rect rect_b;
	struct rect bounds;
	struct rect_rgn_list *dirty_list;
	struct rect_rgn_list *bounds_list;
	struct rect_rgn_list *clip_list;
	struct list *children;
	bool has_background;
};

#define VIEW_SURFACE_OPS(p) ((const struct view_surface_ops *) ((const struct object *) (p))->ops)

extern const struct view_surface_ops VIEW_SURFACE_OPS_TABLE;

void ViewSurfaceInitDefault(struct view_surface *self);
void ViewSurfaceInit(struct view_surface *self, struct rect *rect, struct palette *palette, bool has_overlay,
	bool has_mask);
struct view_surface *ViewSurfaceCreate(struct rect *rect, struct palette *palette, bool has_overlay,
	bool has_mask);
void ViewSurfaceDestroy(struct view_surface **ctx_mod);
void view_surface_destroy(struct object *self);

struct spr_obj *view_surface_hit_test(struct view_surface *self, struct point *pt);
void view_surface_render_pipeline(struct view_surface *self, struct win_dc *window, bool flush);
void view_surface_purge_dead(struct view_surface *self);
void view_surface_for_each_child(struct view_surface *self);
void view_surface_flush_to_window(struct view_surface *self, struct win_dc *windc, struct rect_rgn_list *region);
void view_surface_set_background_source(struct view_surface *self, struct drawable *source, int32_t index);
void view_surface_set_cover_mask(struct view_surface *self, struct drawable *source, int32_t index);
void view_surface_add_child(struct view_surface *self, struct spr_obj *node);
void view_surface_remove_child(struct view_surface *self, struct spr_obj *node);
struct spr_obj *view_surface_head(struct view_surface *self);
struct spr_obj *view_surface_find_by_id(struct view_surface *self, int32_t id);
void view_surface_mark_all_dirty(struct view_surface *self);
void view_surface_blit(struct view_surface *self, struct rect *rect);
void view_surface_draw_region(struct view_surface *self, struct rect_rgn_list *region);
void view_surface_redraw_dirty(struct view_surface *self, struct rect_rgn_list *region);
void view_surface_mark_hit(struct view_surface *self, struct rect_rgn_list *region);
void view_surface_remove_all_children(struct view_surface *self);

struct spr_obj *ViewSurfaceHitTestTyped(struct view_surface *self, struct point *pt, int32_t type);
void ViewSurfaceInvalidateBoth(struct view_surface *self, struct screen_dc *dc, struct rect pt, struct rect src);
void ViewSurfaceInvalidateRect(struct view_surface *self, struct rect rect);
struct rect ViewSurfaceGetBoundsV(const struct view_surface *self);
struct point ViewSurfaceGetOrigin(const struct view_surface *self);
