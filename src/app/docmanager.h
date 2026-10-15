#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/object.h"

struct doc_file;
struct obarray;
struct window;

struct doc_manager {
	struct object base;
	struct doc_file *doc[1];
	struct obarray *objects[1];
};

extern const struct object_ops DOC_MANAGER_OPS_TABLE;

struct doc_manager *DocManagerCreate(void);
void DocManagerDestroy(struct doc_manager **ctx_mod);
void doc_manager_destroy(struct object *self);

void DocManagerAddDoc(struct doc_manager *self, struct doc_file *doc_ptr);
void DocManagerAddWindow(struct doc_manager *self, struct window *win);
bool DocManagerIsEmpty(struct doc_manager *self);
void DocManagerCloseSingle(struct doc_manager *self);
struct doc_file *DocManagerGetSingleDoc(struct doc_manager *self);
int32_t DocManagerSingleIndex(struct doc_manager *self);
bool DocManagerAnyModified(struct doc_manager *self);
void DocManagerFlushModifiedV(struct doc_manager *self);
int32_t DocManagerCloseProceed(struct doc_manager *self);
void DocManagerFindWindowDocV(struct doc_manager *self, struct window *win);
void DocManagerCloseAll(struct doc_manager *self);
