#include "ui.h"

#include "lunar.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

LV_FONT_DECLARE(font_cjk_16);

static lv_style_t root_style, text_style, line_style, row_style, selected_style;

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width,
		       const lv_font_t *font, lv_text_align_t align)
{
	lv_obj_t *object = lv_label_create(parent);
	lv_obj_add_style(object, &text_style, 0); lv_obj_set_style_text_font(object, font, 0);
	lv_obj_set_pos(object, x, y); lv_obj_set_width(object, width);
	lv_obj_set_style_text_align(object, align, 0); lv_label_set_long_mode(object, LV_LABEL_LONG_CLIP);
	lv_label_set_text(object, text); return object;
}

static void rule(lv_obj_t *parent, int y)
{
	lv_obj_t *object = lv_obj_create(parent); lv_obj_add_style(object, &line_style, 0);
	lv_obj_set_pos(object, 8, y); lv_obj_set_size(object, 224, 1);
}

static void header(lv_obj_t *screen, const char *title, const struct eink_connect_status *connect)
{
	char status[32]; time_t now = time(NULL); struct tm value; char clock[8];
	localtime_r(&now, &value); strftime(clock, sizeof(clock), "%H:%M", &value);
	snprintf(status, sizeof(status), "%s  W%c B%c", clock,
		 connect->wifi_connected ? '+' : '-', connect->bt_state >= EINK_BT_ON ? '+' : '-');
	label(screen, title, 8, 6, 130, &font_cjk_16, LV_TEXT_ALIGN_LEFT);
	label(screen, status, 138, 7, 94, &lv_font_montserrat_14, LV_TEXT_ALIGN_RIGHT); rule(screen, 31);
}

static void footer(lv_obj_t *screen, const char *text)
{
	rule(screen, 329); label(screen, text, 4, 337, 232, &font_cjk_16, LV_TEXT_ALIGN_CENTER);
}

static void menu_row(lv_obj_t *screen, int y, const char *name, const char *value, int selected)
{
	lv_obj_t *row = lv_obj_create(screen), *left, *right;
	lv_obj_add_style(row, selected ? &selected_style : &row_style, 0);
	lv_obj_set_pos(row, 8, y); lv_obj_set_size(row, 224, 43); lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
	left = label(row, name, 9, 12, value ? 130 : 206, &font_cjk_16, LV_TEXT_ALIGN_LEFT);
	if (selected) lv_obj_set_style_text_color(left, lv_color_white(), 0);
	if (value) { right = label(row, value, 139, 12, 76, &font_cjk_16, LV_TEXT_ALIGN_RIGHT); if (selected) lv_obj_set_style_text_color(right, lv_color_white(), 0); }
}

static void render_home(lv_obj_t *screen, unsigned int selection,
			const struct dashboard_settings *settings,
			const struct eink_connect_status *connect,
			enum eink_time_quality quality)
{
	static const char *zh[] = {"日历", "阅读", "连接", "更多"};
	static const char *en[] = {"CALENDAR", "READER", "CONNECT", "MORE"};
	static const char *week_zh[] = {"星期日","星期一","星期二","星期三","星期四","星期五","星期六"};
	char clock[8], month[32], day[4], detail[80], status[32], lunar_text[64];
	struct lunar_date lunar; time_t now = time(NULL); struct tm value; unsigned int i;
	localtime_r(&now, &value); strftime(clock, sizeof(clock), "%H:%M", &value);
	snprintf(status, sizeof(status), "WiFi%c  BT%c", connect->wifi_connected ? '+' : '-', connect->bt_state >= EINK_BT_ON ? '+' : '-');
	label(screen, clock, 8, 7, 110, &lv_font_montserrat_28, LV_TEXT_ALIGN_LEFT);
	label(screen, status, 120, 14, 112, &lv_font_montserrat_14, LV_TEXT_ALIGN_RIGHT);
	snprintf(day, sizeof(day), "%d", value.tm_mday); label(screen, day, 8, 52, 72, &lv_font_montserrat_48, LV_TEXT_ALIGN_LEFT);
	if (settings->language == DASH_LANG_EN) strftime(month, sizeof(month), "%B %Y", &value);
	else snprintf(month, sizeof(month), "%d年%d月", value.tm_year + 1900, value.tm_mon + 1);
	label(screen, month, 82, 61, 150, settings->language == DASH_LANG_EN ? &lv_font_montserrat_20 : &font_cjk_16, LV_TEXT_ALIGN_RIGHT);
	if (!lunar_from_solar(value.tm_year + 1900, value.tm_mon + 1, value.tm_mday, &lunar)) lunar_format(&lunar, lunar_text, sizeof(lunar_text)); else strcpy(lunar_text, "农历--");
	if (settings->language == DASH_LANG_EN) strftime(detail, sizeof(detail), "%A  %Y-%m-%d", &value);
	else snprintf(detail, sizeof(detail), "%s    %s", week_zh[value.tm_wday], lunar_text);
	label(screen, detail, 8, 112, 224, &font_cjk_16, LV_TEXT_ALIGN_LEFT);
	if (quality != EINK_TIME_ACCURATE) label(screen, quality == EINK_TIME_RESTORED ? "等待网络校时" : "时间未设置", 8, 137, 224, &font_cjk_16, LV_TEXT_ALIGN_RIGHT);
	rule(screen, 163);
	for (i = 0; i < HOME_ITEM_COUNT; i++) menu_row(screen, 171 + (int)i * 39, settings->language == DASH_LANG_EN ? en[i] : zh[i], NULL, i == selection);
	footer(screen, settings->language == DASH_LANG_EN ? "K1/K3 SELECT  K2 OPEN  HOLD:LANG" : "K1/K3 选择  K2 打开  长按切换语言");
}

static int leap_year(int year) { return (!(year % 4) && year % 100) || !(year % 400); }
static int month_days(int year, int month) { static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31}; return days[month] + (month == 1 && leap_year(year)); }

static void render_calendar(lv_obj_t *screen, int offset, enum dashboard_language language)
{
	static const char *week[] = {"日","一","二","三","四","五","六"};
	time_t now = time(NULL); struct tm today, shown; char title[40], text[16], lunar_text[64];
	struct lunar_date lunar; int year, month, count, day, column, row;
	localtime_r(&now, &today); shown = today; shown.tm_mday = 1; shown.tm_mon += offset; mktime(&shown);
	year = shown.tm_year + 1900; month = shown.tm_mon; count = month_days(year, month);
	snprintf(title, sizeof(title), language == DASH_LANG_EN ? "%04d / %02d" : "%04d年%02d月", year, month + 1);
	label(screen, title, 8, 42, 224, language == DASH_LANG_EN ? &lv_font_montserrat_20 : &font_cjk_16, LV_TEXT_ALIGN_CENTER);
	for (column = 0; column < 7; column++) label(screen, week[column], 9 + column * 32, 73, 30, &font_cjk_16, LV_TEXT_ALIGN_CENTER);
	rule(screen, 95);
	for (day = 1; day <= count; day++) {
		lv_obj_t *day_label; int selected = !offset && day == today.tm_mday;
		column = (shown.tm_wday + day - 1) % 7; row = (shown.tm_wday + day - 1) / 7; snprintf(text, sizeof(text), "%d", day);
		if (selected) {
			lv_obj_t *box = lv_obj_create(screen); lv_obj_add_style(box, &selected_style, 0); lv_obj_set_pos(box, 11 + column * 32, 102 + row * 34); lv_obj_set_size(box, 27, 27);
			day_label = label(box, text, 0, 6, 25, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER); lv_obj_set_style_text_color(day_label, lv_color_white(), 0);
		} else label(screen, text, 10 + column * 32, 108 + row * 34, 28, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
	}
	if (!lunar_from_solar(today.tm_year + 1900, today.tm_mon + 1, today.tm_mday, &lunar)) {
		const char *holiday = calendar_holiday(today.tm_year + 1900, today.tm_mon + 1, today.tm_mday, &lunar);
		lunar_format(&lunar, lunar_text, sizeof(lunar_text)); if (holiday) { strncat(lunar_text, " · ", sizeof(lunar_text) - strlen(lunar_text) - 1); strncat(lunar_text, holiday, sizeof(lunar_text) - strlen(lunar_text) - 1); }
		label(screen, lunar_text, 8, 303, 224, &font_cjk_16, LV_TEXT_ALIGN_CENTER);
	}
}

static void render_reader(lv_obj_t *screen, const struct reader_state *reader, enum dashboard_language language)
{
	if (reader->opened) {
		char page[24]; lv_obj_t *content; unsigned int i;
		label(screen, reader->files[reader->current_file].name, 8, 39, 174, &font_cjk_16, LV_TEXT_ALIGN_LEFT);
		snprintf(page, sizeof(page), "%u%s", reader->page_index + 1, reader->bookmark_set && reader->bookmark_offset == reader->page_offsets[reader->page_index] ? " *" : "");
		label(screen, page, 184, 39, 47, &lv_font_montserrat_14, LV_TEXT_ALIGN_RIGHT); rule(screen, 63);
		content = label(screen, reader->page[0] ? reader->page : "--", 9, 69, 222, &font_cjk_16, LV_TEXT_ALIGN_LEFT); lv_label_set_long_mode(content, LV_LABEL_LONG_WRAP); lv_obj_set_height(content, 252);
		if (reader->action_menu) {
			static const char *actions[] = {"返回书库", "切换书签", "跳到书签", "重新扫描"};
			lv_obj_t *panel = lv_obj_create(screen); lv_obj_add_style(panel, &row_style, 0); lv_obj_set_pos(panel, 23, 96); lv_obj_set_size(panel, 194, 188);
			for (i = 0; i < 4; i++) { lv_obj_t *row = lv_obj_create(panel), *item; lv_obj_add_style(row, i == reader->action_selection ? &selected_style : &row_style, 0); lv_obj_set_pos(row, 5, 5 + i * 43); lv_obj_set_size(row, 184, 39); item = label(row, actions[i], 8, 10, 168, &font_cjk_16, LV_TEXT_ALIGN_LEFT); if (i == reader->action_selection) lv_obj_set_style_text_color(item, lv_color_white(), 0); }
		}
		return;
	}
	if (!reader->file_count) label(screen, language == DASH_LANG_EN ? "NO TXT FILES\n\nCopy to /mnt/UDISK/books" : "没有 TXT 文档\n\n请复制到 /mnt/UDISK/books", 10, 90, 220, &font_cjk_16, LV_TEXT_ALIGN_CENTER);
	else {
		unsigned int first = reader->selection > 5 ? reader->selection - 5 : 0, i;
		for (i = first; i < reader->file_count && i < first + 6; i++) menu_row(screen, 40 + (int)(i - first) * 45, reader->files[i].name, NULL, i == reader->selection);
	}
}

static void render_connect_menu(lv_obj_t *screen, unsigned int selection, const struct eink_connect_status *connect, enum dashboard_language language)
{
	menu_row(screen, 54, language == DASH_LANG_EN ? "WiFi / PHONE SETUP" : "WiFi / 手机配网", connect->wifi_connected ? connect->wifi_ssid : (connect->wifi_mode == EINK_WIFI_PORTAL ? "配网中" : "未连接"), selection == 0);
	menu_row(screen, 105, language == DASH_LANG_EN ? "BLUETOOTH" : "蓝牙配对", connect->bt_state == EINK_BT_UNAVAILABLE ? "不可用" : connect->bt_state >= EINK_BT_ON ? "已开启" : "已关闭", selection == 1);
	label(screen, language == DASH_LANG_EN ? "Phone setup creates a private hotspot.\nSTA resumes automatically on cancel or timeout." : "手机配网会临时创建独立热点。\n取消、超时或失败后自动恢复联网。", 10, 184, 220, &font_cjk_16, LV_TEXT_ALIGN_LEFT);
}

static void render_wifi(lv_obj_t *screen, unsigned int selection, const struct eink_connect_status *connect, enum dashboard_language language)
{
	char timer[32]; unsigned int i;
	menu_row(screen, 40, connect->wifi_mode == EINK_WIFI_PORTAL ? "取消手机配网" : "开始手机配网", connect->wifi_mode == EINK_WIFI_PORTAL ? "运行中" : "", selection == 0);
	menu_row(screen, 84, "扫描附近网络", connect->wifi_count ? "已完成" : "", selection == 1);
	menu_row(screen, 128, "断开当前网络", connect->wifi_connected ? connect->wifi_ssid : "未连接", selection == 2);
	if (connect->wifi_mode == EINK_WIFI_PORTAL) {
		snprintf(timer, sizeof(timer), "%u:%02u", connect->portal_seconds_left / 60, connect->portal_seconds_left % 60);
		label(screen, "热点", 10, 183, 45, &font_cjk_16, LV_TEXT_ALIGN_LEFT); label(screen, connect->portal_ssid, 56, 183, 174, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
		label(screen, "密码", 10, 211, 45, &font_cjk_16, LV_TEXT_ALIGN_LEFT); label(screen, connect->portal_password, 56, 211, 174, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
		label(screen, "192.168.5.1", 10, 239, 150, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT); label(screen, timer, 160, 239, 70, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
	} else {
		for (i = 0; i < connect->wifi_count && i < 4; i++) { char row[80]; snprintf(row, sizeof(row), "%s  %ddBm", connect->wifi[i].ssid, connect->wifi[i].signal); label(screen, row, 10, 184 + (int)i * 27, 220, &font_cjk_16, LV_TEXT_ALIGN_LEFT); }
	}
	label(screen, connect->message, 10, 300, 220, &font_cjk_16, LV_TEXT_ALIGN_CENTER); (void)language;
}

static void render_bluetooth(lv_obj_t *screen, unsigned int selection, const struct eink_connect_status *connect)
{
	unsigned int i;
	if (connect->pair_pending) {
		char passkey[32], hint[64]; snprintf(passkey, sizeof(passkey), "%06u", connect->pair_passkey);
		if (connect->pair_input) snprintf(hint, sizeof(hint), "K1/K3 修改第 %u 位 · K2 下一位", connect->pair_cursor + 1);
		else snprintf(hint, sizeof(hint), "K2 接受 · 长按拒绝");
		label(screen, connect->pair_input ? "输入蓝牙 PIN / PASSKEY" : "确认蓝牙配对", 8, 63, 224, &font_cjk_16, LV_TEXT_ALIGN_CENTER); label(screen, connect->pair_device, 8, 105, 224, &font_cjk_16, LV_TEXT_ALIGN_CENTER); label(screen, passkey, 8, 151, 224, &lv_font_montserrat_28, LV_TEXT_ALIGN_CENTER); label(screen, hint, 8, 221, 224, &font_cjk_16, LV_TEXT_ALIGN_CENTER); return;
	}
	menu_row(screen, 40, connect->bt_state >= EINK_BT_ON ? "关闭蓝牙" : "开启蓝牙", connect->bt_state == EINK_BT_UNAVAILABLE ? "不可用" : "", selection == 0);
	menu_row(screen, 84, "扫描设备", connect->bt_state == EINK_BT_SCANNING ? "扫描中" : "", selection == 1);
	for (i = 0; i < connect->bt_count && i < 4; i++) { char mark[20]; snprintf(mark, sizeof(mark), "%s%s", connect->bt[i].paired ? "已配对" : "", connect->bt[i].connected ? " 已连接" : ""); menu_row(screen, 128 + (int)i * 44, connect->bt[i].name, mark, selection == i + 2); }
}

static void render_more(lv_obj_t *screen, unsigned int selection, enum dashboard_language language)
{
	static const char *zh[] = {"系统资源", "设备诊断", "系统设置", "关于系统"}; static const char *en[] = {"RESOURCES", "DIAGNOSTICS", "SETTINGS", "ABOUT"}; unsigned int i;
	for (i = 0; i < MORE_ITEM_COUNT; i++) menu_row(screen, 49 + (int)i * 53, language == DASH_LANG_EN ? en[i] : zh[i], NULL, selection == i);
}

static void render_resources(lv_obj_t *screen, const struct system_stats *stats)
{
	char body[512]; unsigned int memory = stats->memory_total_kb ? (unsigned int)((stats->memory_total_kb - stats->memory_available_kb) * 100 / stats->memory_total_kb) : 0;
	snprintf(body, sizeof(body), "CPU              %u%%\n负载              %.2f\n\n内存              %u%%\n可用              %llu MB\n\n存储可用          %llu MB\n温度              %.1f C\n运行              %.1f h", stats->cpu_percent, stats->load_average, memory, (unsigned long long)stats->memory_available_kb / 1024, (unsigned long long)stats->storage_available_kb / 1024, stats->temperature_millic / 1000.0, stats->uptime_sec / 3600.0);
	label(screen, body, 12, 48, 216, &font_cjk_16, LV_TEXT_ALIGN_LEFT);
}

static void render_diagnostics(lv_obj_t *screen, const struct system_stats *stats, const struct epd_100ask_info *info, int error)
{
	char body[512]; snprintf(body, sizeof(body), "主机       %s\n内核       %s\n接口       %s\n地址       %s\n\n全刷       %u\n局刷       %u\n跳过       %u\n最近刷新   %s", stats->hostname, stats->kernel, stats->interface, stats->ip_address, info->full_refreshes, info->partial_refreshes, info->skipped_refreshes, error ? "失败" : "成功"); label(screen, body, 10, 45, 220, &font_cjk_16, LV_TEXT_ALIGN_LEFT);
}

static void render_settings(lv_obj_t *screen, unsigned int selection, const struct dashboard_settings *settings)
{
	char interval[24], cycle[24]; snprintf(interval, sizeof(interval), "%us", settings->refresh_interval_sec); snprintf(cycle, sizeof(cycle), "%u", settings->full_refresh_every);
	menu_row(screen, 44, "界面语言", settings->language == DASH_LANG_EN ? "EN" : "中文", selection == 0);
	menu_row(screen, 93, "自动刷新", interval, selection == 1); menu_row(screen, 142, "全刷间隔", cycle, selection == 2); menu_row(screen, 191, "立即全刷", "执行", selection == 3);
	label(screen, "时区", 10, 259, 65, &font_cjk_16, LV_TEXT_ALIGN_LEFT); label(screen, settings->timezone, 75, 259, 155, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
}

static void render_about(lv_obj_t *screen)
{
	label(screen, "EINK OS\n版本 2.0\n\n电子手帐界面\n日历 / 农历 / 节日\n手机 WiFi 配网\n通用蓝牙配对\nUTF-8 / GB18030 阅读\n\nT113 · 240 x 360 · LVGL 8", 12, 49, 216, &font_cjk_16, LV_TEXT_ALIGN_LEFT);
}

void desktop_ui_init(void)
{
	lv_style_init(&root_style); lv_style_set_bg_color(&root_style, lv_color_white()); lv_style_set_bg_opa(&root_style, LV_OPA_COVER); lv_style_set_border_width(&root_style, 0); lv_style_set_pad_all(&root_style, 0);
	lv_style_init(&text_style); lv_style_set_text_color(&text_style, lv_color_black()); lv_style_set_bg_opa(&text_style, LV_OPA_TRANSP); lv_style_set_border_width(&text_style, 0); lv_style_set_pad_all(&text_style, 0);
	lv_style_init(&line_style); lv_style_set_bg_color(&line_style, lv_color_black()); lv_style_set_bg_opa(&line_style, LV_OPA_COVER); lv_style_set_border_width(&line_style, 0); lv_style_set_pad_all(&line_style, 0);
	lv_style_init(&row_style); lv_style_set_bg_color(&row_style, lv_color_white()); lv_style_set_bg_opa(&row_style, LV_OPA_COVER); lv_style_set_border_width(&row_style, 0); lv_style_set_radius(&row_style, 0); lv_style_set_pad_all(&row_style, 0);
	lv_style_init(&selected_style); lv_style_set_bg_color(&selected_style, lv_color_black()); lv_style_set_bg_opa(&selected_style, LV_OPA_COVER); lv_style_set_border_width(&selected_style, 0); lv_style_set_radius(&selected_style, 0); lv_style_set_pad_all(&selected_style, 0);
}

void desktop_ui_render(enum desktop_screen screen, unsigned int selection, int month_offset,
		       const struct dashboard_settings *settings, const struct system_stats *stats,
		       const struct epd_100ask_info *epd_info, const struct reader_state *reader,
		       const struct eink_connect_status *connect, enum eink_time_quality quality,
		       int last_refresh_error)
{
	lv_obj_t *active = lv_scr_act(); static const char *titles[] = {"EINK OS","日历","阅读","连接","更多","WiFi 配网","蓝牙配对","系统资源","设备诊断","系统设置","关于系统"};
	lv_obj_clean(active); lv_obj_add_style(active, &root_style, 0);
	if (screen == DESKTOP_HOME) { render_home(active, selection, settings, connect, quality); return; }
	header(active, titles[screen], connect);
	switch (screen) {
	case DESKTOP_CALENDAR: render_calendar(active, month_offset, settings->language); footer(active, "K1/K3 月份  K2 今天  长按返回"); break;
	case DESKTOP_READER: render_reader(active, reader, settings->language); footer(active, reader->opened ? "K1/K3 翻页  K2 菜单  长按返回" : "K1/K3 选择  K2 阅读  长按返回"); break;
	case DESKTOP_CONNECT: render_connect_menu(active, selection, connect, settings->language); footer(active, "K1/K3 选择  K2 打开  长按返回"); break;
	case DESKTOP_MORE: render_more(active, selection, settings->language); footer(active, "K1/K3 选择  K2 打开  长按返回"); break;
	case DESKTOP_WIFI: render_wifi(active, selection, connect, settings->language); footer(active, "K1/K3 选择  K2 执行  长按返回"); break;
	case DESKTOP_BLUETOOTH: render_bluetooth(active, selection, connect); footer(active, "K1/K3 选择  K2 操作  长按返回"); break;
	case DESKTOP_RESOURCES: render_resources(active, stats); footer(active, "长按 K2 返回"); break;
	case DESKTOP_DIAGNOSTICS: render_diagnostics(active, stats, epd_info, last_refresh_error); footer(active, "长按 K2 返回"); break;
	case DESKTOP_SETTINGS: render_settings(active, selection % DESKTOP_SETTINGS_COUNT, settings); footer(active, "K1/K3 选择  K2 修改  长按返回"); break;
	case DESKTOP_ABOUT: render_about(active); footer(active, "长按 K2 返回"); break;
	default: break;
	}
}
