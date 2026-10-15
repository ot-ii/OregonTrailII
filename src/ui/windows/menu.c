#include "../menu.h"

#include <stdio.h>
#include <string.h>

#include <windows.h>

#include "../../data/assets.h"
#include "../../base/byteorder.h"
#include "../../base/cstr.h"
#include "../../base/list.h"
#include "../../base/log.h"
#include "../../base/memory.h"
#include "../menuitem.h"
#include "../../data/rsrc.h"
#include "../scaler.h"
#include "../../base/str.h"

const struct object_ops MENU_OPS_TABLE = {
	menu_destroy,
};

static struct menu *menu_alloc(int32_t id)
{
	struct menu *self = MemAlloc(1, sizeof(struct menu));
	ObjectInit(&self->base);
	self->base.ops = &MENU_OPS_TABLE;
	self->id = id;
	self->items = ObarrayCreate(NULL);

	return self;
}

struct menu *MenuCreate(int32_t id, struct assets *lib, struct menu_item *owner)
{
	struct menu *self = menu_alloc(id);
	self->owner = owner;

	if (!lib) {
		char err[100];
		StrPrintf(err, "Menu resource %d, no CMECCLib specified", id);
		LogError(LOG_ERROR, err);
		return self;
	}

	struct rsrc *rsrc = AssetsGetElement(lib, "MENU", id, true);

	if (!rsrc || !RsrcSize(rsrc)) {
		RsrcDestroy(&rsrc);
		return self;
	}

	switch (RsrcStorage(rsrc)) {
		case RSRC_TEXT: {
			char err[100];
			StrPrintf(err, "Menu resource %d not packed", id);
			LogError(LOG_ERROR, err);
			break;
		}
		case RSRC_PACKED:
		case RSRC_NATIVE:
			RsrcLoadAndUnpack(rsrc);
			break;
		default: {
			char err[100];
			StrPrintf(err, "Menu resource %d, invalid format", id);
			LogError(LOG_ERROR, err);
			RsrcDestroy(&rsrc);
			return self;
		}
	}

	const uint8_t *cursor = RsrcData(rsrc);
	const uint8_t *end = cursor + RsrcSize(rsrc) - 1;
	struct cstr *label = CStrCreate();

	while (cursor < end) {
		cursor += strlen((const char *) cursor) + 1;
		CStrAssignBuf(label, (const char *) cursor);
		cursor += strlen((const char *) cursor) + 1;
		int32_t command = ReadLe16(cursor);
		cursor += 2;
		int32_t submenu_id = ReadLe16(cursor);
		cursor += 2;
		int32_t sub_command = ReadLe16(cursor);
		cursor += 2;
		bool enabled = *cursor++ != 0;
		cursor++;
		bool checked = (ReadLe16(cursor) & 0xFF) != 0;
		cursor += 2;

		struct menu_item *item = MenuItemCreate(lib, label, command, submenu_id, sub_command, enabled, checked);

		if (item)
			ObarrayAppend(self->items, item);
	}

	CStrDestroy(&label);
	RsrcDestroy(&rsrc);

	return self;
}

struct menu *MenuCreateId(int32_t id)
{
	struct menu *self = menu_alloc(id);
	self->owner = NULL;

	return self;
}

void MenuDestroy(struct menu **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void menu_destroy(struct object *obj)
{
	struct menu *self = (struct menu *) obj;

	if (self->items) {
		while (ObarrayCount(self->items)) {
			struct menu_item *item = ObarrayGetAt(self->items, 0);

			if (item)
				ObjectDelete(item);

			ObarrayRemoveAt(self->items, 0);
		}

		ObarrayDestroy(&self->items);
	}

	object_destroy(obj);
}

void MenuAddItem(struct menu *self, struct menu_item *obj)
{
	if (!self->items)
		self->items = ObarrayCreate(NULL);

	ObarrayAppend(self->items, obj);
}

struct menu_item *MenuFindItem(struct menu *self, int32_t id, struct menu **out_menu, int32_t *out_index)
{
	struct menu_item *found = NULL;
	int32_t count = ObarrayCount(self->items);
	bool done = false;
	int32_t index = 0;

	while (!done && index < count) {
		struct menu_item *item = ObarrayGetAt(self->items, index);

		if (item) {
			if (item->command == id) {
				done = true;

				if (out_menu)
					*out_menu = self;

				if (out_index)
					*out_index = index;

				found = item;

			} else {
				struct menu *submenu = item->submenu;

				if (submenu) {
					found = MenuFindItem(submenu, id, out_menu, out_index);

					if (found)
						done = true;
				}
			}
		}

		if (!done)
			++index;
	}

	return found;
}

void MenuRemoveItem(struct menu *self, int32_t index)
{
	if (self->items) {
		struct menu_item *item = ObarrayGetAt(self->items, index);

		if (item)
			ObjectDelete(item);

		ObarrayRemoveAt(self->items, index);
	}
}

void *MenuBuildPopup(struct menu *self, int32_t index)
{
	HMENU popup = CreatePopupMenu();
	struct menu_item *item = ObarrayGetAt(self->items, index);
	struct menu *submenu = item->submenu;
	int32_t count = ObarrayCount(submenu->items);

	for (int32_t i = 0; i < count; ++i) {
		item = ObarrayGetAt(submenu->items, i);
		const char *text = CStrGet(item->text);

		if (text[0] == '-') {
			AppendMenuA(popup, MF_SEPARATOR, 0, NULL);

		} else {
			AppendMenuA(popup, MF_STRING, (UINT_PTR) (intptr_t) item->command, text);

			if (!strcmp(text, "&Speech")) {
				AudioDeviceMenuAppend(popup);
				ScalerMenuAppend(popup);
			}
		}
	}

	return popup;
}
