#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "modaldialog.h"

struct strrsrc;
struct view;
struct rect;

struct about {
	struct modal_dialog base;
	struct strrsrc *strings;
	int32_t details_shown;
};

extern const struct modal_dialog_ops ABOUT_OPS_TABLE;

void AboutInit(struct about *self);
struct about *AboutCreate(void);
void AboutDestroy(struct about **ctx_mod);
void about_destroy(struct object *self);

void about_on_event(struct evt_handler *self, struct cmd_event *event);

void AboutFillSysInfo(struct about *self, struct view *content);
void AboutBuildSysInfoDisplay(struct about *self, struct rect *rect, struct view *content);
struct view *AboutBuildAboutLogo(struct about *self, const struct rect *view_rect);
void AboutToggleDetails(struct about *self);
void AboutPostToggleEvent(struct about *self, bool which);
