#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "modaldialog.h"

struct cstr;
struct strrsrc;

enum hunt_weapon {
	WEAPON_CANCELLED = -1,
	WEAPON_PISTOL = 0,
	WEAPON_RIFLE = 1,
	WEAPON_SHOTGUN = 2,
};

enum {
	HUNT_MISSING_GUNPOWDER = 1,
	HUNT_MISSING_FIREARM = 2,
	HUNT_MISSING_BULLETS = 4,
	HUNT_MISSING_SHOT = 8,
};

struct hunt_weapon_dlg_ops {
	struct modal_dialog_ops base;
};

struct hunt_weapon_dlg {
	struct modal_dialog base;
};

#define HUNT_WEAPON_DLG_OPS(p) ((const struct hunt_weapon_dlg_ops *) ((const struct object *) (p))->ops)

extern const struct hunt_weapon_dlg_ops HUNT_WEAPON_DLG_OPS_TABLE;

void HuntWeaponDlgInit(struct hunt_weapon_dlg *self, struct strrsrc *weapons);
struct hunt_weapon_dlg *HuntWeaponDlgCreate(struct strrsrc *weapons);
void HuntWeaponDlgDestroy(struct hunt_weapon_dlg **ctx_mod);
void hunt_weapon_dlg_destroy(struct object *self);
void hunt_weapon_dlg_begin(struct modal_dialog *self, intptr_t param);
void hunt_weapon_dlg_accept(struct modal_dialog *self, intptr_t out);
void hunt_weapon_dlg_on_event(struct evt_handler *self, struct cmd_event *event);

bool ChooseHuntingWeapon(int32_t *out_weapon, int32_t *out_ammo);

int32_t HuntMissingItems(void);
struct cstr *HuntMissingText(int32_t missing);
void HuntUseAmmo(int32_t shots, int32_t weapon);
