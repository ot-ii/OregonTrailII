#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "../ui/view.h"

struct cmd_event;
struct end_game;
struct game;

enum end_type {
	END_FINAL_SCORE = 0,
	END_LAND_CLAIM = 1,
	END_GOLD_CLAIM = 2,
	END_EPILOGUE = 3,
};

struct end_frame_ops {
	struct view_ops base;
};

struct end_frame {
	struct view base;
	struct end_game *result;
	struct game *game;
	int32_t state;
};

#define END_FRAME_OPS(p) ((const struct end_frame_ops *) ((const struct object *) (p))->ops)

extern const struct end_frame_ops END_FRAME_OPS_TABLE;

extern bool g_journal_logged_land_deed;
extern bool g_journal_logged_gold_claim;
extern bool g_journal_logged_arrival;
extern bool g_journal_logged_final_score;
extern bool g_journal_logged_epilogue;

void EndFrameInit(struct end_frame *self, int32_t view_id, const struct rect *rect, struct end_game *result,
	struct game *game, int32_t end_type);
struct end_frame *EndFrameCreate(int32_t view_id, const struct rect *rect, struct end_game *result, struct game *game,
	int32_t end_type);
void EndFrameDestroy(struct end_frame **ctx_mod);
void end_frame_destroy(struct object *self);

void end_frame_on_event(struct evt_handler *self, struct cmd_event *evt);
void EndFrameAdvanceState(struct end_frame *self, int32_t state);
