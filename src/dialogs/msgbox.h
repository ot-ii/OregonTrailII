#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "modaldialog.h"

struct cstr;
struct inv_list;

struct msg_box_ops {
	struct modal_dialog_ops base;
};

struct msg_box {
	struct modal_dialog base;
};

#define MSG_BOX_OPS(p) ((const struct msg_box_ops *) ((const struct object *) (p))->ops)

extern const struct msg_box_ops MSG_BOX_OPS_TABLE;

void MsgBoxInit(struct msg_box *self, struct cstr *text, bool two_buttons);
struct msg_box *MsgBoxCreate(struct cstr *text, bool two_buttons);
void MsgBoxDestroy(struct msg_box **ctx_mod);
void msg_box_destroy(struct object *self);
void msg_box_on_event(struct evt_handler *self, struct cmd_event *event);

void ShowItemListBox(const char *lead, struct inv_list *list);
