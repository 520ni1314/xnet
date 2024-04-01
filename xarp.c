#include "xarp.h"
#include "base.h"
#include "xethernet.h"


static const uint8_t ether_broadcast[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };			// 无回报包请求mac地址
static xarp_entry_t arp_entry;															// 本项目是一个表项，要想支持多个表项需要定义数组
static xnet_time_t arp_timer;

// 对表进行初始化
void xarp_init(void) {
	arp_entry.state = XARP_ENTRY_FREE;
	xnet_check_tmo(&arp_timer, 0);
}

void xarp_poll(void) {
	// 每隔一定的时间XARP_TIME_PERIOD，检查一次表项
	if (xnet_check_tmo(&arp_timer, XARP_TIME_PERIOD)) {
		// 判断arp表里的状态
		switch (arp_entry.state)
		{
		case XARP_ENTRY_OK:
			if (--arp_entry.tmo == 0) {
				// 重新进行arp查询
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

// 响应 ：构造一个以太网的包发送出去
xnet_err_t xarp_make_response(xarp_packet_t* arp_packet) {
	xnet_packet_t* packet = xnet_alloc_for_send(sizeof(xarp_packet_t));
	xarp_packet_t* response_packet = (xarp_packet_t*)packet->data;

	response_packet->hw_type = swap_order16(XARP_HW_ETHER);	//硬件类型
	response_packet->pro_type = swap_order16(XNET_PROTOCOL_IP);	//协议类型
	response_packet->hw_len = XNET_MAC_ADDR_SIZE;	//长度
	response_packet->pro_len = XNET_IPV4_ADDR_SIZE;
	response_packet->opcode = swap_order16(XARP_REPLY);
	memcpy(response_packet->send_mac, netif_mac, XNET_MAC_ADDR_SIZE);				// 本地网卡的mac
	memcpy(response_packet->send_ip, netif_ipaddr.array, XNET_IPV4_ADDR_SIZE);		// 自己的IP
	memcpy(response_packet->target_mac, arp_packet->send_mac, XNET_MAC_ADDR_SIZE);
	memcpy(response_packet->target_ip, arp_packet->send_ip, XNET_IPV4_ADDR_SIZE);
	return ethernet_out_to(XNET_PROTOCOL_ARP, arp_packet->send_mac, packet);		// 发送指定mac地址
}

// 实现请求函数
int xarp_make_request(const xipaddr_t* ipaddr) {
	xnet_packet_t* packet = xnet_alloc_for_send(sizeof(xarp_packet_t));				// 首先分配一个包 data_size就是arp包的大小
	xarp_packet_t* arp_packet = (xarp_packet_t*)packet->data;						// 然后定义一个arp数据包的指针，指向数据区

	// 填充arp包的内容
	arp_packet->hw_type = swap_order16(XARP_HW_ETHER);	//硬件类型
	arp_packet->pro_type = swap_order16(XNET_PROTOCOL_IP);	//协议类型
	arp_packet->hw_len = XNET_MAC_ADDR_SIZE;	//长度
	arp_packet->pro_len = XNET_IPV4_ADDR_SIZE;
	arp_packet->opcode = swap_order16(XARP_REQUEST);
	memcpy(arp_packet->send_mac, netif_mac, XNET_MAC_ADDR_SIZE);
	memcpy(arp_packet->send_ip, netif_ipaddr.array, XNET_IPV4_ADDR_SIZE);
	memset(arp_packet->target_mac, 0, XNET_MAC_ADDR_SIZE);
	memcpy(arp_packet->target_ip, ipaddr->array, XNET_IPV4_ADDR_SIZE);
	return ethernet_out_to(XNET_PROTOCOL_ARP, ether_broadcast, packet);				// 发送广播地址
}

// 要解析的ip地址，查到的mac地址存放的位置
xnet_err_t xarp_resolve(const xipaddr_t* ipaddr, uint8_t** mac_addr) {
	// 先看arp状态是否解析成功
	if ((arp_entry.state == XARP_ENTRY_OK) && xipaddr_is_equal(ipaddr, &arp_entry.ipaddr)) {
		// 取出mac地址
		*mac_addr = arp_entry.macaddr;
		return XNET_ERR_OK;
	}

	xarp_make_request(ipaddr);														// 如果没找到
	return XNET_ERR_NONE;
}

// 将mac和ip保存到表里边
void update_arp_entry(uint8_t* src_ip, uint8_t* mac_addr) {
	memcpy(arp_entry.ipaddr.array, src_ip, XNET_IPV4_ADDR_SIZE);
	memcpy(arp_entry.macaddr, mac_addr, XNET_MAC_ADDR_SIZE);
	arp_entry.state = XARP_ENTRY_OK;
	arp_entry.tmo = XARP_CFG_ENTRY_OK_TMO;											// 加一个时间，这个表项有效的时间是多长
	arp_entry.retry_cnt = XARP_CFG_MAX_RETRIES;										// 重试次数：重新查询的时候最多查询几次
}

// arp输入处理的函数
void xarp_in(xnet_packet_t* packet) {
	if (packet->size >= sizeof(xarp_packet_t)) {
		xarp_packet_t* arp_packet = (xarp_packet_t*)packet->data;
		uint16_t opcode = swap_order16(arp_packet->opcode);							// 获取操作码（看看是请求还是响应）

		if ((swap_order16(arp_packet->hw_type) != XARP_HW_ETHER) ||
			(arp_packet->hw_len != XNET_MAC_ADDR_SIZE) ||
			(swap_order16(arp_packet->pro_type) != XNET_PROTOCOL_IP) ||
			(arp_packet->pro_len != XNET_IPV4_ADDR_SIZE) ||
			((opcode != XARP_REQUEST) && (opcode != XARP_REPLY))) {		//opcode有其它值，对于我们协议栈只考虑这两
			return;
		}

		if (!xipaddr_is_equal_buf(&netif_ipaddr, arp_packet->target_ip)) {
			return;
		}

		// 根据opcode类型 区分处理
		switch (opcode) {
		case XARP_REQUEST:// 请求
			xarp_make_response(arp_packet);											// 返回响应
			update_arp_entry(arp_packet->send_ip, arp_packet->send_mac);			// 将对方mac和ip保存到表里边
			break;
		case XARP_REPLY:// 响应
			update_arp_entry(arp_packet->send_ip, arp_packet->send_mac);
			break;
		}
	}
}



int xnet_check_tmo(xnet_time_t* time, uint32_t sec) {			// 超时函数，检查是不是超时 参数：上一次的时间，我们期望多长时间超时
	xnet_time_t curr = xsys_get_time();							// 获取当前时间
	if (sec == 0) {												// ms为0 理解为 就是获取当前时间
		*time = curr;
		return 0;
	}
	else if (curr - *time >= sec) {
		*time = curr;
		return 1;
	}
	return 0;
}