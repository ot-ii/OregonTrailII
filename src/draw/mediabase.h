#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/object.h"

struct media_base;
struct server;

struct media_base_ops {
	struct object_ops base;
	bool (*Free)(struct media_base *self);
	bool (*ReleaseLock)(struct media_base *self);
	bool (*IsUnlocked)(struct media_base *self);
	struct media_base *(*Clone)(struct media_base *self);
	void (*ReleaseShared)(struct media_base *self);
	void (*ReleaseSource)(struct media_base *self);
};

struct media_base {
	struct object base;
	int32_t size;
	int32_t key;
	struct server *server;
	bool valid;
};

#define MEDIA_BASE_OPS(p) ((const struct media_base_ops *) ((const struct object *) (p))->ops)

extern const struct media_base_ops MEDIA_BASE_OPS_TABLE;

void MediaBaseInit(struct media_base *self, struct server *arg);
void media_base_destroy(struct object *self);

struct media_base *media_base_clone(struct media_base *self);
void media_base_release_shared(struct media_base *self);
void media_base_release_source(struct media_base *self);
bool media_base_free(struct media_base *self);
bool media_base_release_lock(struct media_base *self);
bool media_base_is_unlocked(struct media_base *self);

void ReleaseDrawable(struct media_base *obj, bool flag);
