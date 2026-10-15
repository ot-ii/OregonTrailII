#include "serversub.h"

#include <stddef.h>

const struct server_sub_ops SERVER_SUB_OPS_TABLE = {
	{
		{server_sub_destroy},
		server_acquire_object,
		server_create_object,
	},
};

void ServerSubInit(struct server_sub *self)
{
	ServerInit(&self->base, NULL);
	self->base.base.ops = &SERVER_SUB_OPS_TABLE.base.base;
}

void server_sub_destroy(struct object *self)
{
	self->ops = &SERVER_OPS_TABLE.base;
	server_destroy(self);
}
