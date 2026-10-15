#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../ui/control.h"
#include "../app/event.h"
#include "../base/geom.h"

struct ctrl_desc;
struct mci_video;
struct mov_player;
struct path;
struct screen_dc;
struct vfw_probe;
struct window;

struct mci_video_ops {
	struct control_ops base;
	void (*Pause)(struct mci_video *self);
	void (*Play)(struct mci_video *self);
	void (*SetVolume)(struct mci_video *self, int32_t volume);
	void (*Stop)(struct mci_video *self);
	void (*PostEvent)(struct mci_video *self, void *ctrl_hwnd, int32_t reserved, int32_t lparam_lo,
		int32_t lparam_hi, int32_t wparam);
};

struct mci_video {
	struct control base;
	struct vfw_probe *probe;
	struct path *path;
	int32_t ctrl_id;
	bool unused_8C;
	bool autoplay;
	bool is_playing;
	bool is_mov;
	struct mov_player *mov;
};

#define MCI_VIDEO_OPS(p) ((const struct mci_video_ops *) ((const struct object *) (p))->ops)

extern const struct mci_video_ops MCI_VIDEO_OPS_TABLE;

void MciVideoInitPath(struct mci_video *self, int32_t id, const struct event *event, const struct rect *rect,
	struct path *path_arg, bool arg_unused_8C, bool arg_autoplay);
void MciVideoInit(struct mci_video *self, int32_t id, const struct event *event, const struct rect *rect,
	int32_t video_id, bool arg_unused_8C, bool arg_autoplay);
struct mci_video *MciVideoCreatePath(int32_t id, const struct event *event, const struct rect *rect,
	struct path *path_arg, bool arg_unused_8C, bool arg_autoplay);
struct mci_video *MciVideoCreate(int32_t id, const struct event *event, const struct rect *rect, int32_t video_id,
	bool arg_unused_8C, bool arg_autoplay);
void MciVideoDestroy(struct mci_video **ctx_mod);
void mci_video_destroy(struct object *self);

void mci_video_on_event(struct evt_handler *self, struct cmd_event *event);
void mci_video_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);
void mci_video_create_or_destroy(struct control *self, struct window *parent);
void mci_video_pause(struct mci_video *self);
void mci_video_play(struct mci_video *self);
void mci_video_set_volume(struct mci_video *self, int32_t volume);
void mci_video_stop(struct mci_video *self);
void mci_video_post_event(struct mci_video *self, void *ctrl_hwnd, int32_t reserved, int32_t lparam_lo,
	int32_t lparam_hi, int32_t wparam);
void mci_video_on_idle(struct evt_handler *self, void *task);

intptr_t MciVideoHandleClick(struct mci_video *self, void *hwnd, uint32_t msg, uintptr_t wparam, intptr_t lparam);

struct view *CtrlFactoryMciVideo(struct ctrl_desc *desc);

int32_t MciVideoFileSize(int32_t id);

bool Xa2ClipLoad(const void *pcm, int32_t bytes, int32_t samples_per_sec, int32_t channels, int32_t bits_per_sample);
void Xa2ClipPlay(int32_t ms);
void Xa2ClipStop(void);
void Xa2ClipVolume(int32_t volume);
void Xa2ClipUnload(void);
