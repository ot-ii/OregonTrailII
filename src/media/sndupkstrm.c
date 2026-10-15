#include "sndupkstrm.h"

#include <stddef.h>
#include <string.h>

#include "../base/byteorder.h"
#include "../base/memory.h"
#include "pcmdecode.h"
#include "sndunpack.h"

#define SND_MARK_CHUNK_SIZE 10
#define SND_MARK_SIZE 8

const struct snd_upk_strm_ops SND_UPK_STRM_OPS_TABLE = {
	{
		{
			{snd_upk_strm_destroy},
			snd_upk_strm_free,
			snd_upk_strm_unfree,
			snd_upk_strm_purged,
			snd_upk_strm_clone,
			snd_upk_strm_get,
			stream_get_buf,
			snd_upk_strm_seek,
			stream_seek_ahead,
			snd_upk_strm_position,
			snd_upk_strm_size,
			snd_upk_strm_set_start,
			snd_upk_strm_set_end,
			snd_upk_strm_status,
		},
		snd_upk_strm_channels,
		snd_upk_strm_sample_size,
		snd_upk_strm_sample_rate,
		snd_strm_fade_track,
		snd_strm_set_selection,
		snd_strm_sync_to,
	},
};

static void delta_state_reset(uint8_t state[4])
{
	memset(state, 0x80, 4);
}

void SndUpkStrmInit(struct snd_upk_strm *self, struct stream *source)
{
	struct stream *base = &self->base.base;
	uint8_t *raw_buf;
	int32_t buf_len;
	int32_t status;
	int32_t chunk_type;

	SndStrmInit(&self->base);
	base->base.ops = &SND_UPK_STRM_OPS_TABLE.base.base.base;

	base->error_code = 4;
	self->marks = NULL;
	self->mark_count = 0;
	delta_state_reset(self->delta_state);
	self->skip_remaining = 0;
	base->start_pos = 0;
	base->end_pos = 0;
	self->cur_pos = 0;
	self->source = source;
	self->eof_flag = false;

	do {
		status = STREAM_OPS(source)->GetBuf(source, &raw_buf, &buf_len);
	} while (status == 2);

	if (status)
		return;

	do {
		chunk_type = raw_buf[0];
		int32_t chunk_len = ReadLe32(raw_buf + 2);
		int32_t chunk_pos = STREAM_OPS(source)->Position(source);

		switch (chunk_type) {
			case 1:
				self->delta_order = raw_buf[10];
				self->channels = raw_buf[6];
				self->sample_size = raw_buf[7];
				self->rate_code = raw_buf[8];
				break;
			case 2:
				SndUpkStrmReadMarks(self);
				break;
		}

		status = STREAM_OPS(source)->Seek(source, chunk_pos + chunk_len);

		if (!status)
			status = STREAM_OPS(source)->GetBuf(source, &raw_buf, &buf_len);

		if (status)
			return;

	} while (chunk_type);

	self->inner = SndUnpackCreate(source);
	base->end_pos = STREAM_OPS(self->inner)->Size(&self->inner->base);
	base->error_code = 0;
}

struct snd_upk_strm *SndUpkStrmCreate(struct stream *source)
{
	struct snd_upk_strm *self = MemAlloc(1, sizeof(struct snd_upk_strm));
	SndUpkStrmInit(self, source);

	return self;
}

void snd_upk_strm_destroy(struct object *obj)
{
	struct snd_upk_strm *self = (struct snd_upk_strm *) obj;

	if (self->marks != NULL)
		MemFree(self->marks);

	ObjectDelete(self->inner);

	obj->ops = &SND_STRM_OPS_TABLE.base.base;
	snd_strm_destroy(obj);
}

int32_t snd_upk_strm_get(struct stream *obj, uint8_t *buf, int32_t *len)
{
	struct snd_upk_strm *self = (struct snd_upk_strm *) obj;
	struct stream *inner = (struct stream *) self->inner;
	int32_t status;

	if (self->eof_flag) {
		*len = 0;
		return 1;
	}

	if (!inner) {
		*len = 0;
		return 1;
	}

	while (self->skip_remaining > 0) {
		int32_t proc_len = 1024;

		if (self->skip_remaining < proc_len)
			proc_len = self->skip_remaining;

		status = STREAM_OPS(inner)->Get(inner, self->sample_buf, &proc_len);
		DeltaDecode(self->delta_state, self->delta_order, proc_len, self->sample_buf);
		self->skip_remaining -= proc_len;

		if (status) {
			*len = 0;
			return status;
		}
	}

	if (obj->end_pos - obj->start_pos < *len + self->cur_pos)
		*len = obj->end_pos - self->cur_pos - obj->start_pos;

	status = STREAM_OPS(inner)->Get(inner, buf, len);
	DeltaDecode(self->delta_state, self->delta_order, *len, buf);
	self->cur_pos += *len;

	if (self->base.loop_start > 0 && obj->start_pos != self->base.loop_start &&
		self->cur_pos + obj->start_pos > self->base.loop_start)
		STREAM_OPS(obj)->SetStart(obj, self->base.loop_start);

	if (self->cur_pos + obj->start_pos >= obj->end_pos) {
		STREAM_OPS(obj)->Seek(obj, 0);

		if (self->base.loop_start < 0)
			self->eof_flag = true;
	}

	return status;
}

int32_t snd_upk_strm_seek(struct stream *obj, int32_t pos)
{
	struct snd_upk_strm *self = (struct snd_upk_strm *) obj;
	struct stream *inner = (struct stream *) self->inner;
	uint8_t delta_state[4];

	self->eof_flag = false;

	if (self->cur_pos == pos)
		return 0;

	int32_t best_mark = -1;

	for (int32_t index = 0; index < self->mark_count; ++index) {
		if (self->marks[index].sample_pos <= pos + obj->start_pos &&
			(best_mark == -1 || self->marks[best_mark].sample_pos < self->marks[index].sample_pos))
			best_mark = index;
	}

	if (best_mark == -1) {
		self->cur_pos = 0 - obj->start_pos;
		delta_state_reset(delta_state);

	} else {
		self->cur_pos = self->marks[best_mark].sample_pos - obj->start_pos;
		memcpy(delta_state, self->marks[best_mark].delta_state, 4);
	}

	int32_t status = inner ? STREAM_OPS(inner)->Seek(inner, self->cur_pos) : 4;
	memcpy(self->delta_state, delta_state, 4);
	self->skip_remaining = pos - self->cur_pos;
	self->cur_pos = pos;

	return status;
}

int32_t snd_upk_strm_position(struct stream *obj)
{
	struct snd_upk_strm *self = (struct snd_upk_strm *) obj;

	if (self->eof_flag) {
		return obj->end_pos - obj->start_pos;

	} else {
		return self->cur_pos;
	}
}

int32_t snd_upk_strm_size(struct stream *obj)
{
	struct snd_upk_strm *self = (struct snd_upk_strm *) obj;
	struct stream *inner = (struct stream *) self->inner;

	if (!inner)
		return 0;

	return STREAM_OPS(inner)->Size(inner);
}

int32_t snd_upk_strm_channels(struct snd_strm *obj)
{
	struct snd_upk_strm *self = (struct snd_upk_strm *) obj;

	return self->channels;
}

int32_t snd_upk_strm_sample_size(struct snd_strm *obj)
{
	struct snd_upk_strm *self = (struct snd_upk_strm *) obj;

	return self->sample_size;
}

int32_t snd_upk_strm_sample_rate(struct snd_strm *obj)
{
	struct snd_upk_strm *self = (struct snd_upk_strm *) obj;

	return self->rate_code;
}

bool snd_upk_strm_free(struct stream *obj)
{
	struct snd_upk_strm *self = (struct snd_upk_strm *) obj;
	struct stream *inner = (struct stream *) self->inner;

	if (!inner)
		return false;

	return STREAM_OPS(inner)->Free(inner);
}

bool snd_upk_strm_unfree(struct stream *obj)
{
	struct snd_upk_strm *self = (struct snd_upk_strm *) obj;
	struct stream *inner = (struct stream *) self->inner;

	if (!inner)
		return false;

	return STREAM_OPS(inner)->Unfree(inner);
}

bool snd_upk_strm_purged(struct stream *obj)
{
	struct snd_upk_strm *self = (struct snd_upk_strm *) obj;
	struct stream *inner = (struct stream *) self->inner;

	if (!inner)
		return true;

	return STREAM_OPS(inner)->Purged(inner);
}

int32_t snd_upk_strm_set_start(struct stream *obj, int32_t start)
{
	struct snd_upk_strm *self = (struct snd_upk_strm *) obj;
	struct stream *inner = (struct stream *) self->inner;

	if (!inner)
		return 4;

	int32_t old_start = obj->start_pos;
	int32_t size = STREAM_OPS(inner)->Size(inner);
	obj->start_pos = start;

	if (obj->start_pos > size) {
		obj->start_pos = size;

	} else if (obj->start_pos < 0) {
		obj->start_pos = 0;
	}

	self->cur_pos -= obj->start_pos - old_start;

	return STREAM_OPS(inner)->SetStart(inner, obj->start_pos);
}

int32_t snd_upk_strm_set_end(struct stream *obj, int32_t end)
{
	struct snd_upk_strm *self = (struct snd_upk_strm *) obj;
	struct stream *inner = (struct stream *) self->inner;

	if (!inner)
		return 4;

	int32_t size = STREAM_OPS(inner)->Size(inner);
	obj->end_pos = end;

	if (obj->end_pos > size) {
		obj->end_pos = size;

	} else if (obj->end_pos < 0) {
		obj->end_pos = 0;
	}

	return STREAM_OPS(inner)->SetEnd(inner, obj->end_pos);
}

struct stream *snd_upk_strm_clone(struct stream *obj)
{
	struct snd_upk_strm *self = (struct snd_upk_strm *) obj;
	struct stream *cloned = STREAM_OPS(self->source)->Clone(self->source);

	if (cloned) {
		return (struct stream *) SndUpkStrmCreate(cloned);

	} else {
		return NULL;
	}
}

int32_t SndUpkStrmReadMarks(struct snd_upk_strm *self)
{
	struct stream *src = self->source;
	uint8_t *raw_buf;
	int32_t buf_len;

	STREAM_OPS(src)->GetBuf(src, &raw_buf, &buf_len);
	self->mark_count = ReadLe16(raw_buf + 8);

	if (!self->mark_count)
		return 0;

	STREAM_OPS(src)->SeekAhead(src, SND_MARK_CHUNK_SIZE);

	if (self->mark_count < 0)
		return 4;

	self->marks = MemAlloc((size_t) self->mark_count, sizeof(struct snd_mark));

	int32_t mark_index = 0;

	while (self->mark_count > mark_index) {
		int32_t status;

		do {
			status = STREAM_OPS(src)->GetBuf(src, &raw_buf, &buf_len);
		} while (status == 2);

		int32_t buf_mark_index;

		for (buf_mark_index = 0; self->mark_count > mark_index && (int32_t) ((uint32_t) buf_len >> 3) > buf_mark_index;
			++mark_index, ++buf_mark_index) {
			self->marks[mark_index].sample_pos = ReadLe32(raw_buf + SND_MARK_SIZE * buf_mark_index);
			memcpy(self->marks[mark_index].delta_state, raw_buf + SND_MARK_SIZE * buf_mark_index + 4, 4);
		}

		STREAM_OPS(src)->SeekAhead(src, SND_MARK_SIZE * buf_mark_index);
	}

	return 0;
}

int32_t snd_upk_strm_status(struct stream *obj)
{
	struct snd_upk_strm *self = (struct snd_upk_strm *) obj;
	struct stream *inner = (struct stream *) self->inner;

	if (!inner)
		return 1;

	return STREAM_OPS(inner)->Status(inner);
}
