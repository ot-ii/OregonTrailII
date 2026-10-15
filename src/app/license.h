#pragma once

#include <stdbool.h>
#include <stdint.h>

struct product_strings {
	char not_implemented_0[0x20];
	char name_20[0x20];
	char licensee_40[0x20];
	char not_implemented_60[0x20];
};

enum license_type {
	LICENSE_FULL = 2,
	LICENSE_DEMO = 4,
};

void LicenseInitialize(int32_t unused_arg_0);
int32_t LicenseGetType(void);
void LicenseGetLicenseeInformation(struct product_strings *dst);
bool SplashDisplayAndWait(void);
const char *SplashGetErrorText(void);
void SplashHandleErrorDisplay(void);
