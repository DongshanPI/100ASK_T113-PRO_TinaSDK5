#include "connect_proto.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	struct eink_connect_request request = {0}; struct eink_connect_status status = {0};
	request.magic = EINK_CONNECT_MAGIC; request.version = EINK_CONNECT_VERSION; request.command = EINK_CMD_WIFI_PORTAL;
	assert(request.magic == 0x45494e4bU); assert(sizeof(request.name) == 64); assert(sizeof(status.wifi) / sizeof(status.wifi[0]) == EINK_CONNECT_MAX_WIFI);
	strcpy(request.name, "ssid"); assert(!strcmp(request.name, "ssid")); puts("protocol_test: PASS"); return 0;
}
