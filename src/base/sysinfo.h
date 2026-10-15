#pragma once

#include "object.h"

struct strrsrc;

struct sys_info {
	struct object base;
	struct strrsrc *rsrc;
};

extern const struct object_ops SYS_INFO_OPS_TABLE;

void SysInfoInit(struct sys_info *self);
void sys_info_destroy(struct object *self);

void SysInfoInitData(struct sys_info *self);
void SysInfoBuildLists(struct sys_info *self, struct strrsrc **out_list2, struct strrsrc **out_machine);
struct strrsrc *SysInfoBuildList2(struct sys_info *self);
struct strrsrc *SysInfoBuildMachineStrings(struct sys_info *self);
