#include "wifi_results.h"

#include <stdlib.h>
#include <string.h>

static int compare_networks(const void *left, const void *right)
{
	const struct eink_wifi_network *a = left;
	const struct eink_wifi_network *b = right;

	return (int)b->signal - (int)a->signal;
}

static void add_network(struct eink_connect_status *status, const char *ssid,
			int signal, int secure)
{
	unsigned int i;

	if (!ssid[0])
		return;
	for (i = 0; i < status->wifi_count; i++) {
		if (!strcmp(status->wifi[i].ssid, ssid)) {
			if (signal > status->wifi[i].signal)
				status->wifi[i].signal = (int16_t)signal;
			status->wifi[i].secure |= secure != 0;
			return;
		}
	}
	if (status->wifi_count < EINK_CONNECT_MAX_WIFI) {
		i = status->wifi_count++;
	} else {
		unsigned int weakest = 0;

		for (i = 1; i < status->wifi_count; i++)
			if (status->wifi[i].signal < status->wifi[weakest].signal)
				weakest = i;
		if (signal <= status->wifi[weakest].signal)
			return;
		i = weakest;
	}
	snprintf(status->wifi[i].ssid, sizeof(status->wifi[i].ssid), "%.32s", ssid);
	status->wifi[i].signal = (int16_t)signal;
	status->wifi[i].secure = secure != 0;
}

void wifi_results_parse(FILE *stream, struct eink_connect_status *status)
{
	char line[512], ssid[33] = {0};
	int signal = -100, secure = 0, in_bss = 0;

	status->wifi_count = 0;
	memset(status->wifi, 0, sizeof(status->wifi));
	while (fgets(line, sizeof(line), stream)) {
		char *field = line;
		char *end;

		while (*field == ' ' || *field == '\t')
			field++;
		if (!strncmp(field, "BSS ", 4)) {
			if (in_bss)
				add_network(status, ssid, signal, secure);
			ssid[0] = '\0';
			signal = -100;
			secure = 0;
			in_bss = 1;
		} else if (in_bss && !strncmp(field, "signal:", 7)) {
			signal = (int)strtol(field + 7, NULL, 10);
		} else if (in_bss && !strncmp(field, "SSID:", 5)) {
			field += 5;
			while (*field == ' ' || *field == '\t')
				field++;
			end = strpbrk(field, "\r\n");
			if (end)
				*end = '\0';
			snprintf(ssid, sizeof(ssid), "%.32s", field);
		} else if (in_bss && (!strncmp(field, "RSN:", 4) ||
					!strncmp(field, "WPA:", 4))) {
			secure = 1;
		}
	}
	if (in_bss)
		add_network(status, ssid, signal, secure);
	qsort(status->wifi, status->wifi_count, sizeof(status->wifi[0]),
	      compare_networks);
}
