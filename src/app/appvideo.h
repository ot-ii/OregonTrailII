#pragma once

#include <stdbool.h>

#include "app.h"

struct app_video {
	struct app base;
};

extern const struct app_ops APP_VIDEO_OPS_TABLE;

void AppVideoInit(struct app_video *self, const char *app_name);
void app_video_destroy(struct object *self);
void app_video_pump_tick(struct app *self);
void RegisterWindowClasses(void);
bool CheckVideoMode(void);
const char *GetFirstCmdlineArg(void);

struct health_win;
extern struct health_win *g_health_window;
void AudioDeviceMenuOnInitPopup(void *popup);
bool AudioDeviceMenuCommand(int32_t id);
