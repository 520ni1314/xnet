//#pragma once
#ifndef __XETHERNET__
#define __XETHERNET__

#include <stdint.h>
#include "base.h"


xnet_packet_t* xnet_alloc_for_send(uint16_t data_size);				// ���ͺ���
xnet_packet_t* xnet_alloc_for_read(uint16_t data_size);				// ���պ���
void truncate_packet(xnet_packet_t* packet, uint16_t size);			// �ض����ݰ�
																	// д�������� ������������ִ������
void xnet_init(void);												// ����Э��ջ��ʼ��
void xnet_poll(void);												// ��ѯ
xnet_err_t xnet_driver_open(uint8_t* mac_addr);						//�����Ĵ򿪽ӿ�
xnet_err_t xnet_driver_send(xnet_packet_t* packet);
xnet_err_t xnet_driver_read(xnet_packet_t** packet);

//static void add_header(xnet_packet_t* packet, uint16_t header_size);
//static void remove_header(xnet_packet_t* packet, uint16_t header_size);
//static void ethernet_in(xnet_packet_t* packet);
static void ethernet_in(xnet_packet_t* packet);
xnet_err_t ethernet_init(void);
void ethernet_poll(void);
xnet_err_t ethernet_out(xipaddr_t* dest_ip, xnet_packet_t* packet);
xnet_err_t ethernet_out_to(xnet_protocol_t protocol, const uint8_t* mac_addr, xnet_packet_t* packet);
#endif