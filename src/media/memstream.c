#include "memstream.h"

#include <stddef.h>

#include "../base/memory.h"

const struct mem_stream_ops MEM_STREAM_OPS_TABLE = {
	{
		{mem_stream_destroy},
		stream_free,
		stream_unfree,
		stream_purged,
		mem_stream_clone,
		stream_get,
		mem_stream_get_buf,
		mem_stream_seek,
		mem_stream_seek_ahead,
		mem_stream_position,
		stream_size,
		stream_set_start,
		stream_set_end,
		stream_status,
	},
};

void MemStreamInit(struct mem_stream *self, void *data, int32_t size)
{
	StreamInit(&self->base);
	self->base.base.ops = &MEM_STREAM_OPS_TABLE.base.base;

	self->data = data;
	self->size = size;
	self->pos = 0;
}

struct mem_stream *MemStreamCreate(void *data, int32_t size)
{
	struct mem_stream *self = MemAlloc(1, sizeof(struct mem_stream));
	MemStreamInit(self, data, size);

	return self;
}

void mem_stream_destroy(struct object *obj)
{
	obj->ops = &STREAM_OPS_TABLE.base;
	stream_destroy(obj);
}

int32_t mem_stream_get_buf(struct stream *obj, uint8_t **buf, int32_t *len)
{
	struct mem_stream *self = (struct mem_stream *) obj;

	*buf = self->data + self->pos;
	*len = self->size - self->pos;

	if (*len > 0)
		return 0;

	*len = 0;

	return 1;
}

int32_t mem_stream_seek_ahead(struct stream *obj, int32_t delta)
{
	struct mem_stream *self = (struct mem_stream *) obj;

	return STREAM_OPS(self)->Seek(obj, self->pos + delta);
}

int32_t mem_stream_position(struct stream *obj)
{
	struct mem_stream *self = (struct mem_stream *) obj;

	return self->pos;
}

int32_t mem_stream_seek(struct stream *obj, int32_t new_pos)
{
	struct mem_stream *self = (struct mem_stream *) obj;

	if (self->size < new_pos) {
		self->pos = self->size;
		return 1;
	}

	self->pos = new_pos;

	return 0;
}

struct stream *mem_stream_clone(struct stream *obj)
{
	struct mem_stream *self = (struct mem_stream *) obj;

	return (struct stream *) MemStreamCreate(self->data, self->size);
}
