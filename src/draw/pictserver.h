#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "serversub.h"

struct media_base;
struct pict_server;

struct pict_server_ops {
	struct server_sub_ops base;
};

struct pict_server {
	struct server_sub base;
	struct point pt;
};

#define PICT_SERVER_OPS(p) ((const struct pict_server_ops *) ((const struct object *) (p))->ops)

extern const struct pict_server_ops PICT_SERVER_OPS_TABLE;

void PictServerInit(struct pict_server *self);
struct pict_server *PictServerCreate(void);
void PictServerDestroy(struct pict_server **ctx_mod);
void pict_server_destroy(struct object *self);

struct media_base *pict_server_create_object(struct server *self, int32_t key, bool flag);

extern struct pict_server *g_pict_server;
