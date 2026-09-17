#ifndef DASHBOARD_SETTINGS_H
#define DASHBOARD_SETTINGS_H

#define DASHBOARD_CONFIG_PATH "/etc/eink-dashboard.conf"
#define EINK_DEFAULT_TIME_STATE "/etc/eink-os/time.state"
#define EINK_DEFAULT_READER_STATE "/etc/eink-os/reader.state"

enum dashboard_language {
	DASH_LANG_ZH = 0,
	DASH_LANG_EN = 1,
};

struct dashboard_settings {
	enum dashboard_language language;
	unsigned int refresh_interval_sec;
	unsigned int full_refresh_every;
	unsigned int key_previous;
	unsigned int key_next;
	unsigned int key_next_alt;
	unsigned int key_confirm;
	unsigned int key_confirm_alt;
	unsigned int provision_timeout_sec;
	char timezone[48];
	char time_state_path[128];
	char reader_state_path[128];
};

void settings_defaults(struct dashboard_settings *settings);
int settings_load(struct dashboard_settings *settings, const char *path);
int settings_save(const struct dashboard_settings *settings, const char *path);

#endif
