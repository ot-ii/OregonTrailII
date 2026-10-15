#include "sndunpack.h"

#include <stddef.h>
#include <string.h>

#include "../base/byteorder.h"
#include "../base/memory.h"

#define PACKED_HEADER_SIZE 12
#define KEY_FRAME_SIZE 8

static int32_t parse_header(struct snd_unpack *self);
static int32_t read_keyframe_table(struct snd_unpack *self);
static int32_t read_internal(struct snd_unpack *self, uint8_t *buf, int32_t *len);
static int32_t decode_blocks(struct snd_unpack *self, int32_t *count, uint8_t *out);
static int32_t find_keyframe_le(struct snd_unpack *self, int32_t pos, int32_t *out_pos, int32_t *out_bit);
static int32_t find_keyframe_ge(struct snd_unpack *self, int32_t pos, int32_t *out_pos, int32_t *out_bit);

const struct snd_unpack_ops SND_UNPACK_OPS_TABLE = {
	{
		{snd_unpack_destroy},
		snd_unpack_free,
		snd_unpack_unfree,
		snd_unpack_purged,
		snd_unpack_clone,
		snd_unpack_get,
		stream_get_buf,
		snd_unpack_seek,
		stream_seek_ahead,
		snd_unpack_position,
		snd_unpack_size,
		snd_unpack_set_start,
		snd_unpack_set_end,
		snd_unpack_status,
	},
};

void SndUnpackInit(struct snd_unpack *self, struct stream *source)
{
	StreamInit(&self->base);
	self->base.base.ops = &SND_UNPACK_OPS_TABLE.base.base;

	self->source = source;
	self->header_parsed = false;
	self->bit_position = 0;
	self->repeat_count = 0;
	self->tell_base = 0;
	self->keyframe_count = 0;
	self->keyframes = NULL;
	self->tell_offset = 0;
	self->pending_bytes = 0;

	int32_t result;

	do {
		result = parse_header(self);
	} while (result == 2);
}

struct snd_unpack *SndUnpackCreate(struct stream *source)
{
	struct snd_unpack *self = MemAlloc(1, sizeof(struct snd_unpack));
	SndUnpackInit(self, source);

	return self;
}

void snd_unpack_destroy(struct object *obj)
{
	struct snd_unpack *self = (struct snd_unpack *) obj;

	if (self->keyframes != NULL)
		MemFree(self->keyframes);

	ObjectDelete(self->source);

	obj->ops = &STREAM_OPS_TABLE.base;
	stream_destroy(obj);
}

int32_t snd_unpack_get(struct stream *obj, uint8_t *buf, int32_t *len)
{
	struct snd_unpack *self = (struct snd_unpack *) obj;
	int32_t length = *len;
	int32_t st = 0;

	*len = 0;

	while (self->tell_offset > 0 && !st) {
		int32_t chunk = 1024;

		if (self->tell_offset < chunk)
			chunk = self->tell_offset;

		st = read_internal(self, self->skip_buf, &chunk);

		if (!chunk)
			return 3;

		self->tell_offset -= chunk;
	}

	if (st != 0)
		return st;

	st = read_internal(self, buf, &length);
	*len = length;

	return st;
}

int32_t snd_unpack_seek(struct stream *obj, int32_t pos)
{
	struct snd_unpack *self = (struct snd_unpack *) obj;
	struct stream *src = self->source;

	find_keyframe_le(self, pos + obj->start_pos, &self->tell_base, &self->bit_position);
	int32_t err = STREAM_OPS(src)->Seek(src, self->data_offset + (self->bit_position >> 3) - src->start_pos);

	self->bit_position &= 7;

	if (err) {
		obj->error_code = 4;
		return obj->error_code;
	}

	self->repeat_count = 0;
	self->pending_bytes = 0;
	self->block_count = (self->total_len - self->tell_base - obj->start_pos + 63) / 64;
	self->tell_offset = pos - self->tell_base;

	return 0;
}

int32_t snd_unpack_position(struct stream *obj)
{
	struct snd_unpack *self = (struct snd_unpack *) obj;

	return self->tell_base + self->tell_offset;
}

int32_t snd_unpack_size(struct stream *obj)
{
	struct snd_unpack *self = (struct snd_unpack *) obj;

	return self->total_len;
}

int32_t snd_unpack_set_start(struct stream *obj, int32_t pos)
{
	struct snd_unpack *self = (struct snd_unpack *) obj;
	struct stream *src = self->source;
	int32_t keyframe_bit = 0;
	int32_t keyframe_pos;

	obj->start_pos = pos;

	if (self->total_len < obj->start_pos) {
		obj->start_pos = self->total_len;

	} else if (obj->start_pos < 0) {
		obj->start_pos = 0;
	}

	find_keyframe_le(self, obj->start_pos, &keyframe_pos, &keyframe_bit);
	STREAM_OPS(src)->SetStart(src, self->data_offset + ((keyframe_bit + 7) >> 3));

	return 0;
}

int32_t snd_unpack_set_end(struct stream *obj, int32_t pos)
{
	struct snd_unpack *self = (struct snd_unpack *) obj;
	struct stream *src = self->source;
	int32_t keyframe_pos;
	int32_t kf_bit;

	obj->end_pos = pos;

	if (obj->end_pos > self->total_len) {
		obj->end_pos = self->total_len;

	} else if (obj->end_pos < 0) {
		obj->end_pos = 0;
	}

	find_keyframe_ge(self, obj->end_pos, &keyframe_pos, &kf_bit);

	return STREAM_OPS(src)->SetEnd(src, self->data_offset + ((kf_bit + 7) >> 3));
}

bool snd_unpack_free(struct stream *obj)
{
	struct snd_unpack *self = (struct snd_unpack *) obj;

	return STREAM_OPS(self->source)->Free(self->source);
}

bool snd_unpack_unfree(struct stream *obj)
{
	struct snd_unpack *self = (struct snd_unpack *) obj;

	return STREAM_OPS(self->source)->Unfree(self->source);
}

bool snd_unpack_purged(struct stream *obj)
{
	struct snd_unpack *self = (struct snd_unpack *) obj;

	return STREAM_OPS(self->source)->Purged(self->source);
}

struct stream *snd_unpack_clone(struct stream *obj)
{
	struct snd_unpack *self = (struct snd_unpack *) obj;
	struct stream *src = STREAM_OPS(self->source)->Clone(self->source);

	if (src) {
		return (struct stream *) SndUnpackCreate(src);

	} else {
		return NULL;
	}
}

static int32_t parse_header(struct snd_unpack *self)
{
	struct stream *src = self->source;
	uint8_t *header;
	int32_t hdrlen;

	int32_t err = STREAM_OPS(src)->GetBuf(src, &header, &hdrlen);

	if (err)
		return err;

	self->data_offset = STREAM_OPS(src)->Position(src);
	self->total_len = ReadLe32(header + 8);
	self->end_bit_offset = ReadLe32(header + 4);
	self->flags = (int8_t) header[3];

	STREAM_OPS(src)->SeekAhead(src, PACKED_HEADER_SIZE);
	int32_t header_size = PACKED_HEADER_SIZE;

	if ((self->flags & 2) != 0) {
		read_keyframe_table(self);
		header_size += KEY_FRAME_SIZE * self->keyframe_count + 2;
	}

	self->data_offset += header_size;
	self->block_count = (self->total_len + 63) / 64;
	self->mtf_table.count = 256;

	for (int32_t entry = 0; entry < 0x80; entry++) {
		self->mtf_table.table[2 * entry] = (uint8_t) (entry + 0x80);
		self->mtf_table.table[2 * entry + 1] = (uint8_t) (127 - entry);
	}

	self->mtf_table.mode = 0;
	self->header_parsed = true;
	STREAM_OPS(src)->Seek(src, self->data_offset);

	return 0;
}

static int32_t read_internal(struct snd_unpack *self, uint8_t *buf, int32_t *len)
{
	int32_t copy_count;
	uint8_t *dst;
	const uint8_t *src;

	int32_t wanted = *len;

	if (self->total_len - self->tell_base < wanted)
		wanted = self->total_len - self->tell_base;

	*len = 0;

	if (self->pending_bytes) {
		if (self->pending_bytes > wanted) {
			copy_count = wanted;

		} else {
			copy_count = self->pending_bytes;
		}

		src = self->block_buf + 64 - self->pending_bytes;
		dst = buf;
		*len += copy_count;
		self->tell_base += copy_count;
		self->pending_bytes -= copy_count;

		while (--copy_count >= 0)
			*dst++ = *src++;
	}

	int32_t want_blocks = (wanted - *len) / 64;

	if (self->block_count < want_blocks)
		want_blocks = self->block_count;

	dst = buf + *len;
	int32_t blocks_done = want_blocks;
	int32_t result = decode_blocks(self, &blocks_done, dst);
	*len += blocks_done << 6;
	self->tell_base += blocks_done << 6;

	if (blocks_done == want_blocks && *len < wanted) {
		copy_count = wanted - *len;
		int32_t tail_blocks = 1;
		result = decode_blocks(self, &tail_blocks, self->block_buf);

		if (tail_blocks == 1) {
			self->pending_bytes = 64 - copy_count;
			src = self->block_buf;
			dst = buf + *len;
			*len += copy_count;
			self->tell_base += copy_count;

			while (--copy_count >= 0)
				*dst++ = *src++;
		}
	}

	if (self->total_len <= self->tell_base) {
		*len -= self->tell_base - self->total_len;
		self->tell_base = self->total_len;
		result = 1;
	}

	return result;
}

static int32_t decode_blocks(struct snd_unpack *self, int32_t *count, uint8_t *out)
{
	struct stream *source = self->source;
	uint8_t *src;
	int32_t src_len;

	int32_t start_count = *count;
	int32_t result = STREAM_OPS(source)->GetBuf(source, &src, &src_len);
	int32_t bit_limit = 8 * (src_len - 70);

	while (*count > 0 && self->block_count > 0 && !result && self->bit_position <= 8 * src_len) {
		if (self->repeat_count > 0) {
			while (self->repeat_count > 0 && *count > 0) {
				uint8_t *sample = out;

				for (int32_t i = 16; i > 0; --i) {
					memcpy(sample, self->repeat_sample, 4);
					sample += 4;
				}

				out += 64;
				--self->repeat_count;
				--*count;
				--self->block_count;
			}

		} else {
			if (self->bit_position >= bit_limit) {
				result = STREAM_OPS(source)->SeekAhead(source, self->bit_position >> 3);

				if (result)
					break;

				self->bit_position &= 7;
				result = STREAM_OPS(source)->GetBuf(source, &src, &src_len);

				if (result)
					break;

				bit_limit = 8 * (src_len - 70);
			}

			uint32_t repeats = 0;
			self->bit_position = DecodeImageBlock(&self->mtf_table, src, src_len, self->bit_position, out, &repeats);
			self->repeat_count = (int32_t) repeats;

			if (self->repeat_count)
				memcpy(self->repeat_sample, out, 4);

			out += 64;
			--*count;
			--self->block_count;
		}
	}

	*count = start_count - *count;

	return result;
}

static int32_t read_keyframe_table(struct snd_unpack *self)
{
	struct stream *src = self->source;
	uint8_t *buf;
	int32_t buflen;

	STREAM_OPS(src)->GetBuf(src, &buf, &buflen);
	self->keyframe_count = ReadLe16(buf);

	if (!self->keyframe_count)
		return 0;

	STREAM_OPS(src)->SeekAhead(src, 2);

	if (self->keyframe_count < 0)
		return 4;

	self->keyframes = MemAlloc((size_t) self->keyframe_count, sizeof(struct key_frame));

	int32_t frame_index = 0;

	while (self->keyframe_count > frame_index) {
		int32_t result;

		do {
			result = STREAM_OPS(src)->GetBuf(src, &buf, &buflen);
		} while (result == 2);

		int32_t chunk_index;

		for (chunk_index = 0; self->keyframe_count > frame_index && (uint32_t) buflen >> 3 > (uint32_t) chunk_index;
			++frame_index, ++chunk_index) {
			self->keyframes[frame_index].sample_pos = ReadLe32(buf + KEY_FRAME_SIZE * chunk_index);
			self->keyframes[frame_index].bit_offset = ReadLe32(buf + KEY_FRAME_SIZE * chunk_index + 4);
		}

		STREAM_OPS(src)->SeekAhead(src, KEY_FRAME_SIZE * chunk_index);
	}

	return 0;
}

static int32_t find_keyframe_le(struct snd_unpack *self, int32_t pos, int32_t *out_pos, int32_t *out_bit)
{
	int32_t best = -1;

	for (int32_t i = 0; self->keyframe_count > i; ++i) {
		if (self->keyframes[i].sample_pos <= pos &&
			(best == -1 || self->keyframes[best].sample_pos < self->keyframes[i].sample_pos))
			best = i;
	}

	if (best == -1) {
		*out_pos = 0 - self->base.start_pos;
		*out_bit = 0;

	} else {
		*out_pos = self->keyframes[best].sample_pos - self->base.start_pos;
		*out_bit = self->keyframes[best].bit_offset;
	}

	return 0;
}

static int32_t find_keyframe_ge(struct snd_unpack *self, int32_t pos, int32_t *out_pos, int32_t *out_bit)
{
	int32_t best = -1;

	for (int32_t i = 0; self->keyframe_count > i; ++i) {
		if (self->keyframes[i].sample_pos >= pos &&
			(best == -1 || self->keyframes[best].sample_pos >= self->keyframes[i].sample_pos))
			best = i;
	}

	if (best == -1) {
		*out_pos = self->total_len - self->base.start_pos;
		*out_bit = self->end_bit_offset;

	} else {
		*out_pos = self->keyframes[best].sample_pos - self->base.start_pos;
		*out_bit = self->keyframes[best].bit_offset;
	}

	return 0;
}

int32_t snd_unpack_status(struct stream *obj)
{
	struct snd_unpack *self = (struct snd_unpack *) obj;

	return STREAM_OPS(self->source)->Status(self->source);
}
