//#pragma once


#ifndef __XUDP__
#define __XUDP__

#include <stdint.h>
//#include "xip.h"
#include "base.h"

#define XUDP_CFG_MAX_UDP			10






void xudp_init(void);														// ��ʼ������
xnet_err_t xudp_bind(xudp_t* udp, uint16_t local_port);						// ��
xudp_t* xudp_open(xudp_handler_t handler);									// ��udp���ƿ�(����ص�����)

xnet_err_t xudp_out(xudp_t* udp, xipaddr_t* dest_ip, uint16_t dest_port, xnet_packet_t* packet);					// udp���
void xudp_close(xudp_t* udp);												// �ر�


#endif // !__XUDP__

