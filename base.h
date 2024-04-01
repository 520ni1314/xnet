//#pragma once

#ifndef __BASE__
#define __BASE__

#include <stdint.h>
#include <string.h>
#include <stdio.h>

// 给这个程序设置一个IP地址
#define XNET_CFG_NETIF_IP			{192, 168, 254, 2}

/* 要定义几个特殊的宏 */
#define XARP_HW_ETHER				0x1			// 硬件种类 以太网
#define XARP_REQUEST				0x1			// ARP请求包
#define XARP_REPLY					0x2			// ARP响应包

#define XNET_MAC_ADDR_SIZE			6
#define XNET_CFG_PACKET_MAX_SIZE	1516		// 1516 最底层以太网链路，最大发送数据量1514 加两个CRC
#define XNET_IPV4_ADDR_SIZE			4
#define XNET_VERSION_IPV4			4


#define XICMP_CODE_ECHO_REQUEST				8
#define XICMP_CODE_ECHO_REPLY				0
#define XICMP_TYPE_UNREACH					3
#define XICMP_CODE_PORT_UNREACH				3
#define XICMP_CODE_PRO_UNREACH				2


#define tcp_get_init_seq()					((rand() << 16) + rand())
#define swap_order16(v)						((((v) & 0xFF) << 8) | (((v) >> 8) & 0xFF))
// 12 34 56 78
// 78 56 34 12
// 改变大端小端 交换字节序的值得做一篇博客
#define swap_order32(v)						( (((v)&0xFF)<<24) | ((((v)>>8)&0xFF)<<16) | ((((v)>>16)&0xFF)<<8) | ((((v)>>24)&0xFF)<<0) )
#define xipaddr_is_equal_buf(addr, buf)		(memcmp((addr)->array, buf, XNET_IPV4_ADDR_SIZE) == 0)
#define xipaddr_is_equal(addr1, addr2)		((addr1)->addr == (addr2)->addr)
#define xipaddr_from_buf(dest, buf)			((dest)->addr = *(uint32_t*)(buf))


/* 返回错误状态 */
typedef enum _xnet_err_t {
	XNET_ERR_OK = 0,
	XNET_ERR_IO = -1,
	XNET_ERR_NONE = -2,
	XNET_ERR_BINDED = -3,
	XNET_ERR_PARAM = -4,
	XNET_ERR_MEM = -5,
	XNET_ERR_STATE = -6,
	XNET_ERR_WIN_0 = -8,
}xnet_err_t;

typedef enum _xnet_protocol_t {
	XNET_PROTOCOL_ARP = 0x0806,
	XNET_PROTOCOL_IP = 0x0800,
	XNET_PROTOCOL_ICMP = 1,
	XNET_PROTOCOL_UDP = 17,
	XNET_PROTOCOL_TCP = 6,
}xnet_protocol_t;

#pragma pack(1)														// 防止编译器填补字节 结构体不能自动对齐
typedef struct _xether_hdr_t {										/* 只定义包头 */
	uint8_t dest[XNET_MAC_ADDR_SIZE];
	uint8_t src[XNET_MAC_ADDR_SIZE];
	uint16_t protocol;
}xether_hdr_t;
typedef struct _xnet_packet_t {										/* 定义一个以太网包的结构体 */
	uint16_t size;													// 包中的有效数据大小 
	uint8_t* data;													// 包中的数据起始地址 
	uint8_t payload[XNET_CFG_PACKET_MAX_SIZE];						// 最大负载的数据量 
}xnet_packet_t;
#pragma pack()

typedef uint32_t xnet_time_t;

#pragma pack(1)
typedef struct _xip_hdr_t {									// 包头的结构体的定义（可以参考wireshark抓包结构）
	uint8_t hdr_len : 4;									// 利用位的字段？？不太理解
	uint8_t version : 4;
	uint8_t tos;											//服务种类
	uint16_t total_len;
	uint16_t id;
	uint16_t flags_fragment;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t hdr_checksum;
	uint8_t src_ip[XNET_IPV4_ADDR_SIZE];
	uint8_t dest_ip[XNET_IPV4_ADDR_SIZE];
}xip_hdr_t;
#pragma pack()



#pragma pack(1)
typedef union _xipaddr_t {									// 定义一个IP地址的结构，用联合体表示
	// 数组去表示
	uint8_t array[XNET_IPV4_ADDR_SIZE];
	// 定义一个32位的字段方便我们后边访问
	uint32_t addr;
}xipaddr_t;
typedef struct _xudp_hdr_t {								/* 定义udp包头 */
	uint16_t src_port, dest_port;
	uint16_t total_len;
	uint16_t checksum;
}xudp_hdr_t;
#pragma pack()


typedef struct _xudp_t xudp_t;
typedef xnet_err_t(*xudp_handler_t)(xudp_t* udp, xipaddr_t* src_ip, uint16_t src_port, xnet_packet_t* packet);		// 定义回调函数(控制块、哪个ip传过来、哪个端口、数据包)
struct _xudp_t {															/* 创建UDP的控制块 */
	enum {																	// 控制块的状态
		XUDP_STATE_FREE,													// 空闲
		XUDP_STATE_USED,													// 占用
	}state;
	uint16_t local_port;													// 本地端口
	xudp_handler_t handler;													// 回调函数（收到远程数据的进一步处理，检查处理端口和数据等等）
};




uint16_t checksum16(uint16_t* buf, uint16_t len, uint16_t pre_sum, int complement);
uint16_t checksum_peso(const xipaddr_t* src_ip, const xipaddr_t* dest_ip, uint8_t protocol, uint16_t* buf, uint16_t len);
void update_arp_entry(uint8_t* src_ip, uint8_t* mac_addr);


int xarp_make_request(const xipaddr_t* ipaddr);			// 定义一个接口函数(参数是一个ip：向网络上哪个ip发)
xnet_err_t xarp_resolve(const xipaddr_t* ipaddr, uint8_t** mac_addr);// 要解析的ip地址，查到的mac地址存放的位置
void xarp_in(xnet_packet_t* packet);					// arp输入处理的函数


/**
IP包的发送函数(ip数据包发送时候，是由上层协议调用的（UDP、TCP、IMCP等），这里第一个参数指定一个是发送哪种协议的数据（协议类型值）)
第二个参数是发给谁，一个IP地址，第三个参数是上层的包（由上层的TCP、UDP或者ICMP去生成的然后传给他）
函数对packet添加包头，把包头固定好，然后通过以太网发送出去
**/
xnet_err_t xip_out(xnet_protocol_t protocol, xipaddr_t* dest_ip, xnet_packet_t* packet);
void xip_in(xnet_packet_t* packet);							// 输入的处理，在以太网的处理函数中调用


xnet_err_t xicmp_dest_unreach(uint8_t code, xip_hdr_t* ip_hdr);		//ICMP不可达函数(参数一：是什么原因（端口不可达、协议不可达）参数二：包)
void xicmp_in(xipaddr_t* src_ip, xnet_packet_t* packet);			// 输入处理（从哪个机器发过来、发的包）


void xudp_in(xudp_t* udp, xipaddr_t* src_ip, xnet_packet_t* packet);		// udp的输入处理函数（udp控制块、发送方ip、数据包）
xudp_t* xudp_find(uint16_t port);											// 查找


void xtcp_in(xipaddr_t* remote_ip, xnet_packet_t* packet);				//tcp输入



// 以下几个是常用函数：
void add_header(xnet_packet_t* packet, uint16_t header_size);			// 添加包头
void remove_header(xnet_packet_t* packet, uint16_t header_size);		// 移除包头
extern uint8_t netif_mac[XNET_MAC_ADDR_SIZE];
extern const xipaddr_t netif_ipaddr;	// 协议栈使用这个ip通信
#endif // !__BASE__