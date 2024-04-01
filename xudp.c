#include "xudp.h"
#include <string.h>

extern const xipaddr_t netif_ipaddr;
xudp_t udp_socket[XUDP_CFG_MAX_UDP];												// ����ȫ������: UDP���ӿ�

void xudp_init(void) {
	memset(udp_socket, 0, sizeof(udp_socket));
}

// udp�����봦������udp���ƿ顢���ͷ�ip�����ݰ���
void xudp_in(xudp_t* udp, xipaddr_t* src_ip, xnet_packet_t* packet) {
	xudp_hdr_t* udp_hdr = (xudp_hdr_t*)packet->data;
	uint16_t pre_checksum;
	uint16_t src_port;

	// �򵥵� ��С��� (�ֱ�Ƚϰ�ͷ�ĳ��ȡ��ܳ���)
	if ((packet->size < sizeof(xudp_hdr_t)) || (packet->size < swap_order16(udp_hdr->total_len))) {
		return;
	}

	pre_checksum = udp_hdr->checksum;
	udp_hdr->checksum = 0;
	// �����͹����� �������0 ��˼�� ����Ҫ�����ݽ���У��͵ļ��
	if (pre_checksum != 0) {
		uint16_t checksum = checksum_peso(src_ip, &netif_ipaddr, XNET_PROTOCOL_UDP, (uint16_t*)udp_hdr, swap_order16(udp_hdr->total_len));//???
		// �����0 У���ȡ������ֹ������У��͵ļ���
		checksum = (checksum == 0) ? 0xFFFF : checksum;
		if (checksum != pre_checksum) {
			return;
		}
	}

	src_port = swap_order16(udp_hdr->src_port);
	// �Ƴ�udp��ͷ packetֻʣ��udp���ݲ��� ����Ӧ�ó�����ĵ�����
	remove_header(packet, sizeof(xudp_hdr_t));
	// ������handler�����
	if (udp->handler) {
		udp->handler(udp, src_ip, src_port, packet);
	}
}

// udp���
xnet_err_t xudp_out(xudp_t* udp, xipaddr_t* dest_ip, uint16_t dest_port, xnet_packet_t* packet) {
	// ��Ҫ������Ӱ�ͷ ����ip����ȥ
	xudp_hdr_t* udp_hdr;
	uint16_t checksum;

	add_header(packet, sizeof(xudp_hdr_t));
	udp_hdr = (xudp_hdr_t*)packet->data;
	udp_hdr->src_port = swap_order16(udp->local_port);
	udp_hdr->dest_port = swap_order16(dest_port);
	udp_hdr->total_len = swap_order16(packet->size);
	udp_hdr->checksum = 0;
	checksum = checksum_peso(&netif_ipaddr, dest_ip, XNET_PROTOCOL_UDP, (uint16_t*)packet->data, packet->size);
	udp_hdr->checksum = checksum;
	return xip_out(XNET_PROTOCOL_UDP, dest_ip, packet);
}


// ��udp���ƿ�(����ص�����)
xudp_t* xudp_open(xudp_handler_t handler) {
	// ����һ��û��ʹ�õ�

	xudp_t* udp, * end;

	for (udp = udp_socket, end = &udp_socket[XUDP_CFG_MAX_UDP]; udp < end; udp++)
	{
		if (udp->state == XUDP_STATE_FREE) {
			udp->state = XUDP_STATE_USED;
			udp->local_port = 0;
			udp->handler = handler;
			return udp;
		}
	}
	return (xudp_t*)0;
}

// �ͷ�
void xudp_close(xudp_t* udp) {
	udp->state = XUDP_STATE_FREE;
}

// ����:�յ����ݰ����жϴ�����һ��udp���ƿ��еĺ���ȥ����port��Ŀ��˿ڣ�
xudp_t* xudp_find(uint16_t port) {
	// port��ͬ����ͬ���Ļص�����
	xudp_t* curr, * end;

	for (curr = udp_socket, end = &udp_socket[XUDP_CFG_MAX_UDP]; curr < end; curr++)
	{
		if ((curr->state == XUDP_STATE_USED) && (curr->local_port == port)) {
			return curr;
		}
	}
	return (xudp_t*)0;
}

// ��:�����ƿ�ͱ��ص�ĳ���˿ڹ���()  �˿ںͻص���������
xnet_err_t xudp_bind(xudp_t* udp, uint16_t local_port) {
	// ��һ�¶˿���û�б�ʹ��
	xudp_t* curr, * end;

	for (curr = udp_socket, end = &udp_socket[XUDP_CFG_MAX_UDP]; curr < end; curr++)
	{
		if ((curr != udp) && (curr->local_port == local_port)) {
			// ˵��udp���ƿ����������Ѿ�����һ�����ƵĻص�����
			return XNET_ERR_BINDED;
		}
	}
	// û�ҵ��͸�ֵ
	udp->local_port = local_port;
	return XNET_ERR_OK;
}

