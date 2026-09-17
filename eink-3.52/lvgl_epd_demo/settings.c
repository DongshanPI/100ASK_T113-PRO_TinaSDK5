#include "settings.h"

#include <errno.h>
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void settings_defaults(struct dashboard_settings *settings)
{
	settings->language = DASH_LANG_ZH;
	settings->refresh_interval_sec = 60;
	settings->full_refresh_every = 10;
	settings->key_previous = KEY_VOLUMEUP;
	settings->key_next = KEY_VOLUMEDOWN;
	settings->key_next_alt = KEY_PAUSE;
	settings->key_confirm = KEY_ENTER;
	settings->key_confirm_alt = KEY_MODE;
}

static unsigned int bounded_uint(const char *value, unsigned int fallback,
				 unsigned int low, unsigned int high)
{
	char *end;
	unsigned long parsed = strtoul(value, &end, 10);

	if (end == value || *end != '\0' || parsed < low || parsed > high)
		return fallback;
	return (unsigned int)parsed;
}

int settings_load(struct dashboard_settings *settings, const char *path)
{
	char line[160];
	FILE *fp;

	settings_defaults(settings);
	fp = fopen(path, "r");
	if (!fp)
		return errno == ENOENT ? 0 : -errno;

	while (fgets(line, sizeof(line), fp)) {
		char *key = line;
		char *value;
		char *newline;

		if (line[0] == '#' || line[0] == '\n')
			continue;
		value = strchr(line, '=');
		if (!value)
			continue;
		*value++ = '\0';
		newline = strpbrk(value, "\r\n");
		if (newline)
			*newline = '\0';
		if (!strcmp(key, "language"))
			settings->language = !strcmp(value, "en_US") ? DASH_LANG_EN : DASH_LANG_ZH;
		else if (!strcmp(key, "refresh_interval_sec"))
			settings->refresh_interval_sec = bounded_uint(value, 60, 10, 3600);
		else if (!strcmp(key, "full_refresh_every"))
			settings->full_refresh_every = bounded_uint(value, 10, 1, 100);
		else if (!strcmp(key, "key_previous"))
			settings->key_previous = bounded_uint(value, KEY_VOLUMEUP, 1, KEY_MAX);
		else if (!strcmp(key, "key_next"))
			settings->key_next = bounded_uint(value, KEY_VOLUMEDOWN, 1, KEY_MAX);
		else if (!strcmp(key, "key_next_alt"))
			settings->key_next_alt = bounded_uint(value, KEY_PAUSE, 1, KEY_MAX);
		else if (!strcmp(key, "key_confirm"))
			settings->key_confirm = bounded_uint(value, KEY_ENTER, 1, KEY_MAX);
		else if (!strcmp(key, "key_confirm_alt"))
			settings->key_confirm_alt = bounded_uint(value, KEY_MODE, 1, KEY_MAX);
	}
	fclose(fp);
	return 0;
}

int settings_save(const struct dashboard_settings *settings, const char *path)
{
	char tmp_path[256];
	FILE *fp;

	if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path) >= (int)sizeof(tmp_path))
		return -ENAMETOOLONG;
	fp = fopen(tmp_path, "w");
	if (!fp)
		return -errno;
	fprintf(fp, "# 100ASK E-Ink dashboard settings\n");
	fprintf(fp, "language=%s\n", settings->language == DASH_LANG_EN ? "en_US" : "zh_CN");
	fprintf(fp, "refresh_interval_sec=%u\n", settings->refresh_interval_sec);
	fprintf(fp, "full_refresh_every=%u\n", settings->full_refresh_every);
	fprintf(fp, "key_previous=%u\n", settings->key_previous);
	fprintf(fp, "key_next=%u\n", settings->key_next);
	fprintf(fp, "key_next_alt=%u\n", settings->key_next_alt);
	fprintf(fp, "key_confirm=%u\n", settings->key_confirm);
	fprintf(fp, "key_confirm_alt=%u\n", settings->key_confirm_alt);
	if (fclose(fp))
		return -errno;
	if (rename(tmp_path, path))
		return -errno;
	return 0;
}
