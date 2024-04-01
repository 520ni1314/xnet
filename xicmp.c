#include "xicmp.h"
#include <stdlib.h>
#include "base.h"
#include "xethernet.h"

// ICMP的初始化函数
void xicmp_init(void) {

}

// 参数一:要从包头中拷贝一些数据，所以传递包头
static xnet_err_t reply_icmp_request(xicmp_hdr_t* icmp_hdr, xipaddr_t* src_ip, xnet_packet_t* packet) {
	// 构建响应包
	// 响应包 和 请求包大侠一样
	xnet_packet_t* tx = xnet_alloc_for_send(packet->size);
	xicmp_hdr_t* reply_hdr = (xicmp_hdr_t*)tx->data;

	reply_hdr->type = XICMP_CODE_ECHO_REPLY;
	reply_hdr->code = 0;
	reply_hdr->id = icmp_hdr->id;													// 本来就是大端数据 此处就没转换
	reply_hdr->seq = icmp_hdr->seq;
	reply_hdr->checksum = 0;
	// 复制数据部分 Data部分
	memcpy((uint8_t*)reply_hdr + sizeof(xicmp_hdr_t), (uint8_t*)icmp_hdr + sizeof(xicmp_hdr_t), packet->size - sizeof(xicmp_hdr_t));
	reply_hdr->checksum = checksum16((uint16_t*)reply_hdr, tx->size, 0, 1);
	return xip_out(XNET_PROTOCOL_ICMP, src_ip, tx);
}

// icmp 输入处理（从哪个机器发过来、发的包）
void xicmp_in(xipaddr_t* src_ip, xnet_packet_t* packet) {
	// 先简单的检查一下大小和类型
	xicmp_hdr_t* icmphdr = (xicmp_hdr_t*)packet->data;

	if ((packet->size >= sizeof(xicmp_hdr_t)) && (icmphdr->type == XICMP_CODE_ECHO_REQUEST)) {
		reply_icmp_request(icmphdr, src_ip, packet);
	}
}

//ICMP不可达函数(参数一：是什么原因（端口不可达、协议不可达）参数二：包)
xnet_err_t xicmp_dest_unreach(uint8_t code, xip_hdr_t* ip_hdr) {
	xicmp_hdr_t* icmp_hdr;
	xnet_packet_t* packet;
	xipaddr_t dest_ip;

	// 计算ip数据包的长度
	uint16_t ip_hdr_size = ip_hdr->hdr_len * 4;
	// ip数据包数据这块(要求至少8字节原始数据)
	uint16_t ip_data_size = swap_order16(ip_hdr->total_len) - ip_hdr_size;
	ip_data_size = ip_hdr_size + min(ip_data_size, 8);

	// 创建发送包
	packet = xnet_alloc_for_send(sizeof(xicmp_hdr_t) + ip_data_size);

	icmp_hdr = (xicmp_hdr_t*)packet->data;
	icmp_hdr->type = XICMP_TYPE_UNREACH;
	icmp_hdr->code = code;
	// 本来就是大端数据 此处就没转换
	icmp_hdr->id = 0;		//unused置0
	icmp_hdr->seq = 0;		//unused置0
	// 把 原始的包头和8字节的数据复制过来
	memcpy((uint8_t*)icmp_hdr + sizeof(xicmp_hdr_t), ip_hdr, ip_data_size);

	icmp_hdr->checksum = 0;
	icmp_hdr->checksum = checksum16((uint16_t*)icmp_hdr, packet->size, 0, 1);

	// 提取目的IP
	xipaddr_from_buf(&dest_ip, ip_hdr->src_ip);
	return xip_out(XNET_PROTOCOL_ICMP, &dest_ip, packet);
}



