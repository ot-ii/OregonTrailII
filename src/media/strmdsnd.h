#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sndstream.h"

struct assets;
struct media_base;
struct server;
struct strmd_snd;

enum snd_kind {
	SND_KIND_EXE_WAVE = 1000,
	SND_KIND_STREAMED = 1002,
	SND_KIND_IN_MEMORY = 1003,
	SND_KIND_WAV_FILE = 1004,
	SND_KIND_CROSSFADED = 1005,
};

struct strmd_snd_ops {
	struct snd_stream_ops base;
};

struct strmd_snd {
	struct snd_stream base;
	struct assets *source;
};

#define STRMD_SND_OPS(p) ((const struct strmd_snd_ops *) ((const struct object *) (p))->ops)

extern const struct strmd_snd_ops STRMD_SND_OPS_TABLE;

void StrmdSndInitServer(struct strmd_snd *self, struct assets *source);
struct strmd_snd *StrmdSndCreate(struct assets *source);
void strmd_snd_destroy(struct object *self);

struct media_base *strmd_snd_create_object(struct server *self, int32_t key, bool flag);
void strmd_snd_pause(struct snd_stream *self);
void strmd_snd_resume(struct snd_stream *self);

void StrmdSndInstallHooks(void);

void StrmdSndInit(int32_t num_buffers, int32_t buffer_size);
void StrmdSndUnInit(void);
void StrmdSndPauseAll(void);
void StrmdSndResumeAll(void);
void StrmdSndInterrupt(void);
int32_t StrmdSndPlayFile(const char *path);
int32_t StrmdSndPlayMem(void *data, int32_t length, bool loop);
int32_t StrmdSndPlayCallback(void *data, int32_t length, int32_t (*fill_cb)(void *arg), void *cb_arg);
int32_t StrmdSndStatus(int32_t handle);
int32_t StrmdSndAmplitude(void);
void StrmdSndStop(int32_t handle);
void StrmdSndPause(int32_t handle);
void StrmdSndResume(int32_t handle);
void StrmdSndVolume(int32_t handle, int32_t vol);
void StrmdSndStopAll(void);
void StrmdSndSetDevice(const wchar_t *id);
int32_t StrmdSndGetPos(int32_t handle);
void StrmdSndLock(void);
void StrmdSndUnlock(void);
