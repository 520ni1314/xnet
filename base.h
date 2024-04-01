//#pragma once

#ifndef __BASE__
#define __BASE__

#include <stdint.h>
#include <string.h>
#include <stdio.h>

// �������������һ��IP��ַ
#define XNET_CFG_NETIF_IP			{192, 168, 254, 2}

/* Ҫ���弸������ĺ� */
#define XARP_HW_ETHER				0x1			// Ӳ������ ��̫��
#define XARP_REQUEST				0x1			// ARP�����
#define XARP_REPLY					0x2			// ARP��Ӧ��

#define XNET_MAC_ADDR_SIZE			6
#define XNET_CFG_PACKET_MAX_SIZE	1516		// 1516 ��ײ���̫����·�������������1514 ������CRC
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
// �ı���С�� �����ֽ����ֵ����һƪ����
#define swap_order32(v)						( (((v)&0xFF)<<24) | ((((v)>>8)&0xFF)<<16) | ((((v)>>16)&0xFF)<<8) | ((((v)>>24)&0xFF)<<0) )
#define xipaddr_is_equal_buf(addr, buf)		(memcmp((addr)->array, buf, XNET_IPV4_ADDR_SIZE) == 0)
#define xipaddr_is_equal(addr1, addr2)		((addr1)->addr == (addr2)->addr)
#define xipaddr_from_buf(dest, buf)			((dest)->addr = *(uint32_t*)(buf))


/* ���ش���״̬ */
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

#pragma pack(1)														// ��ֹ��������ֽ� �ṹ�岻���Զ�����
typedef struct _xether_hdr_t {										/* ֻ�����ͷ */
	uint8_t dest[XNET_MAC_ADDR_SIZE];
	uint8_t src[XNET_MAC_ADDR_SIZE];
	uint16_t protocol;
}xether_hdr_t;
typedef struct _xnet_packet_t {										/* ����һ����̫�����Ľṹ�� */
	uint16_t size;													// ���е���Ч���ݴ�С 
	uint8_t* data;													// ���е�������ʼ��ַ 
	uint8_t payload[XNET_CFG_PACKET_MAX_SIZE];						// ����ص������� 
}xnet_packet_t;
#pragma pack()

typedef uint32_t xnet_time_t;

#pragma pack(1)
typedef struct _xip_hdr_t {									// ��ͷ�Ľṹ��Ķ��壨���Բο�wiresharkץ���ṹ��
	uint8_t hdr_len : 4;									// ����λ���ֶΣ�����̫���
	uint8_t version : 4;
	uint8_t tos;											//��������
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
typedef union _xipaddr_t {									// ����һ��IP��ַ�Ľṹ�����������ʾ
	// ����ȥ��ʾ
	uint8_t array[XNET_IPV4_ADDR_SIZE];
	// ����һ��32λ���ֶη������Ǻ�߷���
	uint32_t addr;
}xipaddr_t;
typedef struct _xudp_hdr_t {								/* ����udp��ͷ */
	uint16_t src_port, dest_port;
	uint16_t total_len;
	uint16_t checksum;
}xudp_hdr_t;
#pragma pack()


typedef struct _xudp_t xudp_t;
typedef xnet_err_t(*xudp_handler_t)(xudp_t* udp, xipaddr_t* src_ip, uint16_t src_port, xnet_packet_t* packet);		// ����ص�����(���ƿ顢�ĸ�ip���������ĸ��˿ڡ����ݰ�)
struct _xudp_t {															/* ����UDP�Ŀ��ƿ� */
	enum {																	// ���ƿ��״̬
		XUDP_STATE_FREE,													// ����
		XUDP_STATE_USED,													// ռ��
	}state;
	uint16_t local_port;													// ���ض˿�
	xudp_handler_t handler;													// �ص��������յ�Զ�����ݵĽ�һ��������鴦��˿ں����ݵȵȣ�
};




uint16_t checksum16(uint16_t* buf, uint16_t len, uint16_t pre_sum, int complement);
uint16_t checksum_peso(const xipaddr_t* src_ip, const xipaddr_t* dest_ip, uint8_t protocol, uint16_t* buf, uint16_t len);
void update_arp_entry(uint8_t* src_ip, uint8_t* mac_addr);


int xarp_make_request(const xipaddr_t* ipaddr);			// ����һ���ӿں���(������һ��ip�����������ĸ�ip��)
xnet_err_t xarp_resolve(const xipaddr_t* ipaddr, uint8_t** mac_addr);// Ҫ������ip��ַ���鵽��mac��ַ��ŵ�λ��
void xarp_in(xnet_packet_t* packet);					// arp���봦��ĺ���


/**
IP���ķ��ͺ���(ip���ݰ�����ʱ�������ϲ�Э����õģ�UDP��TCP��IMCP�ȣ��������һ������ָ��һ���Ƿ�������Э������ݣ�Э������ֵ��)
�ڶ��������Ƿ���˭��һ��IP��ַ���������������ϲ�İ������ϲ��TCP��UDP����ICMPȥ���ɵ�Ȼ�󴫸�����
������packet��Ӱ�ͷ���Ѱ�ͷ�̶��ã�Ȼ��ͨ����̫�����ͳ�ȥ
**/
xnet_err_t xip_out(xnet_protocol_t protocol, xipaddr_t* dest_ip, xnet_packet_t* packet);
void xip_in(xnet_packet_t* packet);							// ����Ĵ�������̫���Ĵ������е���


xnet_err_t xicmp_dest_unreach(uint8_t code, xip_hdr_t* ip_hdr);		//ICMP���ɴﺯ��(����һ����ʲôԭ�򣨶˿ڲ��ɴЭ�鲻�ɴ����������)
void xicmp_in(xipaddr_t* src_ip, xnet_packet_t* packet);			// ���봦�����ĸ����������������İ���


void xudp_in(xudp_t* udp, xipaddr_t* src_ip, xnet_packet_t* packet);		// udp�����봦������udp���ƿ顢���ͷ�ip�����ݰ���
xudp_t* xudp_find(uint16_t port);											// ����


void xtcp_in(xipaddr_t* remote_ip, xnet_packet_t* packet);				//tcp����



// ���¼����ǳ��ú�����
void add_header(xnet_packet_t* packet, uint16_t header_size);			// ��Ӱ�ͷ
void remove_header(xnet_packet_t* packet, uint16_t header_size);		// �Ƴ���ͷ
extern uint8_t netif_mac[XNET_MAC_ADDR_SIZE];
extern const xipaddr_t netif_ipaddr;	// Э��ջʹ�����ipͨ��
#endif // !__BASE__