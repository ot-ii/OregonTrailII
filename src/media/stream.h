#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/object.h"

struct stream;

struct stream_ops {
	struct object_ops base;
	bool (*Free)(struct stream *self);
	bool (*Unfree)(struct stream *self);
	bool (*Purged)(struct stream *self);
	struct stream *(*Clone)(struct stream *self);
	int32_t (*Get)(struct stream *self, uint8_t *buf, int32_t *len);
	int32_t (*GetBuf)(struct stream *self, uint8_t **buf, int32_t *len);
	int32_t (*Seek)(struct stream *self, int32_t pos);
	int32_t (*SeekAhead)(struct stream *self, int32_t delta);
	int32_t (*Position)(struct stream *self);
	int32_t (*Size)(struct stream *self);
	int32_t (*SetStart)(struct stream *self, int32_t start);
	int32_t (*SetEnd)(struct stream *self, int32_t end);
	int32_t (*Status)(struct stream *self);
};

struct stream {
	struct object base;
	int32_t error_code;
	int32_t start_pos;
	int32_t end_pos;
};

#define STREAM_OPS(p) ((const struct stream_ops *) ((const struct object *) (p))->ops)

extern const struct stream_ops STREAM_OPS_TABLE;

void StreamInit(struct stream *self);
void stream_destroy(struct object *self);

int32_t stream_get(struct stream *self, uint8_t *buf, int32_t *len);
int32_t stream_get_buf(struct stream *self, uint8_t **buf, int32_t *len);
int32_t stream_seek(struct stream *self, int32_t pos);
int32_t stream_seek_ahead(struct stream *self, int32_t delta);
int32_t stream_position(struct stream *self);
int32_t stream_size(struct stream *self);
bool stream_free(struct stream *self);
bool stream_unfree(struct stream *self);
bool stream_purged(struct stream *self);
struct stream *stream_clone(struct stream *self);
int32_t stream_set_start(struct stream *self, int32_t start);
int32_t stream_set_end(struct stream *self, int32_t end);
int32_t stream_status(struct stream *self);

int32_t StreamGetStart(struct stream *self);
int32_t StreamGetEnd(struct stream *self);
int32_t StreamError(struct stream *self);
