#include "xarp.h"
#include "base.h"
#include "xethernet.h"


static const uint8_t ether_broadcast[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };			// �޻ر�������mac��ַ
static xarp_entry_t arp_entry;															// ����Ŀ��һ�����Ҫ��֧�ֶ��������Ҫ��������
static xnet_time_t arp_timer;

// �Ա���г�ʼ��
void xarp_init(void) {
	arp_entry.state = XARP_ENTRY_FREE;
	xnet_check_tmo(&arp_timer, 0);
}

void xarp_poll(void) {
	// ÿ��һ����ʱ��XARP_TIME_PERIOD�����һ�α���
	if (xnet_check_tmo(&arp_timer, XARP_TIME_PERIOD)) {
		// �ж�arp�����״̬
		switch (arp_entry.state)
		{
		case XARP_ENTRY_OK:
			if (--arp_entry.tmo == 0) {
				// ���½���arp��ѯ
				xarp_make_request(&arp_entry.ipaddr);
				arp_entry.state = XARP_ENTRY_PENDING;
				arp_entry.tmo = XARP_CFG_ENTRY_PENDING_TMO;
			}
			break;
		case XARP_ENTRY_PENDING:
			if (--arp_entry.tmo == 0) {
				if (arp_entry.retry_cnt-- == 0) {
					arp_entry.state = XARP_ENTRY_FREE;
				}
				else
				{
					xarp_make_request(&arp_entry.ipaddr);
					arp_entry.state = XARP_ENTRY_PENDING;
					arp_entry.tmo = XARP_CFG_ENTRY_PENDING_TMO;
				}
			}
			break;
		}
	}
}

// ��Ӧ ������һ����̫���İ����ͳ�ȥ
xnet_err_t xarp_make_response(xarp_packet_t* arp_packet) {
	xnet_packet_t* packet = xnet_alloc_for_send(sizeof(xarp_packet_t));
	xarp_packet_t* response_packet = (xarp_packet_t*)packet->data;

	response_packet->hw_type = swap_order16(XARP_HW_ETHER);	//Ӳ������
	response_packet->pro_type = swap_order16(XNET_PROTOCOL_IP);	//Э������
	response_packet->hw_len = XNET_MAC_ADDR_SIZE;	//����
	response_packet->pro_len = XNET_IPV4_ADDR_SIZE;
	response_packet->opcode = swap_order16(XARP_REPLY);
	memcpy(response_packet->send_mac, netif_mac, XNET_MAC_ADDR_SIZE);				// ����������mac
	memcpy(response_packet->send_ip, netif_ipaddr.array, XNET_IPV4_ADDR_SIZE);		// �Լ���IP
	memcpy(response_packet->target_mac, arp_packet->send_mac, XNET_MAC_ADDR_SIZE);
	memcpy(response_packet->target_ip, arp_packet->send_ip, XNET_IPV4_ADDR_SIZE);
	return ethernet_out_to(XNET_PROTOCOL_ARP, arp_packet->send_mac, packet);		// ����ָ��mac��ַ
}

// ʵ��������
int xarp_make_request(const xipaddr_t* ipaddr) {
	xnet_packet_t* packet = xnet_alloc_for_send(sizeof(xarp_packet_t));				// ���ȷ���һ���� data_size����arp���Ĵ�С
	xarp_packet_t* arp_packet = (xarp_packet_t*)packet->data;						// Ȼ����һ��arp���ݰ���ָ�룬ָ��������

	// ���arp��������
	arp_packet->hw_type = swap_order16(XARP_HW_ETHER);	//Ӳ������
	arp_packet->pro_type = swap_order16(XNET_PROTOCOL_IP);	//Э������
	arp_packet->hw_len = XNET_MAC_ADDR_SIZE;	//����
	arp_packet->pro_len = XNET_IPV4_ADDR_SIZE;
	arp_packet->opcode = swap_order16(XARP_REQUEST);
	memcpy(arp_packet->send_mac, netif_mac, XNET_MAC_ADDR_SIZE);
	memcpy(arp_packet->send_ip, netif_ipaddr.array, XNET_IPV4_ADDR_SIZE);
	memset(arp_packet->target_mac, 0, XNET_MAC_ADDR_SIZE);
	memcpy(arp_packet->target_ip, ipaddr->array, XNET_IPV4_ADDR_SIZE);
	return ethernet_out_to(XNET_PROTOCOL_ARP, ether_broadcast, packet);				// ���͹㲥��ַ
}

// Ҫ������ip��ַ���鵽��mac��ַ��ŵ�λ��
xnet_err_t xarp_resolve(const xipaddr_t* ipaddr, uint8_t** mac_addr) {
	// �ȿ�arp״̬�Ƿ�����ɹ�
	if ((arp_entry.state == XARP_ENTRY_OK) && xipaddr_is_equal(ipaddr, &arp_entry.ipaddr)) {
		// ȡ��mac��ַ
		*mac_addr = arp_entry.macaddr;
		return XNET_ERR_OK;
	}

	xarp_make_request(ipaddr);														// ���û�ҵ�
	return XNET_ERR_NONE;
}

// ��mac��ip���浽�����
void update_arp_entry(uint8_t* src_ip, uint8_t* mac_addr) {
	memcpy(arp_entry.ipaddr.array, src_ip, XNET_IPV4_ADDR_SIZE);
	memcpy(arp_entry.macaddr, mac_addr, XNET_MAC_ADDR_SIZE);
	arp_entry.state = XARP_ENTRY_OK;
	arp_entry.tmo = XARP_CFG_ENTRY_OK_TMO;											// ��һ��ʱ�䣬���������Ч��ʱ���Ƕ೤
	arp_entry.retry_cnt = XARP_CFG_MAX_RETRIES;										// ���Դ��������²�ѯ��ʱ������ѯ����
}

// arp���봦��ĺ���
void xarp_in(xnet_packet_t* packet) {
	if (packet->size >= sizeof(xarp_packet_t)) {
		xarp_packet_t* arp_packet = (xarp_packet_t*)packet->data;
		uint16_t opcode = swap_order16(arp_packet->opcode);							// ��ȡ�����루��������������Ӧ��

		if ((swap_order16(arp_packet->hw_type) != XARP_HW_ETHER) ||
			(arp_packet->hw_len != XNET_MAC_ADDR_SIZE) ||
			(swap_order16(arp_packet->pro_type) != XNET_PROTOCOL_IP) ||
			(arp_packet->pro_len != XNET_IPV4_ADDR_SIZE) ||
			((opcode != XARP_REQUEST) && (opcode != XARP_REPLY))) {		//opcode������ֵ����������Э��ջֻ��������
			return;
		}

		if (!xipaddr_is_equal_buf(&netif_ipaddr, arp_packet->target_ip)) {
			return;
		}

		// ����opcode���� ���ִ���
		switch (opcode) {
		case XARP_REQUEST:// ����
			xarp_make_response(arp_packet);											// ������Ӧ
			update_arp_entry(arp_packet->send_ip, arp_packet->send_mac);			// ���Է�mac��ip���浽�����
			break;
		case XARP_REPLY:// ��Ӧ
			update_arp_entry(arp_packet->send_ip, arp_packet->send_mac);
			break;
		}
	}
}



int xnet_check_tmo(xnet_time_t* time, uint32_t sec) {			// ��ʱ����������ǲ��ǳ�ʱ ��������һ�ε�ʱ�䣬���������೤ʱ�䳬ʱ
	xnet_time_t curr = xsys_get_time();							// ��ȡ��ǰʱ��
	if (sec == 0) {												// msΪ0 ���Ϊ ���ǻ�ȡ��ǰʱ��
		*time = curr;
		return 0;
	}
	else if (curr - *time >= sec) {
		*time = curr;
		return 1;
	}
	return 0;
}