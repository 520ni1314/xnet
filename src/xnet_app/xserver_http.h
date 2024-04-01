#ifndef XSERVER_HTTP_H
#define XSERVER_HTTP_H

#include "../../base.h"
#include <stdint.h>

#define XHTTP_DOC_PATH		"E:\\CodeField\\Learning-DIY-TCP_WEB-master\\htdocs"

xnet_err_t xserver_http_create(uint16_t port);
void xserver_http_run(void);

#endif // !XSERVER_HTTP_H
