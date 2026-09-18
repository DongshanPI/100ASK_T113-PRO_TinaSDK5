#include "settings.h"

#include <linux/input-event-codes.h>
#include <stdio.h>

int main(void)
{
	struct dashboard_settings settings;

	settings_defaults(&settings);
	if (settings.key_previous != KEY_VOLUMEUP ||
	    settings.key_next != KEY_MODE ||
	    settings.key_next_alt != KEY_ENTER ||
	    settings.key_confirm != KEY_PAUSE ||
	    settings.key_confirm_alt != KEY_VOLUMEDOWN) {
		fprintf(stderr, "settings_test: incorrect K1/K2/K3 mapping\n");
		return 1;
	}
	puts("settings_test: PASS");
	return 0;
}
