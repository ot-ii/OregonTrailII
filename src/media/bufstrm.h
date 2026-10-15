#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "stream.h"

struct buf_strm;
struct file;

struct buf {
	int32_t count;
	int32_t limit;
	uint8_t *data;
	int32_t size;
	int32_t pos;
	bool overflow;
	bool empty;
};

struct buf_strm_ops {
	struct stream_ops base;
};

struct buf_strm {
	struct stream base;
	struct file *id;
	int32_t file_offset;
	int32_t total_len;
	int32_t lookahead;
	int32_t bufsize;
	int32_t cur_idx;
	int32_t base_pos;
	struct buf *buf[2];
	int32_t started;
	int32_t pos0;
};

#define BUF_STRM_OPS(p) ((const struct buf_strm_ops *) ((const struct object *) (p))->ops)

extern const struct buf_strm_ops BUF_STRM_OPS_TABLE;

void BufStrmInit(struct buf_strm *self, struct file *id, int32_t file_offset, int32_t total_len, int32_t bufsize,
	int32_t lookahead);
struct buf_strm *BufStrmCreate(struct file *id, int32_t file_offset, int32_t total_len, int32_t bufsize,
	int32_t lookahead);
void buf_strm_destroy(struct object *self);

struct stream *buf_strm_clone(struct stream *self);
int32_t buf_strm_get_buf(struct stream *self, uint8_t **ptr, int32_t *len);
int32_t buf_strm_seek(struct stream *self, int32_t pos);
int32_t buf_strm_seek_ahead(struct stream *self, int32_t off);
int32_t buf_strm_position(struct stream *self);
int32_t buf_strm_status(struct stream *self);

int32_t BufStrmBufferStatus(struct buf_strm *self, int32_t idx);
int32_t BufStrmFillBuffers(struct buf_strm *self);
int32_t BufStrmFillBuffer(struct buf_strm *self, int32_t idx, int32_t offset);
