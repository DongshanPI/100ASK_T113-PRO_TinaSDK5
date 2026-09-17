#include "system_stats.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <linux/wireless.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

static unsigned long long previous_total;
static unsigned long long previous_idle;

static unsigned int read_cpu_percent(void)
{
	unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
	unsigned long long total, idle_total, total_delta, idle_delta;
	FILE *fp = fopen("/proc/stat", "r");

	if (!fp)
		return 0;
	if (fscanf(fp, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
		   &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) != 8) {
		fclose(fp);
		return 0;
	}
	fclose(fp);
	total = user + nice + system + idle + iowait + irq + softirq + steal;
	idle_total = idle + iowait;
	total_delta = total - previous_total;
	idle_delta = idle_total - previous_idle;
	previous_total = total;
	previous_idle = idle_total;
	if (!total_delta)
		return 0;
	return (unsigned int)((total_delta - idle_delta) * 100 / total_delta);
}

static void read_memory(struct system_stats *stats)
{
	char key[48];
	unsigned long long value;
	char unit[16];
	FILE *fp = fopen("/proc/meminfo", "r");

	if (!fp)
		return;
	while (fscanf(fp, "%47s %llu %15s", key, &value, unit) == 3) {
		if (!strcmp(key, "MemTotal:"))
			stats->memory_total_kb = value;
		else if (!strcmp(key, "MemAvailable:"))
			stats->memory_available_kb = value;
	}
	fclose(fp);
}

static void read_network(struct system_stats *stats)
{
	struct ifaddrs *addresses, *entry;

	strcpy(stats->ip_address, "--");
	strcpy(stats->interface, "--");
	if (getifaddrs(&addresses))
		return;
	for (entry = addresses; entry; entry = entry->ifa_next) {
		struct sockaddr_in *addr;

		if (!entry->ifa_addr || entry->ifa_addr->sa_family != AF_INET)
			continue;
		if (entry->ifa_flags & IFF_LOOPBACK)
			continue;
		if (!(entry->ifa_flags & IFF_UP))
			continue;
		addr = (struct sockaddr_in *)entry->ifa_addr;
		if (!inet_ntop(AF_INET, &addr->sin_addr, stats->ip_address,
			       sizeof(stats->ip_address)))
			continue;
		strncpy(stats->interface, entry->ifa_name, sizeof(stats->interface) - 1);
		stats->network_online = 1;
		break;
	}
	freeifaddrs(addresses);
}

static void read_wifi(struct system_stats *stats)
{
	struct iwreq request;
	char line[256];
	FILE *wireless;
	int socket_fd;

	strcpy(stats->wifi_ssid, "--");
	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd >= 0) {
		memset(&request, 0, sizeof(request));
		snprintf(request.ifr_name, sizeof(request.ifr_name), "wlan0");
		request.u.essid.pointer = stats->wifi_ssid;
		request.u.essid.length = sizeof(stats->wifi_ssid) - 1;
		request.u.essid.flags = 0;
		if (!ioctl(socket_fd, SIOCGIWESSID, &request) && request.u.essid.length) {
			unsigned int length = request.u.essid.length;

			if (length >= sizeof(stats->wifi_ssid))
				length = sizeof(stats->wifi_ssid) - 1;
			stats->wifi_ssid[length] = '\0';
			stats->wifi_connected = 1;
		}
		close(socket_fd);
	}

	wireless = fopen("/proc/net/wireless", "r");
	if (!wireless)
		return;
	while (fgets(line, sizeof(line), wireless)) {
		char *colon = strchr(line, ':');
		float quality;
		unsigned int flags;

		if (!colon || !strstr(line, "wlan0"))
			continue;
		if (sscanf(colon + 1, "%x %f", &flags, &quality) == 2) {
			int percent = (int)(quality * 100.0f / 70.0f);

			if (percent < 0)
				percent = 0;
			if (percent > 100)
				percent = 100;
			stats->wifi_signal_percent = percent;
		}
		break;
	}
	fclose(wireless);
}

static long read_temperature(void)
{
	unsigned int i;

	for (i = 0; i < 16; i++) {
		char path[80];
		long value;
		FILE *fp;

		snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%u/temp", i);
		fp = fopen(path, "r");
		if (!fp)
			continue;
		if (fscanf(fp, "%ld", &value) == 1) {
			fclose(fp);
			if (value > -100000 && value < 200000)
				return value;
		} else {
			fclose(fp);
		}
	}
	return -1;
}

int system_stats_collect(struct system_stats *stats)
{
	struct sysinfo info;
	struct statvfs fs;
	struct utsname uts;
	time_t now;
	double loads[1];

	memset(stats, 0, sizeof(*stats));
	if (gethostname(stats->hostname, sizeof(stats->hostname) - 1))
		strcpy(stats->hostname, "TinaLinux");
	if (!uname(&uts))
		snprintf(stats->kernel, sizeof(stats->kernel), "%.15s %.47s",
			 uts.sysname, uts.release);
	else
		strcpy(stats->kernel, "Linux --");
	if (!sysinfo(&info))
		stats->uptime_sec = info.uptime;
	if (getloadavg(loads, 1) == 1)
		stats->load_average = loads[0];
	stats->cpu_percent = read_cpu_percent();
	read_memory(stats);
	if (!statvfs("/", &fs)) {
		stats->storage_total_kb = (uint64_t)fs.f_blocks * fs.f_frsize / 1024;
		stats->storage_available_kb = (uint64_t)fs.f_bavail * fs.f_frsize / 1024;
	}
	read_network(stats);
	read_wifi(stats);
	stats->temperature_millic = read_temperature();
	now = time(NULL);
	stats->time_synchronized = now > 1609459200; /* 2021-01-01 */
	return 0;
}
