#pragma once

#include "server.h"

struct server_sub;

struct server_sub_ops {
	struct server_ops base;
};

struct server_sub {
	struct server base;
};

#define SERVER_SUB_OPS(p) ((const struct server_sub_ops *) ((const struct object *) (p))->ops)

extern const struct server_sub_ops SERVER_SUB_OPS_TABLE;

void ServerSubInit(struct server_sub *self);
void server_sub_destroy(struct object *self);
