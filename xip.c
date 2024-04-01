#include "xip.h"
#include "base.h"

extern const xipaddr_t netif_ipaddr;

// ip协议的初始化
void xip_init(void) {

}

// 输入的处理，在以太网的处理函数中调用
void xip_in(xnet_packet_t* packet) {
	// 对于输入包做一个基本的检查，检查包头
	xip_hdr_t* iphdr = (xip_hdr_t*)packet->data;
	uint32_t header_size, total_size;
	uint16_t pre_checksum;
	xipaddr_t src_ip;

	// 版本不对
	if (iphdr->version != XNET_VERSION_IPV4) {
		return;
	}

	// 包头的长度以 4字节 为单位
	header_size = iphdr->hdr_len * 4;
	total_size = swap_order16(iphdr->total_len);

	// 包大小检查
	if ((header_size < sizeof(xip_hdr_t)) || (total_size < header_size)) {
		return;
	}

	pre_checksum = iphdr->hdr_checksum;
	iphdr->hdr_checksum = 0;
	// 校验和检查
	if (pre_checksum != checksum16((uint16_t*)iphdr, header_size, 0, 1)) {
		return;
	}
	iphdr->hdr_checksum = pre_checksum;

	// 看是不是发给我的，不是就丢掉
	if (!xipaddr_is_equal_buf(&netif_ipaddr, iphdr->dest_ip)) {
		return;
	}

	// src_ip 需要从包头中提取
	xipaddr_from_buf(&src_ip, iphdr->src_ip);
	switch (iphdr->protocol)
	{
	case XNET_PROTOCOL_UDP:
		// 接下来要读取包头的内容，所以要提前判断是否合法
		if (packet->size >= sizeof(xudp_hdr_t)) {
			xudp_hdr_t* udp_hdr = (xudp_hdr_t*)(packet->data + header_size);//是ip包 加上头大小 就指向udp包头
			// 查找udp控制块
			xudp_t* udp = xudp_find(swap_order16(udp_hdr->dest_port));
			if (udp) {
				// 移掉ip包 包头
				remove_header(packet, header_size);//没放在if下方 因为方便else传递参数
				// 需要udp控制块 要先去查找
				xudp_in(udp, &src_ip, packet);
			}
			else {
				// 没有响应的udp处理 就返回端口不可达
				xicmp_dest_unreach(XICMP_CODE_PORT_UNREACH, iphdr);
			}
		}
		break;
	case XNET_PROTOCOL_TCP:
		truncate_packet(packet, total_size);//14以太网+20ip+20tcp=54 以太网规范最小60 所以需要截断
		remove_header(packet, header_size);
		xtcp_in(&src_ip, packet);
		break;
	case XNET_PROTOCOL_ICMP:
		// 针对icmp包 移除包头对输入进行处理
		remove_header(packet, header_size);
		xicmp_in(&src_ip, packet);
		break;
	default:
		xicmp_dest_unreach(XICMP_TYPE_UNREACH, iphdr);
		break;
	}
}


// IP包的发送函数(ip数据包发送时候，是由上层协议调用的（UDP、TCP、IMCP等），这里第一个参数指定一个是发送哪种协议的数据（协议类型值）)
// 第二个参数是发给谁，一个IP地址，第三个参数是上层的包（由上层的TCP、UDP或者ICMP去生成的然后传给他）
// 函数对packet添加包头，把包头固定好，然后通过以太网发送出去
xnet_err_t xip_out(xnet_protocol_t protocol, xipaddr_t* dest_ip, xnet_packet_t* packet) {
	// 定义id
	static uint32_t ip_packet_id = 0;
	// 重点：构建IP包的包头（结合抓包进行构造）
	// 定义包头
	xip_hdr_t* iphdr;

	add_header(packet, sizeof(xip_hdr_t));
	iphdr = (xip_hdr_t*)packet->data;
	// 构建包头
	iphdr->version = XNET_VERSION_IPV4;// 构建版本号
	iphdr->hdr_len = sizeof(xip_hdr_t) / 4;// 包头长度（4字节一单位）
	iphdr->tos = 0;
	iphdr->total_len = swap_order16(packet->size);// 总长度
	iphdr->id = swap_order16(ip_packet_id);// 保证id唯一
	iphdr->flags_fragment = 0;// 设置分片、标志位（不考虑）
	iphdr->ttl = XNET_IPDEFAULT_TTL;// 生存时间(不能写0，转发-1)
	iphdr->protocol = protocol;// 协议字段
	memcpy(iphdr->src_ip, &netif_ipaddr.array, XNET_VERSION_IPV4);// 源ip地址
	memcpy(iphdr->dest_ip, dest_ip->array, XNET_VERSION_IPV4);// 目的IP地址
	iphdr->hdr_checksum = 0;// 校验和
	iphdr->hdr_checksum = checksum16((uint16_t*)iphdr, sizeof(xip_hdr_t), 0, 1);// 包头的大小的区域做校验和
	ip_packet_id++;
	// 发出去,以太网的发送函数(参数一：目的ip地址 参数二：发送的数据包IP包)
	return ethernet_out(dest_ip, packet);
}

