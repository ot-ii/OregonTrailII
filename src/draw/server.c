#include "server.h"

#include <stddef.h>

#include "../base/log.h"
#include "mediabase.h"
#include "../base/memory.h"

const struct server_ops SERVER_OPS_TABLE = {
	{server_destroy},
	server_acquire_object,
	server_create_object,
};

static void cache_resize(struct server *self, int32_t count)
{
	if (count <= 0) {
		MemFree(self->cache);
		self->cache = NULL;
		self->cache_count = 0;
		return;
	}

	self->cache = MemRealloc(self->cache, (size_t) count, sizeof(struct cache_rec));
	self->cache_count = count;
}

void ServerInit(struct server *self, struct server *delegate)
{
	ObjectInit(&self->base);
	self->base.ops = &SERVER_OPS_TABLE.base;

	self->delegate = delegate;
	self->cache = NULL;
	self->cache_count = 0;
	self->purge_counter = 0;
}

void server_destroy(struct object *obj)
{
	struct server *self = (struct server *) obj;

	MemFree(self->cache);
	self->cache = NULL;
	self->cache_count = 0;

	object_destroy(obj);
}

static struct media_base *create_valid(struct server *self, int32_t key, bool flag)
{
	struct media_base *obj = SERVER_OPS(self)->CreateObject(self, key, flag);

	if (obj && !obj->valid)
		ObjectDestroy(&obj);

	return obj;
}

struct media_base *server_acquire_object(struct server *self, int32_t key, bool flag)
{
	struct media_base *obj = NULL;

	if (self->delegate) {
		obj = SERVER_OPS(self->delegate)->AcquireObject(self->delegate, key, flag);

		if (obj)
			ServerAddEntry(self, key, obj);

		return obj;
	}

	if (++self->purge_counter > 10) {
		self->purge_counter = 0;
		ServerPurge(self);
	}

	int32_t count = self->cache_count;
	int32_t index = ServerFindByKey(self, key);

	if (index < 0) {
		obj = create_valid(self, key, flag);

		if (obj)
			ServerAddEntry(self, key, obj);

	} else if (self->cache[index].valid) {
		struct media_base *cached = self->cache[index].obj;
		obj = MEDIA_BASE_OPS(cached)->Clone(cached);

		if (obj)
			ServerAddEntry(self, key, obj);

	} else {
		obj = self->cache[index].obj;

		if (!MEDIA_BASE_OPS(obj)->ReleaseLock(obj)) {
			MEDIA_BASE_OPS(obj)->ReleaseSource(obj);
			ObjectDelete(obj);
			obj = create_valid(self, key, flag);
			self->cache[index].obj = obj;
		}

		if (obj == NULL) {
			--count;
			self->cache[index] = self->cache[count];
			cache_resize(self, count);

		} else {
			self->cache[index].valid = true;
		}
	}

	if (obj)
		obj->key = key;

	return obj;
}

void ServerReleaseObject(struct server *self, struct media_base *obj, bool flag)
{
	bool removed = false;
	int32_t index = ServerFindByObj(self, obj);

	if (index < 0) {
		LogError(LOG_ERROR, "Attempt to release object not in list");
		return;
	}

	int32_t count = self->cache_count;
	int32_t key_count = ServerCountByKey(self, self->cache[index].key);

	if (flag) {
		if (key_count > 1) {
			LogError(LOG_ERROR, "Killing an object with multiple users.");
			flag = false;

		} else {
			key_count = 0;
		}
	}

	if (self->delegate) {
		removed = true;
		ServerReleaseObject(self->delegate, obj, flag);

	} else if (key_count == 0) {
		MEDIA_BASE_OPS(obj)->ReleaseSource(obj);
		ObjectDelete(obj);
		removed = true;

	} else if (key_count == 1) {
		if (MEDIA_BASE_OPS(obj)->Free(obj)) {
			self->cache[index].valid = false;
			removed = false;

		} else {
			MEDIA_BASE_OPS(obj)->ReleaseSource(obj);
			ObjectDelete(obj);
			removed = true;
		}

	} else {
		MEDIA_BASE_OPS(obj)->ReleaseShared(obj);
		MemFree(obj);
		removed = true;
	}

	if (removed) {
		count--;
		self->cache[index] = self->cache[count];
		cache_resize(self, count);
	}
}

void ServerReleaseAll(struct server *self)
{
	while (self->cache_count > 0)
		ServerReleaseObject(self, self->cache[0].obj, false);
}

struct media_base *server_create_object(struct server *self, int32_t key, bool flag)
{
	(void) self;
	(void) key;
	(void) flag;

	return NULL;
}

int32_t ServerFindByKey(struct server *self, int32_t key)
{
	if (!self->cache_count)
		return -1;

	for (int32_t index = 0; self->cache_count > index; ++index) {
		if (self->cache[index].key == key)
			return index;
	}

	return -1;
}

int32_t ServerFindByObj(struct server *self, struct media_base *obj)
{
	if (!self->cache_count)
		return -1;

	for (int32_t index = 0; index < self->cache_count; ++index) {
		if (self->cache[index].obj == obj)
			return index;
	}

	return -1;
}

int32_t ServerAddEntry(struct server *self, int32_t key, struct media_base *obj)
{
	int32_t count = self->cache_count;
	cache_resize(self, count + 1);

	self->cache[count].key = key;
	self->cache[count].obj = obj;
	self->cache[count].valid = true;

	return count;
}

int32_t ServerCountByKey(struct server *self, int32_t key)
{
	int32_t users = 0;

	for (int32_t i = 0; i < self->cache_count; i++) {
		if (self->cache[i].key == key && self->cache[i].valid)
			++users;
	}

	return users;
}

void ServerPurge(struct server *self)
{
	int32_t count = self->cache_count;

	if (!count)
		return;

	for (int32_t index = 0; index < count; ++index) {
		if (!self->cache[index].valid) {
			struct media_base *obj = self->cache[index].obj;

			if (MEDIA_BASE_OPS(obj)->IsUnlocked(obj)) {
				MEDIA_BASE_OPS(obj)->ReleaseSource(obj);
				ObjectDelete(obj);
				--count;
				self->cache[index] = self->cache[count];
			}
		}
	}

	cache_resize(self, count);
}

void *ServerCreateVirtual(struct server *self, int32_t key, bool flag)
{
	return SERVER_OPS(self)->AcquireObject(self, key, flag);
}

void *ServerCreateVirtualPacked(struct server *self, int32_t type_id, int32_t index, bool flag)
{
	return ServerCreateVirtual(self, (int32_t) (((uint32_t) type_id << 16) | (uint32_t) index), flag);
}
