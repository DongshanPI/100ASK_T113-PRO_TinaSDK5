#ifndef EINK_CONNECT_CLIENT_H
#define EINK_CONNECT_CLIENT_H

#include "connect_proto.h"

void connect_status_defaults(struct eink_connect_status *status);
int connect_request(enum eink_connect_command command, int value,
		    const char *name, const char *secret,
		    struct eink_connect_status *status);

#endif
