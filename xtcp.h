//#pragma once

#ifndef __XTCP__
#define __XTCP__

#include "xip.h"
//#include "ba"

#define XTCP_KIND_END							0
#define XTCP_KIND_MSS							2
#define XTCP_MSS_DEFAULT						1460



#define XTCP_CFG_MAX_TCP			40			// tcp数据控制块的数量
#define XTCP_CFG_RTX_BUF_SIZE		2048		// 定义发送和接收的滑动窗口的缓存大小


typedef enum _xtcp_state_t {											// 定义tcp控制块状态
	XTCP_STATE_FREE,
	XTCP_STATE_CLOSED,													//关闭状态
	XTCP_STATE_LISTEN,													//监听状态
	XTCP_STATE_SYN_RECVD,
	XTCP_STATE_ESTABLISHED,
	XTCP_STATE_FIN_WAIT_1,
	XTCP_STATE_FIN_WAIT_2,
	XTCP_STATE_CLOSING,
	XTCP_STATE_TIMED_WAIT,
	TTCP_STATE_CLOSE_WAIT,
	XTCP_STATE_LAST_ACK,
}xtcp_state_t;

typedef enum _xtcp_conn_state_t {
	XTCP_CONN_CONNECTED,
	XTCP_CONN_DATA_RECV,
	XTCP_CONN_CLOSED,

}xtcp_conn_state_t;


typedef struct _xtcp_t xtcp_t;
typedef xnet_err_t (*xtcp_handler_t)(xtcp_t* tcp, xtcp_conn_state_t event);		//回调函数（函数指针）
typedef struct _xtcp_buf_t {											/* 滑动窗口 */  //定义一个滑动窗口的数据结构 要符合FIFO 用数组实现循环队列
	uint16_t data_count, unacked_cout;									// 未发 未确认
	uint16_t front, tail, next;											// 队头 队尾 未发送位置
	uint8_t data[XTCP_CFG_RTX_BUF_SIZE];
}xtcp_buf_t;
struct _xtcp_t {														// 定义tcp控制块
	xtcp_state_t state;													// 状态
	uint16_t local_port, remote_port;
	xipaddr_t remote_ip;
	uint32_t next_seq, unacked_seq;										//下一次要发的seq, 指向滑动窗口中红色块的seq
	uint32_t ack;
	uint16_t remote_mss;
	uint16_t remote_win;
	xtcp_handler_t handler;												//回调函数
	xtcp_buf_t tx_buf, rx_buf;											//发送缓存 接收缓存
};


#pragma pack(1)
typedef struct _xtcp_hdr_t {											/* tcp包头 */
	uint16_t src_port, dest_port;
	uint32_t seq, ack;
#define XTCP_FLAG_FIN		(1 << 0)
#define XTCP_FLAG_SYN		(1 << 1)
#define XTCP_FLAG_RST		(1 << 2)
#define XTCP_FLAG_ACK		(1 << 4)
	union {																//标志位
		struct {
			uint16_t flags : 6;
			uint16_t reserved : 6;
			uint16_t hdr_len : 4;
		};
		uint16_t all;
	}hdr_flags;
	uint16_t window;
	uint16_t checksum;
	uint16_t urgent_ptr;
}xtcp_hdr_t;
#pragma pack()

//typedef struct _xtcp_t xtcp_t;

void xtcp_init(void);													// 初始化函数
xnet_err_t xtcp_bind(xtcp_t* tcp, uint16_t port);						// 绑定
xnet_err_t xtcp_listen(xtcp_t* tcp);									// 监听
xtcp_t* xtcp_open(xtcp_handler_t handler);								// 定义打开接口 和udp一样 传入回调函数 传入回调函数

int xtcp_write(xtcp_t* tcp, uint8_t* data, uint16_t size);
int xtcp_read(xtcp_t* tcp, uint8_t* data, uint16_t size);
xnet_err_t xtcp_close(xtcp_t* tcp);										// 关闭
#endif // !__XTCP__