#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../draw/mediabase.h"

struct rsrc;
struct server;
struct snd_obj;

struct snd_obj_ops {
	struct media_base_ops base;
	int32_t (*Begin)(struct snd_obj *self, bool loop);
	int32_t (*Finish)(struct snd_obj *self);
	int32_t (*Start)(struct snd_obj *self);
	int32_t (*Stop)(struct snd_obj *self);
	int32_t (*PlaySync)(struct snd_obj *self);
	int32_t (*SetLevel)(struct snd_obj *self, int32_t level);
	int32_t (*Slot13)(struct snd_obj *self, int32_t a, int32_t b, int32_t c);
	int32_t (*GetLevel)(struct snd_obj *self);
	int32_t (*SetLoop)(struct snd_obj *self, int32_t start, int32_t b, int32_t c, int32_t d);
	int32_t (*PlayCueRange)(struct snd_obj *self, int32_t start_cue, int32_t loop_cue, int32_t end_cue, int32_t flags);
	int32_t (*Slot17)(struct snd_obj *self, int32_t a);
	int32_t (*GetStatus)(struct snd_obj *self);
	int32_t (*GetPosition)(struct snd_obj *self);
	int32_t (*StartPlayback)(struct snd_obj *self);
	int32_t (*ResumePlayback)(struct snd_obj *self);
	int32_t (*Pause)(struct snd_obj *self);
	int32_t (*Resume)(struct snd_obj *self);
	int32_t (*Slot24)(struct snd_obj *self);
};

struct snd_obj {
	struct media_base base;
	int32_t status;
	int32_t level;
	bool playing;
	bool loop;
	struct rsrc *cue_points;
};

#define SND_OBJ_OPS(p) ((const struct snd_obj_ops *) ((const struct object *) (p))->ops)

extern const struct snd_obj_ops SND_OBJ_OPS_TABLE;

void SndObjInit(struct snd_obj *self, struct server *arg);
void snd_obj_destroy(struct object *self);

void snd_obj_release_source(struct media_base *self);
int32_t snd_obj_begin(struct snd_obj *self, bool loop);
int32_t snd_obj_start_playback(struct snd_obj *self);
int32_t snd_obj_start(struct snd_obj *self);
int32_t snd_obj_finish(struct snd_obj *self);
int32_t snd_obj_resume_playback(struct snd_obj *self);
int32_t snd_obj_stop(struct snd_obj *self);
int32_t snd_obj_play_sync(struct snd_obj *self);
int32_t snd_obj_get_status(struct snd_obj *self);
int32_t snd_obj_set_level(struct snd_obj *self, int32_t level);
int32_t snd_obj_slot13(struct snd_obj *self, int32_t a, int32_t b, int32_t c);
int32_t snd_obj_slot17(struct snd_obj *self, int32_t a);
int32_t snd_obj_slot24(struct snd_obj *self);
int32_t snd_obj_pause(struct snd_obj *self);
int32_t snd_obj_resume(struct snd_obj *self);
int32_t snd_obj_get_level(struct snd_obj *self);
int32_t snd_obj_set_loop(struct snd_obj *self, int32_t start, int32_t b, int32_t c, int32_t d);
int32_t snd_obj_play_cue_range(struct snd_obj *self, int32_t start_cue, int32_t loop_cue, int32_t end_cue,
	int32_t flags);
int32_t snd_obj_get_position(struct snd_obj *self);

void SndObjSetSource(struct snd_obj *self, struct rsrc *src);
