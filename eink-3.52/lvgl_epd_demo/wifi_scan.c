#include "wifi_scan.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define WIFI_SCAN_OUTPUT "/tmp/eink-wifi-scan.txt"

static int compare_networks(const void *left, const void *right)
{
	const struct wifi_network *a = left;
	const struct wifi_network *b = right;

	return b->signal_dbm - a->signal_dbm;
}

static void parse_results(struct wifi_scan_state *scan)
{
	char line[256];
	int signal_dbm = -100;
	FILE *file = fopen(WIFI_SCAN_OUTPUT, "r");

	scan->result_count = 0;
	if (!file)
		return;
	while (fgets(line, sizeof(line), file)) {
		char *field = line;
		char *end;
		unsigned int i;

		while (*field == ' ' || *field == '\t')
			field++;
		if (!strncmp(field, "signal:", 7)) {
			signal_dbm = (int)strtol(field + 7, NULL, 10);
			continue;
		}
		if (strncmp(field, "SSID:", 5))
			continue;
		field += 5;
		while (*field == ' ' || *field == '\t')
			field++;
		end = strpbrk(field, "\r\n");
		if (end)
			*end = '\0';
		if (!*field)
			continue;
		for (i = 0; i < scan->result_count; i++) {
			if (!strcmp(scan->results[i].ssid, field)) {
				if (signal_dbm > scan->results[i].signal_dbm)
					scan->results[i].signal_dbm = signal_dbm;
				break;
			}
		}
		if (i < scan->result_count)
			continue;
		if (scan->result_count < WIFI_SCAN_MAX_RESULTS) {
			i = scan->result_count++;
		} else {
			unsigned int weakest = 0;

			for (i = 1; i < scan->result_count; i++)
				if (scan->results[i].signal_dbm < scan->results[weakest].signal_dbm)
					weakest = i;
			if (signal_dbm <= scan->results[weakest].signal_dbm)
				continue;
			i = weakest;
		}
		snprintf(scan->results[i].ssid, sizeof(scan->results[i].ssid),
			 "%.39s", field);
		scan->results[i].signal_dbm = signal_dbm;
	}
	fclose(file);
	qsort(scan->results, scan->result_count, sizeof(scan->results[0]),
	      compare_networks);
}

void wifi_scan_init(struct wifi_scan_state *scan)
{
	memset(scan, 0, sizeof(*scan));
}

int wifi_scan_start(struct wifi_scan_state *scan)
{
	pid_t pid;

	if (scan->running)
		return -EBUSY;
	unlink(WIFI_SCAN_OUTPUT);
	pid = fork();
	if (pid < 0)
		return -errno;
	if (!pid) {
		int output = open(WIFI_SCAN_OUTPUT, O_WRONLY | O_CREAT | O_TRUNC, 0600);

		if (output >= 0) {
			dup2(output, STDOUT_FILENO);
			dup2(output, STDERR_FILENO);
			close(output);
		}
		execl("/usr/sbin/iw", "iw", "dev", "wlan0", "scan", (char *)NULL);
		_exit(127);
	}
	scan->pid = pid;
	scan->running = 1;
	scan->last_status = 0;
	scan->result_count = 0;
	return 0;
}

int wifi_scan_poll(struct wifi_scan_state *scan)
{
	int status;
	pid_t result;

	if (!scan->running)
		return 0;
	result = waitpid(scan->pid, &status, WNOHANG);
	if (!result)
		return 0;
	if (result < 0) {
		scan->last_status = -errno;
	} else if (WIFEXITED(status)) {
		scan->last_status = WEXITSTATUS(status);
	} else {
		scan->last_status = -EIO;
	}
	scan->running = 0;
	scan->pid = 0;
	parse_results(scan);
	return 1;
}

void wifi_scan_close(struct wifi_scan_state *scan)
{
	if (!scan->running)
		return;
	kill(scan->pid, SIGTERM);
	waitpid(scan->pid, NULL, 0);
	scan->running = 0;
	scan->pid = 0;
}
