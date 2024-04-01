#include "xethernet.h"
#include <stdlib.h>

xnet_packet_t tx_packet, rx_packet;

xnet_packet_t* xnet_alloc_for_send(uint16_t data_size) {								// 发送函数
	tx_packet.data = tx_packet.payload + XNET_CFG_PACKET_MAX_SIZE - data_size;
	tx_packet.size = data_size;
	return &tx_packet;
}

xnet_packet_t* xnet_alloc_for_read(uint16_t data_size) {								// 接收函数
	rx_packet.data = rx_packet.payload;
	rx_packet.size = data_size;
	return &rx_packet;
}


void truncate_packet(xnet_packet_t* packet, uint16_t size) {							// 截断包的长短 将packet截断为size大小
	packet->size = min(packet->size, size);												// 选最小的（当前包、设定大小）
}


xnet_err_t ethernet_init(void) {													// 以太网的初始化
	xnet_err_t err = xnet_driver_open(netif_mac);
	if (err < 0) {
		return err;
	}
	return xarp_make_request(&netif_ipaddr);											// 初始化完成之后，直接发送arp请求包，发给自己请求地址
}


void ethernet_poll(void) {									// 实现一个以太网查询函数：看有没有以太网包，如果有就调用一下（这个）处理函数
	xnet_packet_t* packet;
	if (xnet_driver_read(&packet) == XNET_ERR_OK) {
		ethernet_in(packet);										// 以太网的输入处理函数：解析包，将数据交给ip层或arp层处理
	}
}



// 发送函数：将IP包或ARP包 通过以太网发送出去
xnet_err_t ethernet_out_to(xnet_protocol_t protocol, const uint8_t* mac_addr, xnet_packet_t* packet) {
	xether_hdr_t* ether_hdr;

	add_header(packet, sizeof(xether_hdr_t));
	ether_hdr = (xether_hdr_t*)packet->data;
	memcpy(ether_hdr->dest, mac_addr, XNET_MAC_ADDR_SIZE);
	memcpy(ether_hdr->src, netif_mac, XNET_MAC_ADDR_SIZE);
	ether_hdr->protocol = swap_order16(protocol);

	return xnet_driver_send(packet);
}

// 以太网的发送函数(目的ip地址，发送的包)
xnet_err_t ethernet_out(xipaddr_t* dest_ip, xnet_packet_t* packet) {
	// 要想发送，就要把这个目的ip地址转为mac地址，要转换就需要用到前面实现的arp协议实现地址转换
	xnet_err_t err;
	uint8_t* mac_addr;

	// 第一参数：目的ip	第二参数：将查到的mac地址写入 mac_addr 这里
	if ((err = xarp_resolve(dest_ip, &mac_addr)) == XNET_ERR_OK) {
		// 调用以太网发送函数
		return ethernet_out_to(XNET_PROTOCOL_IP, mac_addr, packet);
	}

	// 解析错误 or 没有找到mac地址
	return err;
}

// 进一步处理
static void ethernet_in(xnet_packet_t* packet) {
	xether_hdr_t* ether_hdr;
	uint16_t protocol;

	// 简单的检查，判断包的大小，比包头小（没有数据）就退出
	if (packet->size <= sizeof(xether_hdr_t)) {
		return;
	}

	ether_hdr = (xether_hdr_t*)packet->data;
	//大端->小端
	protocol = swap_order16(ether_hdr->protocol);
	// 根据协议类型交给不同的上层协议（IP、ARP）
	switch (protocol)
	{
	case XNET_PROTOCOL_ARP:
		remove_header(packet, sizeof(xether_hdr_t));
		xarp_in(packet);
		break;
	case XNET_PROTOCOL_IP:
		// 后来写(为更新arp表)
		// 找到ip包头
		xip_hdr_t* iphdr = (xip_hdr_t*)(packet->data + sizeof(xether_hdr_t));
		update_arp_entry(iphdr->src_ip, ether_hdr->src);
		// 没考虑路由器和分片
		remove_header(packet, sizeof(xether_hdr_t));
		xip_in(packet);
		break;
	}
}