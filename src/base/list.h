#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*ListFreeFunc)(void *item);
typedef bool (*ListMatchFunc)(const void *item, const void *key);

struct list_node {
	struct list_node *prev;
	struct list_node *next;
};

struct list {
	struct list_node node;
	struct list_node *head;
	struct list_node *tail;
	int32_t count;
};

void ListInit(struct list *list);
void ListInsertHead(struct list *list, struct list_node *node);
void ListInsertTail(struct list *list, struct list_node *node);
void ListMoveBefore(struct list *list, struct list_node *node, struct list_node *at);
void ListMoveAfter(struct list *list, struct list_node *node, struct list_node *at);
void ListRemove(struct list *list, struct list_node *node);
struct list_node *ListNodeAt(const struct list *list, int32_t index);

struct oblist;

struct oblist *OblistCreate(ListFreeFunc free_func);
void OblistDestroy(struct oblist **ctx_mod);

void OblistInsertAtCursor(struct oblist *ctx, void *obj);
void OblistPushFront(struct oblist *ctx, void *obj);
void OblistPushBack(struct oblist *ctx, void *obj);
void OblistClear(struct oblist *ctx);
void *OblistExtract(struct oblist *ctx, const void *obj);
bool OblistRemove(struct oblist *ctx, const void *obj);
void *OblistDataAt(struct oblist *ctx, int32_t index, bool set_cursor);
bool OblistSeekTo(struct oblist *ctx, const void *obj);
void *OblistCurrent(const struct oblist *ctx);
bool OblistIsEmpty(const struct oblist *ctx);
void *OblistNext(struct oblist *ctx, bool advance);
void *OblistPrev(struct oblist *ctx, bool advance);
void *OblistFirst(struct oblist *ctx);
int32_t OblistCount(const struct oblist *ctx);

struct obarray;

struct obarray *ObarrayCreate(ListFreeFunc free_func);
void ObarrayDestroy(struct obarray **ctx_mod);

int32_t ObarrayCount(const struct obarray *ctx);
void ObarrayRemoveAll(struct obarray *ctx);
void ObarraySwap(struct obarray *ctx, int32_t i, int32_t j);
int32_t ObarrayIndexOf(const struct obarray *ctx, const void *obj);
void ObarraySetAt(struct obarray *ctx, int32_t index, void *obj);
void ObarrayAppend(struct obarray *ctx, void *obj);
void *ObarrayGetAt(const struct obarray *ctx, int32_t index);
void ObarrayRemoveAt(struct obarray *ctx, int32_t index);
bool ObarrayAddUnique(struct obarray *ctx, void *obj);
void *ObarrayFind(const struct obarray *ctx, ListMatchFunc match, const void *key);
bool ObarrayRemoveMatch(struct obarray *ctx, ListMatchFunc match, const void *key);
