#include "xethernet.h"
#include "xarp.h"
#include "xicmp.h"
#include "xip.h"
#include "xudp.h"
#include "xtcp.h"
#include <stdlib.h>


void xnet_init(void) {								// ����Э��ջ��ʼ��
	ethernet_init();
	xarp_init();
	xip_init();
	xicmp_init();
	xudp_init();
	xtcp_init();
	srand(xsys_get_time());							//��ʼ���������
}


void xnet_poll(void) {								// ��ѯ ��������ѯ��ʱ�䡢��ʱ���Ĳ�ѯ������û�а�
	ethernet_poll();								// �а��� ethernet_poll���� ����
	xarp_poll();									// ��ѯ
}