#ifndef __XARP__
#define __XARP__

#include <stdint.h>
#include "base.h"

#define XARP_CFG_ENTRY_OK_TMO		(5)			// һ���������Чʱ�䣬���﷽��������С��
#define XARP_CFG_ENTRY_PENDING_TMO	(1)			// ���·������ȶ೤ʱ��
#define XARP_CFG_MAX_RETRIES		4			// ���²�ѯ�����Դ���

#pragma pack(1)
typedef struct _xarp_packet_t {										// ���� ARP ���ṹ
	uint16_t hw_type, pro_type;										// Ӳ�����ͣ�Э������
	uint8_t hw_len, pro_len;										// Ӳ�����ȣ�Э�鳤��
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
#define XARP_TIME_PERIOD			1								// ÿ���೤ʱ����һ��arp��ļ��


typedef struct _xarp_entry_t {							// ʵ��ARP����
	xipaddr_t ipaddr;									// ip��ַ
	uint8_t macaddr[XNET_MAC_ADDR_SIZE];				// MAC��ַ
	uint8_t state;										// ״̬
	uint16_t tmo;										// ��ʱʱ��
	uint8_t retry_cnt;									//���Դ���
}xarp_entry_t;


void xarp_init(void);									// �Ա���г�ʼ��
void xarp_poll(void);									// ��ѯ����

int xnet_check_tmo(xnet_time_t* time, uint32_t sec);


const xnet_time_t xsys_get_time(void);					// ��ȡϵͳʱ��
#endif // !__XARP__