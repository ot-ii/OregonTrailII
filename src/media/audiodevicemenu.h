#pragma once

#include <stdbool.h>
#include <stdint.h>

#define AUDIO_DEVICE_MENU_DEFAULT 6400

void AudioDeviceMenuAppend(void *popup);
void AudioDeviceMenuOnInitPopup(void *popup);
bool AudioDeviceMenuCommand(int32_t id);
