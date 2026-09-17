#ifndef EINK_CONNECT_PROTO_H
#define EINK_CONNECT_PROTO_H

#include <stdint.h>

#define EINK_CONNECT_SOCKET "/var/run/eink-connectd.sock"
#define EINK_CONNECT_MAGIC 0x45494e4bU
#define EINK_CONNECT_VERSION 1U
#define EINK_CONNECT_MAX_WIFI 8
#define EINK_CONNECT_MAX_BT 8

enum eink_connect_command {
	EINK_CMD_STATUS = 1,
	EINK_CMD_WIFI_SCAN,
	EINK_CMD_WIFI_PORTAL,
	EINK_CMD_WIFI_CANCEL,
	EINK_CMD_WIFI_DISCONNECT,
	EINK_CMD_WIFI_FORGET,
	EINK_CMD_BT_POWER,
	EINK_CMD_BT_SCAN,
	EINK_CMD_BT_PAIR,
	EINK_CMD_BT_CONFIRM,
	EINK_CMD_BT_CONNECT,
	EINK_CMD_BT_DISCONNECT,
	EINK_CMD_BT_REMOVE,
};

enum eink_wifi_mode { EINK_WIFI_OFF, EINK_WIFI_STA, EINK_WIFI_PORTAL };
enum eink_bt_state { EINK_BT_UNAVAILABLE, EINK_BT_OFF, EINK_BT_ON, EINK_BT_SCANNING };

struct eink_connect_request {
	uint32_t magic;
	uint16_t version;
	uint16_t command;
	int32_t value;
	char name[64];
	char secret[64];
};

struct eink_wifi_network {
	char ssid[33];
	int16_t signal;
	uint8_t secure;
	uint8_t reserved;
};

struct eink_bt_device {
	char address[18];
	char name[48];
	int16_t rssi;
	uint8_t paired;
	uint8_t connected;
};

struct eink_connect_status {
	uint32_t magic;
	uint16_t version;
	int16_t result;
	uint8_t wifi_mode;
	uint8_t wifi_connected;
	uint8_t wifi_count;
	uint8_t bt_state;
	uint8_t bt_count;
	uint8_t pair_pending;
	uint8_t pair_input; /* 0 confirm, 1 PIN string, 2 numeric passkey */
	uint8_t pair_cursor;
	uint16_t portal_seconds_left;
	uint32_t pair_passkey;
	char wifi_ssid[33];
	char ip_address[40];
	char portal_ssid[33];
	char portal_password[16];
	char pair_device[48];
	char message[80];
	struct eink_wifi_network wifi[EINK_CONNECT_MAX_WIFI];
	struct eink_bt_device bt[EINK_CONNECT_MAX_BT];
};

#endif
