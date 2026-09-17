#ifndef EINK_TIME_STATE_H
#define EINK_TIME_STATE_H

#include <time.h>

enum eink_time_quality {
	EINK_TIME_INVALID = 0,
	EINK_TIME_RESTORED,
	EINK_TIME_ACCURATE,
};

enum eink_time_quality time_state_restore(const char *path);
enum eink_time_quality time_state_quality(void);
int time_state_save(const char *path, time_t value);
int time_state_set(const char *path, time_t value);

#endif
