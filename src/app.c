#include <stdio.h>

#include "xnet_app/xserver_datetime.h"
#include "xnet_app/xserver_http.h"

#include "../boot.h"

int main (void) {
    // 初始化不加的话，内存报错
    xnet_init();

    xserver_datatime_create(13);
    xserver_http_create(80);//创建http服务

    printf("xnet running\n");
    while (1) {
        // 查询网卡上有无数据，进一步层层处理
        xserver_http_run();
        xnet_poll();
    }

    return 0;
}
