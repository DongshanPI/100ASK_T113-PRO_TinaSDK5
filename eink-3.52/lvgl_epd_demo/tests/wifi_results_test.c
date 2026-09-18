#define _GNU_SOURCE
#include "wifi_results.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	char fixture[] =
		"BSS 00:00:00:00:00:01(on wlan0)\n"
		"\tsignal: -72.00 dBm\n"
		"\tSSID: Office WiFi\n"
		"\tRSN:\t * Version: 1\n"
		"BSS 00:00:00:00:00:02(on wlan0)\n"
		"\tsignal: -41.00 dBm\n"
		"\tSSID: Guest\n"
		"BSS 00:00:00:00:00:03(on wlan0)\n"
		"\tsignal: -55.00 dBm\n"
		"\tSSID: Office WiFi\n"
		"\tWPA:\t * Version: 1\n"
		"BSS 00:00:00:00:00:04(on wlan0)\n"
		"\tsignal: -20.00 dBm\n"
		"\tSSID: \n";
	struct eink_connect_status status = {0};
	FILE *stream = fmemopen(fixture, sizeof(fixture) - 1, "r");

	assert(stream);
	wifi_results_parse(stream, &status);
	fclose(stream);
	assert(status.wifi_count == 2);
	assert(!strcmp(status.wifi[0].ssid, "Guest"));
	assert(status.wifi[0].signal == -41 && !status.wifi[0].secure);
	assert(!strcmp(status.wifi[1].ssid, "Office WiFi"));
	assert(status.wifi[1].signal == -55 && status.wifi[1].secure);
	puts("wifi_results_test: PASS");
	return 0;
}
