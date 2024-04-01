#include "xserver_datetime.h"
#include <time.h>
#include "../../xethernet.h"
#include "../../xudp.h"
//#include "../../xudp.h"
//#include "xnet_tiny.h"

#define TIME_STR_SIZE		128

// 定义回调函数(控制块、哪个ip传过来、哪个端口、数据包)
xnet_err_t datetime_handler(xudp_t* udp, xipaddr_t* src_ip, uint16_t src_port, xnet_packet_t* packet) {
	// 简单处理数据及发送
	time_t rawtime;
	const struct tm* timeinfo;
	xnet_packet_t* tx_packet;
	size_t str_size;
	tx_packet = xnet_alloc_for_send(TIME_STR_SIZE);
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	str_size = strftime((char*)tx_packet->data, TIME_STR_SIZE, "%A, %B, %d, %Y, %T-%z", timeinfo);
	truncate_packet(tx_packet, (uint16_t)str_size);						// 截短一点 实际有效长度
	return xudp_out(udp, src_ip, src_port, tx_packet);					// 发送
}

xnet_err_t xserver_datatime_create(uint16_t port) {
	xudp_t* udp = xudp_open(datetime_handler);			// 打开一个控制块，传递一个回调
	xudp_bind(udp, port);								// 将控制块与特定端口绑定
	return XNET_ERR_OK;
}