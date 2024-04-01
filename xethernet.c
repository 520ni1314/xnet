#include "xethernet.h"
#include <stdlib.h>

xnet_packet_t tx_packet, rx_packet;

xnet_packet_t* xnet_alloc_for_send(uint16_t data_size) {								// ���ͺ���
	tx_packet.data = tx_packet.payload + XNET_CFG_PACKET_MAX_SIZE - data_size;
	tx_packet.size = data_size;
	return &tx_packet;
}

xnet_packet_t* xnet_alloc_for_read(uint16_t data_size) {								// ���պ���
	rx_packet.data = rx_packet.payload;
	rx_packet.size = data_size;
	return &rx_packet;
}


void truncate_packet(xnet_packet_t* packet, uint16_t size) {							// �ضϰ��ĳ��� ��packet�ض�Ϊsize��С
	packet->size = min(packet->size, size);												// ѡ��С�ģ���ǰ�����趨��С��
}


xnet_err_t ethernet_init(void) {													// ��̫���ĳ�ʼ��
	xnet_err_t err = xnet_driver_open(netif_mac);
	if (err < 0) {
		return err;
	}
	return xarp_make_request(&netif_ipaddr);											// ��ʼ�����֮��ֱ�ӷ���arp������������Լ������ַ
}


void ethernet_poll(void) {									// ʵ��һ����̫����ѯ����������û����̫����������о͵���һ�£������������
	xnet_packet_t* packet;
	if (xnet_driver_read(&packet) == XNET_ERR_OK) {
		ethernet_in(packet);										// ��̫�������봦�������������������ݽ���ip���arp�㴦��
	}
}



// ���ͺ�������IP����ARP�� ͨ����̫�����ͳ�ȥ
xnet_err_t ethernet_out_to(xnet_protocol_t protocol, const uint8_t* mac_addr, xnet_packet_t* packet) {
	xether_hdr_t* ether_hdr;

	add_header(packet, sizeof(xether_hdr_t));
	ether_hdr = (xether_hdr_t*)packet->data;
	memcpy(ether_hdr->dest, mac_addr, XNET_MAC_ADDR_SIZE);
	memcpy(ether_hdr->src, netif_mac, XNET_MAC_ADDR_SIZE);
	ether_hdr->protocol = swap_order16(protocol);

	return xnet_driver_send(packet);
}

// ��̫���ķ��ͺ���(Ŀ��ip��ַ�����͵İ�)
xnet_err_t ethernet_out(xipaddr_t* dest_ip, xnet_packet_t* packet) {
	// Ҫ�뷢�ͣ���Ҫ�����Ŀ��ip��ַתΪmac��ַ��Ҫת������Ҫ�õ�ǰ��ʵ�ֵ�arpЭ��ʵ�ֵ�ַת��
	xnet_err_t err;
	uint8_t* mac_addr;

	// ��һ������Ŀ��ip	�ڶ����������鵽��mac��ַд�� mac_addr ����
	if ((err = xarp_resolve(dest_ip, &mac_addr)) == XNET_ERR_OK) {
		// ������̫�����ͺ���
		return ethernet_out_to(XNET_PROTOCOL_IP, mac_addr, packet);
	}

	// �������� or û���ҵ�mac��ַ
	return err;
}

// ��һ������
static void ethernet_in(xnet_packet_t* packet) {
	xether_hdr_t* ether_hdr;
	uint16_t protocol;

	// �򵥵ļ�飬�жϰ��Ĵ�С���Ȱ�ͷС��û�����ݣ����˳�
	if (packet->size <= sizeof(xether_hdr_t)) {
		return;
	}

	ether_hdr = (xether_hdr_t*)packet->data;
	//���->С��
	protocol = swap_order16(ether_hdr->protocol);
	// ����Э�����ͽ�����ͬ���ϲ�Э�飨IP��ARP��
	switch (protocol)
	{
	case XNET_PROTOCOL_ARP:
		remove_header(packet, sizeof(xether_hdr_t));
		xarp_in(packet);
		break;
	case XNET_PROTOCOL_IP:
		// ����д(Ϊ����arp��)
		// �ҵ�ip��ͷ
		xip_hdr_t* iphdr = (xip_hdr_t*)(packet->data + sizeof(xether_hdr_t));
		update_arp_entry(iphdr->src_ip, ether_hdr->src);
		// û����·�����ͷ�Ƭ
		remove_header(packet, sizeof(xether_hdr_t));
		xip_in(packet);
		break;
	}
}