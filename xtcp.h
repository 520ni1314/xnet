//#pragma once

#ifndef __XTCP__
#define __XTCP__

#include "xip.h"
//#include "ba"

#define XTCP_KIND_END							0
#define XTCP_KIND_MSS							2
#define XTCP_MSS_DEFAULT						1460



#define XTCP_CFG_MAX_TCP			40			// tcp���ݿ��ƿ������
#define XTCP_CFG_RTX_BUF_SIZE		2048		// ���巢�ͺͽ��յĻ������ڵĻ����С


typedef enum _xtcp_state_t {											// ����tcp���ƿ�״̬
	XTCP_STATE_FREE,
	XTCP_STATE_CLOSED,													//�ر�״̬
	XTCP_STATE_LISTEN,													//����״̬
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
typedef xnet_err_t (*xtcp_handler_t)(xtcp_t* tcp, xtcp_conn_state_t event);		//�ص�����������ָ�룩
typedef struct _xtcp_buf_t {											/* �������� */  //����һ���������ڵ����ݽṹ Ҫ����FIFO ������ʵ��ѭ������
	uint16_t data_count, unacked_cout;									// δ�� δȷ��
	uint16_t front, tail, next;											// ��ͷ ��β δ����λ��
	uint8_t data[XTCP_CFG_RTX_BUF_SIZE];
}xtcp_buf_t;
struct _xtcp_t {														// ����tcp���ƿ�
	xtcp_state_t state;													// ״̬
	uint16_t local_port, remote_port;
	xipaddr_t remote_ip;
	uint32_t next_seq, unacked_seq;										//��һ��Ҫ����seq, ָ�򻬶������к�ɫ���seq
	uint32_t ack;
	uint16_t remote_mss;
	uint16_t remote_win;
	xtcp_handler_t handler;												//�ص�����
	xtcp_buf_t tx_buf, rx_buf;											//���ͻ��� ���ջ���
};


#pragma pack(1)
typedef struct _xtcp_hdr_t {											/* tcp��ͷ */
	uint16_t src_port, dest_port;
	uint32_t seq, ack;
#define XTCP_FLAG_FIN		(1 << 0)
#define XTCP_FLAG_SYN		(1 << 1)
#define XTCP_FLAG_RST		(1 << 2)
#define XTCP_FLAG_ACK		(1 << 4)
	union {																//��־λ
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

void xtcp_init(void);													// ��ʼ������
xnet_err_t xtcp_bind(xtcp_t* tcp, uint16_t port);						// ��
xnet_err_t xtcp_listen(xtcp_t* tcp);									// ����
xtcp_t* xtcp_open(xtcp_handler_t handler);								// ����򿪽ӿ� ��udpһ�� ����ص����� ����ص�����

int xtcp_write(xtcp_t* tcp, uint8_t* data, uint16_t size);
int xtcp_read(xtcp_t* tcp, uint8_t* data, uint16_t size);
xnet_err_t xtcp_close(xtcp_t* tcp);										// �ر�
#endif // !__XTCP__