#include "docmanager.h"

#include <stddef.h>

#include "../dialogs/alert.h"
#include "../base/cstr.h"
#include "../data/docfile.h"
#include "../data/gfxdev.h"
#include "../base/list.h"
#include "../base/log.h"
#include "../base/memory.h"
#include "../data/strrsrc.h"
#include "../ui/window.h"

static int32_t g_open_doc_count;

const struct object_ops DOC_MANAGER_OPS_TABLE = {
	doc_manager_destroy,
};

struct doc_manager *DocManagerCreate(void)
{
	struct doc_manager *self = MemAlloc(1, sizeof(struct doc_manager));
	ObjectInit(&self->base);
	self->base.ops = &DOC_MANAGER_OPS_TABLE;

	for (int32_t i = 0; i < 1; ++i) {
		self->doc[i] = NULL;
		self->objects[i] = NULL;
	}

	return self;
}

void DocManagerDestroy(struct doc_manager **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void doc_manager_destroy(struct object *obj)
{
	DocManagerCloseAll((struct doc_manager *) obj);

	object_destroy(obj);
}

void DocManagerAddDoc(struct doc_manager *self, struct doc_file *doc_ptr)
{
	if (g_open_doc_count == 1)
		return;

	for (int32_t i = 0; i < 1; ++i) {
		if (!self->doc[i]) {
			self->doc[i] = doc_ptr;
			++g_open_doc_count;
			break;
		}
	}
}

void DocManagerAddWindow(struct doc_manager *self, struct window *win)
{
	int32_t doc_index = DocManagerSingleIndex(self);

	if (doc_index == -1)
		return;

	struct obarray *windows = self->objects[doc_index];

	if (!windows)
		windows = ObarrayCreate(NULL);

	self->objects[doc_index] = windows;
	ObarrayAppend(windows, win);
	WindowSetState(win, self->doc[doc_index]);
}

bool DocManagerIsEmpty(struct doc_manager *self)
{
	(void) self;

	return g_open_doc_count < 1;
}

static void close_doc(struct doc_manager *self, int32_t doc_index)
{
	if (doc_index < 0)
		return;

	if (!self->doc[doc_index])
		return;

	ObjectDelete(self->doc[doc_index]);
	self->doc[doc_index] = NULL;

	struct obarray *windows = self->objects[doc_index];

	if (windows) {
		int32_t count = ObarrayCount(windows);

		for (int32_t i = 0; i < count; ++i) {
			struct window *win = ObarrayGetAt(windows, i);

			if (win)
				ObjectDelete(win);
		}

		ObarrayDestroy(&self->objects[doc_index]);
	}

	--g_open_doc_count;
}

void DocManagerCloseAll(struct doc_manager *self)
{
	for (int32_t i = 0; i < 1; ++i)
		close_doc(self, i);
}

void DocManagerCloseSingle(struct doc_manager *self)
{
	close_doc(self, DocManagerSingleIndex(self));
}

int32_t DocManagerCloseProceed(struct doc_manager *self)
{
	(void) self;
	struct strrsrc *strings = StrRsrcLoad(g_text_lib, 5, true);

	if (!strings || StrRsrcId(strings) != 5)
		LogError(LOG_ERROR, "Miscellaneous strings not found");

	struct cstr *message = StrRsrcGetCStr(strings, 5);
	int32_t choice = AlertShow(g_alert_handler, 3, message, 3, 1);
	CStrDestroy(&message);
	StrRsrcDestroy(&strings);

	if (choice == 2)
		return 0;

	if (choice == 3)
		return 1;

	return 2;
}

void DocManagerFindWindowDocV(struct doc_manager *self, struct window *win)
{
	int32_t doc_index = DocManagerSingleIndex(self);

	if (doc_index == -1)
		return;

	struct obarray *windows = self->objects[doc_index];

	if (!windows)
		return;

	int32_t count = ObarrayCount(windows);

	for (int32_t i = 0; i < count; ++i) {
		if (ObarrayGetAt(windows, i) == win) {
			ObarrayRemoveAt(windows, i);
			WindowSetState(win, NULL);
			break;
		}
	}

	if (ObarrayCount(windows) == 0)
		ObarrayDestroy(&self->objects[doc_index]);
}

struct doc_file *DocManagerGetSingleDoc(struct doc_manager *self)
{
	if (!g_open_doc_count)
		return NULL;

	if (g_open_doc_count == 1)
		return self->doc[0];

	return NULL;
}

int32_t DocManagerSingleIndex(struct doc_manager *self)
{
	(void) self;

	if (!g_open_doc_count)
		return -1;

	if (g_open_doc_count == 1)
		return 0;

	return -1;
}

bool DocManagerAnyModified(struct doc_manager *self)
{
	for (int32_t i = 0; g_open_doc_count > i; ++i) {
		struct doc_file *file = self->doc[i];

		if (file && DocFileIsDirty(file))
			return true;
	}

	return false;
}

void DocManagerFlushModifiedV(struct doc_manager *self)
{
	for (int32_t i = 0; g_open_doc_count > i; ++i) {
		struct doc_file *file = self->doc[i];

		if (file && DocFileIsDirty(file))
			DOC_FILE_OPS(file)->OpenDefault(file);
	}
}
