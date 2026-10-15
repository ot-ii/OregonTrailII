#include "bufstrm.h"

#include <stddef.h>

#include "../base/file.h"
#include "../base/memory.h"

const struct buf_strm_ops BUF_STRM_OPS_TABLE = {
	{
		{buf_strm_destroy},
		stream_free,
		stream_unfree,
		stream_purged,
		buf_strm_clone,
		stream_get,
		buf_strm_get_buf,
		buf_strm_seek,
		buf_strm_seek_ahead,
		buf_strm_position,
		stream_size,
		stream_set_start,
		stream_set_end,
		buf_strm_status,
	},
};

void BufStrmInit(struct buf_strm *self, struct file *id, int32_t file_offset, int32_t total_len, int32_t bufsize,
	int32_t lookahead)
{
	StreamInit(&self->base);
	self->base.base.ops = &BUF_STRM_OPS_TABLE.base.base;

	self->id = id;
	self->file_offset = file_offset;
	self->total_len = total_len;
	self->lookahead = lookahead;
	self->bufsize = bufsize;

	if (self->bufsize > 65534)
		self->bufsize = 65534;

	self->base.end_pos = self->total_len;
	self->started = 0;
	self->pos0 = 0;
	self->cur_idx = 0;
	self->base_pos = 0;
	self->buf[0] = MemAlloc(1, sizeof(struct buf));
	self->buf[1] = MemAlloc(1, sizeof(struct buf));
	self->buf[0]->overflow = self->buf[1]->overflow = false;
	self->buf[0]->empty = self->buf[1]->empty = true;
	self->buf[0]->count = self->buf[1]->count = 0;
	self->buf[0]->data = MemAlloc((size_t) (self->bufsize / 2), 1);
	self->buf[0]->size = self->bufsize / 2;
	self->buf[1]->data = MemAlloc((size_t) (self->bufsize - self->bufsize / 2), 1);
	self->buf[1]->size = self->bufsize - self->buf[0]->size;
	self->buf[0]->limit = self->buf[1]->limit = 0;
	BufStrmFillBuffers(self);
}

struct buf_strm *BufStrmCreate(struct file *id, int32_t file_offset, int32_t total_len, int32_t bufsize,
	int32_t lookahead)
{
	struct buf_strm *self = MemAlloc(1, sizeof(struct buf_strm));
	BufStrmInit(self, id, file_offset, total_len, bufsize, lookahead);

	return self;
}

void buf_strm_destroy(struct object *obj)
{
	struct buf_strm *self = (struct buf_strm *) obj;

	if (self->buf[0]) {
		MemFree(self->buf[0]->data);
		MemFree(self->buf[0]);
	}

	if (self->buf[1]) {
		MemFree(self->buf[1]->data);
		MemFree(self->buf[1]);
	}

	obj->ops = &STREAM_OPS_TABLE.base;
	stream_destroy(obj);
}

int32_t buf_strm_get_buf(struct stream *obj, uint8_t **ptr, int32_t *len)
{
	struct buf_strm *self = (struct buf_strm *) obj;

	BufStrmFillBuffers(self);
	*len = 0;

	int32_t status = BufStrmBufferStatus(self, self->cur_idx);

	if (status != 0)
		return status;

	*ptr = self->buf[self->cur_idx]->data + self->base_pos;
	*len = self->buf[self->cur_idx]->limit - self->base_pos;

	return 0;
}

int32_t buf_strm_seek_ahead(struct stream *obj, int32_t off)
{
	struct buf_strm *self = (struct buf_strm *) obj;

	if (self->started == 0)
		BufStrmFillBuffers(self);

	if (self->started == 0)
		return 2;

	return STREAM_OPS(self)->Seek(obj, self->buf[self->cur_idx]->pos + self->base_pos - obj->start_pos + off);
}

int32_t buf_strm_seek(struct stream *obj, int32_t pos)
{
	struct buf_strm *self = (struct buf_strm *) obj;
	int32_t delta;

	pos += obj->start_pos;

	if (self->started >= 1) {
		delta = pos - self->buf[self->cur_idx]->pos;

		if (delta >= 0 && self->buf[self->cur_idx]->size - self->lookahead > delta) {
			self->base_pos = delta;
			return 0;
		}
	}

	if (self->started == 2) {
		delta = pos - self->buf[!self->cur_idx]->pos;

		if (delta >= 0 && self->buf[!self->cur_idx]->size - self->lookahead > delta) {
			self->buf[self->cur_idx]->count = 0;

			if (self->cur_idx == 0) {
				self->cur_idx = 1;

			} else {
				self->cur_idx = 0;
			}

			--self->started;
			self->base_pos = delta;
			BufStrmFillBuffers(self);

			return 0;
		}
	}

	self->base_pos = 0;
	self->started = 0;
	self->buf[1]->count = 0;
	self->buf[0]->count = self->buf[1]->count;
	self->pos0 = pos;

	return BufStrmFillBuffers(self);
}

int32_t buf_strm_position(struct stream *obj)
{
	struct buf_strm *self = (struct buf_strm *) obj;

	if (self->started == 0) {
		return self->pos0 - obj->start_pos;

	} else {
		return self->buf[self->cur_idx]->pos + self->base_pos - obj->start_pos;
	}
}

struct stream *buf_strm_clone(struct stream *obj)
{
	struct buf_strm *self = (struct buf_strm *) obj;

	return (struct stream *) BufStrmCreate(self->id, self->file_offset, self->total_len, self->bufsize,
		self->lookahead);
}

int32_t BufStrmFillBuffers(struct buf_strm *self)
{
	int32_t index;

	if (self->started == 0) {
		for (index = 0; index < 2; ++index) {
			if (self->buf[index]->count == 0)
				break;
		}

		if (index == 2)
			return 2;

		self->cur_idx = index;
		BufStrmFillBuffer(self, index, self->pos0);
	}

	if (self->started == 1)
		BufStrmFillBuffer(self, !self->cur_idx,
			self->buf[self->cur_idx]->size + self->buf[self->cur_idx]->pos - self->lookahead);

	return 0;
}

int32_t BufStrmFillBuffer(struct buf_strm *self, int32_t idx, int32_t offset)
{
	struct buf *buf = self->buf[idx];
	buf->pos = offset;

	if (self->base.end_pos < buf->pos)
		buf->pos = self->base.start_pos;

	buf->overflow = false;

	if (buf->size + buf->pos > self->total_len)
		buf->overflow = true;

	FileSeek(self->id, (int64_t) buf->pos + self->file_offset);
	buf->limit = FileRead(self->id, buf->data, buf->size);
	buf->count = 1;
	buf->empty = false;
	++self->started;

	return 0;
}

int32_t BufStrmBufferStatus(struct buf_strm *self, int32_t idx)
{
	struct buf *buf = self->buf[idx];

	if (buf->empty)
		return 0;

	if (buf->count == 0)
		return 2;

	if (buf->overflow && buf->limit <= self->base_pos)
		return 1;

	return 0;
}

int32_t buf_strm_status(struct stream *obj)
{
	struct buf_strm *self = (struct buf_strm *) obj;

	STREAM_OPS(self)->SeekAhead(obj, 0);

	return BufStrmBufferStatus(self, self->cur_idx);
}
