#pragma once

#include <stdint.h>

#include "modaldialog.h"

struct assets;

enum {
	GLOSSARY_STRLST = 30110,
};

struct glossary_dlg_ops {
	struct modal_dialog_ops base;
};

struct glossary_dlg {
	struct modal_dialog base;
	int32_t unused_28;
	struct assets *owner;
	int32_t unused_30;
};

#define GLOSSARY_DLG_OPS(p) ((const struct glossary_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct glossary_dlg_ops GLOSSARY_DLG_OPS_TABLE;

void GlossaryDlgInit(struct glossary_dlg *self, struct assets *owner);
struct glossary_dlg *GlossaryDlgCreate(struct assets *owner);
void GlossaryDlgDestroy(struct glossary_dlg **ctx_mod);
void glossary_dlg_destroy(struct object *self);

void glossary_dlg_begin(struct modal_dialog *self, intptr_t param);
void glossary_dlg_on_event(struct evt_handler *self, struct cmd_event *event);
void glossary_dlg_accept(struct modal_dialog *self, intptr_t out);
