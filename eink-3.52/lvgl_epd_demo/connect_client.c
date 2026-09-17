#include "connect_client.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

void connect_status_defaults(struct eink_connect_status *status)
{
	memset(status, 0, sizeof(*status));
	status->magic = EINK_CONNECT_MAGIC;
	status->version = EINK_CONNECT_VERSION;
	status->wifi_mode = EINK_WIFI_STA;
	status->bt_state = EINK_BT_UNAVAILABLE;
	strcpy(status->ip_address, "--");
	strcpy(status->message, "connectd offline");
}

int connect_request(enum eink_connect_command command, int value,
		    const char *name, const char *secret,
		    struct eink_connect_status *status)
{
	struct sockaddr_un address;
	struct eink_connect_request request;
	int fd, error = 0;
	ssize_t size;

	memset(&request, 0, sizeof(request));
	request.magic = EINK_CONNECT_MAGIC;
	request.version = EINK_CONNECT_VERSION;
	request.command = command;
	request.value = value;
	if (name) snprintf(request.name, sizeof(request.name), "%s", name);
	if (secret) snprintf(request.secret, sizeof(request.secret), "%s", secret);
	fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
	if (fd < 0) return -errno;
	memset(&address, 0, sizeof(address));
	address.sun_family = AF_UNIX;
	strncpy(address.sun_path, EINK_CONNECT_SOCKET, sizeof(address.sun_path) - 1);
	if (connect(fd, (struct sockaddr *)&address, sizeof(address))) {
		error = -errno;
		goto done;
	}
	if (send(fd, &request, sizeof(request), MSG_NOSIGNAL) != sizeof(request)) {
		error = -errno;
		goto done;
	}
	size = recv(fd, status, sizeof(*status), 0);
	if (size != sizeof(*status) || status->magic != EINK_CONNECT_MAGIC ||
	    status->version != EINK_CONNECT_VERSION)
		error = -EPROTO;
done:
	close(fd);
	return error;
}
