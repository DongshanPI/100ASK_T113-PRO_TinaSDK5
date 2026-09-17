#ifndef DASHBOARD_SYSTEM_STATS_H
#define DASHBOARD_SYSTEM_STATS_H

#include <stdint.h>

struct system_stats {
	char hostname[64];
	char kernel[64];
	char ip_address[48];
	char interface[24];
	char wifi_ssid[40];
	int wifi_connected;
	int wifi_signal_percent;
	int network_online;
	int time_synchronized;
	double load_average;
	unsigned int cpu_percent;
	uint64_t memory_total_kb;
	uint64_t memory_available_kb;
	uint64_t storage_total_kb;
	uint64_t storage_available_kb;
	long temperature_millic;
	double uptime_sec;
};

int system_stats_collect(struct system_stats *stats);

#endif
