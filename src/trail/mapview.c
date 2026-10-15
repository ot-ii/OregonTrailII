#include "mapview.h"

#include <stddef.h>

#include "../data/assets.h"
#include "../base/byteorder.h"
#include "../draw/canvas.h"
#include "../ui/clipview.h"
#include "../ui/control.h"
#include "../base/cstr.h"
#include "../draw/drawable.h"
#include "../ui/framewin.h"
#include "../game/game.h"
#include "../game/gamecmd.h"
#include "../data/gfxdev.h"
#include "../draw/label.h"
#include "../base/log.h"
#include "../media/mcivideo.h"
#include "../draw/mediabase.h"
#include "../base/memory.h"
#include "meter.h"
#include "node.h"
#include "../draw/objbase.h"
#include "../draw/palette.h"
#include "../draw/pictserver.h"
#include "../draw/region.h"
#include "../base/rng.h"
#include "../data/rsrc.h"
#include "../draw/server.h"
#include "../draw/shape.h"
#include "../data/strrsrc.h"
#include "../game/weather.h"
#include "../draw/windc.h"
#include "../ui/window.h"

struct mci_video *g_map_marker;
int32_t g_map_marker_kept;

int32_t g_step_dx[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
int32_t g_step_dy[8] = {0, -1, -1, -1, 0, 1, 1, 1};

int32_t g_map_clip_count[17] = {0, 3, 4, 6, 5, 3, 3, 3, 4, 3, 3, 3, 4, 3, 3, 3, 3};
int32_t g_map_clip_roll = 5;
int32_t g_clip_kind[96] = {2, 0, 1, 0, 0, 0, 0, 2, 1, 2, 0, 0, 1, 2, 2, 0, 0, 1, 1, 2, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0,
	1, 0, 0, 0, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0};
int32_t g_clip_top[96] = {171, 7, 88, 0, 0, 0, 0, 9, 33, 36, 0, 0, 39, 13, 4, 50, 233, 22, 5, 8, 101, 147, 358, 0, 26,
	17, 96, 0, 0, 0, 2, 23, 2, 0, 0, 0, 14, 31, 238, 0, 0, 0, 19, 34, 37, 24, 0, 0, 23, 0, 165, 0, 0, 0, 22, 70, 101,
	0, 0, 0, 48, 4, 97, 0, 0, 0, 11, 172, 58, 108, 0, 0, 231, 104, 49, 0, 0, 0, 25, 193, 35, 0, 0, 0, 198, 74, 286, 0,
	0, 0, 45, 205, 17, 0, 0, 0};
int32_t g_clip_left[96] = {17, 10, 14, 0, 0, 0, 0, 7, 7, 14, 0, 0, 50, 27, 26, 10, 9, 115, 67, 9, 10, 25, 0, 0, 87, 82,
	0, 0, 0, 0, 2, 25, 3, 0, 0, 0, 28, 19, 41, 0, 0, 0, 148, 236, 15, 161, 0, 0, 32, 62, 0, 0, 0, 0, 14, 36, 116, 0, 0,
	0, 32, 53, 51, 0, 0, 0, 19, 189, 0, 0, 0, 0, 111, 124, 17, 0, 0, 0, 12, 9, 160, 0, 0, 0, 5, 19, 29, 0, 0, 0, 3, 20,
	18, 0, 0, 0};
int32_t g_clip_h[96] = {78, 108, 126, 0, 0, 0, 160, 126, 88, 108, 0, 0, 78, 128, 126, 88, 108, 148, 144, 120, 84, 88,
	148, 0, 148, 144, 160, 0, 0, 0, 192, 144, 144, 0, 0, 0, 144, 138, 114, 0, 0, 0, 128, 144, 138, 114, 0, 0, 192, 144,
	114, 0, 0, 0, 144, 144, 152, 0, 0, 0, 162, 144, 160, 0, 0, 0, 192, 144, 144, 152, 0, 0, 108, 144, 160, 0, 0, 0, 96,
	104, 162, 0, 0, 0, 126, 144, 156, 0, 0, 0, 126, 144, 156, 0, 0, 0};
int32_t g_clip_w[96] = {58, 104, 90, 0, 0, 0, 120, 138, 92, 104, 0, 0, 96, 96, 138, 92, 104, 112, 90, 80, 92, 92, 0, 0,
	104, 110, 112, 0, 0, 0, 144, 90, 104, 0, 0, 0, 90, 108, 108, 0, 0, 0, 96, 90, 108, 108, 0, 0, 90, 90, 105, 0, 0, 0,
	90, 104, 120, 0, 0, 0, 108, 90, 130, 0, 0, 0, 144, 90, 108, 120, 0, 0, 64, 90, 130, 0, 0, 0, 56, 72, 120, 0, 0, 0,
	90, 104, 116, 0, 0, 0, 90, 104, 116, 0, 0, 0};
int32_t g_clip_dx[96] = {29, 115, 22, 229, 303, 16, 187, 20, 26, 216, 438, 94, 213, 12, 375, 12, 22, 168, 48, 21, 319,
	175, 32, 218, 416, 148, 196, 244, 86, 36, 21, 13, 294, 68, 174, 264, 219, 115, 193, 196, 418, 12, 33, 11, 299, 55,
	0, 0, 20, 128, 298, 180, 273, 11, 29, 17, 281, 14, 0, 0, 26, 178, 299, 10, 0, 0, 21, 18, 479, 20, 0, 0, 100, 128,
	0, 0, 0, 0, 269, 12, 20, 49, 245, 260, 63, 23, 52, 242, 0, 0, 171, 10, 40, 217, 0, 0};

const struct map_view_ops MAP_VIEW_OPS_TABLE = {
	{
		{
			{
				{map_view_destroy},
				evt_handler_is_handled,
				evt_handler_set_handled_for,
				evt_handler_set_handled,
				evt_handler_clear_handled_for,
				evt_handler_clear_handled,
				map_view_on_event,
				view_on_idle,
				map_view_on_idle_spare,
			},
			view_add_child,
			view_get_bounds,
			view_get_clip_region,
			view_hide,
			view_remove_child,
			view_remove_from_parent,
			view_set_cursor_id,
			view_set_device,
			view_show,
			map_view_draw_children,
			view_move_children,
		},
	},
};

static struct view *as_view(struct map_view *self)
{
	return &self->base.base;
}

static struct rect get_bounds(struct map_view *self)
{
	return VIEW_OPS(self)->GetBounds(as_view(self));
}

static void offset_by(struct rect *r, const struct point *p)
{
	r->left += p->h;
	r->right += p->h;
	r->top += p->v;
	r->bottom += p->v;
}

static void marker_box(struct map_view *self, struct rect *box, int32_t d)
{
	box->top = self->marker.v - d;
	box->left = self->marker.h - d;
	box->right = self->marker.h + d;
	box->bottom = self->marker.v + d;
}

static void draw_label(struct map_view *self, int32_t node_index, struct screen_dc *dc)
{
	int32_t align = 0;

	struct cstr *label_text = StrRsrcGetCStr(self->labels, self->point_node[node_index]);
	struct rect bounds = get_bounds(self);
	struct rect box = RectMake(bounds.top - 30, bounds.left, bounds.top, bounds.left + 200);

	switch (self->label_align[node_index]) {
		case LABEL_ALIGN_LEFT:
			RectOffset(&box, self->label_pos[node_index].h, self->label_pos[node_index].v);
			align = 0;
			break;

		case LABEL_ALIGN_RIGHT:
			RectOffset(&box, self->label_pos[node_index].h - (box.right - box.left), self->label_pos[node_index].v);
			align = 2;
			break;

		case LABEL_ALIGN_CENTRE:
			RectOffset(&box, self->label_pos[node_index].h - (box.right - box.left) / 2,
				self->label_pos[node_index].v);
			align = 1;
			break;
	}

	struct label *label = LabelCreateStr(&box, label_text, 9, align, 2, 0, 0, 255, 0, 1);

	if (dc)
		SHAPE_OPS(label)->Draw(&label->base, dc);

	ObjectDelete(label);
}

void MapViewInit(struct map_view *self, int32_t view_id, const struct point *pos, int32_t map_id,
	struct game *game)
{
	PicViewInitPointEmpty(&self->base, view_id, pos);
	self->base.base.base.base.ops = &MAP_VIEW_OPS_TABLE.base.base.base.base;

	struct view *view = as_view(self);
	int32_t handler_ids[2];
	struct point art_size = PointMake(0, 0);
	struct point inset_size = PointMake(0, 0);
	struct rect inset_area = RectMake(0, 0, 0, 0);
	struct rect inset_bounds = RectMake(0, 0, 0, 0);
	struct rect frame_bounds = RectMake(0, 0, 0, 0);
	struct rect map_bounds = RectMake(0, 0, 0, 0);

	handler_ids[0] = CMD_MOUSE;
	handler_ids[1] = CMD_MEDIA_DONE;
	EvtHandlerBindHandler(&view->base, 2, handler_ids);

	self->labels = StrRsrcLoad(g_text_lib, 901, true);
	StrRsrcRemapHighChars(self->labels, 0);
	self->lib = g_data_lib;

	struct drawable *map_art = ServerCreateVirtual(&g_pict_server->base.base, map_id + 5000, true);
	OBJ_BASE_OPS(map_art)->GetSize(&map_art->base, &art_size);

	view->rect_b.right = view->rect_b.left + art_size.h;
	view->rect_b.bottom = view->rect_b.top + art_size.v;

	PicViewSetImage(&self->base, map_art);
	self->game = game;
	self->mpvct = NULL;
	self->path_steps_drawn = 0;
	self->path_step_count = self->path_steps_drawn;
	self->cur_node = -1;
	self->pending = NULL;
	self->unused_592 = 0;
	MapViewLoadIndex(self, map_id);

	ObjectDelete(view->region_bounds);

	struct rect own_bounds = view_get_bounds(view);
	view->region_bounds = RegionCreateRect(&own_bounds, 1, 0, 0);

	int32_t clip_count = g_map_clip_count[map_id];

	if (clip_count && RandRangeAvg(1, 5, 1) == g_map_clip_roll && !GetGameObj()->weather->snow_depth) {
		int32_t map_row = map_id - 1;
		--clip_count;
		int32_t slot = RandRangeAvg(0, clip_count, 1);
		int32_t video_id = 100 * map_id + slot + 1001;
		int32_t clip_kind = g_clip_kind[map_row * 6 + slot];

		int32_t width = inset_area.right - inset_area.left;
		int32_t height = inset_area.bottom - inset_area.top;
		inset_area.top = g_clip_left[map_row * 6 + slot];
		inset_area.left = g_clip_top[map_row * 6 + slot];
		inset_area.right = inset_area.left + width;
		inset_area.bottom = inset_area.top + height;

		inset_area.right = inset_area.left + g_clip_h[map_row * 6 + slot];
		inset_area.bottom = inset_area.top + g_clip_w[map_row * 6 + slot];

		struct point top_left = PointMake(view->rect_b.left, view->rect_b.top);
		offset_by(&inset_area, &top_left);
		RectOffset(&inset_area, g_clip_dx[map_row * 6 + clip_kind * 2], g_clip_dx[map_row * 6 + clip_kind * 2 + 1]);

		g_map_marker = MciVideoCreate(-2, &view->base.eh_event, &inset_area, video_id, false, false);
		ControlSetCommand(&g_map_marker->base, CMD_MEDIA_DONE, 0, 0);

	} else {
		g_map_marker = NULL;

		int32_t slot = RandRangeAvg(0, self->inset_count - 1, 1);
		inset_area.top = self->inset_rect[slot].top;
		inset_area.left = self->inset_rect[slot].left;
		inset_area.bottom = self->inset_rect[slot].bottom;
		inset_area.right = self->inset_rect[slot].right;

		int32_t inset_style = NodeInsetId((struct node *) game->cur_node, game);
		struct drawable *inset_art = ServerCreateVirtual(&g_pict_server->base.base, inset_style, true);

		if (!inset_art)
			return;

		OBJ_BASE_OPS(inset_art)->GetSize(&inset_art->base, &inset_size);
		struct canvas *inset_canvas = OBJ_BASE_OPS(inset_art)->GetRect(&inset_art->base, &inset_bounds);
		struct rect placed_rect = inset_bounds;

		int32_t offset_h = (inset_area.right - inset_area.left) - inset_size.h;

		if (offset_h > 0)
			offset_h = RandRangeAvg(0, offset_h, 1);
		else
			offset_h = 0;

		int32_t offset_v = (inset_area.bottom - inset_area.top) - inset_size.v;

		if (offset_v > 0)
			offset_v = RandRangeAvg(0, offset_v, 1);
		else
			offset_v = 0;

		int32_t placed_w = placed_rect.right - placed_rect.left;
		int32_t placed_h = placed_rect.bottom - placed_rect.top;
		placed_rect.top = inset_area.top + offset_v;
		placed_rect.left = inset_area.left + offset_h;
		placed_rect.right = placed_w + placed_rect.left;
		placed_rect.bottom = placed_rect.top + placed_h;

		int32_t frame_id;

		switch (inset_bounds.right - inset_bounds.left) {
			case 112:
				frame_id = 10900;
				break;

			case 119:
				frame_id = 10920;
				break;

			case 149:
				frame_id = 10910;
				break;

			default:
				frame_id = 10900;
				break;
		}

		frame_id = RandRangeAvg(1, 2, 1) + frame_id;

		struct drawable *frame_art = ServerCreateVirtual(&g_pict_server->base.base, frame_id, true);

		if (!frame_art) {
			ReleaseDrawable(&inset_art->base.base, false);
			return;
		}

		struct canvas *frame_canvas = OBJ_BASE_OPS(frame_art)->GetRect(&frame_art->base, &frame_bounds);
		struct canvas *map_canvas = OBJ_BASE_OPS(self->base.image)->GetRect(&self->base.image->base, &map_bounds);
		struct canvas *blend_canvas = CanvasCreateRect(&inset_bounds, 8, 255);
		CANVAS_OPS(blend_canvas)->StretchBlit(blend_canvas, map_canvas, &placed_rect, &inset_bounds, 0, NULL);

		struct point blend_pos = PointMake(inset_bounds.left, inset_bounds.top);
		CANVAS_OPS(blend_canvas)->AlphaBlend(blend_canvas, inset_canvas, frame_canvas, &blend_pos, &inset_bounds, 0,
			&inset_bounds, g_palette);

		struct point top_left = PointMake(view->rect_b.left, view->rect_b.top);
		offset_by(&placed_rect, &top_left);

		ReleaseDrawable(&inset_art->base.base, false);
		ReleaseDrawable(&frame_art->base.base, false);
		SHAPE_OPS(blend_canvas)->SetRect(&blend_canvas->base, &placed_rect);

		struct clip_view *clip_view = ClipViewCreate(-2, &placed_rect, &blend_canvas->base, false);
		MapViewSetPending(self, clip_view);
	}

	g_map_marker_kept = 0;
}

struct map_view *MapViewCreate(int32_t view_id, const struct point *pos, int32_t map_id, struct game *game)
{
	struct map_view *self = MemAlloc(1, sizeof(struct map_view));
	MapViewInit(self, view_id, pos, map_id, game);

	return self;
}

void MapViewDestroy(struct map_view **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

int32_t MapViewLoadIndex(struct map_view *self, int32_t map_id)
{
	if (!self->lib)
		return 0;

	struct rsrc *index_rsrc = AssetsGetElement(self->lib, "MPIDX", map_id + 4000, true);

	if (!index_rsrc)
		return 0;

	if (RsrcStorage(index_rsrc) != RSRC_TEXT) {
		const uint8_t *cursor = RsrcData(index_rsrc);

		self->node_count = ReadBe16(cursor);
		cursor += 2;

		for (int32_t entry_index = 0; self->node_count > entry_index; ++entry_index) {
			self->point_node[entry_index] = ReadBe16(cursor);
			self->point_segment[entry_index] = (uint8_t) ReadBe16(cursor + 2);
			self->marker_xy[entry_index].h = ReadBe16(cursor + 4);
			self->marker_xy[entry_index].v = ReadBe16(cursor + 6);
			self->label_pos[entry_index].h = ReadBe16(cursor + 8);
			self->label_pos[entry_index].v = ReadBe16(cursor + 10);
			self->label_align[entry_index] = ReadBe16(cursor + 12);
			cursor += 14;
		}

		self->inset_count = ReadBe16(cursor);
		cursor += 2;

		for (int32_t entry_index = 0; self->inset_count > entry_index; ++entry_index) {
			self->inset_rect[entry_index].top = ReadBe16(cursor);
			self->inset_rect[entry_index].left = ReadBe16(cursor + 2);
			self->inset_rect[entry_index].bottom = ReadBe16(cursor + 4);
			self->inset_rect[entry_index].right = ReadBe16(cursor + 6);
			cursor += 8;
		}

	} else {
		LogError(LOG_ERROR, "Attempt load a raw resource");
	}

	RsrcDestroy(&index_rsrc);

	return 0;
}

void map_view_destroy(struct object *obj)
{
	struct map_view *self = (struct map_view *) obj;

	if (!g_map_marker_kept && g_map_marker)
		ObjectDelete(g_map_marker);

	g_map_marker = NULL;
	g_map_marker_kept = 0;
	self->game->map_view = NULL;
	StrRsrcDestroy(&self->labels);

	if (self->mpvct)
		RsrcDestroy(&self->mpvct);

	obj->ops = &PIC_VIEW_OPS_TABLE.base.base.base;
	pic_view_destroy(obj);
}

void map_view_on_event(struct evt_handler *handler, struct cmd_event *event)
{
	struct map_view *self = (struct map_view *) handler;

	if (event->command == CMD_MOUSE) {
		struct mouse_event *mouse_ev = (struct mouse_event *) event;
		event->handled = true;

		if (mouse_ev->kind == MOUSE_DOWN) {
			if (NodeBaseGetId(self->game->cur_node) == NODE_KIND_TRAIL) {
				EvtHandlerRemoveIdleHandler(&self->game->base, false);
				GameSetTravelState(self->game, 3);
				DispatchGlobalEvent(CmdEventCreate(GCMD_NODE_CONTINUE));
			}
		}

	} else {
		if (event->command == CMD_MEDIA_DONE) {
			event->handled = true;

			if (g_map_marker) {
				struct view *marker = &g_map_marker->base.base;
				VIEW_OPS(marker)->RemoveFromParent(marker);
				ObjectDelete(g_map_marker);
				g_map_marker = NULL;
			}
		}
	}
}

void MapViewSetPending(struct map_view *self, struct clip_view *evt)
{
	struct view *view = as_view(self);

	if (self->pending) {
		struct event spec = ((struct evt_handler *) self->pending)->eh_event;
		VIEW_OPS(view)->RemoveChild(view, &spec);
		ObjectDelete(self->pending);
	}

	self->pending = evt;

	if (evt)
		VIEW_OPS(view)->AddChild(view, (struct view *) evt);
}

void map_view_draw_children(struct view *view, struct screen_dc *dc, const struct rect *bounds)
{
	struct map_view *self = (struct map_view *) view;

	pic_view_draw_children(view, dc, bounds);
	RegionPaint(self->game->region, dc, 139);

	if (!g_map_marker_kept && g_map_marker) {
		VIEW_OPS(view)->AddChild(view, &g_map_marker->base.base);
		mci_video_play(g_map_marker);
	}

	g_map_marker_kept = 1;

	for (int32_t path_index = 0; self->game->path_len > path_index; path_index++) {
		int32_t node_index = self->game->path_nodes[path_index];

		if (node_index > 999)
			node_index -= 1000;

		if (node_index >= 0) {
			if (self->label_align[node_index])
				draw_label(self, node_index, dc);
		}
	}
}

void MapViewUpdateMarker(struct map_view *self, int32_t pos)
{
	struct evt_handler *handler = &as_view(self)->base;

	if (pos == MAP_MARKER_REDRAW) {
		EVT_HANDLER_OPS(handler)->OnIdleSpare(handler, NULL);
		return;
	}

	struct rect dirty_rect = RectMake(0, 0, 0, 0);
	int32_t last_index = self->game->path_len - 1;
	int32_t node_index = self->game->path_nodes[last_index];

	self->marker_pos = pos;
	self->small_marker_cur = false;

	if (node_index > 999) {
		node_index -= 1000;
		self->small_marker_cur = true;
		self->small_marker[node_index] = true;
	}

	EVT_HANDLER_OPS(handler)->OnIdleSpare(handler, NULL);

	if (pos < 0) {
		self->cur_node = node_index;

		if (node_index >= 0) {
			struct rect bounds = get_bounds(self);
			self->marker.h = self->marker_xy[node_index].h + bounds.left;
			bounds = get_bounds(self);
			self->marker.v = self->marker_xy[node_index].v + bounds.top;

			if (self->small_marker[node_index])
				marker_box(self, &dirty_rect, 1);
			else
				marker_box(self, &dirty_rect, 3);

			RegionFromRect(self->game->region, &dirty_rect);
		}

		if (self->mpvct)
			RsrcDestroy(&self->mpvct);

		self->mpvct = NULL;
		self->mpvct_data = NULL;
		EVT_HANDLER_OPS(handler)->OnIdleSpare(handler, NULL);
		return;
	}

	int32_t path_id = self->game->cur_node_id + 1000 * self->game->cur_node->map_id;

	if (node_index >= 0) {
		if (!self->marker_pos)
			self->marker_pos = 1;

		if (self->mpvct)
			RsrcDestroy(&self->mpvct);

		self->mpvct = AssetsGetElement(self->lib, "MPVCT", path_id, true);

		if (self->mpvct) {
			self->mpvct_data = RsrcData(self->mpvct);
			int32_t start_point = MeterMpvctStartPoint(self->mpvct_data);
			self->path_step_count = MeterMpvctStepCount(self->mpvct_data);

			if (self->point_segment[start_point] == self->point_segment[node_index]) {
				self->same_seg = true;
				self->path_steps_drawn = 0;

			} else {
				self->same_seg = false;
				self->path_steps_drawn = self->path_step_count;
			}
		}
	}
}

static void draw_marker_box(struct map_view *self, struct shape *marker_shape, struct screen_dc *screen_dc)
{
	struct rect box;
	marker_box(self, &box, 1);
	shape_set_rect(marker_shape, &box);

	if (screen_dc)
		shape_draw(marker_shape, screen_dc);

	RegionFromRect(self->game->region, &box);
}

void map_view_on_idle_spare(struct evt_handler *handler, void *param)
{
	struct map_view *self = (struct map_view *) handler;

	struct rect marker_box_rect;
	struct screen_dc *screen_dc = NULL;
	int32_t count;

	struct shape marker_shape;
	ShapeInit(&marker_shape, 0, 255, 255, 1, 0);
	marker_shape.brushColor = 139;

	if (g_map_marker_kept)
		screen_dc = &g_parent_win->base.windc->base;

	if (!self->mpvct) {
		if (self->marker_pos < 0) {
			self->marker_pos = 0;

			if (!self->marker.h) {
				if (!self->marker.v) {
					int32_t first_node = self->game->path_nodes[0];
					struct rect bounds = get_bounds(self);
					self->marker.h = self->marker_xy[first_node].h + bounds.left;
					bounds = get_bounds(self);
					self->marker.v = self->marker_xy[first_node].v + bounds.top;

					int32_t node_kind = NodeBaseGetId(self->game->cur_node);

					if (node_kind == NODE_KIND_M || node_kind == NODE_KIND_N)
						self->small_marker_cur = true;
					else
						self->small_marker_cur = false;
				}
			}

			if (self->small_marker_cur)
				marker_box(self, &marker_box_rect, 1);
			else
				marker_box(self, &marker_box_rect, 3);

			shape_set_rect(&marker_shape, &marker_box_rect);
			RegionFromRect(self->game->region, &marker_box_rect);

			if (screen_dc)
				shape_draw(&marker_shape, screen_dc);
		}

		if (self->cur_node >= 0 && self->label_align[self->cur_node])
			draw_label(self, self->cur_node, screen_dc);

	} else {
		float frac = (float) (self->marker_pos - self->game->miles_to_go);
		float divisor = (float) self->marker_pos;

		if (divisor == 0.0f)
			divisor = 1.0f;

		frac = frac / divisor;

		if (frac > 1.0)
			frac = 1.0f;

		const uint8_t *steps = self->mpvct_data + MPVCT_STEPS;

		if (self->same_seg) {
			frac = self->path_step_count * frac;
			count = (int16_t) (int32_t) frac;
			draw_marker_box(self, &marker_shape, screen_dc);

			for (int32_t step_index = self->path_steps_drawn; step_index < count; step_index++) {
				uint8_t step_code = steps[step_index];
				self->marker.h = g_step_dx[step_code] + self->marker.h;
				self->marker.v = g_step_dy[step_code] + self->marker.v;
				draw_marker_box(self, &marker_shape, screen_dc);
			}

		} else {
			if (self->path_steps_drawn > 0)
				--self->path_steps_drawn;

			frac = self->path_step_count * frac;
			count = (int16_t) (self->path_step_count - (int16_t) (int32_t) frac);
			draw_marker_box(self, &marker_shape, screen_dc);

			for (int32_t step_index = self->path_steps_drawn; step_index >= count; step_index--) {
				uint8_t step_code = steps[step_index];
				self->marker.h = self->marker.h - g_step_dx[step_code];
				self->marker.v = self->marker.v - g_step_dy[step_code];
				draw_marker_box(self, &marker_shape, screen_dc);
			}
		}

		self->path_steps_drawn = count;
	}

	shape_destroy(&marker_shape.base);
}
