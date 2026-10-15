#pragma once

#include <stdint.h>

#include "bufstrm.h"

struct fil_strm;
struct file;

struct fil_strm_ops {
	struct buf_strm_ops base;
};

struct fil_strm {
	struct buf_strm base;
	struct file *file;
};

#define FIL_STRM_OPS(p) ((const struct fil_strm_ops *) ((const struct object *) (p))->ops)

extern const struct fil_strm_ops FIL_STRM_OPS_TABLE;

void FilStrmInit(struct fil_strm *self, struct file *file, int32_t file_offset, int32_t total_len, int32_t bufsize,
	int32_t lookahead);
struct fil_strm *FilStrmCreate(struct file *file, int32_t file_offset, int32_t total_len, int32_t bufsize,
	int32_t lookahead);
void fil_strm_destroy(struct object *self);
