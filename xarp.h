#ifndef __XARP__
#define __XARP__

#include <stdint.h>
#include "base.h"

#define XARP_CFG_ENTRY_OK_TMO		(5)			// 一个表项的有效时间，这里方便检测设置小点
#define XARP_CFG_ENTRY_PENDING_TMO	(1)			// 重新发，最多等多长时间
#define XARP_CFG_MAX_RETRIES		4			// 重新查询的重试次数

#pragma pack(1)
typedef struct _xarp_packet_t {										// 定义 ARP 包结构
	uint16_t hw_type, pro_type;										// 硬件类型，协议类型
	uint8_t hw_len, pro_len;										// 硬件长度，协议长度
	uint16_t opcode;
	uint8_t send_mac[XNET_MAC_ADDR_SIZE];
	uint8_t send_ip[XNET_IPV4_ADDR_SIZE];
	uint8_t target_mac[XNET_MAC_ADDR_SIZE];
	uint8_t target_ip[XNET_IPV4_ADDR_SIZE];
}xarp_packet_t;
#pragma pack()


#define XARP_ENTRY_FREE				0
#define XARP_ENTRY_OK				1
#define XARP_ENTRY_PENDING			2
#define XARP_TIME_PERIOD			1								// 每隔多长时间做一次arp表的检查


typedef struct _xarp_entry_t {							// 实现ARP表项
	xipaddr_t ipaddr;									// ip地址
	uint8_t macaddr[XNET_MAC_ADDR_SIZE];				// MAC地址
	uint8_t state;										// 状态
	uint16_t tmo;										// 超时时间
	uint8_t retry_cnt;									//重试次数
}xarp_entry_t;


void xarp_init(void);									// 对表进行初始化
void xarp_poll(void);									// 查询函数

int xnet_check_tmo(xnet_time_t* time, uint32_t sec);


const xnet_time_t xsys_get_time(void);					// 获取系统时间
#endif // !__XARP__