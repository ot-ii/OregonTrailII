#include "../otiiapp.h"

#include <windows.h>

#include "../../base/cstr.h"
#include "../../ui/framewin.h"
#include "../../ui/menubar.h"
#include "../../base/path.h"

void OtiiAppFatalMessage(const char *text)
{
	MessageBoxA(0, text, "Oregon Trail II", 16);
	FatalExit(1);
}

void OtiiAppWinHelp(struct otii_app *self, uint32_t command)
{
	struct path *help_path_p = APP_OPS(self)->GetResourcePath(&self->base.base);
	PathSetPath(help_path_p, "oregonii.hlp");
	struct cstr *help_file_h = PathCopyPath(help_path_p);
	WinHelpA(g_parent_win ? (HWND) g_parent_win->base.hwnd : NULL, CStrGet(help_file_h), command, 0);
	CStrDestroy(&help_file_h);
	PathDestroy(&help_path_p);
}

void OtiiAppEnableMenuCommand(uint32_t id, bool enable)
{
	EnableMenuItem((HMENU) g_hmnu, id, MF_BYCOMMAND | (enable ? MF_ENABLED : MF_GRAYED));
}

void ForceSaveAddMenuItem(void *menu_bar)
{
	int popup, pos, n;

	for (popup = 0; popup < GetMenuItemCount((HMENU) menu_bar); ++popup) {
		HMENU sub = GetSubMenu((HMENU) menu_bar, popup);
		n = sub ? GetMenuItemCount(sub) : 0;
		for (pos = 0; pos < n; ++pos)
			if (GetMenuItemID(sub, pos) == MENU_ITEM_SAVE_GAME) {
				InsertMenuA(sub, pos + 1, MF_BYPOSITION | MF_STRING, FORCE_SAVE_MENU_ID, "&Force Save");
				return;
			}
	}
}

void PlayIntroAddMenuItem(void *menu_bar)
{
	int popup, pos, n;

	for (popup = 0; popup < GetMenuItemCount((HMENU) menu_bar); ++popup) {
		HMENU sub = GetSubMenu((HMENU) menu_bar, popup);
		n = sub ? GetMenuItemCount(sub) : 0;
		for (pos = 0; pos < n; ++pos)
			if (GetMenuItemID(sub, pos) == MENU_ITEM_SAVE_GAME) {
				pos = n - 1;
				if (pos > 0 && (GetMenuState(sub, pos - 1, MF_BYPOSITION) & MF_SEPARATOR))
					--pos;
				else
					InsertMenuA(sub, pos, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
				InsertMenuA(sub, pos, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
				InsertMenuA(sub, pos + 1, MF_BYPOSITION | MF_STRING, PLAY_INTRO_MENU_ID, "Play &Intro");
				return;
			}
	}
}
