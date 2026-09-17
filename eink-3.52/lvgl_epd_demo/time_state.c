#include "time_state.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#define VALID_EPOCH 1609459200L

static enum eink_time_quality quality;

static time_t build_epoch(void)
{
#if defined(EINK_BUILD_EPOCH) && EINK_BUILD_EPOCH >= VALID_EPOCH
	return (time_t)EINK_BUILD_EPOCH;
#else
	static const char *months[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	struct tm value;
	char month[4];
	int day, year, hour, minute, second, i;

	memset(&value, 0, sizeof(value));
	if (sscanf(__DATE__, "%3s %d %d", month, &day, &year) != 3 ||
	    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second) != 3)
		return 0;
	for (i = 0; i < 12 && strcmp(month, months[i]); i++);
	if (i == 12)
		return 0;
	value.tm_year = year - 1900;
	value.tm_mon = i;
	value.tm_mday = day;
	value.tm_hour = hour;
	value.tm_min = minute;
	value.tm_sec = second;
	value.tm_isdst = -1;
	return mktime(&value);
#endif
}

static void ensure_parent(const char *path)
{
	char copy[256];
	char *slash;

	snprintf(copy, sizeof(copy), "%s", path);
	slash = strrchr(copy, '/');
	if (slash && slash != copy) {
		*slash = '\0';
		mkdir(copy, 0755);
	}
}

int time_state_save(const char *path, time_t value)
{
	char temporary[300];
	FILE *file;

	if (!path || !*path || value < VALID_EPOCH)
		return -EINVAL;
	ensure_parent(path);
	if (snprintf(temporary, sizeof(temporary), "%s.tmp", path) >= (int)sizeof(temporary))
		return -ENAMETOOLONG;
	file = fopen(temporary, "w");
	if (!file)
		return -errno;
	fprintf(file, "version=1\nepoch=%lld\n", (long long)value);
	if (fflush(file) || fsync(fileno(file))) { int error = errno; fclose(file); return -error; }
	if (fclose(file)) return -errno;
	if (rename(temporary, path))
		return -errno;
	return 0;
}

enum eink_time_quality time_state_restore(const char *path)
{
	FILE *file;
	char line[96];
	long long saved = 0;
	time_t now = time(NULL);
	time_t fallback = build_epoch();

	file = fopen(path, "r");
	if (file) {
		while (fgets(line, sizeof(line), file))
			if (sscanf(line, "epoch=%lld", &saved) == 1)
				break;
		fclose(file);
	}
	if (saved >= VALID_EPOCH && (time_t)saved > fallback)
		fallback = (time_t)saved;
	/* A battery-less RTC may contain a plausible but stale date. */
	quality = now >= VALID_EPOCH && now >= fallback ?
		EINK_TIME_ACCURATE : EINK_TIME_INVALID;
	if (quality == EINK_TIME_ACCURATE)
		return quality;
	if (fallback >= VALID_EPOCH) {
		struct timeval value = {fallback, 0};
		if (!settimeofday(&value, NULL)) {
			quality = EINK_TIME_RESTORED;
			time_state_save(path, fallback);
		}
	}
	return quality;
}

enum eink_time_quality time_state_quality(void)
{
	return quality;
}

int time_state_set(const char *path, time_t value)
{
	struct timeval tv = {value, 0};
	if (value < VALID_EPOCH || settimeofday(&tv, NULL))
		return -errno;
	quality = EINK_TIME_ACCURATE;
	return time_state_save(path, value);
}
