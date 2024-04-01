#include "base.h"

// ���¼����ǳ��ú�����
// ��Ӱ�ͷ
void add_header(xnet_packet_t* packet, uint16_t header_size) {
	packet->data -= header_size;
	packet->size += header_size;
}
// �Ƴ���ͷ
void remove_header(xnet_packet_t* packet, uint16_t header_size) {
	packet->data += header_size;
	packet->size -= header_size;
}
uint8_t netif_mac[XNET_MAC_ADDR_SIZE];
const xipaddr_t netif_ipaddr = XNET_CFG_NETIF_IP;	// Э��ջʹ�����ipͨ��