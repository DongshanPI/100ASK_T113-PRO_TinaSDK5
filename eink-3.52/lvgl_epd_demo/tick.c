/* LVGL tick provider */
#include <stdio.h>
#include <time.h>
#include <stdint.h>

uint32_t custom_tick_get(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
