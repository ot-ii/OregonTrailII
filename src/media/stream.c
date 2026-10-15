#include "stream.h"

#include <stddef.h>

const struct stream_ops STREAM_OPS_TABLE = {
	{stream_destroy},
	stream_free,
	stream_unfree,
	stream_purged,
	stream_clone,
	stream_get,
	stream_get_buf,
	stream_seek,
	stream_seek_ahead,
	stream_position,
	stream_size,
	stream_set_start,
	stream_set_end,
	stream_status,
};

void StreamInit(struct stream *self)
{
	ObjectInit(&self->base);
	self->base.ops = &STREAM_OPS_TABLE.base;

	self->error_code = 0;
	self->start_pos = self->end_pos = 0;
}

void stream_destroy(struct object *self)
{
	object_destroy(self);
}

int32_t stream_get(struct stream *self, uint8_t *buf, int32_t *len)
{
	(void) self;
	(void) buf;

	*len = 0;

	return 1;
}

int32_t stream_get_buf(struct stream *self, uint8_t **buf, int32_t *len)
{
	(void) self;
	(void) buf;

	*len = 0;

	return 1;
}

int32_t stream_seek(struct stream *self, int32_t pos)
{
	(void) self;
	(void) pos;

	return 1;
}

int32_t stream_seek_ahead(struct stream *self, int32_t delta)
{
	(void) self;
	(void) delta;

	return 1;
}

int32_t stream_position(struct stream *self)
{
	(void) self;

	return 0;
}

int32_t stream_size(struct stream *self)
{
	(void) self;

	return -1;
}

bool stream_free(struct stream *self)
{
	(void) self;

	return false;
}

bool stream_unfree(struct stream *self)
{
	(void) self;

	return false;
}

bool stream_purged(struct stream *self)
{
	(void) self;

	return true;
}

struct stream *stream_clone(struct stream *self)
{
	(void) self;

	return NULL;
}

int32_t stream_set_start(struct stream *self, int32_t start)
{
	self->start_pos = start;

	return 0;
}

int32_t stream_set_end(struct stream *self, int32_t end)
{
	self->end_pos = end;

	return 0;
}

int32_t stream_status(struct stream *self)
{
	(void) self;

	return 1;
}

int32_t StreamGetStart(struct stream *self)
{
	return self->start_pos;
}

int32_t StreamGetEnd(struct stream *self)
{
	return self->end_pos;
}

int32_t StreamError(struct stream *self)
{
	return self->error_code;
}
