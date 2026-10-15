#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "modaldialog.h"

struct media_view;
struct view;

struct about_otii_dlg_ops {
	struct modal_dialog_ops base;
};

struct about_otii_dlg {
	struct modal_dialog base;
	struct media_view *media;
	struct view *sysinfo_view;
	bool media_visible;
	int32_t clip_id;
	int32_t media_bg_id;
	int32_t media_fg_id;
	struct point pt_a;
	struct point pt_b;
};

#define ABOUT_OTII_DLG_OPS(p) ((const struct about_otii_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct about_otii_dlg_ops ABOUT_OTII_DLG_OPS_TABLE;

void AboutOtiiDlgInit(struct about_otii_dlg *self, int32_t a2);
struct about_otii_dlg *AboutOtiiDlgCreate(int32_t a2);
void AboutOtiiDlgDestroy(struct about_otii_dlg **ctx_mod);
void about_otii_dlg_destroy(struct object *self);

void about_otii_dlg_on_idle_spare(struct evt_handler *self, void *param);

void about_otii_dlg_begin(struct modal_dialog *self, intptr_t param);
void about_otii_dlg_accept(struct modal_dialog *self, intptr_t param);

void about_otii_dlg_on_event(struct evt_handler *self, struct cmd_event *event);

struct view *BuildSysinfoLabels(struct rect rc);
