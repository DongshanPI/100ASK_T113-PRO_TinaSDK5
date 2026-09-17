#ifndef EINK_WIFI_SCAN_H
#define EINK_WIFI_SCAN_H

#include <sys/types.h>

#define WIFI_SCAN_MAX_RESULTS 6

struct wifi_network {
	char ssid[40];
	int signal_dbm;
};

struct wifi_scan_state {
	pid_t pid;
	int running;
	int last_status;
	unsigned int result_count;
	struct wifi_network results[WIFI_SCAN_MAX_RESULTS];
};

void wifi_scan_init(struct wifi_scan_state *scan);
int wifi_scan_start(struct wifi_scan_state *scan);
int wifi_scan_poll(struct wifi_scan_state *scan);
void wifi_scan_close(struct wifi_scan_state *scan);

#endif
