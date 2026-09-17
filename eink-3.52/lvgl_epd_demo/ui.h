#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include "epd_disp.h"
#include "reader.h"
#include "settings.h"
#include "system_stats.h"
#include "wifi_scan.h"

#define DESKTOP_APP_COUNT 8
#define DESKTOP_SETTINGS_COUNT 4

enum desktop_screen {
	DESKTOP_LAUNCHER = 0,
	DESKTOP_OVERVIEW,
	DESKTOP_CALENDAR,
	DESKTOP_READER,
	DESKTOP_NETWORK,
	DESKTOP_RESOURCES,
	DESKTOP_DIAGNOSTICS,
	DESKTOP_SETTINGS,
	DESKTOP_ABOUT,
};

void desktop_ui_init(void);
void desktop_ui_render(enum desktop_screen screen, unsigned int selection,
		       int calendar_month_offset,
		       const struct dashboard_settings *settings,
		       const struct system_stats *stats,
		       const struct epd_100ask_info *epd_info,
		       const struct reader_state *reader,
		       const struct wifi_scan_state *wifi_scan,
		       int last_refresh_error);

#endif
