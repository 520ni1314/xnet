#include "xudp.h"
#include <string.h>

extern const xipaddr_t netif_ipaddr;
xudp_t udp_socket[XUDP_CFG_MAX_UDP];												// 创建全局数组: UDP连接块

void xudp_init(void) {
	memset(udp_socket, 0, sizeof(udp_socket));
}

// udp的输入处理函数（udp控制块、发送方ip、数据包）
void xudp_in(xudp_t* udp, xipaddr_t* src_ip, xnet_packet_t* packet) {
	xudp_hdr_t* udp_hdr = (xudp_hdr_t*)packet->data;
	uint16_t pre_checksum;
	uint16_t src_port;

	// 简单的 大小检查 (分别比较包头的长度、总长度)
	if ((packet->size < sizeof(xudp_hdr_t)) || (packet->size < swap_order16(udp_hdr->total_len))) {
		return;
	}

	pre_checksum = udp_hdr->checksum;
	udp_hdr->checksum = 0;
	// 若发送过来的 检验和是0 意思是 不需要对数据进行校验和的检查
	if (pre_checksum != 0) {
		uint16_t checksum = checksum_peso(src_ip, &netif_ipaddr, XNET_PROTOCOL_UDP, (uint16_t*)udp_hdr, swap_order16(udp_hdr->total_len));//???
		// 如果是0 校验和取反，防止不进行校验和的计算
		checksum = (checksum == 0) ? 0xFFFF : checksum;
		if (checksum != pre_checksum) {
			return;
		}
	}

	src_port = swap_order16(udp_hdr->src_port);
	// 移除udp包头 packet只剩下udp数据部分 就是应用程序关心的数据
	remove_header(packet, sizeof(xudp_hdr_t));
	// 有设置handler则调用
	if (udp->handler) {
		udp->handler(udp, src_ip, src_port, packet);
	}
}

// udp输出
xnet_err_t xudp_out(xudp_t* udp, xipaddr_t* dest_ip, uint16_t dest_port, xnet_packet_t* packet) {
	// 主要就是添加包头 调用ip发出去
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


// 打开udp控制块(传入回调函数)
xudp_t* xudp_open(xudp_handler_t handler) {
	// 分配一个没有使用的

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

// 释放
void xudp_close(xudp_t* udp) {
	udp->state = XUDP_STATE_FREE;
}

// 查找:收到数据包后，判断传给哪一个udp控制块中的函数去处理（port是目标端口）
xudp_t* xudp_find(uint16_t port) {
	// port相同调用同样的回调函数
	xudp_t* curr, * end;

	for (curr = udp_socket, end = &udp_socket[XUDP_CFG_MAX_UDP]; curr < end; curr++)
	{
		if ((curr->state == XUDP_STATE_USED) && (curr->local_port == port)) {
			return curr;
		}
	}
	return (xudp_t*)0;
}

// 绑定:将控制块和本地的某个端口关联()  端口和回调函数关联
xnet_err_t xudp_bind(xudp_t* udp, uint16_t local_port) {
	// 查一下端口有没有被使用
	xudp_t* curr, * end;

	for (curr = udp_socket, end = &udp_socket[XUDP_CFG_MAX_UDP]; curr < end; curr++)
	{
		if ((curr != udp) && (curr->local_port == local_port)) {
			// 说明udp控制块数组里面已经有了一个控制的回调函数
			return XNET_ERR_BINDED;
		}
	}
	// 没找到就赋值
	udp->local_port = local_port;
	return XNET_ERR_OK;
}

