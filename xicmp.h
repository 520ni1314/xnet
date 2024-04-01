//#pragma once

#ifndef __XICMP__
#define __XICMP__

#include <stdint.h>
#include "base.h"




#pragma pack(1)
typedef struct _xicmp_hdr_t {										// 定义IMCP的包头
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
	uint16_t id;
	uint16_t seq;
}xicmp_hdr_t;
#pragma pack()


void xicmp_init(void);												// ICMP的初始化函数
#endif