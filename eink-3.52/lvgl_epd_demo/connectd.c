#define _GNU_SOURCE
#include "connect_proto.h"
#include "time_state.h"
#include "wifi_results.h"

#include <arpa/inet.h>
#include <bt_manager.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <linux/wireless.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CONFIG_PATH "/etc/eink-dashboard.conf"
#define BT_MARKER_PATH "/etc/eink-os/bluetooth.enabled"
#define PORTAL_DNS_PATH "/tmp/dnsmasq.d/eink-portal.conf"

static volatile sig_atomic_t running = 1;
static struct eink_connect_status state;
static time_t portal_deadline;
static unsigned int portal_timeout = 300;
static char time_state_path[256] = "/etc/eink-os/time.state";
static btmg_callback_t *bt_callbacks;
static void *pair_handle;
static void *bt_library;

static struct {
	int (*preinit)(btmg_callback_t **);
	int (*init)(btmg_callback_t *);
	int (*deinit)(btmg_callback_t *);
	int (*enable)(bool);
	int (*set_default_profile)(bool);
	int (*set_adapter_name)(const char *);
	int (*set_io)(btmg_io_capability_t);
	int (*set_scan_mode)(btmg_scan_mode_t);
	int (*start_scan)(void);
	int (*pair)(char *);
	int (*connect)(const char *);
	int (*disconnect)(const char *);
	int (*remove_device)(const char *);
	int (*send_pincode)(void *, char *);
	int (*send_passkey)(void *, unsigned int);
	int (*send_pair_error)(void *, btmg_pair_request_error_t, const char *);
	int (*send_empty)(void *);
} bt;

static int load_bt_api(void)
{
#define LOAD(member, symbol) do { *(void **)(&bt.member) = dlsym(bt_library, symbol); if (!bt.member) goto failed; } while (0)
	if (bt_library) return 0;
	bt_library = dlopen("libbtmg.so", RTLD_NOW | RTLD_LOCAL);
	if (!bt_library) return -ENOENT;
	LOAD(preinit, "bt_manager_preinit"); LOAD(init, "bt_manager_init"); LOAD(deinit, "bt_manager_deinit");
	LOAD(enable, "bt_manager_enable"); LOAD(set_default_profile, "bt_manager_set_default_profile");
	LOAD(set_adapter_name, "bt_manager_set_adapter_name"); LOAD(set_io, "bt_manager_agent_set_io_capability");
	LOAD(set_scan_mode, "bt_manager_set_scan_mode"); LOAD(start_scan, "bt_manager_start_scan");
	LOAD(pair, "bt_manager_pair"); LOAD(connect, "bt_manager_connect"); LOAD(disconnect, "bt_manager_disconnect");
	LOAD(remove_device, "bt_manager_remove_device"); LOAD(send_pincode, "bt_manager_agent_send_pincode");
	LOAD(send_passkey, "bt_manager_agent_send_passkey");
	LOAD(send_pair_error, "bt_manager_agent_send_pair_error"); LOAD(send_empty, "bt_manager_agent_pair_send_empty_response");
	return 0;
failed:
	dlclose(bt_library); bt_library = NULL; memset(&bt, 0, sizeof(bt)); return -ENOSYS;
#undef LOAD
}

#define bt_manager_preinit bt.preinit
#define bt_manager_init bt.init
#define bt_manager_deinit bt.deinit
#define bt_manager_enable bt.enable
#define bt_manager_set_default_profile bt.set_default_profile
#define bt_manager_set_adapter_name bt.set_adapter_name
#define bt_manager_agent_set_io_capability bt.set_io
#define bt_manager_set_scan_mode bt.set_scan_mode
#define bt_manager_start_scan bt.start_scan
#define bt_manager_pair bt.pair
#define bt_manager_connect bt.connect
#define bt_manager_disconnect bt.disconnect
#define bt_manager_remove_device bt.remove_device
#define bt_manager_agent_send_pincode bt.send_pincode
#define bt_manager_agent_send_passkey bt.send_passkey
#define bt_manager_agent_send_pair_error bt.send_pair_error
#define bt_manager_agent_pair_send_empty_response bt.send_empty

static void on_signal(int signal_number) { (void)signal_number; running = 0; }

static void load_config(void)
{
	FILE *file = fopen(CONFIG_PATH, "r"); char line[320];
	if (!file) return;
	while (fgets(line, sizeof(line), file)) {
		unsigned int timeout;
		if (sscanf(line, "provision_timeout_sec=%u", &timeout) == 1 && timeout >= 60 && timeout <= 1800) portal_timeout = timeout;
		else if (!strncmp(line, "time_state_path=", 16)) { line[strcspn(line, "\r\n")] = 0; snprintf(time_state_path, sizeof(time_state_path), "%s", line + 16); }
	}
	fclose(file);
}

static int run_argv(char *const argv[], char *output, size_t output_size)
{
	int pipes[2] = {-1, -1};
	pid_t child;
	int status;

	if (output && pipe(pipes)) return -errno;
	child = fork();
	if (child < 0) return -errno;
	if (!child) {
		if (output) {
			close(pipes[0]);
			dup2(pipes[1], STDOUT_FILENO);
			dup2(pipes[1], STDERR_FILENO);
			close(pipes[1]);
		}
		execv(argv[0], argv);
		_exit(127);
	}
	if (output) {
		ssize_t got;
		size_t used = 0;
		close(pipes[1]);
		while (used + 1 < output_size && (got = read(pipes[0], output + used, output_size - used - 1)) > 0)
			used += (size_t)got;
		output[used] = '\0';
		close(pipes[0]);
	}
	while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
	return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -EIO;
}

static void refresh_network(void)
{
	struct ifaddrs *addresses = NULL, *entry;
	struct iwreq request;
	int fd;

	state.wifi_connected = 0;
	strcpy(state.wifi_ssid, "--");
	strcpy(state.ip_address, state.wifi_mode == EINK_WIFI_PORTAL ? "192.168.5.1" : "--");
	fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (fd >= 0) {
		memset(&request, 0, sizeof(request));
		strcpy(request.ifr_name, "wlan0");
		request.u.essid.pointer = state.wifi_ssid;
		request.u.essid.length = sizeof(state.wifi_ssid) - 1;
		if (!ioctl(fd, SIOCGIWESSID, &request) && request.u.essid.length) {
			state.wifi_ssid[request.u.essid.length < sizeof(state.wifi_ssid) ? request.u.essid.length : sizeof(state.wifi_ssid) - 1] = '\0';
			state.wifi_connected = state.wifi_mode == EINK_WIFI_STA;
		}
		close(fd);
	}
	if (!getifaddrs(&addresses)) {
		for (entry = addresses; entry; entry = entry->ifa_next) {
			struct sockaddr_in *address;
			if (!entry->ifa_addr || strcmp(entry->ifa_name, "wlan0") || entry->ifa_addr->sa_family != AF_INET)
				continue;
			address = (struct sockaddr_in *)entry->ifa_addr;
			inet_ntop(AF_INET, &address->sin_addr, state.ip_address, sizeof(state.ip_address));
			break;
		}
		freeifaddrs(addresses);
	}
}

static int scan_wifi(void)
{
	int pipes[2], status, waited;
	pid_t child;
	FILE *stream;

	if (state.wifi_mode == EINK_WIFI_PORTAL) {
		snprintf(state.message, sizeof(state.message), "Cannot scan while hotspot is active");
		return -EBUSY;
	}
	if (pipe(pipes))
		return -errno;
	child = fork();
	if (child < 0) {
		close(pipes[0]); close(pipes[1]);
		return -errno;
	}
	if (!child) {
		close(pipes[0]);
		dup2(pipes[1], STDOUT_FILENO);
		dup2(pipes[1], STDERR_FILENO);
		close(pipes[1]);
		execl("/usr/sbin/iw", "iw", "dev", "wlan0", "scan", (char *)NULL);
		_exit(127);
	}
	close(pipes[1]);
	stream = fdopen(pipes[0], "r");
	if (!stream) {
		close(pipes[0]);
		waitpid(child, &status, 0);
		return -errno;
	}
	wifi_results_parse(stream, &state);
	fclose(stream);
	do {
		waited = waitpid(child, &status, 0);
	} while (waited < 0 && errno == EINTR);
	if (waited < 0) {
		snprintf(state.message, sizeof(state.message), "WiFi scan failed");
		return -errno;
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
		snprintf(state.message, sizeof(state.message), "WiFi scan failed");
		return -EIO;
	}
	snprintf(state.message, sizeof(state.message), "%u network(s)", state.wifi_count);
	return 0;
}

static int set_captive_dns(int enabled)
{
	char *restart[] = {"/etc/init.d/dnsmasq", "restart", NULL};
	FILE *file;

	if (!enabled) {
		unlink(PORTAL_DNS_PATH);
		return run_argv(restart, NULL, 0);
	}
	if (mkdir("/tmp/dnsmasq.d", 0755) && errno != EEXIST)
		return -errno;
	file = fopen(PORTAL_DNS_PATH, "w");
	if (!file)
		return -errno;
	fputs("address=/#/192.168.5.1\n"
	      "dhcp-option=3,192.168.5.1\n"
	      "dhcp-option=6,192.168.5.1\n", file);
	if (fclose(file))
		return -errno;
	return run_argv(restart, NULL, 0);
}

static void stop_portal(int resume_sta)
{
	char *off[] = {"/bin/wifi", "-f", "ap", NULL};
	char *clear_ip[] = {"/sbin/ip", "addr", "flush", "dev", "wlan0", NULL};
	char *sta[] = {"/bin/wifi", "-o", "sta", NULL};
	run_argv(off, NULL, 0);
	/* The vendor AP teardown can leave 192.168.5.1 on wlan0. */
	run_argv(clear_ip, NULL, 0);
	set_captive_dns(0);
	portal_deadline = 0;
	state.portal_ssid[0] = '\0';
	state.portal_password[0] = '\0';
	if (resume_sta) run_argv(sta, NULL, 0);
	state.wifi_mode = resume_sta ? EINK_WIFI_STA : EINK_WIFI_OFF;
}

static void random_password(char output[16])
{
	unsigned int value;
	if (getrandom(&value, sizeof(value), 0) != sizeof(value))
		value = (unsigned int)(time(NULL) ^ getpid());
	snprintf(output, 16, "%08u", value % 100000000U);
}

static void read_mac_suffix(char suffix[5])
{
	FILE *file = fopen("/sys/class/net/wlan0/address", "r");
	char mac[32] = {0};
	strcpy(suffix, "0000");
	if (file && fgets(mac, sizeof(mac), file)) {
		char compact[13] = {0};
		unsigned int i, j = 0;
		for (i = 0; mac[i] && j < 12; i++) if (isxdigit((unsigned char)mac[i])) compact[j++] = toupper((unsigned char)mac[i]);
		if (j >= 4) snprintf(suffix, 5, "%s", compact + j - 4);
	}
	if (file) fclose(file);
}

static int start_portal(void)
{
	char suffix[5];
	char *sta_off[] = {"/bin/wifi", "-f", "sta", NULL};
	char *ap[8];

	read_mac_suffix(suffix);
	scan_wifi();
	snprintf(state.portal_ssid, sizeof(state.portal_ssid), "EINKOS-%s", suffix);
	random_password(state.portal_password);
	run_argv(sta_off, NULL, 0);
	ap[0] = "/bin/wifi"; ap[1] = "-o"; ap[2] = "ap"; ap[3] = state.portal_ssid;
	ap[4] = state.portal_password; ap[5] = "6"; ap[6] = "wpa2"; ap[7] = NULL;
	if (run_argv(ap, NULL, 0)) {
		stop_portal(1);
		snprintf(state.message, sizeof(state.message), "Cannot start hotspot");
		return -EIO;
	}
	state.wifi_mode = EINK_WIFI_PORTAL;
	portal_deadline = time(NULL) + portal_timeout;
	if (set_captive_dns(1))
		snprintf(state.message, sizeof(state.message), "Open http://192.168.5.1");
	else
		snprintf(state.message, sizeof(state.message), "%u network(s); portal ready", state.wifi_count);
	return 0;
}

static int valid_ssid(const char *ssid) { size_t n = strlen(ssid); return n >= 1 && n <= 32; }
static int valid_password(const char *password) { size_t n = strlen(password); return n == 0 || (n >= 8 && n <= 63); }

static void url_decode(char *text)
{
	char *read = text, *write = text;
	while (*read) {
		if (*read == '+') { *write++ = ' '; read++; }
		else if (*read == '%' && isxdigit((unsigned char)read[1]) && isxdigit((unsigned char)read[2])) {
			char hex[3] = {read[1], read[2], 0};
			*write++ = (char)strtol(hex, NULL, 16); read += 3;
		} else *write++ = *read++;
	}
	*write = '\0';
}

static void form_value(char *body, const char *key, char *out, size_t size)
{
	char *token, *save;
	out[0] = '\0';
	for (token = strtok_r(body, "&", &save); token; token = strtok_r(NULL, "&", &save)) {
		char *equal = strchr(token, '=');
		if (!equal) continue;
		*equal++ = '\0';
		if (!strcmp(token, key)) { snprintf(out, size, "%s", equal); url_decode(out); return; }
	}
}

static int provision(const char *ssid, const char *password, const char *epoch_text)
{
	char *sta[] = {"/bin/wifi", "-o", "sta", NULL};
	char *connect_open[] = {"/bin/wifi", "-c", (char *)ssid, NULL};
	char *connect_secure[] = {"/bin/wifi", "-c", (char *)ssid, (char *)password, NULL};
	long long epoch = strtoll(epoch_text ? epoch_text : "0", NULL, 10);

	if (!valid_ssid(ssid) || !valid_password(password)) return -EINVAL;
	stop_portal(0);
	run_argv(sta, NULL, 0);
	state.wifi_mode = EINK_WIFI_STA;
	if (run_argv(password[0] ? connect_secure : connect_open, NULL, 0)) {
		start_portal();
		snprintf(state.message, sizeof(state.message), "Connection failed; hotspot restored");
		return -EIO;
	}
	if (epoch > 1609459200LL) time_state_set(time_state_path, (time_t)epoch);
	{
		char *ntpd[] = {"/usr/sbin/ntpd", "-q", "-p", "pool.ntp.org", NULL};
		run_argv(ntpd, NULL, 0);
	}
	time_state_save(time_state_path, time(NULL));
	snprintf(state.message, sizeof(state.message), "Connected to %.32s", ssid);
	return 0;
}

static void http_reply(int client, const char *status, const char *body)
{
	char header[256];
	int length = snprintf(header, sizeof(header), "HTTP/1.1 %s\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", status, strlen(body));
	send(client, header, length, MSG_NOSIGNAL);
	send(client, body, strlen(body), MSG_NOSIGNAL);
}

static void html_escape(const char *input, char *output, size_t size)
{
	size_t used = 0;

	while (*input && used + 1 < size) {
		const char *replacement = NULL;
		size_t length;

		switch (*input) {
		case '&': replacement = "&amp;"; break;
		case '<': replacement = "&lt;"; break;
		case '>': replacement = "&gt;"; break;
		case '"': replacement = "&quot;"; break;
		case '\'': replacement = "&#39;"; break;
		default: output[used++] = *input++; continue;
		}
		length = strlen(replacement);
		if (used + length >= size)
			break;
		memcpy(output + used, replacement, length);
		used += length;
		input++;
	}
	output[used] = '\0';
}

static void serve_http(int listener)
{
	char request[4096], page[8192], networks[4096] = {0};
	int client = accept4(listener, NULL, NULL, SOCK_CLOEXEC);
	ssize_t got;
	unsigned int i;
	if (client < 0) return;
	got = recv(client, request, sizeof(request) - 1, 0);
	if (got <= 0) { close(client); return; }
	request[got] = '\0';
	if (!strncmp(request, "POST ", 5)) {
		char *body = strstr(request, "\r\n\r\n");
		char ssid[64], manual_ssid[64], password[80], epoch[32];
		char copy1[2048], copy2[2048], copy3[2048], copy4[2048];
		if (!body) { http_reply(client, "400 Bad Request", "Bad request"); close(client); return; }
		body += 4;
		snprintf(copy1, sizeof(copy1), "%s", body); snprintf(copy2, sizeof(copy2), "%s", body); snprintf(copy3, sizeof(copy3), "%s", body); snprintf(copy4, sizeof(copy4), "%s", body);
		form_value(copy1, "ssid", ssid, sizeof(ssid));
		form_value(copy2, "manual_ssid", manual_ssid, sizeof(manual_ssid));
		form_value(copy3, "password", password, sizeof(password));
		form_value(copy4, "epoch", epoch, sizeof(epoch));
		if (manual_ssid[0]) snprintf(ssid, sizeof(ssid), "%s", manual_ssid);
		if (!provision(ssid, password, epoch))
			http_reply(client, "200 OK", "<meta charset=utf-8><h1>连接成功</h1><p>设备正在切回 WiFi。</p>");
		else
			http_reply(client, "400 Bad Request", "<meta charset=utf-8><h1>连接失败</h1><p>请检查名称和密码后重试。</p>");
		close(client); return;
	}
	for (i = 0; i < state.wifi_count; i++) {
		char option[320], escaped[200];
		html_escape(state.wifi[i].ssid, escaped, sizeof(escaped));
		snprintf(option, sizeof(option), "<option value=\"%s\">%s · %ddBm · %s</option>", escaped, escaped, state.wifi[i].signal, state.wifi[i].secure ? "加密" : "开放");
		strncat(networks, option, sizeof(networks) - strlen(networks) - 1);
	}
	if (!state.wifi_count)
		snprintf(networks, sizeof(networks), "<option value=\"\">未扫描到网络，请手动输入</option>");
	snprintf(page, sizeof(page), "<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'><title>EINK OS 配网</title><style>body{font:18px sans-serif;max-width:32em;margin:1.5em auto;padding:0 1em;color:#222}h1{font-size:1.5em}label{display:block;margin-top:.8em}input,select,button{font:inherit;width:100%%;padding:.75em;margin:.3em 0;box-sizing:border-box;border:1px solid #888;border-radius:.35em}button{background:#111;color:#fff;border:0;margin-top:1em}.hint{color:#666;font-size:.85em}</style></head><body><h1>EINK OS WiFi 配网</h1><p>已自动扫描附近网络，默认选择信号最强的 WiFi。</p><form method=post action=/><label>选择 WiFi</label><select name=ssid>%s</select><label>其他或隐藏网络</label><input name=manual_ssid maxlength=32 placeholder='可选，填写后优先使用'><label>WiFi 密码</label><input name=password type=password maxlength=63 placeholder='开放网络留空'><input id=e name=epoch type=hidden><button type=submit>连接设备</button></form><p class=hint>若手机没有自动弹出此页面，请访问 http://192.168.5.1</p><script>e.value=Math.floor(Date.now()/1000)</script></body></html>", networks);
	http_reply(client, "200 OK", page);
	close(client);
}

static void bt_adapter_state(btmg_adapter_state_t adapter)
{
	state.bt_state = adapter == BTMG_ADAPTER_ON ? EINK_BT_ON : EINK_BT_OFF;
	if (adapter == BTMG_ADAPTER_ON) {
		bt_manager_set_adapter_name("EINK OS");
		bt_manager_agent_set_io_capability(BTMG_IO_CAP_DISPLAYYESNO);
		bt_manager_set_scan_mode(BTMG_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
	}
}
static void bt_scan_state(btmg_scan_state_t scan)
{
	state.bt_state = scan == BTMG_SCAN_STARTED ? EINK_BT_SCANNING : EINK_BT_ON;
}
static void bt_device_add(btmg_bt_device_t *device)
{
	unsigned int i;
	if (!device || !device->remote_address) return;
	for (i = 0; i < state.bt_count; i++) if (!strcmp(state.bt[i].address, device->remote_address)) break;
	if (i == EINK_CONNECT_MAX_BT) return;
	if (i == state.bt_count) state.bt_count++;
	snprintf(state.bt[i].address, sizeof(state.bt[i].address), "%s", device->remote_address);
	snprintf(state.bt[i].name, sizeof(state.bt[i].name), "%s", device->remote_name ? device->remote_name : device->remote_address);
	state.bt[i].rssi = device->rssi; state.bt[i].paired = device->paired; state.bt[i].connected = device->connected;
}
static void bt_bond_state(btmg_bond_state_t bond, const char *address)
{
	unsigned int i;
	for (i = 0; i < state.bt_count; i++) if (!strcmp(state.bt[i].address, address)) state.bt[i].paired = bond == BTMG_BOND_STATE_BONDED;
	if (bond == BTMG_BOND_STATE_BONDED) { FILE *f = fopen(BT_MARKER_PATH, "w"); if (f) { fputs("1\n", f); fclose(f); } }
}
static void bt_confirm(void *handle, char *device, unsigned int passkey)
{
	pair_handle = handle; state.pair_pending = 1; state.pair_input = 0; state.pair_passkey = passkey;
	snprintf(state.pair_device, sizeof(state.pair_device), "%s", device ? device : "Bluetooth device");
}
static void bt_request_pin(void *handle, char *device)
{
	bt_confirm(handle, device, 0); state.pair_input = 1;
}
static void bt_request_passkey(void *handle, char *device)
{
	bt_confirm(handle, device, 0); state.pair_input = 2;
}
static void bt_display_pin(char *device, char *pin)
{
	state.pair_pending = 1; state.pair_input = 0; state.pair_passkey = (uint32_t)strtoul(pin ? pin : "0", NULL, 10);
	snprintf(state.pair_device, sizeof(state.pair_device), "%s", device ? device : "Bluetooth device");
}
static void bt_display_passkey(char *device, unsigned int passkey, unsigned int entered)
{ (void)entered; bt_confirm(NULL, device, passkey); }
static void bt_authorize(void *handle, char *device) { bt_confirm(handle, device, 0); }
static void bt_authorize_service(void *handle, char *device, char *uuid) { (void)uuid; bt_confirm(handle, device, 0); }

static int bt_init(void)
{
	if (bt_callbacks) return 0;
	if (access("/dev/ttyS1", R_OK | W_OK)) { state.bt_state = EINK_BT_UNAVAILABLE; return -ENODEV; }
	if (load_bt_api()) { state.bt_state = EINK_BT_UNAVAILABLE; return -ENOSYS; }
	if (bt_manager_preinit(&bt_callbacks)) return -EIO;
	bt_callbacks->btmg_adapter_cb.adapter_state_cb = bt_adapter_state;
	bt_callbacks->btmg_gap_cb.gap_scan_status_cb = bt_scan_state;
	bt_callbacks->btmg_gap_cb.gap_device_add_cb = bt_device_add;
	bt_callbacks->btmg_gap_cb.gap_bond_state_cb = bt_bond_state;
	bt_callbacks->btmg_agent_cb.agent_request_pincode = bt_request_pin;
	bt_callbacks->btmg_agent_cb.agent_display_pincode = bt_display_pin;
	bt_callbacks->btmg_agent_cb.agent_request_passkey = bt_request_passkey;
	bt_callbacks->btmg_agent_cb.agent_display_passkey = bt_display_passkey;
	bt_callbacks->btmg_agent_cb.agent_request_confirm_passkey = bt_confirm;
	bt_callbacks->btmg_agent_cb.agent_request_authorize = bt_authorize;
	bt_callbacks->btmg_agent_cb.agent_authorize_service = bt_authorize_service;
	bt_manager_set_default_profile(true);
	if (bt_manager_init(bt_callbacks)) { bt_callbacks = NULL; return -EIO; }
	return 0;
}

static int handle_command(const struct eink_connect_request *request)
{
	unsigned int index = (unsigned int)request->value;
	int result = 0;
	char *disconnect[] = {"/bin/wifi", "-d", NULL};
	char *forget[] = {"/bin/wifi", "-r", request->name[0] ? (char *)request->name : "all", NULL};

	switch (request->command) {
	case EINK_CMD_STATUS: break;
	case EINK_CMD_WIFI_SCAN: result = scan_wifi(); break;
	case EINK_CMD_WIFI_PORTAL: result = start_portal(); break;
	case EINK_CMD_WIFI_CANCEL: stop_portal(1); break;
	case EINK_CMD_WIFI_DISCONNECT: result = run_argv(disconnect, NULL, 0); break;
	case EINK_CMD_WIFI_FORGET: result = run_argv(forget, NULL, 0); break;
	case EINK_CMD_BT_POWER:
		result = bt_init(); if (!result) result = bt_manager_enable(request->value != 0); break;
	case EINK_CMD_BT_SCAN:
		result = bt_init(); if (!result) { state.bt_count = 0; result = bt_manager_start_scan(); } break;
	case EINK_CMD_BT_PAIR:
		result = index < state.bt_count ? bt_manager_pair(state.bt[index].address) : -EINVAL; break;
	case EINK_CMD_BT_CONFIRM:
		if (!state.pair_pending) result = -EINVAL;
		else if (request->value) {
			if (pair_handle && state.pair_input == 2) result = bt_manager_agent_send_passkey(pair_handle, (unsigned int)strtoul(request->secret, NULL, 10));
			else if (pair_handle && state.pair_input == 1) result = bt_manager_agent_send_pincode(pair_handle, (char *)request->secret);
			else if (pair_handle) result = bt_manager_agent_pair_send_empty_response(pair_handle);
		} else if (pair_handle) result = bt_manager_agent_send_pair_error(pair_handle, BT_PAIR_REQUEST_REJECTED, "Rejected by user");
		state.pair_pending = 0; state.pair_input = 0; pair_handle = NULL; break;
	case EINK_CMD_BT_CONNECT: result = index < state.bt_count ? bt_manager_connect(state.bt[index].address) : -EINVAL; break;
	case EINK_CMD_BT_DISCONNECT: result = index < state.bt_count ? bt_manager_disconnect(state.bt[index].address) : -EINVAL; break;
	case EINK_CMD_BT_REMOVE: result = index < state.bt_count ? bt_manager_remove_device(state.bt[index].address) : -EINVAL; break;
	default: result = -ENOSYS;
	}
	return result;
}

static void serve_control(int listener)
{
	struct eink_connect_request request;
	int client = accept4(listener, NULL, NULL, SOCK_CLOEXEC);
	ssize_t got;
	if (client < 0) return;
	got = recv(client, &request, sizeof(request), 0);
	state.result = -EPROTO;
	if (got == sizeof(request) && request.magic == EINK_CONNECT_MAGIC && request.version == EINK_CONNECT_VERSION)
		state.result = handle_command(&request);
	refresh_network();
	if (portal_deadline > time(NULL)) state.portal_seconds_left = (uint16_t)(portal_deadline - time(NULL));
	else state.portal_seconds_left = 0;
	send(client, &state, sizeof(state), MSG_NOSIGNAL);
	close(client);
}

static int unix_listener(void)
{
	struct sockaddr_un address;
	int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
	if (fd < 0) return -1;
	unlink(EINK_CONNECT_SOCKET);
	memset(&address, 0, sizeof(address)); address.sun_family = AF_UNIX;
	strcpy(address.sun_path, EINK_CONNECT_SOCKET);
	if (bind(fd, (struct sockaddr *)&address, sizeof(address)) || listen(fd, 8)) { close(fd); return -1; }
	chmod(EINK_CONNECT_SOCKET, 0660);
	return fd;
}

static int http_listener(void)
{
	struct sockaddr_in address;
	int yes = 1, fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) return -1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	memset(&address, 0, sizeof(address)); address.sin_family = AF_INET; address.sin_port = htons(80); address.sin_addr.s_addr = INADDR_ANY;
	if (bind(fd, (struct sockaddr *)&address, sizeof(address)) || listen(fd, 4)) { close(fd); return -1; }
	return fd;
}

int main(void)
{
	struct pollfd descriptors[2];
	int control, http;

	signal(SIGINT, on_signal); signal(SIGTERM, on_signal); signal(SIGPIPE, SIG_IGN); load_config();
	memset(&state, 0, sizeof(state)); state.magic = EINK_CONNECT_MAGIC; state.version = EINK_CONNECT_VERSION;
	state.wifi_mode = EINK_WIFI_STA; state.bt_state = access("/dev/ttyS1", F_OK) ? EINK_BT_UNAVAILABLE : EINK_BT_OFF;
	control = unix_listener(); http = http_listener();
	if (control < 0) return 1;
	if (!access(BT_MARKER_PATH, F_OK) && !bt_init()) bt_manager_enable(true);
	descriptors[0].fd = control; descriptors[0].events = POLLIN;
	descriptors[1].fd = http; descriptors[1].events = POLLIN;
	while (running) {
		int result = poll(descriptors, http >= 0 ? 2 : 1, 1000);
		if (result > 0 && (descriptors[0].revents & POLLIN)) serve_control(control);
		if (result > 0 && http >= 0 && (descriptors[1].revents & POLLIN)) serve_http(http);
		if (portal_deadline && time(NULL) >= portal_deadline) { stop_portal(1); snprintf(state.message, sizeof(state.message), "Hotspot timed out"); }
		refresh_network();
	}
	if (portal_deadline) stop_portal(1);
	if (bt_callbacks) { bt_manager_enable(false); bt_manager_deinit(bt_callbacks); }
	if (bt_library) dlclose(bt_library);
	if (http >= 0) close(http);
	close(control);
	unlink(EINK_CONNECT_SOCKET);
	return 0;
}
