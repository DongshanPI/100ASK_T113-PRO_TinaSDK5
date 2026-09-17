#include "connect_client.h"
#include "epd_disp.h"
#include "input.h"
#include "reader.h"
#include "settings.h"
#include "system_stats.h"
#include "time_state.h"
#include "ui.h"

#include "lvgl.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;
static void stop_handler(int signal_number) { (void)signal_number; running = 0; }

static time_t monotonic_seconds(void)
{
	struct timespec value; clock_gettime(CLOCK_MONOTONIC, &value); return value.tv_sec;
}

static void save_settings(const struct dashboard_settings *settings)
{
	if (settings_save(settings, DASHBOARD_CONFIG_PATH)) fprintf(stderr, "Unable to save settings\n");
}

static void cycle_interval(struct dashboard_settings *settings)
{
	settings->refresh_interval_sec = settings->refresh_interval_sec <= 30 ? 60 : settings->refresh_interval_sec <= 60 ? 300 : 30;
}

static void cycle_full(struct dashboard_settings *settings)
{
	settings->full_refresh_every = settings->full_refresh_every <= 5 ? 10 : settings->full_refresh_every <= 10 ? 20 : 5;
}

static int render(enum desktop_screen screen, unsigned int selection, int month_offset,
		  const struct dashboard_settings *settings, enum epd_100ask_refresh_mode mode,
		  struct system_stats *stats, struct epd_100ask_info *info,
		  const struct reader_state *reader, const struct eink_connect_status *connect,
		  enum eink_time_quality quality)
{
	system_stats_collect(stats); epd_disp_set_mode(mode); memset(info, 0, sizeof(*info)); epd_disp_get_info(info);
	desktop_ui_render(screen, selection, month_offset, settings, stats, info, reader, connect, quality, epd_disp_last_error());
	return epd_disp_render_now();
}

static unsigned int current_selection(enum desktop_screen screen, unsigned int home,
		unsigned int connect, unsigned int more, unsigned int wifi,
		unsigned int bluetooth, unsigned int settings, const struct reader_state *reader)
{
	switch (screen) {
	case DESKTOP_HOME: return home;
	case DESKTOP_CONNECT: return connect;
	case DESKTOP_MORE: return more;
	case DESKTOP_WIFI: return wifi;
	case DESKTOP_BLUETOOTH: return bluetooth;
	case DESKTOP_SETTINGS: return settings;
	case DESKTOP_READER: return reader->selection;
	default: return 0;
	}
}

int main(void)
{
	struct dashboard_settings settings; struct dashboard_input *input;
	struct system_stats stats; struct epd_100ask_info epd_info; struct reader_state reader;
	struct eink_connect_status connect, updated;
	enum desktop_screen screen = DESKTOP_HOME; enum eink_time_quality time_quality;
	unsigned int home_selection = 0, connect_selection = 0, more_selection = 0;
	unsigned int wifi_selection = 0, bt_selection = 0, settings_selection = 0;
	unsigned int partial_since_full = 0; int calendar_month_offset = 0;
	time_t next_refresh, next_connect_poll, next_time_save, last_full_refresh;

	signal(SIGINT, stop_handler); signal(SIGTERM, stop_handler);
	settings_load(&settings, DASHBOARD_CONFIG_PATH); setenv("TZ", settings.timezone, 1); tzset();
	time_quality = time_state_restore(settings.time_state_path);
	reader_init(&reader, settings.reader_state_path); connect_status_defaults(&connect);
	connect_request(EINK_CMD_STATUS, 0, NULL, NULL, &connect);
	lv_init(); if (!epd_disp_init()) return 1; desktop_ui_init(); input = dashboard_input_open(&settings);
	render(screen, home_selection, calendar_month_offset, &settings, EPD_100ASK_REFRESH_GC, &stats, &epd_info, &reader, &connect, time_quality);
	last_full_refresh = monotonic_seconds(); next_refresh = last_full_refresh + settings.refresh_interval_sec;
	next_connect_poll = last_full_refresh + 2; next_time_save = last_full_refresh + 21600;

	while (running) {
		enum dashboard_key_action action = dashboard_input_poll(input);
		time_t now = monotonic_seconds(); int refresh = 0; enum epd_100ask_refresh_mode mode = EPD_100ASK_REFRESH_DU;

		if (now >= next_connect_poll) {
			connect_status_defaults(&updated);
			if (!connect_request(EINK_CMD_STATUS, 0, NULL, NULL, &updated)) {
				if (connect.pair_pending && connect.pair_input && updated.pair_pending && updated.pair_input) { updated.pair_passkey = connect.pair_passkey; updated.pair_cursor = connect.pair_cursor; }
				if (memcmp(&updated, &connect, sizeof(connect))) { connect = updated; refresh = 1; }
			}
			next_connect_poll = now + 2;
			if (connect.wifi_connected && time(NULL) > 1609459200) time_quality = EINK_TIME_ACCURATE;
		}
		if (now >= next_time_save) { time_state_save(settings.time_state_path, time(NULL)); next_time_save = now + 21600; }

		if (screen == DESKTOP_HOME) {
			if (action == DASH_KEY_PREVIOUS) { home_selection = (home_selection + HOME_ITEM_COUNT - 1) % HOME_ITEM_COUNT; refresh = 1; }
			else if (action == DASH_KEY_NEXT) { home_selection = (home_selection + 1) % HOME_ITEM_COUNT; refresh = 1; }
			else if (action == DASH_KEY_CONFIRM) { static const enum desktop_screen targets[] = {DESKTOP_CALENDAR,DESKTOP_READER,DESKTOP_CONNECT,DESKTOP_MORE}; screen = targets[home_selection]; if (screen == DESKTOP_READER) reader_scan(&reader); mode = EPD_100ASK_REFRESH_GC; refresh = 1; }
			else if (action == DASH_KEY_CONFIRM_LONG) { settings.language = settings.language == DASH_LANG_ZH ? DASH_LANG_EN : DASH_LANG_ZH; save_settings(&settings); mode = EPD_100ASK_REFRESH_GC; refresh = 1; }
		} else if (screen == DESKTOP_CALENDAR) {
			if (action == DASH_KEY_PREVIOUS) { calendar_month_offset--; mode = EPD_100ASK_REFRESH_GC; refresh = 1; }
			else if (action == DASH_KEY_NEXT) { calendar_month_offset++; mode = EPD_100ASK_REFRESH_GC; refresh = 1; }
			else if (action == DASH_KEY_CONFIRM) { calendar_month_offset = 0; mode = EPD_100ASK_REFRESH_GC; refresh = 1; }
			else if (action == DASH_KEY_CONFIRM_LONG) { screen = DESKTOP_HOME; mode = EPD_100ASK_REFRESH_GC; refresh = 1; }
		} else if (screen == DESKTOP_READER) {
			if (!reader.opened) {
				if (action == DASH_KEY_PREVIOUS && reader.file_count) { reader.selection = (reader.selection + reader.file_count - 1) % reader.file_count; refresh = 1; }
				else if (action == DASH_KEY_NEXT && reader.file_count) { reader.selection = (reader.selection + 1) % reader.file_count; refresh = 1; }
				else if (action == DASH_KEY_CONFIRM && reader.file_count) { if (!reader_open(&reader, reader.selection)) { mode = EPD_100ASK_REFRESH_GC; refresh = 1; } }
				else if (action == DASH_KEY_CONFIRM_LONG) { screen = DESKTOP_HOME; mode = EPD_100ASK_REFRESH_GC; refresh = 1; }
			} else if (reader.action_menu) {
				if (action == DASH_KEY_PREVIOUS) { reader.action_selection = (reader.action_selection + 3) % 4; refresh = 1; }
				else if (action == DASH_KEY_NEXT) { reader.action_selection = (reader.action_selection + 1) % 4; refresh = 1; }
				else if (action == DASH_KEY_CONFIRM) {
					if (reader.action_selection == 0) reader_close_book(&reader);
					else if (reader.action_selection == 1) { reader_toggle_bookmark(&reader); reader.action_menu = 0; }
					else if (reader.action_selection == 2) { reader_jump_bookmark(&reader); reader.action_menu = 0; }
					else { reader_close_book(&reader); reader_scan(&reader); }
					mode = EPD_100ASK_REFRESH_GC; refresh = 1;
				} else if (action == DASH_KEY_CONFIRM_LONG) { reader.action_menu = 0; refresh = 1; }
			} else {
				if (action == DASH_KEY_PREVIOUS && reader_previous_page(&reader) > 0) { mode = EPD_100ASK_REFRESH_GC; refresh = 1; }
				else if (action == DASH_KEY_NEXT && reader_next_page(&reader) > 0) { mode = EPD_100ASK_REFRESH_GC; refresh = 1; }
				else if (action == DASH_KEY_CONFIRM) { reader.action_menu = 1; reader.action_selection = 0; mode = EPD_100ASK_REFRESH_GC; refresh = 1; }
				else if (action == DASH_KEY_CONFIRM_LONG) { reader_close_book(&reader); mode = EPD_100ASK_REFRESH_GC; refresh = 1; }
			}
		} else if (screen == DESKTOP_CONNECT || screen == DESKTOP_MORE) {
			unsigned int *selection = screen == DESKTOP_CONNECT ? &connect_selection : &more_selection;
			unsigned int count = screen == DESKTOP_CONNECT ? CONNECT_ITEM_COUNT : MORE_ITEM_COUNT;
			if (action == DASH_KEY_PREVIOUS) { *selection = (*selection + count - 1) % count; refresh = 1; }
			else if (action == DASH_KEY_NEXT) { *selection = (*selection + 1) % count; refresh = 1; }
			else if (action == DASH_KEY_CONFIRM) {
				if (screen == DESKTOP_CONNECT) screen = *selection ? DESKTOP_BLUETOOTH : DESKTOP_WIFI;
				else { static const enum desktop_screen targets[] = {DESKTOP_RESOURCES,DESKTOP_DIAGNOSTICS,DESKTOP_SETTINGS,DESKTOP_ABOUT}; screen = targets[*selection]; }
				mode = EPD_100ASK_REFRESH_GC; refresh = 1;
			} else if (action == DASH_KEY_CONFIRM_LONG) { screen = DESKTOP_HOME; mode = EPD_100ASK_REFRESH_GC; refresh = 1; }
		} else if (screen == DESKTOP_WIFI) {
			if (action == DASH_KEY_PREVIOUS) { wifi_selection = (wifi_selection + 2) % 3; refresh = 1; }
			else if (action == DASH_KEY_NEXT) { wifi_selection = (wifi_selection + 1) % 3; refresh = 1; }
			else if (action == DASH_KEY_CONFIRM) {
				enum eink_connect_command command = wifi_selection == 0 ? (connect.wifi_mode == EINK_WIFI_PORTAL ? EINK_CMD_WIFI_CANCEL : EINK_CMD_WIFI_PORTAL) : wifi_selection == 1 ? EINK_CMD_WIFI_SCAN : EINK_CMD_WIFI_DISCONNECT;
				connect_request(command, 0, NULL, NULL, &connect); mode = EPD_100ASK_REFRESH_GC; refresh = 1;
			} else if (action == DASH_KEY_CONFIRM_LONG) { screen = DESKTOP_CONNECT; mode = EPD_100ASK_REFRESH_GC; refresh = 1; }
		} else if (screen == DESKTOP_BLUETOOTH) {
			unsigned int count = connect.bt_count + 2; if (count > EINK_CONNECT_MAX_BT + 2) count = EINK_CONNECT_MAX_BT + 2;
			if (connect.pair_pending) {
				if (connect.pair_input && (action == DASH_KEY_PREVIOUS || action == DASH_KEY_NEXT)) {
					static const unsigned int powers[] = {100000,10000,1000,100,10,1}; unsigned int power = powers[connect.pair_cursor]; unsigned int digit = (connect.pair_passkey / power) % 10;
					digit = action == DASH_KEY_NEXT ? (digit + 1) % 10 : (digit + 9) % 10; connect.pair_passkey = connect.pair_passkey - ((connect.pair_passkey / power) % 10) * power + digit * power; refresh = 1;
				} else if (action == DASH_KEY_CONFIRM && connect.pair_input && connect.pair_cursor < 5) { connect.pair_cursor++; refresh = 1; }
				else if (action == DASH_KEY_CONFIRM) { char code[8]; snprintf(code, sizeof(code), "%06u", connect.pair_passkey); connect_request(EINK_CMD_BT_CONFIRM, 1, NULL, code, &connect); mode = EPD_100ASK_REFRESH_GC; refresh = 1; }
				else if (action == DASH_KEY_CONFIRM_LONG) { connect_request(EINK_CMD_BT_CONFIRM, 0, NULL, NULL, &connect); mode = EPD_100ASK_REFRESH_GC; refresh = 1; }
			} else if (action == DASH_KEY_PREVIOUS) { bt_selection = (bt_selection + count - 1) % count; refresh = 1; }
			else if (action == DASH_KEY_NEXT) { bt_selection = (bt_selection + 1) % count; refresh = 1; }
			else if (action == DASH_KEY_CONFIRM) {
				if (bt_selection == 0) connect_request(EINK_CMD_BT_POWER, connect.bt_state < EINK_BT_ON, NULL, NULL, &connect);
				else if (bt_selection == 1) connect_request(EINK_CMD_BT_SCAN, 0, NULL, NULL, &connect);
				else { unsigned int index = bt_selection - 2; enum eink_connect_command command = connect.bt[index].connected ? EINK_CMD_BT_DISCONNECT : connect.bt[index].paired ? EINK_CMD_BT_CONNECT : EINK_CMD_BT_PAIR; connect_request(command, (int)index, NULL, NULL, &connect); }
				mode = EPD_100ASK_REFRESH_GC; refresh = 1;
			} else if (action == DASH_KEY_CONFIRM_LONG) { screen = DESKTOP_CONNECT; mode = EPD_100ASK_REFRESH_GC; refresh = 1; }
		} else if (screen == DESKTOP_SETTINGS) {
			if (action == DASH_KEY_PREVIOUS) { settings_selection = (settings_selection + DESKTOP_SETTINGS_COUNT - 1) % DESKTOP_SETTINGS_COUNT; refresh = 1; }
			else if (action == DASH_KEY_NEXT) { settings_selection = (settings_selection + 1) % DESKTOP_SETTINGS_COUNT; refresh = 1; }
			else if (action == DASH_KEY_CONFIRM) { if (!settings_selection) settings.language = settings.language == DASH_LANG_ZH ? DASH_LANG_EN : DASH_LANG_ZH; else if (settings_selection == 1) cycle_interval(&settings); else if (settings_selection == 2) cycle_full(&settings); else mode = EPD_100ASK_REFRESH_GC; save_settings(&settings); refresh = 1; }
			else if (action == DASH_KEY_CONFIRM_LONG) { screen = DESKTOP_MORE; mode = EPD_100ASK_REFRESH_GC; refresh = 1; }
		} else if (action == DASH_KEY_CONFIRM_LONG) { screen = DESKTOP_MORE; mode = EPD_100ASK_REFRESH_GC; refresh = 1; }

		if (!refresh && now >= next_refresh) { refresh = 1; if (now - last_full_refresh >= 1800) mode = EPD_100ASK_REFRESH_GC; }
		if (refresh) {
			unsigned int selection = current_selection(screen, home_selection, connect_selection, more_selection, wifi_selection, bt_selection, settings_selection, &reader);
			if (partial_since_full >= settings.full_refresh_every) mode = EPD_100ASK_REFRESH_GC;
			if (!render(screen, selection, calendar_month_offset, &settings, mode, &stats, &epd_info, &reader, &connect, time_quality)) {
				if (mode == EPD_100ASK_REFRESH_GC) { partial_since_full = 0; last_full_refresh = now; } else partial_since_full++;
			}
			next_refresh = now + settings.refresh_interval_sec;
		}
		lv_timer_handler(); usleep(20000);
	}
	time_state_save(settings.time_state_path, time(NULL)); reader_destroy(&reader); dashboard_input_close(input); epd_disp_close(); return 0;
}
