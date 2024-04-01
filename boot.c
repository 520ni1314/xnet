#include "xethernet.h"
#include "xarp.h"
#include "xicmp.h"
#include "xip.h"
#include "xudp.h"
#include "xtcp.h"
#include <stdlib.h>


void xnet_init(void) {								// 控制协议栈初始化
	ethernet_init();
	xarp_init();
	xip_init();
	xicmp_init();
	xudp_init();
	xtcp_init();
	srand(xsys_get_time());							//初始化随机种子
}


void xnet_poll(void) {								// 查询 和其它查询（时间、定时器的查询）看有没有包
	ethernet_poll();								// 有包用 ethernet_poll函数 处理
	xarp_poll();									// 查询
}