#include "xserver_http.h"
#include "../../xtcp.h"
#include <stdio.h>
#include <string.h>

#define XTCP_FIFO_SIZE	40
//#define XNET_ERR_MEM	-1

static char rx_buffer[1024], tx_buffer[1024];
static char url_path[255], file_path[255];

typedef struct _xhttp_fifo_t {								/* 队列处理 */
	xtcp_t* buffer[XTCP_FIFO_SIZE];							// tcp请求 的 指针数组 
	uint8_t front, tail, count;
} xhttp_fifo_t;


static xhttp_fifo_t http_fifo;								// 定义http请求队列


static void xhttp_fifo_init(xhttp_fifo_t* fifo) {			/* 初始化 */
	fifo->count = 0;
	fifo->front = fifo->tail = 0;
}


static xnet_err_t xhttp_fifo_in(xhttp_fifo_t* fifo, xtcp_t* tcp) {	/* 写入 */
	if (fifo->count >= XTCP_FIFO_SIZE) {							// 内存满了
		return XNET_ERR_MEM;
	}
	fifo->buffer[fifo->front++] = tcp;
	if (fifo->count >= XTCP_FIFO_SIZE) {							// 边界溢出的回绕
		fifo->front = 0;
	}
	fifo->count++;
	return XNET_ERR_OK;
}


static xtcp_t* xhttp_fifo_out(xhttp_fifo_t* fifo) {					/* 读取 */
	xtcp_t* tcp;
	if (fifo->count == 0) {
		return (xtcp_t*)0;
	}
	tcp = fifo->buffer[fifo->tail++];
	if (fifo->tail >= XTCP_FIFO_SIZE) {
		fifo->tail = 0;
	}
	fifo->count--;
	return tcp;
}


static int get_line(xtcp_t* tcp, char* buf, int size) {				/* 获取一行的内容 */
	int i = 0;
	while (i < size) {
		char c;
		if (xtcp_read(tcp, (uint8_t*)&c, 1) > 0) {
			if ((c != '\n') && (c != '\r')) {
				buf[i++] = c;
			}
			else if (c == '\n') {
				break;
			}
		}
		xnet_poll();												//调用tcpip协议栈，否则死循环 ，没有数据出现
	}
	buf[i] = '\0';
	return i;
}


static int http_send(xtcp_t* tcp, char* buf, int size) {			/* 发送函数 有空间就不断发 */
	int sended_size = 0;											//总共发了多少数据
	while (size > 0) {
		int curr_size = xtcp_write(tcp, (uint8_t*)buf, (uint16_t)size);// 获取写了多少，因为不一定都能发送
		//if (curr_size < 0) return;
		if (curr_size < 0) break;
		size -= curr_size;
		buf += curr_size;
		sended_size += curr_size;
		xnet_poll();												//调用tcpip协议栈，否则死循环 ，没有数据出现
	}
	return sended_size;
}


static void close_http(xtcp_t* tcp) {
	xtcp_close(tcp);
	printf("http closed...\n");
}

static void send_404_not_found(xtcp_t* tcp) {
	sprintf(tx_buffer,
		"HTTP/1.0 404 NOT FOUND\r\n"
		"\r\n"							//空行
	);
	http_send(tcp, tx_buffer, (int)strlen(tx_buffer));
}

static void send_file(xtcp_t* tcp, const char* url) {
	FILE* file;
	uint32_t size;
	const char* context_type = "text/html";

	while (*url == '/') url++;
	sprintf(file_path, "%s/%s", XHTTP_DOC_PATH, url);

	file = fopen(file_path, "rb");
	if (file == NULL) {
		send_404_not_found(tcp);
		return;
	}

	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);
	sprintf(tx_buffer,
		"HTTP/1.0 200 OK\r\n"
		"Content-Length:%d\r\n"
		"\r\n",											//空行
		(int)size
	);
	// 就是C语言里，如果两个字符串之间是空白符，C编译器会自动将空白符忽略掉，认为两个字符串是一个字符串，只不过写法上分开了。空白符，比如空格、TAB键、空行
	http_send(tcp, tx_buffer, (int)strlen(tx_buffer));
	while (!feof(file)) {								// 接下来是文件内容
		size = (int)fread(tx_buffer, 1, sizeof(tx_buffer), file);
		if (http_send(tcp, tx_buffer, size) <= 0) {
			fclose(file);
			return;
		}
	}
	fclose(file);
}


//实现回调函数
static xnet_err_t http_handler(xtcp_t* tcp, xtcp_conn_state_t state) {
	static char* num = "0123456789ABCDEF";

	if (state == XTCP_CONN_CONNECTED) {
		// 有tcp连接成功之后，就把tcp放入连接队列
		xhttp_fifo_in(&http_fifo, tcp);
		printf("http connected\n");
		//// 初始化
		//int i = 0;
		//for (i = 0; i < 1024; i++) {
		//	tx_buffer[i] = num[i % 16];
		//}
		////发送 滑动窗口需要缓存 在tcp中定义
		//xtcp_write(tcp, tx_buffer, sizeof(tx_buffer));

		////xtcp_close(tcp);	// 测试主动关闭连接
	}
	//else if (state == XTCP_CONN_DATA_RECV) {	// 接收的数据再发送回去
	//	uint8_t* data = tx_buffer;

	//	uint16_t read_size = xtcp_read(tcp, tx_buffer, sizeof(tx_buffer));
	//	while (read_size) {
	//		uint16_t curr_size = xtcp_write(tcp, data, read_size);
	//		data += curr_size;
	//		read_size -= curr_size;
	//	}
	//}
	else if (state == XTCP_CONN_CLOSED) {
		printf("http closed..\n");
	}
	return XNET_ERR_OK;
}



xnet_err_t xserver_http_create(uint16_t port) {
	xnet_err_t err;
	// 打开tcp控制块 传递回调函数
	xtcp_t* tcp = xtcp_open(http_handler);
	if (!tcp) return XNET_ERR_MEM;
	// 绑定控制块到端口
	err = xtcp_bind(tcp, port);	//http熟知端口
	if (err < 0) return err;
	// 设置监听状态 监听不是处理请求 像是转发 给其它控制块处理请求
	//xtcp_listen(tcp);
	
	// fifo初始化
	xhttp_fifo_init(&http_fifo);
	//return XNET_ERR_OK;
	return xtcp_listen(tcp);
}


// 逐个处理http请求，取出 处理
void xserver_http_run(void) {
	// 取tcp控制块
	xtcp_t* tcp;

	while ( ( tcp = xhttp_fifo_out(&http_fifo) ) != (xtcp_t*)0 ) {
		int i;
		char* c = rx_buffer;
		if (get_line(tcp, rx_buffer, sizeof(rx_buffer)) <= 0) {	//读第一行
			close_http(tcp);//读取失败 关闭http连接
			continue;
		}

		while (*c == ' ') c++;      // 跳过空格 后来添加
		if (strncmp(rx_buffer, "GET", 3) != 0) {	// 不是get请求
			close_http(tcp);
			continue;
		}

		// 获取路径
		while (*c != ' ') c++;
		while (*c == ' ') c++;
		for (i = 0; i < sizeof(url_path); i++) {
			if (*c == ' ') break;
			/*if (*c != ' ') {
				url_path[i] = *c++;
			}*/
			url_path[i] = *c++;
		}
		url_path[i] = '\0';
		
		// 这种情况 http://792.168.0.2/
		if (url_path[strlen(url_path) - 1] == '/') {
			strcat(url_path, "index.html");
		}

		send_file(tcp, url_path);//读取文件 发送文件出去

		close_http(tcp);//发完关闭
	}
}

