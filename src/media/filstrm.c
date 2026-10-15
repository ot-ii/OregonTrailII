#include "filstrm.h"

#include <stddef.h>

#include "../base/file.h"
#include "../base/memory.h"

const struct fil_strm_ops FIL_STRM_OPS_TABLE = {
	{
		{
			{fil_strm_destroy},
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
	},
};

void FilStrmInit(struct fil_strm *self, struct file *file, int32_t file_offset, int32_t total_len, int32_t bufsize,
	int32_t lookahead)
{
	BufStrmInit(&self->base, file, file_offset, total_len, bufsize, lookahead);
	self->base.base.base.ops = &FIL_STRM_OPS_TABLE.base.base.base;

	self->file = file;
}

struct fil_strm *FilStrmCreate(struct file *file, int32_t file_offset, int32_t total_len, int32_t bufsize,
	int32_t lookahead)
{
	struct fil_strm *self = MemAlloc(1, sizeof(struct fil_strm));
	FilStrmInit(self, file, file_offset, total_len, bufsize, lookahead);

	return self;
}

void fil_strm_destroy(struct object *obj)
{
	struct fil_strm *self = (struct fil_strm *) obj;

	FileClose(&self->file);

	obj->ops = &BUF_STRM_OPS_TABLE.base.base;
	buf_strm_destroy(obj);
}
