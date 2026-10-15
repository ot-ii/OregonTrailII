#include "list.h"

#include <string.h>

#include "memory.h"

void ListInit(struct list *list)
{
	memset(list, 0, sizeof(struct list));
}

void ListInsertHead(struct list *list, struct list_node *node)
{
	if (list->head) {
		list->head->prev = node;
		node->next = list->head;
		node->prev = NULL;
		list->head = node;

	} else {
		list->head = list->tail = node;
	}

	list->count++;
}

void ListInsertTail(struct list *list, struct list_node *node)
{
	if (list->tail) {
		list->tail->next = node;
		node->prev = list->tail;
		node->next = NULL;
		list->tail = node;

	} else {
		list->head = list->tail = node;
	}

	list->count++;
}

static void unlink_for_move(struct list *list, struct list_node *node)
{
	if (node->next && node->prev) {
		node->prev->next = node->next;
		node->next->prev = node->prev;

	} else if (!node->next && node->prev) {
		node->prev->next = NULL;
		list->tail = node->prev;

	} else if (node->next && !node->prev) {
		node->next->prev = NULL;
		list->head = node->next;
	}
}

void ListMoveBefore(struct list *list, struct list_node *node, struct list_node *at)
{
	if (!at || at == node)
		return;

	unlink_for_move(list, node);

	if (at->prev) {
		at->prev->next = node;
		node->prev = at->prev;
		at->prev = node;
		node->next = at;

	} else if (list->head == at) {
		at->prev = node;
		node->next = at;
		node->prev = NULL;
		list->head = node;
	}
}

void ListMoveAfter(struct list *list, struct list_node *node, struct list_node *at)
{
	if (!at || at == node)
		return;

	unlink_for_move(list, node);

	if (at->next) {
		at->next->prev = node;
		node->next = at->next;
		at->next = node;
		node->prev = at;

	} else if (list->tail == at) {
		at->next = node;
		node->prev = at;
		node->next = NULL;
		list->tail = node;
	}
}

void ListRemove(struct list *list, struct list_node *node)
{
	if (list->head == node) {
		list->head = node->next;

		if (list->head) {
			list->head->prev = NULL;

		} else {
			list->tail = NULL;
		}

	} else if (list->tail == node) {
		list->tail = node->prev;
		list->tail->next = NULL;

	} else {
		node->prev->next = node->next;
		node->next->prev = node->prev;
	}

	node->prev = node->next = NULL;
	list->count--;
}

struct list_node *ListNodeAt(const struct list *list, int32_t index)
{
	for (struct list_node *node = list->head; node; node = node->next)
		if (index-- == 0)
			return node;

	return NULL;
}

struct ob_node {
	struct ob_node *next;
	struct ob_node *prev;
	void *data;
};

struct oblist {
	struct ob_node head;
	struct ob_node tail;
	struct ob_node *cursor;
	int32_t count;
	ListFreeFunc free_func;
};

static void oblist_reset(struct oblist *ctx)
{
	ctx->head.next = &ctx->tail;
	ctx->head.prev = NULL;
	ctx->tail.next = NULL;
	ctx->tail.prev = &ctx->head;
	ctx->cursor = &ctx->head;
	ctx->count = 0;
}

struct oblist *OblistCreate(ListFreeFunc free_func)
{
	struct oblist *ctx = MemAlloc(1, sizeof(struct oblist));
	ctx->free_func = free_func;
	oblist_reset(ctx);

	return ctx;
}

void OblistDestroy(struct oblist **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct oblist *ctx = *ctx_mod;
	*ctx_mod = NULL;

	OblistClear(ctx);

	MemFree(ctx);
}

static void insert_after(struct oblist *ctx, void *obj, struct ob_node *at)
{
	struct ob_node *node = MemAlloc(1, sizeof(struct ob_node));
	node->data = obj;
	node->prev = at;
	node->next = at->next;
	at->next->prev = node;
	at->next = node;
	ctx->cursor = node;
	ctx->count++;
}

void OblistInsertAtCursor(struct oblist *ctx, void *obj)
{
	if (!ctx->cursor || ctx->cursor == &ctx->tail) {
		OblistPushFront(ctx, obj);

	} else {
		insert_after(ctx, obj, ctx->cursor);
	}
}

void OblistPushFront(struct oblist *ctx, void *obj)
{
	insert_after(ctx, obj, &ctx->head);
}

void OblistPushBack(struct oblist *ctx, void *obj)
{
	insert_after(ctx, obj, ctx->tail.prev);
}

void OblistClear(struct oblist *ctx)
{
	struct ob_node *node = ctx->head.next;

	while (node != &ctx->tail) {
		struct ob_node *next = node->next;

		if (ctx->free_func && node->data)
			ctx->free_func(node->data);

		MemFree(node);
		node = next;
	}

	oblist_reset(ctx);
}

static struct ob_node *find_node(const struct oblist *ctx, const void *obj)
{
	for (struct ob_node *node = ctx->head.next; node != &ctx->tail; node = node->next)
		if (node->data == obj)
			return node;

	return NULL;
}

static void *remove_node(struct oblist *ctx, struct ob_node *node)
{
	if (!node)
		return NULL;

	bool was_cursor = ctx->cursor == node;
	void *data = node->data;

	node->prev->next = node->next;
	node->next->prev = node->prev;
	MemFree(node);
	ctx->count--;

	if (was_cursor)
		ctx->cursor = ctx->head.next == &ctx->tail ? NULL : ctx->head.next;

	return data;
}

void *OblistExtract(struct oblist *ctx, const void *obj)
{
	return remove_node(ctx, find_node(ctx, obj));
}

bool OblistRemove(struct oblist *ctx, const void *obj)
{
	struct ob_node *node = find_node(ctx, obj);

	if (!node)
		return false;

	void *data = remove_node(ctx, node);

	if (ctx->free_func && data)
		ctx->free_func(data);

	return true;
}

void *OblistDataAt(struct oblist *ctx, int32_t index, bool set_cursor)
{
	struct ob_node *node = ctx->head.next;

	for (int32_t pos = 0; node != &ctx->tail && pos < index; pos++)
		node = node->next;

	if (node == &ctx->tail)
		return NULL;

	if (set_cursor)
		ctx->cursor = node;

	return node->data;
}

bool OblistSeekTo(struct oblist *ctx, const void *obj)
{
	struct ob_node *node = find_node(ctx, obj);

	if (!node)
		return false;

	ctx->cursor = node;

	return true;
}

void *OblistCurrent(const struct oblist *ctx)
{
	if (!ctx->cursor || !ctx->cursor->next || !ctx->cursor->prev)
		return NULL;

	return ctx->cursor->data;
}

bool OblistIsEmpty(const struct oblist *ctx)
{
	return ctx->head.next == &ctx->tail;
}

void *OblistNext(struct oblist *ctx, bool advance)
{
	struct ob_node *c = ctx->cursor;

	if (!c || !c->next || !c->next->next)
		return NULL;

	if (!advance)
		return c->next->data;

	ctx->cursor = c->next;

	return ctx->cursor->data;
}

void *OblistPrev(struct oblist *ctx, bool advance)
{
	struct ob_node *c = ctx->cursor;

	if (!c || !c->prev || !c->prev->prev)
		return NULL;

	if (!advance)
		return c->prev->data;

	ctx->cursor = c->prev;

	return ctx->cursor->data;
}

void *OblistFirst(struct oblist *ctx)
{
	return OblistDataAt(ctx, 0, true);
}

int32_t OblistCount(const struct oblist *ctx)
{
	return ctx->count;
}

struct obarray {
	void **items;
	int32_t count;
	int32_t capacity;
	ListFreeFunc free_func;
};

struct obarray *ObarrayCreate(ListFreeFunc free_func)
{
	struct obarray *ctx = MemAlloc(1, sizeof(struct obarray));
	ctx->free_func = free_func;

	return ctx;
}

void ObarrayDestroy(struct obarray **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct obarray *ctx = *ctx_mod;
	*ctx_mod = NULL;

	while (ctx->count) {
		void *obj = ctx->items[0];
		ObarrayRemoveAt(ctx, 0);

		if (ctx->free_func && obj)
			ctx->free_func(obj);
	}

	MemFree(ctx->items);
	MemFree(ctx);
}

int32_t ObarrayCount(const struct obarray *ctx)
{
	return ctx->count;
}

void ObarrayRemoveAll(struct obarray *ctx)
{
	while (ctx->count)
		ObarrayRemoveAt(ctx, 0);
}

void ObarraySwap(struct obarray *ctx, int32_t i, int32_t j)
{
	if (i < 0 || i >= ctx->count || j < 0 || j >= ctx->count)
		return;

	void *tmp = ctx->items[i];
	ctx->items[i] = ctx->items[j];
	ctx->items[j] = tmp;
}

int32_t ObarrayIndexOf(const struct obarray *ctx, const void *obj)
{
	for (int32_t i = 0; i < ctx->count; i++)
		if (ctx->items[i] == obj)
			return i;

	return -1;
}

void ObarraySetAt(struct obarray *ctx, int32_t index, void *obj)
{
	if (index < 0 || index >= ctx->count)
		return;

	ctx->items[index] = obj;
}

void ObarrayAppend(struct obarray *ctx, void *obj)
{
	if (ctx->count == ctx->capacity) {
		ctx->capacity = ctx->capacity ? ctx->capacity * 2 : 8;
		ctx->items = MemRealloc(ctx->items, (size_t) ctx->capacity, sizeof(void *));
	}

	ctx->items[ctx->count++] = obj;
}

void *ObarrayGetAt(const struct obarray *ctx, int32_t index)
{
	if (index < 0 || index >= ctx->count)
		return NULL;

	return ctx->items[index];
}

void ObarrayRemoveAt(struct obarray *ctx, int32_t index)
{
	if (index < 0 || index >= ctx->count)
		return;

	memmove(ctx->items + index, ctx->items + index + 1, (size_t) (ctx->count - index - 1) * sizeof(void *));
	ctx->count--;
}

bool ObarrayAddUnique(struct obarray *ctx, void *obj)
{
	if (ObarrayIndexOf(ctx, obj) > -1)
		return false;

	ObarrayAppend(ctx, obj);

	return true;
}

void *ObarrayFind(const struct obarray *ctx, ListMatchFunc match, const void *key)
{
	for (int32_t i = 0; i < ctx->count; i++)
		if (match(ctx->items[i], key))
			return ctx->items[i];

	return NULL;
}

bool ObarrayRemoveMatch(struct obarray *ctx, ListMatchFunc match, const void *key)
{
	for (int32_t i = 0; i < ctx->count; i++) {
		if (match(ctx->items[i], key)) {
			ObarrayRemoveAt(ctx, i);
			return true;
		}
	}

	return false;
}
