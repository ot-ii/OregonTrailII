#include "raftframe.h"

#include <stddef.h>

#include "../draw/animsprite.h"
#include "../data/assets.h"
#include "../base/byteorder.h"
#include "../app/evthandler.h"
#include "fordwagon.h"
#include "../game/gamecmd.h"
#include "../base/log.h"
#include "../base/memory.h"
#include "../data/rsrc.h"
#include "../draw/screendc.h"
#include "../draw/sprite.h"
#include "tilemap.h"
#include "../draw/viewsurface.h"
#include "../ui/window.h"

const struct raft_frame_ops RAFT_FRAME_OPS_TABLE = {
	{
		{
			{raft_frame_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			view_on_event,
			view_on_idle,
			raft_frame_on_idle_spare,
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
		raft_frame_draw_children,
		view_move_children,
	},
	raft_frame_find_start_cell,
	raft_frame_finish,
	raft_frame_get_progress,
	raft_frame_get_drift,
};

static uint32_t wrld_cell(const struct raft_frame *self, int32_t index)
{
	return ReadLeU16(self->wrld_data + 4 + 2 * (intptr_t) index);
}

void RaftFrameInit(struct raft_frame *self, int32_t view_id, struct rect *rect, struct palette *palette,
	struct assets *data_lib, int32_t map_id, int32_t wagon_rsrc, int32_t progress, int32_t speed)
{
	ViewInit(&self->base, EVCLS_PICVIEW, view_id, rect);
	self->base.base.base.ops = &RAFT_FRAME_OPS_TABLE.base.base.base;

	struct rect bounds_i;
	uint32_t alt_counter_k = 0;
	int32_t i;

	for (i = 0; i < 11; i++) {
		self->aseq[i] = NULL;
		self->acel[i] = self->aseq[i];
	}

	self->progress = progress;
	self->result = 0;
	self->drift = speed * 0.25 + 1.0;

	int32_t rect_w = rect->right - rect->left;
	int32_t rect_h = rect->bottom - rect->top;

	if (rect_w % 16)
		LogError(LOG_ERROR, "Width not divisible by 16.");

	if (rect_h % 16)
		LogError(LOG_ERROR, "Height not divisible by 16.");

	self->wrld_handle = AssetsGetElement(data_lib, "WRLD", map_id, true);

	int32_t word_count_l = RsrcSize(self->wrld_handle) / 2;
	uint8_t *word_ptr_e = RsrcData(self->wrld_handle);

	for (int32_t word_index_p = 0; word_index_p < word_count_l; word_index_p++, word_ptr_e += 2) {
		uint8_t hi = word_ptr_e[0];
		word_ptr_e[0] = word_ptr_e[1];
		word_ptr_e[1] = hi;
	}

	self->wrld_data = RsrcData(self->wrld_handle);
	self->blck_handle = AssetsGetElement(data_lib, "BLCK", map_id, true);
	self->blck_data = RsrcData(self->blck_handle);

	for (i = 0; i < 11; i++) {
		self->rlea[i] = AssetsGetElement(data_lib, "RLEA", i + 1000, true);
		self->sprite[i] = SpriteCreate(self->rlea[i]);
		SPRITE_OPS(self->sprite[i])->ByteswapResource(self->sprite[i]);
	}

	bounds_i = RectMake(0, 0, rect_h / 16, rect_w / 16);
	self->tilemap = TileMapCreate(rect, palette, &bounds_i, self->wrld_data, self->blck_data, RAFT_SHORE_FIRST);

	struct view_surface *surface = &self->tilemap->base;

	i = 0;
	self->grid_width = ReadLeU16(self->wrld_data);
	self->grid_height = ReadLeU16(self->wrld_data + 2);

	int32_t row_index = (int32_t) self->grid_width;

	for (uint32_t row_f = 1; self->grid_height > row_f; row_f++) {
		int32_t cell_index = row_index;

		for (uint32_t col_h = 0; self->grid_width > col_h; col_h++) {
			uint32_t cell_i = wrld_cell(self, cell_index++);

			if (cell_i > RAFT_CURRENT_LAST) {
				uint32_t anim_rsrc_id_n = 0;

				switch (cell_i) {
					case RAFT_WHIRLPOOL:
						anim_rsrc_id_n = 1000;
						break;
					case RAFT_RAPIDS_1:
						anim_rsrc_id_n = 1009;
						break;
					case RAFT_RAPIDS_2:
						anim_rsrc_id_n = 1010;
						break;
					case RAFT_ROCK_1:
						++alt_counter_k;
						anim_rsrc_id_n = (alt_counter_k & 1) + 1001;
						break;
					case RAFT_ROCK_2:
						++alt_counter_k;
						anim_rsrc_id_n = (alt_counter_k & 1) + 1003;
						break;
					case RAFT_ROCK_3:
						++alt_counter_k;
						anim_rsrc_id_n = (alt_counter_k & 1) + 1005;
						break;
					case RAFT_ROCK_4:
						anim_rsrc_id_n = (alt_counter_k & 1) + 1007;
						++alt_counter_k;
						break;
				}

				if (anim_rsrc_id_n) {
					uint32_t anim_slot_b = anim_rsrc_id_n - 1000;

					if (!self->aseq[anim_slot_b]) {
						self->aseq[anim_slot_b] = AssetsGetElement(data_lib, "ASeq", anim_rsrc_id_n, true);
						RsrcLoadAndUnpack(self->aseq[anim_slot_b]);
					}

					if (!self->acel[anim_slot_b]) {
						self->acel[anim_slot_b] = AssetsGetElement(data_lib, "ACel", anim_rsrc_id_n, true);
						RsrcLoadAndUnpack(self->acel[anim_slot_b]);
					}

					struct anim_sprite *deco = AnimSpriteCreateSeq(surface, self->sprite[anim_slot_b],
						self->aseq[anim_slot_b], self->acel[anim_slot_b], false);

					if (i < 100)
						self->decorations[i] = deco;

					ANIM_SPRITE_OPS(deco)->PositionFrame(deco, 0);
					SPR_OBJ_OPS(deco)->SetAnchor(&deco->base, 16 * (int32_t) col_h + 16, 16 * (int32_t) row_f + 16);
					ANIM_SPRITE_OPS(deco)->PlaySequence(deco, 0, -1);
					deco->base.tag = (int32_t) anim_rsrc_id_n;
					i++;
				}
			}
		}

		row_index += (int32_t) self->grid_width;
	}

	self->wagon = FordWagonCreate(surface, data_lib, (uint16_t) wagon_rsrc, self);
	ANIM_SPRITE_OPS(self->wagon)->PositionFrame(&self->wagon->base, 0);

	int32_t spot_x_g = 0;
	int32_t spot_y_d = CMD_MOUSE;

	TILE_MAP_OPS(self->tilemap)->ScrollTo(self->tilemap, 0, 0);
	RAFT_FRAME_OPS(self)->FindStartCell(self, &spot_x_g, &spot_y_d);
	SPR_OBJ_OPS(self->wagon)->SetAnchor(&self->wagon->base.base, spot_x_g, spot_y_d);

	struct point start_pt_c;
	start_pt_c.h = spot_x_g;
	start_pt_c.v = spot_y_d;
	bounds_i = ViewSurfaceGetBoundsV(surface);

	int32_t half_width_g = (bounds_i.right - bounds_i.left) / 2;
	int32_t three_quarter_height_a = 3 * ((bounds_i.bottom - bounds_i.top) / 4);

	start_pt_c.h = self->tilemap->scroll_pos.h + start_pt_c.h;
	start_pt_c.v = self->tilemap->scroll_pos.v + start_pt_c.v;
	TILE_MAP_OPS(self->tilemap)->ScrollTo(self->tilemap, start_pt_c.h - half_width_g,
		start_pt_c.v - three_quarter_height_a);
	VIEW_SURFACE_OPS(surface)->MarkAllDirty(surface);
	self->finished = false;
	EvtHandlerAddIdleHandler(&self->base.base, false, 0);
}

struct raft_frame *RaftFrameCreate(int32_t view_id, struct rect *rect, struct palette *palette,
	struct assets *data_lib, int32_t map_id, int32_t wagon_rsrc, int32_t progress, int32_t speed)
{
	struct raft_frame *self = MemAlloc(1, sizeof(struct raft_frame));
	RaftFrameInit(self, view_id, rect, palette, data_lib, map_id, wagon_rsrc, progress, speed);

	return self;
}

void RaftFrameDestroy(struct raft_frame **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void raft_frame_destroy(struct object *obj)
{
	struct raft_frame *self = (struct raft_frame *) obj;

	TileMapDestroy(&self->tilemap);
	RsrcDestroy(&self->wrld_handle);
	self->wrld_data = NULL;
	RsrcDestroy(&self->blck_handle);
	self->blck_data = NULL;

	for (int32_t i = 0; i < 11; i++) {
		SpriteDestroy(&self->sprite[i]);
		RsrcDestroy(&self->rlea[i]);
		RsrcDestroy(&self->acel[i]);
		RsrcDestroy(&self->aseq[i]);
	}

	obj->ops = &VIEW_OPS_TABLE.base.base;
	view_destroy(obj);
}

void raft_frame_on_idle_spare(struct evt_handler *handler, void *param)
{
	struct raft_frame *self = (struct raft_frame *) handler;
	struct window *win_o = self->base.owner_window;

	if (win_o && self->tilemap) {
		struct view_surface *surface = &self->tilemap->base;
		VIEW_SURFACE_OPS(surface)->RenderPipeline(surface, win_o->windc, true);
	}

	if (self->finished) {
		((struct timer_task *) param)->done = true;

		struct cmd_event *cmd = CmdEventCreate(GCMD_BUILD_MAIN_SCREEN);
		cmd->param1 = self->result;
		cmd->param2 = self->progress;
		DispatchGlobalEvent(cmd);
	}
}

void raft_frame_draw_children(struct view *view, struct screen_dc *dc, const struct rect *bounds)
{
	struct raft_frame *self = (struct raft_frame *) view;

	(void) bounds;

	if (!self->finished) {
		struct view_surface *surface = &self->tilemap->base;

		VIEW_SURFACE_OPS(surface)->MarkAllDirty(surface);
		TILE_MAP_OPS(self->tilemap)->ScrollTo(self->tilemap, self->tilemap->scroll_pos.h,
			self->tilemap->scroll_pos.v);

		struct canvas *canvas_b = surface->main_canvas;
		struct point origin_j = RectTopLeft(&view->rect_b);
		SCREEN_DC_OPS(dc)->DrawImage(dc, canvas_b, &origin_j, 0);
	}
}

void raft_frame_find_start_cell(struct raft_frame *self, int32_t *out_x, int32_t *out_y)
{
	*out_x /= 16;
	*out_y /= 16;

	bool found_h = false;
	int32_t found_col_f = 0;
	int32_t found_row_e = 0;
	int32_t width_b = (int32_t) self->grid_width;
	int32_t height_g = (int32_t) self->grid_height;
	int32_t target_row_h = *out_y;
	int32_t row_d;
	int32_t grid_m;

	if (height_g < target_row_h) {
		row_d = 1;
		grid_m = width_b;

	} else {
		row_d = target_row_h;
		grid_m = width_b * row_d;
	}

	while (height_g > row_d) {
		int32_t row_ptr_i = grid_m;

		for (int32_t col_g = 0; width_b > col_g; col_g++) {
			uint32_t tile = wrld_cell(self, row_ptr_i);

			if (tile == RAFT_START) {
				found_h = true;
				found_col_f = col_g;
				found_row_e = row_d;

				if (target_row_h < row_d)
					goto done;
			}

			row_ptr_i++;
		}

		row_d++;
		grid_m += width_b;
	}

	if (!found_h)
		LogError(LOG_ERROR, "No safe spots found");

done:
	found_col_f = 16 * found_col_f + 8;
	found_row_e = 16 * found_row_e + 8;
	*out_x = found_col_f;
	*out_y = found_row_e;
}

bool raft_frame_finish(struct raft_frame *self, int32_t result)
{
	self->result = result;
	self->finished = true;

	return EvtHandlerRemoveIdleHandler(&self->base.base, false);
}

int32_t raft_frame_get_progress(struct raft_frame *self)
{
	return self->progress;
}

double raft_frame_get_drift(struct raft_frame *self)
{
	return self->drift;
}

struct tile_map *RaftFrameGetTileMap(struct raft_frame *self)
{
	return self->tilemap;
}
