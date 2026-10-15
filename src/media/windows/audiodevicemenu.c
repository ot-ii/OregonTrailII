#include "../audiodevicemenu.h"

#include <stdlib.h>
#include <wchar.h>

#include <windows.h>

#include "../strmdsnd.h"
#include "../xaudio2sink.h"

static HMENU g_device_menu;

static wchar_t g_item_ids[XA2_MAX_DEVICES][256];
static int32_t g_item_count;

static void fill(HMENU menu)
{
	const wchar_t *current = Xa2GetDevice();
	int32_t checked = AUDIO_DEVICE_MENU_DEFAULT;

	while (GetMenuItemCount(menu) > 0)
		DeleteMenu(menu, 0, MF_BYPOSITION);

	AppendMenuW(menu, MF_STRING, AUDIO_DEVICE_MENU_DEFAULT, L"&Default");
	g_item_count = Xa2EnumDevices();

	if (g_item_count > 0)
		AppendMenuW(menu, MF_SEPARATOR, 0, NULL);

	for (int32_t i = 0; i < g_item_count; ++i) {
		wcsncpy_s(g_item_ids[i], _countof(g_item_ids[i]), Xa2DeviceId(i), _TRUNCATE);
		AppendMenuW(menu, MF_STRING, (UINT_PTR) (AUDIO_DEVICE_MENU_DEFAULT + 1 + i), Xa2DeviceName(i));

		if (current[0] && !wcscmp(current, g_item_ids[i]))
			checked = AUDIO_DEVICE_MENU_DEFAULT + 1 + i;
	}

	CheckMenuRadioItem(menu, AUDIO_DEVICE_MENU_DEFAULT, AUDIO_DEVICE_MENU_DEFAULT + g_item_count, (UINT) checked,
		MF_BYCOMMAND);
}

void AudioDeviceMenuAppend(void *popup)
{
	g_device_menu = CreatePopupMenu();
	fill(g_device_menu);
	AppendMenuA((HMENU) popup, MF_SEPARATOR, 0, NULL);
	AppendMenuA((HMENU) popup, MF_POPUP, (UINT_PTR) g_device_menu, "Audio &Device");
}

void AudioDeviceMenuOnInitPopup(void *popup)
{
	if (popup && (HMENU) popup == g_device_menu)
		fill((HMENU) popup);
}

bool AudioDeviceMenuCommand(int32_t id)
{
	int32_t index = id - AUDIO_DEVICE_MENU_DEFAULT - 1;

	if (id < AUDIO_DEVICE_MENU_DEFAULT || index >= g_item_count)
		return false;

	StrmdSndSetDevice(index < 0 ? NULL : g_item_ids[index]);

	if (g_device_menu)
		CheckMenuRadioItem(g_device_menu, AUDIO_DEVICE_MENU_DEFAULT, AUDIO_DEVICE_MENU_DEFAULT + g_item_count,
			(UINT) id, MF_BYCOMMAND);

	return true;
}
