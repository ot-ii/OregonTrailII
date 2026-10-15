#include "mediabase.h"

#include <stddef.h>
#include <string.h>

#include "../base/memory.h"
#include "server.h"

const struct media_base_ops MEDIA_BASE_OPS_TABLE = {
	{media_base_destroy},
	media_base_free,
	media_base_release_lock,
	media_base_is_unlocked,
	media_base_clone,
	media_base_release_shared,
	media_base_release_source,
};

void MediaBaseInit(struct media_base *self, struct server *arg)
{
	ObjectInit(&self->base);
	self->base.ops = &MEDIA_BASE_OPS_TABLE.base;

	self->size = sizeof(struct media_base);
	self->server = arg;
	self->valid = true;
}

void media_base_destroy(struct object *self)
{
	object_destroy(self);
}

struct media_base *media_base_clone(struct media_base *self)
{
	struct media_base *copy = MemAlloc(1, (size_t) self->size);
	memcpy(copy, self, (size_t) self->size);

	return copy;
}

void media_base_release_shared(struct media_base *self)
{
	(void) self;
}

void ReleaseDrawable(struct media_base *obj, bool flag)
{
	if (!obj->server) {
		MEDIA_BASE_OPS(obj)->ReleaseSource(obj);
		ObjectDelete(obj);

	} else {
		ServerReleaseObject(obj->server, obj, flag);
	}
}

void media_base_release_source(struct media_base *self)
{
	(void) self;
}

bool media_base_free(struct media_base *self)
{
	(void) self;

	return false;
}

bool media_base_release_lock(struct media_base *self)
{
	(void) self;

	return false;
}

bool media_base_is_unlocked(struct media_base *self)
{
	(void) self;

	return true;
}
