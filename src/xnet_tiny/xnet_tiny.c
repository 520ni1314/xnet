#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "xnet_tiny.h"
#include <stdint.h>
#include "../../base.h"

											// 以后实现ARP时候也会用得到

//xipaddr_t netif_ipaddr = XNET_CFG_NETIF_IP;									// 协议栈使用这个ip通信


// 校验和算法（相加取反）
uint16_t checksum16(uint16_t* buf, uint16_t len, uint16_t pre_sum, int complement) {
	uint32_t checksum = pre_sum;	// pre_sum
	uint16_t high;
	while (len > 1)	{
		checksum += *buf++;
		len -= 2;
	}
	if (len > 0) {
		checksum += *(uint8_t*)buf;
	}
	while ((high = checksum >> 16) != 0) {											// 高16位累加到低16位，有进位继续累加
		checksum = high + (checksum & 0xFFFF);
	}
	//return (uint16_t)~checksum;
	return complement ? (uint16_t)~checksum : (uint16_t)checksum;
}


uint16_t checksum_peso(const xipaddr_t* src_ip, const xipaddr_t* dest_ip, uint8_t protocol, uint16_t*buf, uint16_t len) {
	uint8_t zero_protocol[] = { 0, protocol };										// 填充和协议号 结合到一起 16位
	uint16_t c_len = swap_order16(len);												// UDP包长度值
	// 累加 先不进行取反
	uint32_t sum = checksum16((uint16_t*)src_ip->array, XNET_IPV4_ADDR_SIZE, 0, 0);	// 对原ip地址进行累加
	sum = checksum16((uint16_t*)dest_ip->array, XNET_IPV4_ADDR_SIZE, sum, 0);		// 对目的ip地址进行累加
	sum = checksum16((uint16_t*)zero_protocol, 2, sum, 0);							//对 填充和协议号 进行累加
	sum = checksum16((uint16_t*)&c_len, 2, sum, 0);									//对UDP包长度值进行累加
	return checksum16(buf, len, sum, 1);											//对数据包进行累加
}


