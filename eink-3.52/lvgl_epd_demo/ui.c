#include "ui.h"

#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

LV_FONT_DECLARE(font_cjk_16);

static lv_style_t root_style;
static lv_style_t text_style;
static lv_style_t line_style;
static lv_style_t card_style;
static lv_style_t selected_style;
static lv_style_t bar_bg_style;
static lv_style_t bar_indic_style;

static lv_obj_t *add_label(lv_obj_t *parent, const char *text, int x, int y,
			   int width, const lv_font_t *font, lv_text_align_t align)
{
	lv_obj_t *label = lv_label_create(parent);

	lv_obj_add_style(label, &text_style, 0);
	lv_obj_set_style_text_font(label, font, 0);
	lv_obj_set_pos(label, x, y);
	lv_obj_set_width(label, width);
	lv_obj_set_style_text_align(label, align, 0);
	lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
	lv_label_set_text(label, text);
	return label;
}

static void add_rule(lv_obj_t *parent, int y)
{
	lv_obj_t *line = lv_obj_create(parent);

	lv_obj_add_style(line, &line_style, 0);
	lv_obj_set_pos(line, 8, y);
	lv_obj_set_size(line, 224, 1);
}

static void add_header(lv_obj_t *screen, const char *title,
		       const struct system_stats *stats)
{
	char status[32];
	char time_text[8];
	time_t now = time(NULL);
	struct tm tm_value;

	localtime_r(&now, &tm_value);
	if (stats->time_synchronized)
		strftime(time_text, sizeof(time_text), "%H:%M", &tm_value);
	else
		strcpy(time_text, "--:--");
	snprintf(status, sizeof(status), "%s %s",
		 stats->network_online ? "NET" : "OFF", time_text);
	add_label(screen, title, 8, 6, 132, &font_cjk_16, LV_TEXT_ALIGN_LEFT);
	add_label(screen, status, 140, 7, 92, &lv_font_montserrat_14,
		  LV_TEXT_ALIGN_RIGHT);
	add_rule(screen, 31);
}

static void add_footer(lv_obj_t *screen, enum desktop_screen current,
		       enum dashboard_language language,
		       const struct reader_state *reader)
{
	const char *text;

	add_rule(screen, 328);
	if (current == DESKTOP_LAUNCHER)
		text = language == DASH_LANG_EN ?
			"< > SELECT  OK OPEN  HOLD:LANG" :
			"< > 选择  OK 打开  长按:语言";
	else if (current == DESKTOP_CALENDAR)
		text = language == DASH_LANG_EN ?
			"< > MONTH  OK TODAY  HOLD:BACK" :
			"< > 月份  OK 今天  长按:返回";
	else if (current == DESKTOP_READER && reader->opened)
		text = language == DASH_LANG_EN ?
			"< > PAGE  OK LIBRARY  HOLD:BACK" :
			"< > 翻页  OK 书库  长按:返回";
	else if (current == DESKTOP_READER)
		text = language == DASH_LANG_EN ?
			"< > SELECT  OK READ  HOLD:BACK" :
			"< > 选择  OK 阅读  长按:返回";
	else if (current == DESKTOP_NETWORK)
		text = language == DASH_LANG_EN ?
			"< > APPS  OK SCAN  HOLD:BACK" :
			"< > 应用  OK 扫描  长按:返回";
	else if (current == DESKTOP_SETTINGS)
		text = language == DASH_LANG_EN ?
			"< > SELECT  OK CHANGE  HOLD:BACK" :
			"< > 选择  OK 修改  长按:返回";
	else
		text = language == DASH_LANG_EN ?
			"< > APPS  OK REFRESH  HOLD:BACK" :
			"< > 切换  OK 刷新  长按:返回";
	add_label(screen, text, 5, 336, 230, &font_cjk_16, LV_TEXT_ALIGN_CENTER);
}

static void add_tile(lv_obj_t *screen, int x, int y, const char *mark,
		     const char *title, int selected)
{
	lv_obj_t *tile = lv_obj_create(screen);
	lv_obj_t *label;

	lv_obj_add_style(tile, selected ? &selected_style : &card_style, 0);
	lv_obj_set_pos(tile, x, y);
	lv_obj_set_size(tile, 108, 62);
	lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
	label = add_label(tile, mark, 4, 5, 98, &lv_font_montserrat_16,
			  LV_TEXT_ALIGN_CENTER);
	if (selected)
		lv_obj_set_style_text_color(label, lv_color_white(), 0);
	label = add_label(tile, title, 4, 32, 98, &font_cjk_16,
			  LV_TEXT_ALIGN_CENTER);
	if (selected)
		lv_obj_set_style_text_color(label, lv_color_white(), 0);
}

static void render_launcher(lv_obj_t *screen, unsigned int selection,
			    enum dashboard_language language)
{
	static const char *marks[] = {"01", "02", "03", "04", "05", "06", "07", "08"};
	static const char *titles_zh[] = {"系统概览", "日历", "阅读器", "网络状态",
					  "系统资源", "设备诊断", "系统设置", "关于系统"};
	static const char *titles_en[] = {"OVERVIEW", "CALENDAR", "READER", "NETWORK",
					  "RESOURCES", "DIAGNOSTICS", "SETTINGS", "ABOUT"};
	unsigned int i;

	selection %= DESKTOP_APP_COUNT;
	for (i = 0; i < DESKTOP_APP_COUNT; i++) {
		int x = (i & 1) ? 124 : 8;
		int y = 39 + (int)(i / 2) * 70;

		add_tile(screen, x, y, marks[i],
			 language == DASH_LANG_EN ? titles_en[i] : titles_zh[i],
			 i == selection);
	}
}

static int is_leap_year(int year)
{
	return (!(year % 4) && year % 100) || !(year % 400);
}

static int days_in_month(int year, int month)
{
	static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	return month == 1 ? days[month] + is_leap_year(year) : days[month];
}

static void render_calendar(lv_obj_t *screen, enum dashboard_language language,
			    int month_offset)
{
	static const char *week_zh[] = {"日", "一", "二", "三", "四", "五", "六"};
	static const char *week_en[] = {"S", "M", "T", "W", "T", "F", "S"};
	char title[48], day_text[12];
	time_t now = time(NULL);
	struct tm today, shown;
	int year, month, first_weekday, count, day, column, row;

	localtime_r(&now, &today);
	shown = today;
	shown.tm_mday = 1;
	shown.tm_mon += month_offset;
	mktime(&shown);
	year = shown.tm_year + 1900;
	month = shown.tm_mon;
	first_weekday = shown.tm_wday;
	count = days_in_month(year, month);
	if (language == DASH_LANG_EN)
		strftime(title, sizeof(title), "%B %Y", &shown);
	else
		snprintf(title, sizeof(title), "%04d年%02d月", year, month + 1);
	add_label(screen, title, 8, 40, 224,
		  language == DASH_LANG_EN ? &lv_font_montserrat_20 : &font_cjk_16,
		  LV_TEXT_ALIGN_CENTER);
	for (column = 0; column < 7; column++)
		add_label(screen, language == DASH_LANG_EN ? week_en[column] : week_zh[column],
			  9 + column * 32, 72, 30, &font_cjk_16, LV_TEXT_ALIGN_CENTER);
	add_rule(screen, 94);
	for (day = 1; day <= count; day++) {
		lv_obj_t *label;
		int is_today;

		column = (first_weekday + day - 1) % 7;
		row = (first_weekday + day - 1) / 7;
		is_today = !month_offset && day == today.tm_mday;
		snprintf(day_text, sizeof(day_text), "%u", (unsigned int)day);
		if (is_today) {
			lv_obj_t *box = lv_obj_create(screen);

			lv_obj_add_style(box, &selected_style, 0);
			lv_obj_set_pos(box, 11 + column * 32, 101 + row * 36);
			lv_obj_set_size(box, 27, 29);
			lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
			label = add_label(box, day_text, 0, 6, 25, &lv_font_montserrat_14,
					  LV_TEXT_ALIGN_CENTER);
			lv_obj_set_style_text_color(label, lv_color_white(), 0);
		} else {
			add_label(screen, day_text, 10 + column * 32, 108 + row * 36,
				  28, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
		}
	}
}

static void format_uptime(char *buffer, size_t size, double seconds,
			  enum dashboard_language language)
{
	unsigned int days = (unsigned int)seconds / 86400;
	unsigned int hours = ((unsigned int)seconds / 3600) % 24;
	unsigned int minutes = ((unsigned int)seconds / 60) % 60;

	if (language == DASH_LANG_EN)
		snprintf(buffer, size, "%ud %02uh %02um", days, hours, minutes);
	else
		snprintf(buffer, size, "%u天 %02u时 %02u分", days, hours, minutes);
}

static void render_overview(lv_obj_t *screen, enum dashboard_language language,
			    const struct system_stats *stats)
{
	char time_text[16], date_text[80], uptime[48], row[128];
	time_t now = time(NULL);
	struct tm tm_value;
	static const char *week_zh[] = {"日", "一", "二", "三", "四", "五", "六"};

	localtime_r(&now, &tm_value);
	strftime(time_text, sizeof(time_text), "%H:%M", &tm_value);
	if (!stats->time_synchronized)
		strcpy(time_text, "--:--");
	if (language == DASH_LANG_EN)
		strftime(date_text, sizeof(date_text), "%Y-%m-%d  %A", &tm_value);
	else
		snprintf(date_text, sizeof(date_text), "%04d年%02d月%02d日  星期%s",
			 tm_value.tm_year + 1900, tm_value.tm_mon + 1, tm_value.tm_mday,
			 week_zh[tm_value.tm_wday]);

	add_label(screen, time_text, 10, 42, 220, &lv_font_montserrat_36,
		  LV_TEXT_ALIGN_CENTER);
	add_label(screen, date_text, 8, 90, 224, &font_cjk_16, LV_TEXT_ALIGN_CENTER);
	add_rule(screen, 120);
	format_uptime(uptime, sizeof(uptime), stats->uptime_sec, language);
	snprintf(row, sizeof(row), "%s\n%s",
		 language == DASH_LANG_EN ? "UPTIME" : "运行时间", uptime);
	add_label(screen, row, 12, 137, 216, &font_cjk_16, LV_TEXT_ALIGN_CENTER);
	snprintf(row, sizeof(row), "%s\n%s  %s",
		 language == DASH_LANG_EN ? "NETWORK" : "网络状态",
		 stats->network_online ? stats->interface : "--",
		 stats->network_online ? stats->ip_address :
		 (language == DASH_LANG_EN ? "OFFLINE" : "未连接"));
	add_label(screen, row, 12, 215, 216, &font_cjk_16, LV_TEXT_ALIGN_CENTER);
}

static void add_metric(lv_obj_t *screen, int y, const char *name,
		       const char *value, unsigned int percent)
{
	lv_obj_t *bar;

	add_label(screen, name, 10, y, 92, &font_cjk_16, LV_TEXT_ALIGN_LEFT);
	add_label(screen, value, 105, y, 125, &font_cjk_16, LV_TEXT_ALIGN_RIGHT);
	bar = lv_bar_create(screen);
	lv_obj_add_style(bar, &bar_bg_style, LV_PART_MAIN);
	lv_obj_add_style(bar, &bar_indic_style, LV_PART_INDICATOR);
	lv_obj_set_pos(bar, 10, y + 24);
	lv_obj_set_size(bar, 220, 12);
	lv_bar_set_range(bar, 0, 100);
	lv_bar_set_value(bar, percent > 100 ? 100 : percent, LV_ANIM_OFF);
}

static void render_resources(lv_obj_t *screen, enum dashboard_language language,
			     const struct system_stats *stats)
{
	char value[64];
	unsigned int memory_percent = 0, storage_percent = 0;

	if (stats->memory_total_kb)
		memory_percent = (unsigned int)((stats->memory_total_kb -
			stats->memory_available_kb) * 100 / stats->memory_total_kb);
	if (stats->storage_total_kb)
		storage_percent = (unsigned int)((stats->storage_total_kb -
			stats->storage_available_kb) * 100 / stats->storage_total_kb);

	snprintf(value, sizeof(value), "%u%%  L %.2f", stats->cpu_percent,
		 stats->load_average);
	add_metric(screen, 48, language == DASH_LANG_EN ? "CPU" : "处理器",
		   value, stats->cpu_percent);
	snprintf(value, sizeof(value), "%llu/%llu MB",
		 (unsigned long long)(stats->memory_total_kb - stats->memory_available_kb) / 1024,
		 (unsigned long long)stats->memory_total_kb / 1024);
	add_metric(screen, 111, language == DASH_LANG_EN ? "MEMORY" : "内存",
		   value, memory_percent);
	snprintf(value, sizeof(value), "%llu/%llu MB",
		 (unsigned long long)(stats->storage_total_kb - stats->storage_available_kb) / 1024,
		 (unsigned long long)stats->storage_total_kb / 1024);
	add_metric(screen, 174, language == DASH_LANG_EN ? "STORAGE" : "存储",
		   value, storage_percent);
	if (stats->temperature_millic >= 0)
		snprintf(value, sizeof(value), "%.1f C", stats->temperature_millic / 1000.0);
	else
		strcpy(value, "--");
	add_label(screen, language == DASH_LANG_EN ? "TEMPERATURE" : "温度",
		  10, 251, 130, &font_cjk_16, LV_TEXT_ALIGN_LEFT);
	add_label(screen, value, 140, 251, 90, &font_cjk_16, LV_TEXT_ALIGN_RIGHT);
}

static void add_info_row(lv_obj_t *screen, int y, const char *name, const char *value)
{
	add_label(screen, name, 10, y, 82, &font_cjk_16, LV_TEXT_ALIGN_LEFT);
	add_label(screen, value, 92, y, 138, &font_cjk_16, LV_TEXT_ALIGN_RIGHT);
	add_rule(screen, y + 27);
}

static void render_reader(lv_obj_t *screen, enum dashboard_language language,
			  const struct reader_state *reader)
{
	if (reader->opened) {
		char page_number[32];
		lv_obj_t *content;

		add_label(screen, reader->files[reader->current_file].name, 9, 39, 170,
			  &font_cjk_16, LV_TEXT_ALIGN_LEFT);
		snprintf(page_number, sizeof(page_number), "%u", reader->page_index + 1);
		add_label(screen, page_number, 180, 39, 50, &font_cjk_16,
			  LV_TEXT_ALIGN_RIGHT);
		add_rule(screen, 63);
		content = add_label(screen, reader->page[0] ? reader->page : "--", 9, 70,
				    222, &font_cjk_16, LV_TEXT_ALIGN_LEFT);
		lv_label_set_long_mode(content, LV_LABEL_LONG_WRAP);
		lv_obj_set_height(content, 248);
		return;
	}

	if (!reader->file_count) {
		add_label(screen,
			  language == DASH_LANG_EN ?
			  "NO TXT BOOKS\n\nCopy files to:\n/mnt/UDISK/books\n/mnt/SDCARD/books" :
			  "没有 TXT 书籍\n\n请复制文件到：\n/mnt/UDISK/books\n/mnt/SDCARD/books",
			  12, 65, 216, &font_cjk_16, LV_TEXT_ALIGN_CENTER);
	} else {
		unsigned int first = reader->selection >= 5 ? reader->selection - 5 : 0;
		unsigned int i;

		for (i = first; i < reader->file_count && i < first + 6; i++) {
			lv_obj_t *row = lv_obj_create(screen);
			lv_obj_t *label;
			int selected = i == reader->selection;

			lv_obj_add_style(row, selected ? &selected_style : &card_style, 0);
			lv_obj_set_pos(row, 8, 40 + (int)(i - first) * 45);
			lv_obj_set_size(row, 224, 38);
			lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
			label = add_label(row, reader->files[i].name, 7, 10, 208,
					  &font_cjk_16, LV_TEXT_ALIGN_LEFT);
			if (selected)
				lv_obj_set_style_text_color(label, lv_color_white(), 0);
		}
	}
}

static void render_network(lv_obj_t *screen, enum dashboard_language language,
			   const struct system_stats *stats,
			   const struct wifi_scan_state *scan)
{
	char signal[24];
	unsigned int i;

	snprintf(signal, sizeof(signal), "%d%%", stats->wifi_signal_percent);
	add_info_row(screen, 40, language == DASH_LANG_EN ? "WIFI" : "无线网络",
		     stats->wifi_connected ? stats->wifi_ssid :
		     (language == DASH_LANG_EN ? "DISCONNECTED" : "未连接"));
	add_info_row(screen, 73, language == DASH_LANG_EN ? "ADDRESS" : "网络地址",
		     stats->ip_address);
	add_info_row(screen, 106, language == DASH_LANG_EN ? "SIGNAL" : "信号强度",
		     stats->wifi_connected ? signal : "--");
	add_label(screen, language == DASH_LANG_EN ? "NEARBY NETWORKS" : "附近网络",
		  10, 143, 220, &font_cjk_16, LV_TEXT_ALIGN_LEFT);
	add_rule(screen, 166);
	if (scan->running) {
		add_label(screen, language == DASH_LANG_EN ? "SCANNING..." : "正在扫描...",
			  10, 181, 220, &font_cjk_16, LV_TEXT_ALIGN_CENTER);
	} else if (!scan->result_count) {
		add_label(screen,
			  language == DASH_LANG_EN ? "PRESS OK TO SCAN" : "按确认键扫描",
			  10, 181, 220, &font_cjk_16, LV_TEXT_ALIGN_CENTER);
	} else {
		for (i = 0; i < scan->result_count; i++) {
			char row[96];

			snprintf(row, sizeof(row), "%d dBm  %s",
				 scan->results[i].signal_dbm, scan->results[i].ssid);
			add_label(screen, row, 10, 174 + (int)i * 24, 220,
				  &font_cjk_16, LV_TEXT_ALIGN_LEFT);
		}
	}
}

static void render_diagnostics(lv_obj_t *screen, enum dashboard_language language,
			       const struct system_stats *stats,
			       const struct epd_100ask_info *info, int last_error)
{
	char body[512];
	const char *mode = "GC";

	if (info->refresh_mode == EPD_100ASK_REFRESH_DU)
		mode = "DU";
	else if (info->refresh_mode == EPD_100ASK_REFRESH_5S)
		mode = "5S";
	if (language == DASH_LANG_EN) {
		snprintf(body, sizeof(body),
			 "HOST       %s\nKERNEL     %s\nINTERFACE  %s\nIP         %s\n\nEPD MODE   %s\nFULL       %u\nPARTIAL    %u\nSKIPPED    %u\nLAST       %s",
			 stats->hostname, stats->kernel, stats->interface, stats->ip_address,
			 mode, info->full_refreshes, info->partial_refreshes,
			 info->skipped_refreshes, last_error ? "FAILED" : "OK");
	} else {
		snprintf(body, sizeof(body),
			 "主机       %s\n内核       %s\n网络接口   %s\n地址       %s\n\n刷新模式   %s\n全刷次数   %u\n局刷次数   %u\n跳过次数   %u\n最近刷新   %s",
			 stats->hostname, stats->kernel, stats->interface, stats->ip_address,
			 mode, info->full_refreshes, info->partial_refreshes,
			 info->skipped_refreshes, last_error ? "失败" : "成功");
	}
	add_label(screen, body, 10, 43, 220, &font_cjk_16, LV_TEXT_ALIGN_LEFT);
}

static void add_setting_row(lv_obj_t *screen, int y, const char *name,
			    const char *value, int selected)
{
	lv_obj_t *row = lv_obj_create(screen);
	lv_obj_t *label;

	lv_obj_add_style(row, selected ? &selected_style : &card_style, 0);
	lv_obj_set_pos(row, 8, y);
	lv_obj_set_size(row, 224, 55);
	lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
	label = add_label(row, name, 8, 16, 128, &font_cjk_16, LV_TEXT_ALIGN_LEFT);
	if (selected)
		lv_obj_set_style_text_color(label, lv_color_white(), 0);
	label = add_label(row, value, 138, 16, 76, &font_cjk_16, LV_TEXT_ALIGN_RIGHT);
	if (selected)
		lv_obj_set_style_text_color(label, lv_color_white(), 0);
}

static void render_settings(lv_obj_t *screen, unsigned int selection,
			    const struct dashboard_settings *settings)
{
	char interval[32], full[32];
	int en = settings->language == DASH_LANG_EN;

	snprintf(interval, sizeof(interval), "%us", settings->refresh_interval_sec);
	snprintf(full, sizeof(full), "%u", settings->full_refresh_every);
	add_setting_row(screen, 42, en ? "LANGUAGE" : "界面语言",
			settings->language == DASH_LANG_EN ? "EN" : "中文", selection == 0);
	add_setting_row(screen, 107, en ? "AUTO REFRESH" : "自动刷新",
			interval, selection == 1);
	add_setting_row(screen, 172, en ? "FULL CYCLE" : "全刷间隔",
			full, selection == 2);
	add_setting_row(screen, 237, en ? "CLEAN GHOST" : "清理残影",
			en ? "RUN" : "执行", selection == 3);
}

static void render_about(lv_obj_t *screen, enum dashboard_language language)
{
	char body[512];

	if (language == DASH_LANG_EN)
		snprintf(body, sizeof(body),
			 "EINK OS\nVersion 1.2\n\nPlatform   Allwinner T113\nDisplay    3.52 inch\nResolution 240 x 360\nApps       8 built-in\nUI Engine  LVGL 8\nInput      3 physical keys\n\nLicense    MIT / OFL-1.1");
	else
		snprintf(body, sizeof(body),
			 "EINK OS\n版本 1.2\n\n平台       Allwinner T113\n屏幕       3.52 英寸\n分辨率     240 x 360\n应用       8 个内置\n界面引擎   LVGL 8\n输入       三个实体按键\n\n许可证     MIT / OFL-1.1");
	add_label(screen, body, 12, 50, 216, &font_cjk_16, LV_TEXT_ALIGN_LEFT);
}

void desktop_ui_init(void)
{
	lv_style_init(&root_style);
	lv_style_set_bg_color(&root_style, lv_color_white());
	lv_style_set_bg_opa(&root_style, LV_OPA_COVER);
	lv_style_set_border_width(&root_style, 0);
	lv_style_set_pad_all(&root_style, 0);

	lv_style_init(&text_style);
	lv_style_set_text_color(&text_style, lv_color_black());
	lv_style_set_bg_opa(&text_style, LV_OPA_TRANSP);
	lv_style_set_border_width(&text_style, 0);
	lv_style_set_pad_all(&text_style, 0);

	lv_style_init(&line_style);
	lv_style_set_bg_color(&line_style, lv_color_black());
	lv_style_set_bg_opa(&line_style, LV_OPA_COVER);
	lv_style_set_border_width(&line_style, 0);
	lv_style_set_pad_all(&line_style, 0);

	lv_style_init(&card_style);
	lv_style_set_bg_color(&card_style, lv_color_white());
	lv_style_set_bg_opa(&card_style, LV_OPA_COVER);
	lv_style_set_border_color(&card_style, lv_color_black());
	lv_style_set_border_width(&card_style, 2);
	lv_style_set_radius(&card_style, 0);
	lv_style_set_pad_all(&card_style, 0);

	lv_style_init(&selected_style);
	lv_style_set_bg_color(&selected_style, lv_color_black());
	lv_style_set_bg_opa(&selected_style, LV_OPA_COVER);
	lv_style_set_border_color(&selected_style, lv_color_black());
	lv_style_set_border_width(&selected_style, 2);
	lv_style_set_radius(&selected_style, 0);
	lv_style_set_pad_all(&selected_style, 0);

	lv_style_init(&bar_bg_style);
	lv_style_set_bg_color(&bar_bg_style, lv_color_white());
	lv_style_set_bg_opa(&bar_bg_style, LV_OPA_COVER);
	lv_style_set_border_color(&bar_bg_style, lv_color_black());
	lv_style_set_border_width(&bar_bg_style, 1);
	lv_style_set_radius(&bar_bg_style, 0);
	lv_style_set_pad_all(&bar_bg_style, 1);

	lv_style_init(&bar_indic_style);
	lv_style_set_bg_color(&bar_indic_style, lv_color_black());
	lv_style_set_bg_opa(&bar_indic_style, LV_OPA_COVER);
	lv_style_set_radius(&bar_indic_style, 0);
}

void desktop_ui_render(enum desktop_screen screen, unsigned int selection,
		       int calendar_month_offset,
		       const struct dashboard_settings *settings,
		       const struct system_stats *stats,
		       const struct epd_100ask_info *epd_info,
		       const struct reader_state *reader,
		       const struct wifi_scan_state *wifi_scan,
		       int last_refresh_error)
{
	lv_obj_t *active = lv_scr_act();
	static const char *titles_zh[] = {"EINK OS", "系统概览", "日历", "阅读器",
					  "网络状态", "系统资源", "设备诊断", "系统设置", "关于系统"};
	static const char *titles_en[] = {"EINK OS", "OVERVIEW", "CALENDAR", "READER",
					  "NETWORK", "RESOURCES", "DIAGNOSTICS", "SETTINGS", "ABOUT"};
	enum dashboard_language language = settings->language;

	if (screen < DESKTOP_LAUNCHER || screen > DESKTOP_ABOUT)
		screen = DESKTOP_LAUNCHER;
	lv_obj_clean(active);
	lv_obj_add_style(active, &root_style, 0);
	add_header(active, language == DASH_LANG_EN ? titles_en[screen] : titles_zh[screen], stats);

	switch (screen) {
	case DESKTOP_LAUNCHER:
		render_launcher(active, selection, language);
		break;
	case DESKTOP_OVERVIEW:
		render_overview(active, language, stats);
		break;
	case DESKTOP_CALENDAR:
		render_calendar(active, language, calendar_month_offset);
		break;
	case DESKTOP_READER:
		render_reader(active, language, reader);
		break;
	case DESKTOP_NETWORK:
		render_network(active, language, stats, wifi_scan);
		break;
	case DESKTOP_RESOURCES:
		render_resources(active, language, stats);
		break;
	case DESKTOP_DIAGNOSTICS:
		render_diagnostics(active, language, stats, epd_info, last_refresh_error);
		break;
	case DESKTOP_SETTINGS:
		render_settings(active, selection % DESKTOP_SETTINGS_COUNT, settings);
		break;
	case DESKTOP_ABOUT:
		render_about(active, language);
		break;
	}
	add_footer(active, screen, language, reader);
}
