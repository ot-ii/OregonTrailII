#pragma once

#include <stdbool.h>
#include <stdint.h>

struct vfw_probe {
	bool unused;
};

struct vfw_probe *VfwProbeCreate(void);
void VfwProbeDestroy(struct vfw_probe **ctx_mod);

void VfwProbeCheck(struct vfw_probe *self);
bool VfwProbeIsAvail(struct vfw_probe *self);
void VfwProbeRelease(struct vfw_probe *self);

extern bool g_vfw_available;
extern int32_t g_vfw_check_count;
