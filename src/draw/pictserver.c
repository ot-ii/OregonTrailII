#include "pictserver.h"

#include <stddef.h>

#include "../data/assets.h"
#include "../ui/cursormgr.h"
#include "../data/gfxdev.h"
#include "../base/memory.h"
#include "multiimage.h"
#include "../data/rsrc.h"

static const char *g_cbmp_text = "CBMP";

const struct pict_server_ops PICT_SERVER_OPS_TABLE = {
	{
		{
			{pict_server_destroy},
			server_acquire_object,
			pict_server_create_object,
		},
	},
};

void PictServerInit(struct pict_server *self)
{
	ServerSubInit(&self->base);
	self->base.base.base.ops = &PICT_SERVER_OPS_TABLE.base.base.base;
}

struct pict_server *PictServerCreate(void)
{
	struct pict_server *self = MemAlloc(1, sizeof(struct pict_server));
	PictServerInit(self);

	return self;
}

void PictServerDestroy(struct pict_server **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void pict_server_destroy(struct object *self)
{
	self->ops = &SERVER_SUB_OPS_TABLE.base.base;
	server_sub_destroy(self);
}

struct media_base *pict_server_create_object(struct server *self, int32_t key, bool flag)
{
	struct pict_server *ps = (struct pict_server *) self;
	struct multi_image *image = NULL;

	int32_t pict_id = (int16_t) (key & 0xFFFF);
	int32_t base_pict_id = (int16_t) (key >> 16);

	struct assets *lib = g_data_lib;
	CursorSetBusy(true);

	struct rsrc *rsrc = AssetsGetElement(lib, g_cbmp_text, pict_id, flag);

	if (rsrc) {
		if (!base_pict_id) {
			image = MultiImageCreate(rsrc, self);

		} else {
			struct rsrc *base_rsrc = AssetsGetElement(lib, g_cbmp_text, base_pict_id, true);
			image = MultiImageCreate(base_rsrc, self);
			MultiImageLoadFrames(image, rsrc);
			RsrcDestroy(&base_rsrc);
		}

		const uint8_t *tail = RsrcData(rsrc) + RsrcSize(rsrc);
		int32_t hot_v = (int16_t) ((int8_t) tail[-4] * 256 + (int8_t) tail[-3]);
		int32_t hot_h = (int16_t) ((int8_t) tail[-2] * 256 + (int8_t) tail[-1]);
		ps->pt.h = hot_h;
		ps->pt.v = hot_v;

		RsrcDestroy(&rsrc);
	}

	CursorSetBusy(false);

	return (struct media_base *) image;
}
