#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "modaldialog.h"

struct canvas;
struct clip_view;
struct drawable;
struct pict_ctrl;
struct rsrc;
struct snd_obj;
struct strrsrc;
struct text_view;
struct timer;

enum {
	CONV_TEXT_BASE = 6000,
};

struct conv_dlg_ops {
	struct modal_dialog_ops base;
};

struct conv_dlg {
	struct modal_dialog base;
	struct point origin;
	int32_t mode;
	int32_t conv_id;
	int32_t node;
	int32_t can_trade;
	struct drawable *sprite;
	struct canvas *canvas_bg;
	struct rect bg_rect;
	struct canvas *canvas_face;
	struct rect face_rect;
	struct clip_view *clip_bg;
	struct clip_view *face_view;
	struct text_view *body_view;
	struct pict_ctrl *response_btn[6];
	struct pict_ctrl *continue_btn;
	int32_t continue_id;
	struct strrsrc *strings;
	struct rsrc *tree;
	struct snd_obj *speech;
	struct rsrc *lipsync;
	const uint8_t *marker_count;
	const uint8_t *markers;
	int32_t marker_index;
	struct timer *tick_timer;
	bool speech_primed;
};

#define CONV_DLG_OPS(p) ((const struct conv_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct conv_dlg_ops CONV_DLG_OPS_TABLE;

void ConvDlgInit(struct conv_dlg *self, int32_t conv_id, int32_t mode, int32_t region);
struct conv_dlg *ConvDlgCreate(int32_t conv_id, int32_t mode, int32_t region);
void ConvDlgDestroy(struct conv_dlg **ctx_mod);
void conv_dlg_destroy(struct object *self);

void conv_dlg_on_event(struct evt_handler *self, struct cmd_event *event);
void conv_dlg_begin(struct modal_dialog *self, intptr_t param);
void conv_dlg_accept(struct modal_dialog *self, intptr_t param);

int32_t ConvDlgGetRsrcId(struct conv_dlg *self);
int32_t ConvDlgGetResponseLink(struct conv_dlg *self, int32_t node, int32_t resp);
int32_t ConvDlgCountResponses(struct conv_dlg *self, int32_t node);
void ConvDlgLipsyncStop(struct conv_dlg *self);
void ConvDlgLipsyncStart(struct conv_dlg *self, int32_t speech_id);
void ConvDlgShowNode(struct conv_dlg *self);
void ConvDlgLoadStrings(struct conv_dlg *self);
void ConvDlgSetMouth(struct conv_dlg *self, uint32_t shape);
void ConvDlgLipsyncTick(struct conv_dlg *self);
void ConvDlgLoadTree(struct conv_dlg *self);
struct rsrc *ConvDlgParseConvTable(struct conv_dlg *self, struct rsrc *src);
void ConvDlgBuildTradeList(struct conv_dlg *self, struct strrsrc *months, struct strrsrc *costs);
int32_t ConvGetSpeaker(int32_t conv_id);
int32_t ConvGetPortraitId(int32_t conv_id);

extern struct strrsrc *g_portrait_table;
extern uint8_t g_slot_flags[1200];
