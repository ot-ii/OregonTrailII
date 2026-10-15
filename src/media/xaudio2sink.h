#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int32_t Xa2Open(int32_t samples_per_sec, int32_t channels, int32_t bits_per_sample);
void Xa2Close(void);
int32_t Xa2Submit(const void *data, int32_t bytes);
int32_t Xa2TakeBuffersDone(void);
int32_t Xa2QueuedBuffers(void);
uint32_t Xa2SamplesPlayed(void);
void Xa2Pause(void);
void Xa2Resume(void);
void Xa2Flush(void);
bool Xa2IsOpen(void);

#define XA2_MAX_DEVICES 64
int32_t Xa2EnumDevices(void);
const wchar_t *Xa2DeviceId(int32_t index);
const wchar_t *Xa2DeviceName(int32_t index);
void Xa2SetDevice(const wchar_t *id);
const wchar_t *Xa2GetDevice(void);

bool Xa2ClipLoad(const void *pcm, int32_t bytes, int32_t samples_per_sec, int32_t channels, int32_t bits_per_sample);
void Xa2ClipPlay(int32_t ms);
void Xa2ClipStop(void);
void Xa2ClipVolume(int32_t volume);
void Xa2ClipUnload(void);
