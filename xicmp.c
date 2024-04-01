#include "xicmp.h"
#include <stdlib.h>
#include "base.h"
#include "xethernet.h"

// ICMP�ĳ�ʼ������
void xicmp_init(void) {

}

// ����һ:Ҫ�Ӱ�ͷ�п���һЩ���ݣ����Դ��ݰ�ͷ
static xnet_err_t reply_icmp_request(xicmp_hdr_t* icmp_hdr, xipaddr_t* src_ip, xnet_packet_t* packet) {
	// ������Ӧ��
	// ��Ӧ�� �� ���������һ��
	xnet_packet_t* tx = xnet_alloc_for_send(packet->size);
	xicmp_hdr_t* reply_hdr = (xicmp_hdr_t*)tx->data;

	reply_hdr->type = XICMP_CODE_ECHO_REPLY;
	reply_hdr->code = 0;
	reply_hdr->id = icmp_hdr->id;													// �������Ǵ������ �˴���ûת��
	reply_hdr->seq = icmp_hdr->seq;
	reply_hdr->checksum = 0;
	// �������ݲ��� Data����
	memcpy((uint8_t*)reply_hdr + sizeof(xicmp_hdr_t), (uint8_t*)icmp_hdr + sizeof(xicmp_hdr_t), packet->size - sizeof(xicmp_hdr_t));
	reply_hdr->checksum = checksum16((uint16_t*)reply_hdr, tx->size, 0, 1);
	return xip_out(XNET_PROTOCOL_ICMP, src_ip, tx);
}

// icmp ���봦�����ĸ����������������İ���
void xicmp_in(xipaddr_t* src_ip, xnet_packet_t* packet) {
	// �ȼ򵥵ļ��һ�´�С������
	xicmp_hdr_t* icmphdr = (xicmp_hdr_t*)packet->data;

	if ((packet->size >= sizeof(xicmp_hdr_t)) && (icmphdr->type == XICMP_CODE_ECHO_REQUEST)) {
		reply_icmp_request(icmphdr, src_ip, packet);
	}
}

//ICMP���ɴﺯ��(����һ����ʲôԭ�򣨶˿ڲ��ɴЭ�鲻�ɴ����������)
xnet_err_t xicmp_dest_unreach(uint8_t code, xip_hdr_t* ip_hdr) {
	xicmp_hdr_t* icmp_hdr;
	xnet_packet_t* packet;
	xipaddr_t dest_ip;

	// ����ip���ݰ��ĳ���
	uint16_t ip_hdr_size = ip_hdr->hdr_len * 4;
	// ip���ݰ��������(Ҫ������8�ֽ�ԭʼ����)
	uint16_t ip_data_size = swap_order16(ip_hdr->total_len) - ip_hdr_size;
	ip_data_size = ip_hdr_size + min(ip_data_size, 8);

	// �������Ͱ�
	packet = xnet_alloc_for_send(sizeof(xicmp_hdr_t) + ip_data_size);

	icmp_hdr = (xicmp_hdr_t*)packet->data;
	icmp_hdr->type = XICMP_TYPE_UNREACH;
	icmp_hdr->code = code;
	// �������Ǵ������ �˴���ûת��
	icmp_hdr->id = 0;		//unused��0
	icmp_hdr->seq = 0;		//unused��0
	// �� ԭʼ�İ�ͷ��8�ֽڵ����ݸ��ƹ���
	memcpy((uint8_t*)icmp_hdr + sizeof(xicmp_hdr_t), ip_hdr, ip_data_size);

	icmp_hdr->checksum = 0;
	icmp_hdr->checksum = checksum16((uint16_t*)icmp_hdr, packet->size, 0, 1);

	// ��ȡĿ��IP
	xipaddr_from_buf(&dest_ip, ip_hdr->src_ip);
	return xip_out(XNET_PROTOCOL_ICMP, &dest_ip, packet);
}



