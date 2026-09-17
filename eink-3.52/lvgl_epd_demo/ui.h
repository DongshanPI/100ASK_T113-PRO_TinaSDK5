#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include "connect_proto.h"
#include "epd_disp.h"
#include "reader.h"
#include "settings.h"
#include "system_stats.h"
#include "time_state.h"

#define HOME_ITEM_COUNT 4
#define CONNECT_ITEM_COUNT 2
#define MORE_ITEM_COUNT 4
#define DESKTOP_SETTINGS_COUNT 4

enum desktop_screen {
	DESKTOP_HOME = 0,
	DESKTOP_CALENDAR,
	DESKTOP_READER,
	DESKTOP_CONNECT,
	DESKTOP_MORE,
	DESKTOP_WIFI,
	DESKTOP_BLUETOOTH,
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
		       const struct eink_connect_status *connect,
		       enum eink_time_quality time_quality,
		       int last_refresh_error);

#endif
