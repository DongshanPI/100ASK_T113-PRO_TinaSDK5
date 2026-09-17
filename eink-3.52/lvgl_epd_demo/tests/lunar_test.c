#include "lunar.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	struct lunar_date date; char text[64];
	assert(!lunar_from_solar(2024, 2, 10, &date)); assert(date.month == 1 && date.day == 1 && !date.leap); assert(!strcmp(calendar_holiday(2024, 2, 10, &date), "春节"));
	assert(!lunar_from_solar(2026, 9, 17, &date)); assert(date.month == 8 && date.day == 7); lunar_format(&date, text, sizeof(text)); assert(strstr(text, "八月初七"));
	assert(lunar_from_solar(1899, 1, 1, &date)); puts("lunar_test: PASS"); return 0;
}
