#include "sndstream.h"

#include <stddef.h>

#include "../draw/mediabase.h"
#include "../base/memory.h"
#include "sndobj.h"

const struct snd_stream_ops SND_STREAM_OPS_TABLE = {
	{
		{snd_stream_destroy},
		snd_stream_acquire_object,
		snd_stream_create_object,
	},
	snd_stream_pause,
	snd_stream_resume,
	snd_stream_set_level,
};

static void streams_resize(struct snd_list *list, int32_t count)
{
	if (count <= 0) {
		MemFree(list->items);
		list->items = NULL;
		list->count = 0;
		return;
	}

	list->items = MemRealloc(list->items, (size_t) count, sizeof(struct snd_obj *));
	list->count = count;
}

void SndStreamInit(struct snd_stream *self, struct server *source)
{
	ServerInit(&self->base, source);
	self->base.base.ops = &SND_STREAM_OPS_TABLE.base.base;

	self->unused_E = 0;
	self->level = 255;
	self->streams = MemAlloc(1, sizeof(struct snd_list));
	self->enabled = true;
}

void snd_stream_destroy(struct object *obj)
{
	struct snd_stream *self = (struct snd_stream *) obj;

	if (self->streams) {
		MemFree(self->streams->items);
		MemFree(self->streams);
		self->streams = NULL;
	}

	obj->ops = &SERVER_OPS_TABLE.base;
	server_destroy(obj);
}

void snd_stream_pause(struct snd_stream *self)
{
	struct server *server = &self->base;

	if (server->delegate)
		SND_STREAM_OPS(server->delegate)->Pause((struct snd_stream *) server->delegate);

	if (server->cache_count == 0)
		return;

	for (int32_t index = 0; index < server->cache_count; index++) {
		struct snd_obj *stream = (struct snd_obj *) server->cache[index].obj;
		SND_OBJ_OPS(stream)->Pause(stream);
	}
}

void snd_stream_resume(struct snd_stream *self)
{
	struct server *server = &self->base;

	if (server->delegate)
		SND_STREAM_OPS(server->delegate)->Resume((struct snd_stream *) server->delegate);

	if (server->cache_count == 0)
		return;

	for (int32_t index = 0; index < server->cache_count; index++) {
		struct snd_obj *stream = (struct snd_obj *) server->cache[index].obj;
		SND_OBJ_OPS(stream)->Resume(stream);
	}
}

int32_t snd_stream_set_level(struct snd_stream *self, int32_t level)
{
	struct server *server = &self->base;

	self->level = level;

	if (server->delegate)
		SND_STREAM_OPS(server->delegate)->SetLevel((struct snd_stream *) server->delegate, level);

	if (server->cache_count == 0)
		return 0;

	for (int32_t index = 0; index < server->cache_count; index++) {
		struct snd_obj *stream = (struct snd_obj *) server->cache[index].obj;
		SND_OBJ_OPS(stream)->SetLevel(stream, level);
	}

	return 0;
}

void SndStreamStartStream(struct snd_stream *self, int32_t key_hi, int32_t key_lo, bool flag)
{
	if (!self->streams)
		return;

	if (!self->enabled)
		return;

	int32_t key = (int32_t) (((uint32_t) (uint16_t) key_hi << 16) + (uint16_t) key_lo);
	struct snd_obj *stream = (struct snd_obj *) SERVER_OPS(self)->AcquireObject(&self->base, key, flag);

	if (!stream)
		return;

	int32_t count = self->streams->count;
	streams_resize(self->streams, count + 1);
	self->streams->items[count] = stream;
	SND_OBJ_OPS(stream)->Begin(stream, false);
}

struct media_base *snd_stream_acquire_object(struct server *obj, int32_t key, bool flag)
{
	struct snd_stream *self = (struct snd_stream *) obj;

	struct media_base *result = server_acquire_object(obj, key, flag);

	if (self->streams) {
		int32_t count = self->streams->count;

		if (count > 0) {
			struct snd_obj **streams = self->streams->items;
			int32_t index = 0;

			while (index < count) {
				int32_t status = SND_OBJ_OPS(streams[index])->GetStatus(streams[index]);

				if (status != 1) {
					ReleaseDrawable(&streams[index]->base, false);
					--count;
					streams[index] = streams[count];

				} else {
					++index;
				}
			}
		}

		streams_resize(self->streams, count);
	}

	return result;
}

void SndStreamStopAll(struct snd_stream *self)
{
	struct server *server = &self->base;

	self->enabled = false;

	if (server->delegate)
		SND_STREAM_OPS(server->delegate)->Pause((struct snd_stream *) server->delegate);

	if (server->cache_count == 0)
		return;

	for (int32_t index = 0; index < server->cache_count; index++) {
		struct snd_obj *stream = (struct snd_obj *) server->cache[index].obj;
		SND_OBJ_OPS(stream)->Stop(stream);
	}
}

void SndStreamMarkChain(struct snd_stream *self)
{
	self->enabled = true;

	if (self->base.delegate)
		SndStreamMarkChain((struct snd_stream *) self->base.delegate);
}

struct media_base *snd_stream_create_object(struct server *self, int32_t key, bool flag)
{
	(void) self;
	(void) key;
	(void) flag;

	return NULL;
}
