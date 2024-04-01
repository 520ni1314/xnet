//#pragma once


#ifndef __XUDP__
#define __XUDP__

#include <stdint.h>
//#include "xip.h"
#include "base.h"

#define XUDP_CFG_MAX_UDP			10






void xudp_init(void);														// 初始化函数
xnet_err_t xudp_bind(xudp_t* udp, uint16_t local_port);						// 绑定
xudp_t* xudp_open(xudp_handler_t handler);									// 打开udp控制块(传入回调函数)

xnet_err_t xudp_out(xudp_t* udp, xipaddr_t* dest_ip, uint16_t dest_port, xnet_packet_t* packet);					// udp输出
void xudp_close(xudp_t* udp);												// 关闭


#endif // !__XUDP__

