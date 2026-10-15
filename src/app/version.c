#include "version.h"

const char *GameVersionString(void)
{
	return VER_STRING;
}

int32_t GameVersionMajor(void)
{
	return VER_MAJOR;
}

int32_t GameVersionMinor(void)
{
	return VER_MINOR;
}

int32_t GameVersionBuild(void)
{
	return VER_BUILD;
}
