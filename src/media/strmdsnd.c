#include "strmdsnd.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

#include "../app/app.h"
#include "../data/assets.h"
#include "../base/file.h"
#include "filesnd.h"
#include "filstrm.h"
#include "../data/gfxdev.h"
#include "../base/ini.h"
#include "../base/memory.h"
#include "memsnd.h"
#include "../app/replay.h"
#include "ressnd.h"
#include "../data/rsrc.h"
#include "sndupkstrm.h"
#include "winmixedstreamedsnd.h"
#include "xfadestrm.h"

struct strmd_snd *g_sound_server;

const struct strmd_snd_ops STRMD_SND_OPS_TABLE = {
	{
		{
			{strmd_snd_destroy},
			snd_stream_acquire_object,
			strmd_snd_create_object,
		},
		strmd_snd_pause,
		strmd_snd_resume,
		snd_stream_set_level,
	},
};

void StrmdSndInitServer(struct strmd_snd *self, struct assets *source)
{
	char setting_text[64];

	SndStreamInit(&self->base, NULL);
	self->base.base.base.ops = &STRMD_SND_OPS_TABLE.base.base.base;

	const char *default_setting = "30,2048";
	self->source = source;
	IniGetString("sound", "buffers", default_setting, setting_text, sizeof(setting_text));

	int16_t buffer_count = 30;
	int16_t buffer_size = 2048;
	(void) sscanf(setting_text, "%" SCNd16 ",%" SCNd16, &buffer_count, &buffer_size);
	StrmdSndInit(buffer_count, buffer_size);
}

struct strmd_snd *StrmdSndCreate(struct assets *source)
{
	struct strmd_snd *self = MemAlloc(1, sizeof(struct strmd_snd));
	StrmdSndInitServer(self, source);

	return self;
}

void strmd_snd_destroy(struct object *obj)
{
	StrmdSndUnInit();

	obj->ops = &SND_STREAM_OPS_TABLE.base.base;
	snd_stream_destroy(obj);
}

static struct file *get_element_file(struct assets *lib, int32_t id, int32_t *offset, int32_t *length)
{
	char path[1024];
	int64_t where = 0;
	int32_t size = 0;

	if (!AssetsGetElementInfo(lib, "Csnd", id, path, sizeof(path), &where, &size))
		return NULL;

	*offset = (int32_t) where;
	*length = size;

	return FileOpen(path, FILE_READ);
}

struct media_base *strmd_snd_create_object(struct server *obj, int32_t key, bool flag)
{
	struct strmd_snd *self = (struct strmd_snd *) obj;
	struct snd_base *sound = NULL;
	struct file *file = NULL;
	struct stream *file_stream = NULL;
	struct snd_upk_strm *unpack_stream = NULL;
	struct snd_strm *xfade_stream = NULL;
	uint8_t *block = NULL;
	int32_t block_size = 0;
	int32_t offset = 0;
	int32_t length = 0;

	int32_t kind = (key >> 16) & 0xFFFF;
	int32_t res_id = (int16_t) key;

	self->source = g_data_lib;

	switch (kind) {
		case SND_KIND_STREAMED:
			file = get_element_file(self->source, res_id, &offset, &length);

			if (file) {
				file_stream = (struct stream *) FilStrmCreate(file, offset, length, 0x8000, 70);
				unpack_stream = SndUpkStrmCreate(file_stream);
				sound = (struct snd_base *) WinMixedStreamedSndCreate(obj, &unpack_stream->base, 0x1000);
			}
			break;

		case SND_KIND_IN_MEMORY:
			file = get_element_file(self->source, res_id, &offset, &length);

			if (file) {
				file_stream = (struct stream *) FilStrmCreate(file, offset, length, 0x8000, 70);
				unpack_stream = SndUpkStrmCreate(file_stream);

				struct stream *upk = &unpack_stream->base.base;
				int32_t total_size = STREAM_OPS(upk)->Size(upk);
				block_size = STREAM_OPS(upk)->Size(upk);

				if (block_size > 0) {
					block = MemAlloc((size_t) block_size, 1);
					offset = 0;

					while (offset < total_size) {
						int32_t remaining = total_size - offset;
						int32_t status = STREAM_OPS(upk)->Get(upk, block + offset, &remaining);
						offset += remaining;

						if (status && status != 2)
							break;
					}
				}
			}

			if (block)
				sound = (struct snd_base *) MemSndCreate(obj, block, block_size);

			if (unpack_stream) {
				ObjectDelete(unpack_stream);

			} else {
				if (file_stream) {
					ObjectDelete(file_stream);

				} else if (file) {
					FileClose(&file);
				}
			}
			break;

		case SND_KIND_CROSSFADED:
			file = get_element_file(self->source, res_id, &offset, &length);

			if (file) {
				file_stream = (struct stream *) FilStrmCreate(file, offset, length, 0x8000, 70);
				unpack_stream = SndUpkStrmCreate(file_stream);
				xfade_stream = (struct snd_strm *) XfadeStrmCreate(&unpack_stream->base);
				sound = (struct snd_base *) WinMixedStreamedSndCreate(obj, xfade_stream, 0x1000);
			}
			break;

		case SND_KIND_EXE_WAVE:
			sound = (struct snd_base *) ResSndCreate(obj, res_id);
			break;

		case SND_KIND_WAV_FILE:
			sound = (struct snd_base *) FileSndCreate(obj, res_id);
			break;

		default:
			snd_stream_create_object(obj, key, flag);
			break;
	}

	if (sound)
		SndObjSetSource(&sound->base, AssetsGetElement(self->source, "SndQ", res_id, flag));

	return (struct media_base *) sound;
}

void strmd_snd_pause(struct snd_stream *self)
{
	StrmdSndPauseAll();
	snd_stream_pause(self);
}

void strmd_snd_resume(struct snd_stream *self)
{
	StrmdSndResumeAll();
	snd_stream_resume(self);
}

static void *hook_create(struct assets *lib)
{
	g_sound_server = StrmdSndCreate(lib);

	return g_sound_server;
}

static void hook_resume(void *server)
{
	struct snd_stream *s = server;
	SND_STREAM_OPS(s)->Resume(s);
}

static void hook_release(void *server)
{
	struct strmd_snd *s = server;
	ServerReleaseAll(&s->base.base);
	ObjectDelete(s);

	if (g_sound_server == s)
		g_sound_server = NULL;
}

static void hook_stop(int32_t handle)
{
	StrmdSndStop(handle);
}

void StrmdSndInstallHooks(void)
{
	AppSetSoundServerHooks(hook_create, hook_resume, hook_release);
	RpSetSoundStop(hook_stop);
}
