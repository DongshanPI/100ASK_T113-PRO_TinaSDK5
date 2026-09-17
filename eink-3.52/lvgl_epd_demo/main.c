#include "epd_disp.h"
#include "input.h"
#include "reader.h"
#include "settings.h"
#include "system_stats.h"
#include "ui.h"
#include "wifi_scan.h"

#include "lvgl.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

static void stop_handler(int signal_number)
{
	(void)signal_number;
	running = 0;
}

static time_t monotonic_seconds(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec;
}

static unsigned int active_selection(enum desktop_screen screen,
				     unsigned int launcher_selection,
				     unsigned int settings_selection,
				     const struct reader_state *reader)
{
	if (screen == DESKTOP_SETTINGS)
		return settings_selection;
	if (screen == DESKTOP_READER)
		return reader->selection;
	return launcher_selection;
}

static int refresh_desktop(enum desktop_screen screen, unsigned int selection,
			   int calendar_month_offset,
			   const struct dashboard_settings *settings,
			   enum epd_100ask_refresh_mode mode,
			   struct system_stats *stats,
			   struct epd_100ask_info *info,
			   const struct reader_state *reader,
			   const struct wifi_scan_state *wifi_scan)
{
	int result;

	system_stats_collect(stats);
	epd_disp_set_mode(mode);
	memset(info, 0, sizeof(*info));
	epd_disp_get_info(info);
	desktop_ui_render(screen, selection, calendar_month_offset, settings, stats,
			  info, reader, wifi_scan, epd_disp_last_error());
	result = epd_disp_render_now();
	epd_disp_get_info(info);
	return result;
}

static void cycle_refresh_interval(struct dashboard_settings *settings)
{
	if (settings->refresh_interval_sec <= 30)
		settings->refresh_interval_sec = 60;
	else if (settings->refresh_interval_sec <= 60)
		settings->refresh_interval_sec = 300;
	else
		settings->refresh_interval_sec = 30;
}

static void cycle_full_refresh(struct dashboard_settings *settings)
{
	if (settings->full_refresh_every <= 5)
		settings->full_refresh_every = 10;
	else if (settings->full_refresh_every <= 10)
		settings->full_refresh_every = 20;
	else
		settings->full_refresh_every = 5;
}

static void save_settings(const struct dashboard_settings *settings)
{
	if (settings_save(settings, DASHBOARD_CONFIG_PATH))
		fprintf(stderr, "Unable to save %s\n", DASHBOARD_CONFIG_PATH);
}

static enum desktop_screen adjacent_app(enum desktop_screen screen,
					enum dashboard_key_action action)
{
	int index = (int)screen - 1;

	index += action == DASH_KEY_NEXT ? 1 : DESKTOP_APP_COUNT - 1;
	index %= DESKTOP_APP_COUNT;
	return (enum desktop_screen)(index + 1);
}

static void prepare_app(enum desktop_screen screen, struct reader_state *reader)
{
	if (screen == DESKTOP_READER && !reader->opened)
		reader_scan(reader);
}

int main(void)
{
	struct dashboard_settings settings;
	struct dashboard_input *input;
	struct system_stats stats;
	struct epd_100ask_info epd_info;
	struct reader_state reader;
	struct wifi_scan_state wifi_scan;
	enum desktop_screen screen = DESKTOP_LAUNCHER;
	unsigned int launcher_selection = 0;
	unsigned int settings_selection = 0;
	unsigned int partial_since_full = 0;
	int calendar_month_offset = 0;
	time_t next_refresh;
	time_t last_full_refresh;

	signal(SIGINT, stop_handler);
	signal(SIGTERM, stop_handler);
	settings_load(&settings, DASHBOARD_CONFIG_PATH);
	reader_init(&reader);
	wifi_scan_init(&wifi_scan);

	lv_init();
	if (!epd_disp_init())
		return 1;
	desktop_ui_init();
	input = dashboard_input_open(&settings);

	refresh_desktop(screen, launcher_selection, calendar_month_offset, &settings,
			EPD_100ASK_REFRESH_GC, &stats, &epd_info, &reader, &wifi_scan);
	last_full_refresh = monotonic_seconds();
	next_refresh = last_full_refresh + settings.refresh_interval_sec;

	while (running) {
		enum dashboard_key_action action = dashboard_input_poll(input);
		time_t now = monotonic_seconds();
		int refresh = 0;
		enum epd_100ask_refresh_mode mode = EPD_100ASK_REFRESH_DU;

		if (wifi_scan_poll(&wifi_scan) && screen == DESKTOP_NETWORK) {
			refresh = 1;
			mode = EPD_100ASK_REFRESH_GC;
		}

		if (screen == DESKTOP_LAUNCHER) {
			if (action == DASH_KEY_PREVIOUS) {
				launcher_selection = (launcher_selection + DESKTOP_APP_COUNT - 1) %
					DESKTOP_APP_COUNT;
				refresh = 1;
			} else if (action == DASH_KEY_NEXT) {
				launcher_selection = (launcher_selection + 1) % DESKTOP_APP_COUNT;
				refresh = 1;
			} else if (action == DASH_KEY_CONFIRM) {
				screen = (enum desktop_screen)(launcher_selection + 1);
				prepare_app(screen, &reader);
				mode = EPD_100ASK_REFRESH_GC;
				refresh = 1;
			} else if (action == DASH_KEY_CONFIRM_LONG) {
				settings.language = settings.language == DASH_LANG_ZH ?
					DASH_LANG_EN : DASH_LANG_ZH;
				save_settings(&settings);
				mode = EPD_100ASK_REFRESH_GC;
				refresh = 1;
			}
		} else if (screen == DESKTOP_SETTINGS) {
			if (action == DASH_KEY_PREVIOUS) {
				settings_selection = (settings_selection + DESKTOP_SETTINGS_COUNT - 1) %
					DESKTOP_SETTINGS_COUNT;
				refresh = 1;
			} else if (action == DASH_KEY_NEXT) {
				settings_selection = (settings_selection + 1) % DESKTOP_SETTINGS_COUNT;
				refresh = 1;
			} else if (action == DASH_KEY_CONFIRM) {
				if (settings_selection == 0) {
					settings.language = settings.language == DASH_LANG_ZH ?
						DASH_LANG_EN : DASH_LANG_ZH;
					mode = EPD_100ASK_REFRESH_GC;
				} else if (settings_selection == 1) {
					cycle_refresh_interval(&settings);
				} else if (settings_selection == 2) {
					cycle_full_refresh(&settings);
				} else {
					mode = EPD_100ASK_REFRESH_GC;
				}
				save_settings(&settings);
				refresh = 1;
			} else if (action == DASH_KEY_CONFIRM_LONG) {
				screen = DESKTOP_LAUNCHER;
				mode = EPD_100ASK_REFRESH_GC;
				refresh = 1;
			}
		} else if (screen == DESKTOP_CALENDAR) {
			if (action == DASH_KEY_PREVIOUS && calendar_month_offset > -1200) {
				calendar_month_offset--;
				mode = EPD_100ASK_REFRESH_GC;
				refresh = 1;
			} else if (action == DASH_KEY_NEXT && calendar_month_offset < 1200) {
				calendar_month_offset++;
				mode = EPD_100ASK_REFRESH_GC;
				refresh = 1;
			} else if (action == DASH_KEY_CONFIRM) {
				calendar_month_offset = 0;
				mode = EPD_100ASK_REFRESH_GC;
				refresh = 1;
			} else if (action == DASH_KEY_CONFIRM_LONG) {
				screen = DESKTOP_LAUNCHER;
				mode = EPD_100ASK_REFRESH_GC;
				refresh = 1;
			}
		} else if (screen == DESKTOP_READER) {
			if (!reader.opened && action == DASH_KEY_PREVIOUS && reader.file_count) {
				reader.selection = (reader.selection + reader.file_count - 1) %
					reader.file_count;
				refresh = 1;
			} else if (!reader.opened && action == DASH_KEY_NEXT && reader.file_count) {
				reader.selection = (reader.selection + 1) % reader.file_count;
				refresh = 1;
			} else if (!reader.opened && action == DASH_KEY_CONFIRM && reader.file_count) {
				if (!reader_open(&reader, reader.selection)) {
					mode = EPD_100ASK_REFRESH_GC;
					refresh = 1;
				}
			} else if (reader.opened && action == DASH_KEY_PREVIOUS) {
				if (reader_previous_page(&reader) > 0) {
					mode = EPD_100ASK_REFRESH_GC;
					refresh = 1;
				}
			} else if (reader.opened && action == DASH_KEY_NEXT) {
				if (reader_next_page(&reader) > 0) {
					mode = EPD_100ASK_REFRESH_GC;
					refresh = 1;
				}
			} else if (reader.opened && action == DASH_KEY_CONFIRM) {
				reader_close_book(&reader);
				mode = EPD_100ASK_REFRESH_GC;
				refresh = 1;
			} else if (action == DASH_KEY_CONFIRM_LONG) {
				reader_close_book(&reader);
				screen = DESKTOP_LAUNCHER;
				mode = EPD_100ASK_REFRESH_GC;
				refresh = 1;
			}
		} else if (screen == DESKTOP_NETWORK) {
			if (action == DASH_KEY_CONFIRM) {
				if (!wifi_scan_start(&wifi_scan))
					refresh = 1;
			} else if (action == DASH_KEY_CONFIRM_LONG) {
				screen = DESKTOP_LAUNCHER;
				mode = EPD_100ASK_REFRESH_GC;
				refresh = 1;
			} else if (action == DASH_KEY_PREVIOUS || action == DASH_KEY_NEXT) {
				screen = adjacent_app(screen, action);
				launcher_selection = (unsigned int)screen - 1;
				prepare_app(screen, &reader);
				mode = EPD_100ASK_REFRESH_GC;
				refresh = 1;
			}
		} else {
			if (action == DASH_KEY_PREVIOUS || action == DASH_KEY_NEXT) {
				screen = adjacent_app(screen, action);
				launcher_selection = (unsigned int)screen - 1;
				prepare_app(screen, &reader);
				mode = EPD_100ASK_REFRESH_GC;
				refresh = 1;
			} else if (action == DASH_KEY_CONFIRM) {
				refresh = 1;
			} else if (action == DASH_KEY_CONFIRM_LONG) {
				screen = DESKTOP_LAUNCHER;
				mode = EPD_100ASK_REFRESH_GC;
				refresh = 1;
			}
		}

		if (!refresh && now >= next_refresh) {
			refresh = 1;
			if (partial_since_full >= settings.full_refresh_every ||
			    now - last_full_refresh >= 1800)
				mode = EPD_100ASK_REFRESH_GC;
		}

		if (refresh) {
			unsigned int selection = active_selection(screen, launcher_selection,
							 settings_selection, &reader);

			if (!refresh_desktop(screen, selection, calendar_month_offset, &settings,
					     mode, &stats, &epd_info, &reader, &wifi_scan)) {
				if (mode == EPD_100ASK_REFRESH_GC) {
					partial_since_full = 0;
					last_full_refresh = now;
				} else {
					partial_since_full++;
				}
			}
			next_refresh = now + settings.refresh_interval_sec;
		}

		lv_timer_handler();
		usleep(20000);
	}

	wifi_scan_close(&wifi_scan);
	dashboard_input_close(input);
	epd_disp_close();
	return 0;
}
