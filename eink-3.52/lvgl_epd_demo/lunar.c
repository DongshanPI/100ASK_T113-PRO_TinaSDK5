#include "lunar.h"

#include <stdio.h>
#include <time.h>

/* Encoded lunar years 1900..2100: low nibble is leap month, bit 16 its size. */
static const unsigned int lunar_info[] = {
	0x04bd8,0x04ae0,0x0a570,0x054d5,0x0d260,0x0d950,0x16554,0x056a0,0x09ad0,0x055d2,
	0x04ae0,0x0a5b6,0x0a4d0,0x0d250,0x1d255,0x0b540,0x0d6a0,0x0ada2,0x095b0,0x14977,
	0x04970,0x0a4b0,0x0b4b5,0x06a50,0x06d40,0x1ab54,0x02b60,0x09570,0x052f2,0x04970,
	0x06566,0x0d4a0,0x0ea50,0x06e95,0x05ad0,0x02b60,0x186e3,0x092e0,0x1c8d7,0x0c950,
	0x0d4a0,0x1d8a6,0x0b550,0x056a0,0x1a5b4,0x025d0,0x092d0,0x0d2b2,0x0a950,0x0b557,
	0x06ca0,0x0b550,0x15355,0x04da0,0x0a5d0,0x14573,0x052d0,0x0a9a8,0x0e950,0x06aa0,
	0x0aea6,0x0ab50,0x04b60,0x0aae4,0x0a570,0x05260,0x0f263,0x0d950,0x05b57,0x056a0,
	0x096d0,0x04dd5,0x04ad0,0x0a4d0,0x0d4d4,0x0d250,0x0d558,0x0b540,0x0b5a0,0x195a6,
	0x095b0,0x049b0,0x0a974,0x0a4b0,0x0b27a,0x06a50,0x06d40,0x0af46,0x0ab60,0x09570,
	0x04af5,0x04970,0x064b0,0x074a3,0x0ea50,0x06b58,0x055c0,0x0ab60,0x096d5,0x092e0,
	0x0c960,0x0d954,0x0d4a0,0x0da50,0x07552,0x056a0,0x0abb7,0x025d0,0x092d0,0x0cab5,
	0x0a950,0x0b4a0,0x0baa4,0x0ad50,0x055d9,0x04ba0,0x0a5b0,0x15176,0x052b0,0x0a930,
	0x07954,0x06aa0,0x0ad50,0x05b52,0x04b60,0x0a6e6,0x0a4e0,0x0d260,0x0ea65,0x0d530,
	0x05aa0,0x076a3,0x096d0,0x04afb,0x04ad0,0x0a4d0,0x1d0b6,0x0d250,0x0d520,0x0dd45,
	0x0b5a0,0x056d0,0x055b2,0x049b0,0x0a577,0x0a4b0,0x0aa50,0x1b255,0x06d20,0x0ada0,
	0x14b63,0x09370,0x049f8,0x04970,0x064b0,0x168a6,0x0ea50,0x06b20,0x1a6c4,0x0aae0,
	0x0a2e0,0x0d2e3,0x0c960,0x0d557,0x0d4a0,0x0da50,0x05d55,0x056a0,0x0a6d0,0x055d4,
	0x052d0,0x0a9b8,0x0a950,0x0b4a0,0x0b6a6,0x0ad50,0x055a0,0x0aba4,0x0a5b0,0x052b0,
	0x0b273,0x06930,0x07337,0x06aa0,0x0ad50,0x14b55,0x04b60,0x0a570,0x054e4,0x0d160,
	0x0e968,0x0d520,0x0daa0,0x16aa6,0x056d0,0x04ae0,0x0a9d4,0x0a2d0,0x0d150,0x0f252,
	0x0d520
};

static int leap_month(int year) { return lunar_info[year - 1900] & 0xf; }
static int leap_days(int year)
{
	return leap_month(year) ? ((lunar_info[year - 1900] & 0x10000) ? 30 : 29) : 0;
}
static int month_days(int year, int month)
{
	return (lunar_info[year - 1900] & (0x10000 >> month)) ? 30 : 29;
}
static int year_days(int year)
{
	unsigned int mask;
	int total = 348;
	for (mask = 0x8000; mask > 0x8; mask >>= 1)
		if (lunar_info[year - 1900] & mask)
			total++;
	return total + leap_days(year);
}

static long civil_days(int year, int month, int day)
{
	struct tm value = {0};
	value.tm_year = year - 1900;
	value.tm_mon = month - 1;
	value.tm_mday = day;
	/* UTC midnight keeps pre-1970 division on an exact day boundary. */
	value.tm_hour = 0;
	return (long)(timegm(&value) / 86400);
}

int lunar_from_solar(int year, int month, int day, struct lunar_date *out)
{
	long offset;
	int leap, days, lunar_month;

	if (!out || year < 1900 || year > 2100 || month < 1 || month > 12 || day < 1)
		return -1;
	offset = civil_days(year, month, day) - civil_days(1900, 1, 31);
	if (offset < 0)
		return -1;
	for (out->year = 1900; out->year <= 2100; out->year++) {
		days = year_days(out->year);
		if (offset < days)
			break;
		offset -= days;
	}
	if (out->year > 2100)
		return -1;
	leap = leap_month(out->year);
	out->leap = 0;
	for (lunar_month = 1; lunar_month <= 12; lunar_month++) {
		days = month_days(out->year, lunar_month);
		if (offset < days)
			break;
		offset -= days;
		if (lunar_month == leap) {
			days = leap_days(out->year);
			if (offset < days) {
				out->leap = 1;
				break;
			}
			offset -= days;
		}
	}
	out->month = lunar_month;
	out->day = (int)offset + 1;
	return 0;
}

void lunar_format(const struct lunar_date *date, char *buffer, size_t size)
{
	static const char *months[] = {"正", "二", "三", "四", "五", "六", "七", "八", "九", "十", "冬", "腊"};
	static const char *digits[] = {"一", "二", "三", "四", "五", "六", "七", "八", "九", "十"};
	char day[24];

	if (!date || date->month < 1 || date->month > 12 || date->day < 1 || date->day > 30) {
		snprintf(buffer, size, "农历--");
		return;
	}
	if (date->day == 10)
		snprintf(day, sizeof(day), "初十");
	else if (date->day == 20)
		snprintf(day, sizeof(day), "二十");
	else if (date->day == 30)
		snprintf(day, sizeof(day), "三十");
	else
		snprintf(day, sizeof(day), "%s%s", date->day < 11 ? "初" : date->day < 21 ? "十" : "廿",
			 digits[(date->day - 1) % 10]);
	snprintf(buffer, size, "农历%s%s月%s", date->leap ? "闰" : "", months[date->month - 1], day);
}

const char *calendar_holiday(int year, int month, int day, const struct lunar_date *lunar)
{
	(void)year;
	if (month == 1 && day == 1) return "元旦";
	if (month == 4 && (day == 4 || day == 5)) return "清明";
	if (month == 5 && day == 1) return "劳动节";
	if (month == 10 && day == 1) return "国庆节";
	if (!lunar || lunar->leap) return NULL;
	if (lunar->month == 1 && lunar->day == 1) return "春节";
	if (lunar->month == 1 && lunar->day == 15) return "元宵节";
	if (lunar->month == 5 && lunar->day == 5) return "端午节";
	if (lunar->month == 8 && lunar->day == 15) return "中秋节";
	return NULL;
}
