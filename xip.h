//#pragma once
#ifndef __XIP__
#define __XIP__

#include <stdint.h>
#include "xethernet.h"
#include "base.h"




#define XNET_IPDEFAULT_TTL				64



void xip_init(void);										// ip协议的初始化




#endif // !__XIP__
