#include "../clock.h"

#include <time.h>

#include <windows.h>

uint32_t TimeTicks(void)
{
	return (uint32_t) GetTickCount();
}

uint32_t TimeUnix(void)
{
	return (uint32_t) time(NULL);
}

void TimeSleep(uint32_t ms)
{
	Sleep((DWORD) ms);
}
