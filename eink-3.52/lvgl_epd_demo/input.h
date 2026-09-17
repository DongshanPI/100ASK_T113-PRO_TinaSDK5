#ifndef DASHBOARD_INPUT_H
#define DASHBOARD_INPUT_H

#include "settings.h"

enum dashboard_key_action {
	DASH_KEY_NONE = 0,
	DASH_KEY_PREVIOUS,
	DASH_KEY_NEXT,
	DASH_KEY_CONFIRM,
	DASH_KEY_CONFIRM_LONG,
};

struct dashboard_input;

struct dashboard_input *dashboard_input_open(const struct dashboard_settings *settings);
enum dashboard_key_action dashboard_input_poll(struct dashboard_input *input);
void dashboard_input_close(struct dashboard_input *input);

#endif
