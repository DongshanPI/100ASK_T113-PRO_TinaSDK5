#ifndef EINK_WIFI_RESULTS_H
#define EINK_WIFI_RESULTS_H

#include "connect_proto.h"

#include <stdio.h>

void wifi_results_parse(FILE *stream, struct eink_connect_status *status);

#endif
