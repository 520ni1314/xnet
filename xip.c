#include "xip.h"
#include "base.h"

extern const xipaddr_t netif_ipaddr;

// ipЭ��ĳ�ʼ��
void xip_init(void) {

}

// ����Ĵ�������̫���Ĵ������е���
void xip_in(xnet_packet_t* packet) {
	// �����������һ�������ļ�飬����ͷ
	xip_hdr_t* iphdr = (xip_hdr_t*)packet->data;
	uint32_t header_size, total_size;
	uint16_t pre_checksum;
	xipaddr_t src_ip;

	// �汾����
	if (iphdr->version != XNET_VERSION_IPV4) {
		return;
	}

	// ��ͷ�ĳ����� 4�ֽ� Ϊ��λ
	header_size = iphdr->hdr_len * 4;
	total_size = swap_order16(iphdr->total_len);

	// ����С���
	if ((header_size < sizeof(xip_hdr_t)) || (total_size < header_size)) {
		return;
	}

	pre_checksum = iphdr->hdr_checksum;
	iphdr->hdr_checksum = 0;
	// У��ͼ��
	if (pre_checksum != checksum16((uint16_t*)iphdr, header_size, 0, 1)) {
		return;
	}
	iphdr->hdr_checksum = pre_checksum;

	// ���ǲ��Ƿ����ҵģ����ǾͶ���
	if (!xipaddr_is_equal_buf(&netif_ipaddr, iphdr->dest_ip)) {
		return;
	}

	// src_ip ��Ҫ�Ӱ�ͷ����ȡ
	xipaddr_from_buf(&src_ip, iphdr->src_ip);
	switch (iphdr->protocol)
	{
	case XNET_PROTOCOL_UDP:
		// ������Ҫ��ȡ��ͷ�����ݣ�����Ҫ��ǰ�ж��Ƿ�Ϸ�
		if (packet->size >= sizeof(xudp_hdr_t)) {
			xudp_hdr_t* udp_hdr = (xudp_hdr_t*)(packet->data + header_size);//��ip�� ����ͷ��С ��ָ��udp��ͷ
			// ����udp���ƿ�
			xudp_t* udp = xudp_find(swap_order16(udp_hdr->dest_port));
			if (udp) {
				// �Ƶ�ip�� ��ͷ
				remove_header(packet, header_size);//û����if�·� ��Ϊ����else���ݲ���
				// ��Ҫudp���ƿ� Ҫ��ȥ����
				xudp_in(udp, &src_ip, packet);
			}
			else {
				// û����Ӧ��udp���� �ͷ��ض˿ڲ��ɴ�
				xicmp_dest_unreach(XICMP_CODE_PORT_UNREACH, iphdr);
			}
		}
		break;
	case XNET_PROTOCOL_TCP:
		truncate_packet(packet, total_size);//14��̫��+20ip+20tcp=54 ��̫���淶��С60 ������Ҫ�ض�
		remove_header(packet, header_size);
		xtcp_in(&src_ip, packet);
		break;
	case XNET_PROTOCOL_ICMP:
		// ���icmp�� �Ƴ���ͷ��������д���
		remove_header(packet, header_size);
		xicmp_in(&src_ip, packet);
		break;
	default:
		xicmp_dest_unreach(XICMP_TYPE_UNREACH, iphdr);
		break;
	}
}


// IP���ķ��ͺ���(ip���ݰ�����ʱ�������ϲ�Э����õģ�UDP��TCP��IMCP�ȣ��������һ������ָ��һ���Ƿ�������Э������ݣ�Э������ֵ��)
// �ڶ��������Ƿ���˭��һ��IP��ַ���������������ϲ�İ������ϲ��TCP��UDP����ICMPȥ���ɵ�Ȼ�󴫸�����
// ������packet��Ӱ�ͷ���Ѱ�ͷ�̶��ã�Ȼ��ͨ����̫�����ͳ�ȥ
xnet_err_t xip_out(xnet_protocol_t protocol, xipaddr_t* dest_ip, xnet_packet_t* packet) {
	// ����id
	static uint32_t ip_packet_id = 0;
	// �ص㣺����IP���İ�ͷ�����ץ�����й��죩
	// �����ͷ
	xip_hdr_t* iphdr;

	add_header(packet, sizeof(xip_hdr_t));
	iphdr = (xip_hdr_t*)packet->data;
	// ������ͷ
	iphdr->version = XNET_VERSION_IPV4;// �����汾��
	iphdr->hdr_len = sizeof(xip_hdr_t) / 4;// ��ͷ���ȣ�4�ֽ�һ��λ��
	iphdr->tos = 0;
	iphdr->total_len = swap_order16(packet->size);// �ܳ���
	iphdr->id = swap_order16(ip_packet_id);// ��֤idΨһ
	iphdr->flags_fragment = 0;// ���÷�Ƭ����־λ�������ǣ�
	iphdr->ttl = XNET_IPDEFAULT_TTL;// ����ʱ��(����д0��ת��-1)
	iphdr->protocol = protocol;// Э���ֶ�
	memcpy(iphdr->src_ip, &netif_ipaddr.array, XNET_VERSION_IPV4);// Դip��ַ
	memcpy(iphdr->dest_ip, dest_ip->array, XNET_VERSION_IPV4);// Ŀ��IP��ַ
	iphdr->hdr_checksum = 0;// У���
	iphdr->hdr_checksum = checksum16((uint16_t*)iphdr, sizeof(xip_hdr_t), 0, 1);// ��ͷ�Ĵ�С��������У���
	ip_packet_id++;
	// ����ȥ,��̫���ķ��ͺ���(����һ��Ŀ��ip��ַ �����������͵����ݰ�IP��)
	return ethernet_out(dest_ip, packet);
}

