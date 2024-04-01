#include "base.h"

// 以下几个是常用函数：
// 添加包头
void add_header(xnet_packet_t* packet, uint16_t header_size) {
	packet->data -= header_size;
	packet->size += header_size;
}
// 移除包头
void remove_header(xnet_packet_t* packet, uint16_t header_size) {
	packet->data += header_size;
	packet->size -= header_size;
}
uint8_t netif_mac[XNET_MAC_ADDR_SIZE];
const xipaddr_t netif_ipaddr = XNET_CFG_NETIF_IP;	// 协议栈使用这个ip通信