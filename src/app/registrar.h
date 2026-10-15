#pragma once

#include <stdint.h>

struct view;
struct ctrl_desc;
struct registrar;

typedef struct view *(*RegistrarCtor)(struct ctrl_desc *args);

struct registrar_entry {
	int32_t type_id;
	RegistrarCtor ctor;
};

struct registrar *RegistrarCreate(void);
void RegistrarDestroy(struct registrar **ctx_mod);

void RegistrarRegister(int32_t type_id, RegistrarCtor ctor);
struct view *RegistrarCreateObject(int32_t type_id, struct ctrl_desc *args);
int32_t RegistrarFindType(const struct registrar_entry *table, int32_t type_id);

extern struct registrar *g_registrar_singleton;
