#include "registrar.h"

#include <stddef.h>

#include "../base/log.h"
#include "../base/memory.h"
#include "../base/str.h"

struct registrar {
	int32_t unused;
};

static struct registrar_entry *g_registrar_types;
static int32_t g_registrar_type_count;

struct registrar *g_registrar_singleton;

struct registrar *RegistrarCreate(void)
{
	return MemAlloc(1, sizeof(struct registrar));
}

void RegistrarDestroy(struct registrar **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct registrar *ctx = *ctx_mod;
	*ctx_mod = NULL;

	MemFree(g_registrar_types);
	g_registrar_types = NULL;
	g_registrar_type_count = 0;

	MemFree(ctx);
}

void RegistrarRegister(int32_t type_id, RegistrarCtor ctor)
{
	g_registrar_types = MemRealloc(g_registrar_types, (size_t) g_registrar_type_count + 1,
		sizeof(struct registrar_entry));

	if (RegistrarFindType(g_registrar_types, type_id) < 0) {
		g_registrar_types[g_registrar_type_count].type_id = type_id;
		g_registrar_types[g_registrar_type_count].ctor = ctor;
		g_registrar_type_count++;
	}
}

struct view *RegistrarCreateObject(int32_t type_id, struct ctrl_desc *args)
{
	struct view *view = NULL;

	if (g_registrar_type_count > 0) {
		int32_t index = RegistrarFindType(g_registrar_types, type_id);

		if (index >= 0) {
			view = g_registrar_types[index].ctor(args);

		} else {
			char err[52];
			StrPrintf(err, "Object type %d not registered", type_id);
			LogError(LOG_ERROR, err);
		}

	} else {
		LogError(LOG_ERROR, "No objects registered");
	}

	return view;
}

int32_t RegistrarFindType(const struct registrar_entry *table, int32_t type_id)
{
	int32_t found = -1;

	for (int32_t i = 0; i < g_registrar_type_count; ++i) {
		if (table[i].type_id == type_id) {
			found = i;
			break;
		}
	}

	return found;
}
