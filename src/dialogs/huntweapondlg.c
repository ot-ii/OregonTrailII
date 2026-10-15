#include "huntweapondlg.h"

#include <stddef.h>

#include "../ui/cliplabelview.h"
#include "../ui/combobox.h"
#include "../base/cstr.h"
#include "../app/event.h"
#include "../ui/framewin.h"
#include "../game/game.h"
#include "../game/invitem.h"
#include "../game/invlist.h"
#include "../base/memory.h"
#include "../game/party.h"
#include "../ui/pictctrl.h"
#include "../ui/picview.h"
#include "../data/strrsrc.h"
#include "../ui/view.h"

const struct hunt_weapon_dlg_ops HUNT_WEAPON_DLG_OPS_TABLE = {
	{
		{
			{hunt_weapon_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			hunt_weapon_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		hunt_weapon_dlg_begin,
		hunt_weapon_dlg_accept,
	},
};

void HuntWeaponDlgInit(struct hunt_weapon_dlg *self, struct strrsrc *weapons)
{
	ModalDialogInit(&self->base, -2, false, true);
	self->base.base.base.ops = &HUNT_WEAPON_DLG_OPS_TABLE.base.base.base;

	struct rect rect;
	struct point pt;
	struct view *child;
	const struct event *ev = &self->base.base.eh_event;

	self->base.title = CStrCreateFrom("Firearm");

	pt.h = 0;
	pt.v = 0;
	self->base.content = (struct view *) PicViewCreatePoint(0, &pt, 2600);
	struct view *content = self->base.content;

	rect.top = 30;
	rect.left = 60;
	rect.right = 160;
	rect.bottom = 50;
	child = (struct view *) ClipLabelViewCreate(0, &rect, CStrCreateFrom("Select a firearm"), 3, 0, 1, 0, 0, 255, 0,
		1);
	VIEW_OPS(content)->AddChild(content, child);

	rect.top = 30;
	rect.left = 170;
	rect.right = 270;
	rect.bottom = 50;
	child = (struct view *) ComboBoxCreate(9001, ev, &rect, 70, weapons, 0);
	((struct combo_box *) child)->font_id = 3;
	ViewNotifyChanged(child);
	VIEW_OPS(content)->AddChild(content, child);

	rect.top = 150;
	rect.left = 120;
	rect.right = 180;
	rect.bottom = 170;
	pt.v = rect.top;
	pt.h = rect.left;
	child = (struct view *) PictCtrlCreate(1, ev, &pt, 2102, 0, 0, CStrCreateFrom("OK"), 3, 1, 1);
	VIEW_OPS(content)->AddChild(content, child);

	rect.top = 150;
	rect.left = 220;
	rect.right = 280;
	rect.bottom = 170;
	pt.v = rect.top;
	pt.h = rect.left;
	child = (struct view *) PictCtrlCreate(2, ev, &pt, 2102, 0, 0, CStrCreateFrom("Cancel"), 3, 1, 1);
	VIEW_OPS(content)->AddChild(content, child);

	int32_t handlers[1];
	handlers[0] = CMD_CONTROL_ACTION;
	EvtHandlerBindHandler(&self->base.base, 1, handlers);
}

struct hunt_weapon_dlg *HuntWeaponDlgCreate(struct strrsrc *weapons)
{
	struct hunt_weapon_dlg *self = MemAlloc(1, sizeof(struct hunt_weapon_dlg));
	HuntWeaponDlgInit(self, weapons);

	return self;
}

void HuntWeaponDlgDestroy(struct hunt_weapon_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void hunt_weapon_dlg_destroy(struct object *obj)
{
	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

static struct inv_item *find_item(const char *name)
{
	struct cstr *key = CStrCreateFrom(name);
	struct inv_item *item = InvListFindByName(&GetGameObj()->party->inventory, key);
	CStrDestroy(&key);

	return item;
}

bool ChooseHuntingWeapon(int32_t *out_weapon, int32_t *out_ammo)
{
	bool have_rifle = false;
	bool have_shotgun = false;
	bool have_pistol = false;

	struct inv_item *rifle_it = find_item("rifle");
	struct inv_item *shotgun_it = find_item("shotgun");
	struct inv_item *pistol_it = find_item("pistol");
	struct inv_item *bullets_it = find_item("bullets");
	struct inv_item *shot_it = find_item("shot");
	struct inv_item *gunpowder_it = find_item("gunpowder");

	*out_ammo = 0;
	*out_weapon = *out_ammo;

	if (!gunpowder_it)
		return false;

	if (rifle_it && bullets_it)
		have_rifle = true;

	if (shotgun_it && shot_it)
		have_shotgun = true;

	if (pistol_it && bullets_it)
		have_pistol = true;

	if (have_rifle && !have_shotgun && !have_pistol) {
		*out_weapon = WEAPON_RIFLE;

		if (InvItemTotalPortions(bullets_it) <= 20)
			*out_ammo = InvItemTotalPortions(bullets_it);
		else
			*out_ammo = 20;

	} else if (!have_rifle && have_shotgun && !have_pistol) {
		*out_weapon = WEAPON_SHOTGUN;

		if (InvItemTotalPortions(shot_it) <= 10)
			*out_ammo = InvItemTotalPortions(shot_it);
		else
			*out_ammo = 10;

	} else if (!have_rifle && !have_shotgun && have_pistol) {
		*out_weapon = WEAPON_PISTOL;

		if (InvItemTotalPortions(bullets_it) <= 20)
			*out_ammo = InvItemTotalPortions(bullets_it);
		else
			*out_ammo = 20;

	} else if (have_rifle || have_shotgun || have_pistol) {
		struct cstr *sel = NULL;
		struct strrsrc *weapons = StrRsrcCreate();

		if (have_rifle)
			StrRsrcAppend(weapons, CStrGet(InvItemGetNameCStr(rifle_it)));

		if (have_shotgun)
			StrRsrcAppend(weapons, CStrGet(InvItemGetNameCStr(shotgun_it)));

		if (have_pistol)
			StrRsrcAppend(weapons, CStrGet(InvItemGetNameCStr(pistol_it)));

		struct hunt_weapon_dlg *dlg = HuntWeaponDlgCreate(weapons);

		if (ModalDialogRun(&dlg->base, (intptr_t) &sel, (struct window *) g_parent_win) == DLG_CANCEL) {
			*out_weapon = WEAPON_CANCELLED;
			HuntWeaponDlgDestroy(&dlg);
			return false;
		}

		if (have_rifle && CStrEqual(sel, InvItemGetNameCStr(rifle_it))) {
			*out_weapon = WEAPON_RIFLE;

			if (InvItemTotalPortions(bullets_it) <= 20)
				*out_ammo = InvItemTotalPortions(bullets_it);
			else
				*out_ammo = 20;

		} else if (have_shotgun && CStrEqual(sel, InvItemGetNameCStr(shotgun_it))) {
			*out_weapon = WEAPON_SHOTGUN;

			if (InvItemTotalPortions(shot_it) <= 10)
				*out_ammo = InvItemTotalPortions(shot_it);
			else
				*out_ammo = 10;

		} else if (have_pistol && CStrEqual(sel, InvItemGetNameCStr(pistol_it))) {
			*out_weapon = WEAPON_PISTOL;

			if (InvItemTotalPortions(bullets_it) <= 20)
				*out_ammo = InvItemTotalPortions(bullets_it);
			else
				*out_ammo = 20;
		}

		CStrDestroy(&sel);
		HuntWeaponDlgDestroy(&dlg);

	} else {
		return false;
	}

	return true;
}

void hunt_weapon_dlg_begin(struct modal_dialog *self, intptr_t param)
{
	(void) self;
	(void) param;
}

void hunt_weapon_dlg_accept(struct modal_dialog *self, intptr_t out)
{
	struct cstr **out_name = (struct cstr **) out;
	struct combo_box *combo = (struct combo_box *) ViewFindById(self->content, 9001);

	*out_name = StrRsrcGetCStr(combo->items, combo->base.value);
}

void hunt_weapon_dlg_on_event(struct evt_handler *self, struct cmd_event *evt)
{
	struct modal_dialog *dlg = (struct modal_dialog *) self;
	int32_t control_id = evt->spec_b.id;

	if (evt->command == CMD_CONTROL_ACTION) {
		switch (control_id) {
			case 1:
				dlg->result = DLG_OK;
				break;
			case 2:
				dlg->result = DLG_CANCEL;
				break;
		}

		evt->handled = true;
	}
}

int32_t HuntMissingItems(void)
{
	struct inv_item *rifle = find_item("rifle");
	struct inv_item *shotgun = find_item("shotgun");
	struct inv_item *pistol = find_item("pistol");
	struct inv_item *bullets = find_item("bullets");
	struct inv_item *shot = find_item("shot");
	int32_t missing = 0;

	if (!find_item("gunpowder"))
		missing |= HUNT_MISSING_GUNPOWDER;

	if (!shotgun && !rifle && !pistol)
		missing |= HUNT_MISSING_FIREARM;

	if (!bullets && (!shotgun || !shot) && (rifle || pistol))
		missing |= HUNT_MISSING_BULLETS;

	if (!shot && shotgun && !rifle && !pistol)
		missing |= HUNT_MISSING_SHOT;

	return missing;
}

struct cstr *HuntMissingText(int32_t missing)
{
	const char *what;

	switch (missing) {
		case 0:
			return NULL;
		case 1:
			what = "gunpowder";
			break;
		case 2:
			what = "firearms";
			break;
		case 3:
			what = "firearms or gunpowder";
			break;
		case 4:
			what = "bullets";
			break;
		case 5:
			what = "bullets or gunpowder";
			break;
		case 6:
			what = "firearms or bullets";
			break;
		case 7:
			what = "firearms, bullets or gunpowder";
			break;
		case 8:
			what = "shot";
			break;
		case 9:
			what = "shot or gunpowder";
			break;
		case 12:
			what = "bullets or shot";
			break;
		default:
			what = "";
			break;
	}

	struct cstr *text = CStrCreateFrom("You do not have any ");
	CStrAppendBuf(text, what);
	CStrAppendBuf(text, ", so you cannot go hunting.");

	return text;
}

void HuntUseAmmo(int32_t shots, int32_t weapon)
{
	if (!shots)
		return;

	struct inv_list *inv = &GetGameObj()->party->inventory;
	struct inv_item *item = find_item("gunpowder");

	if (item)
		InvItemConsumePortions(item, shots);

	if (weapon == WEAPON_RIFLE || weapon == WEAPON_PISTOL)
		item = find_item("bullets");
	else
		item = find_item("shot");

	if (item)
		InvItemConsumePortions(item, shots);

	InvListPurgeEmpty(inv);
}
