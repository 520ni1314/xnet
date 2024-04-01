//#pragma once
#ifndef __XETHERNET__
#define __XETHERNET__

#include <stdint.h>
#include "base.h"


xnet_packet_t* xnet_alloc_for_send(uint16_t data_size);				// 发送函数
xnet_packet_t* xnet_alloc_for_read(uint16_t data_size);				// 接收函数
void truncate_packet(xnet_packet_t* packet, uint16_t size);			// 截断数据包
																	// 写两个函数 控制整个程序执行流程
void xnet_init(void);												// 控制协议栈初始化
void xnet_poll(void);												// 查询
xnet_err_t xnet_driver_open(uint8_t* mac_addr);						//驱动的打开接口
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