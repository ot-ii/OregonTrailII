#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "multidraw.h"

struct cbmp;
struct multi_image;
struct rsrc;
struct server;

struct multi_image_ops {
	struct multi_draw_ops base;
};

struct multi_image {
	struct multi_draw base;
	struct cbmp *image;
};

#define MULTI_IMAGE_OPS(p) ((const struct multi_image_ops *) ((const struct object *) (p))->ops)

extern const struct multi_image_ops MULTI_IMAGE_OPS_TABLE;

void MultiImageInit(struct multi_image *self, struct rsrc *body, struct server *arg);
struct multi_image *MultiImageCreate(struct rsrc *body, struct server *arg);
void MultiImageDestroy(struct multi_image **ctx_mod);
void multi_image_destroy(struct object *self);

void MultiImageLoadFrames(struct multi_image *self, struct rsrc *body);
