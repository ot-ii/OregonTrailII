#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/object.h"

struct media_base;
struct server;

struct cache_rec {
	struct media_base *obj;
	int32_t key;
	bool valid;
};

struct server_ops {
	struct object_ops base;
	struct media_base *(*AcquireObject)(struct server *self, int32_t key, bool flag);
	struct media_base *(*CreateObject)(struct server *self, int32_t key, bool flag);
};

struct server {
	struct object base;
	struct cache_rec *cache;
	int32_t cache_count;
	struct server *delegate;
	int32_t purge_counter;
};

#define SERVER_OPS(p) ((const struct server_ops *) ((const struct object *) (p))->ops)

extern const struct server_ops SERVER_OPS_TABLE;

void ServerInit(struct server *self, struct server *delegate);
void server_destroy(struct object *self);

struct media_base *server_acquire_object(struct server *self, int32_t key, bool flag);
struct media_base *server_create_object(struct server *self, int32_t key, bool flag);

void ServerReleaseObject(struct server *self, struct media_base *obj, bool flag);
void ServerReleaseAll(struct server *self);
int32_t ServerFindByKey(struct server *self, int32_t key);
int32_t ServerFindByObj(struct server *self, struct media_base *obj);
int32_t ServerAddEntry(struct server *self, int32_t key, struct media_base *obj);
int32_t ServerCountByKey(struct server *self, int32_t key);
void ServerPurge(struct server *self);
void *ServerCreateVirtual(struct server *self, int32_t key, bool flag);
void *ServerCreateVirtualPacked(struct server *self, int32_t type_id, int32_t index, bool flag);
