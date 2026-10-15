#pragma once

#include <stdint.h>

#include "stream.h"

struct mem_stream;

struct mem_stream_ops {
	struct stream_ops base;
};

struct mem_stream {
	struct stream base;
	uint8_t *data;
	int32_t size;
	int32_t pos;
};

#define MEM_STREAM_OPS(p) ((const struct mem_stream_ops *) ((const struct object *) (p))->ops)

extern const struct mem_stream_ops MEM_STREAM_OPS_TABLE;

void MemStreamInit(struct mem_stream *self, void *data, int32_t size);
struct mem_stream *MemStreamCreate(void *data, int32_t size);
void mem_stream_destroy(struct object *self);

struct stream *mem_stream_clone(struct stream *self);
int32_t mem_stream_get_buf(struct stream *self, uint8_t **buf, int32_t *len);
int32_t mem_stream_seek(struct stream *self, int32_t new_pos);
int32_t mem_stream_seek_ahead(struct stream *self, int32_t delta);
int32_t mem_stream_position(struct stream *self);
