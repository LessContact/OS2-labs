#ifndef PROXY_H
#define PROXY_H

#include <stdint.h>
#include "../threading/threadpool.h"

void proxy_start(const uint16_t server_port);
void process_request(connection_ctx_t *conn);

#endif
