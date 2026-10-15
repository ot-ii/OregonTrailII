#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "modaldialog.h"

struct overview_map_dlg_ops {
	struct modal_dialog_ops base;
};

struct overview_map_dlg {
	struct modal_dialog base;
};

#define OVERVIEW_MAP_DLG_OPS(p) ((const struct overview_map_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct overview_map_dlg_ops OVERVIEW_MAP_DLG_OPS_TABLE;

void OverviewMapDlgInit(struct overview_map_dlg *self, int32_t str_index);
struct overview_map_dlg *OverviewMapDlgCreate(int32_t str_index);
void OverviewMapDlgDestroy(struct overview_map_dlg **ctx_mod);
void overview_map_dlg_destroy(struct object *self);

void overview_map_dlg_begin(struct modal_dialog *self, intptr_t param);
void overview_map_dlg_accept(struct modal_dialog *self, intptr_t param);
void overview_map_dlg_on_event(struct evt_handler *self, struct cmd_event *event);

extern bool g_overviewmap_open;
