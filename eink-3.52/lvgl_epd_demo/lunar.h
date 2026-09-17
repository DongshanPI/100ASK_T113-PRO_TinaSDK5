#ifndef EINK_LUNAR_H
#define EINK_LUNAR_H

#include <stddef.h>

struct lunar_date {
	int year;
	int month;
	int day;
	int leap;
};

int lunar_from_solar(int year, int month, int day, struct lunar_date *out);
void lunar_format(const struct lunar_date *date, char *buffer, size_t size);
const char *calendar_holiday(int year, int month, int day,
			     const struct lunar_date *lunar);

#endif
