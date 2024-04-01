#include "xtcp.h"
#include <stdlib.h>

#define XTCP_DATA_MAX_SIZE					(XNET_CFG_PACKET_MAX_SIZE - sizeof(xether_hdr_t) - sizeof(xip_hdr_t) - sizeof(xtcp_hdr_t))

extern const xipaddr_t netif_ipaddr;
static xtcp_t tcp_socket[XTCP_CFG_MAX_TCP];							// TCP连接块


//初始化
static void tcp_buf_init(xtcp_buf_t* tcp_buf) {
	tcp_buf->tail = tcp_buf->next = tcp_buf->front = 0;///都指向0单元
	tcp_buf->data_count = tcp_buf->unacked_cout = 0;//数据的数量 已发送哦为确认的数量
}

// 查询 空闲的数量 滑动窗口中空闲的数量（绿色块）
static uint16_t tcp_buf_free_count(xtcp_buf_t* tcp_buf) {
	//return XTCP_CFG_RTX_BUF_SIZE - tcp_buf->data_count - tcp_buf->unacked_cout;
	return XTCP_CFG_RTX_BUF_SIZE - tcp_buf->data_count;//data视为已发送未确认和待发送的总合
}

//取一下当前还有多少数据待发
static uint16_t tcp_buf_wait_send_count(xtcp_buf_t* tcp_buf) {
	return tcp_buf->data_count - tcp_buf->unacked_cout;
}

//增加 已确认的数据量 tail前移
static void tcp_buf_add_acked_count(xtcp_buf_t* tcp_buf, uint16_t size) {
	tcp_buf->tail += size;
	if (tcp_buf->tail >= XTCP_CFG_RTX_BUF_SIZE) {
		tcp_buf->tail = 0;
	}
	tcp_buf->data_count -= size;
	//tcp_buf->unacked_cout += size;	//只有这一句？？？
	tcp_buf->unacked_cout -= size;	//只有这一句？？？
}

//增加 未确认的数据量 
static void tcp_buf_add_unacked_count(xtcp_buf_t* tcp_buf, uint16_t size) {
	/*tcp_buf->tail += size;
	if (tcp_buf->tail >= XTCP_CFG_RTX_BUF_SIZE) {
		tcp_buf->tail = 0;
	}
	tcp_buf->data_count -= size;
	tcp_buf->unacked_cout -= size;*/			//全部注释
	tcp_buf->unacked_cout += size;
}
//缓存写入函数
static uint16_t tcp_buf_write(xtcp_buf_t* tcp_buf, uint8_t* from, uint16_t size) {
	int i;
	size = min(size, tcp_buf_free_count(tcp_buf));

	for (i = 0; i < size; i++) {
		tcp_buf->data[tcp_buf->front++] = *from++;
		if (tcp_buf->front >= XTCP_CFG_RTX_BUF_SIZE) { //绕过来
			tcp_buf->front = 0;
		}
	}
	tcp_buf->data_count += size;
	return size;
}


//读取函数 为了发送
static uint16_t tcp_buf_read_for_send(xtcp_buf_t* tcp_buf, uint8_t* to, uint16_t size) {
	int i;
	uint16_t wait_send_count = tcp_buf->data_count - tcp_buf->unacked_cout;
	size = min(size, wait_send_count);

	for (i = 0; i < size; i++) {
		*to++ = tcp_buf->data[tcp_buf->next++];
		if (tcp_buf->next >= XTCP_CFG_RTX_BUF_SIZE) { //超过界限绕过来
			tcp_buf->next = 0;
		}
	}
	//tcp_buf->data_count += size; 总数据量不用变 只是变为已发送未确认
	return size;
}

// 读取tcp缓存中的数据 收到但未读取的部分
static uint16_t tcp_buf_read(xtcp_buf_t* tcp_buf, uint8_t* to, uint16_t size) {
	int i;
	size = min(size, tcp_buf->data_count);
	for (i = 0; i < size; i++) {
		*to++ = tcp_buf->data[tcp_buf->tail++];
		if (tcp_buf->tail >= XTCP_CFG_RTX_BUF_SIZE) {	// 超过则回绕
			tcp_buf->tail = 0;
		}
	}

	tcp_buf->data_count -= size;
	return size;
}

// 将数据从tcp包取出 放入接收缓存
static uint16_t tcp_recv(xtcp_t* tcp, uint8_t flags, uint8_t* from, uint16_t size) {
	uint16_t read_size = tcp_buf_write(&tcp->rx_buf, from, size);
	tcp->ack += read_size;
	//if (flags & (XTCP_FLAG_FIN | XTCP_FLAG_ACK)) { // ?????????????
	if (flags & (XTCP_FLAG_FIN | XTCP_FLAG_SYN)) {
		tcp->ack++;
	}
	return read_size;
}

// 分配空闲tcp控制块函数
static xtcp_t* tcp_alloc(void) {
	// 找到返回，没找到返回空
	xtcp_t* tcp, *end;
	for (tcp = tcp_socket, end = tcp_socket + XTCP_CFG_MAX_TCP; tcp < end; tcp++) {
		if (tcp->state == XTCP_STATE_FREE) {
			tcp->state = XTCP_STATE_CLOSED;
			tcp->local_port = 0;
			tcp->remote_port = 0;
			tcp->remote_ip.addr = 0;//addr方便赋值？
			tcp->handler = (xtcp_handler_t)0;
			tcp->remote_win = XTCP_MSS_DEFAULT;
			tcp->remote_mss = XTCP_MSS_DEFAULT;
			tcp->next_seq = tcp->unacked_seq = tcp_get_init_seq();
			tcp->ack = 0;
			tcp_buf_init(&tcp->tx_buf);//在tcp分配时，加上缓存初始化
			tcp_buf_init(&tcp->rx_buf);//在tcp分配时，加上缓存初始化
			return tcp;
		}
	}
	return (xtcp_t*)0;
}

// 回收 释放函数
static void tcp_free(xtcp_t* tcp) {
	tcp->state = XTCP_STATE_FREE;
}

static xtcp_t* tcp_find(xipaddr_t* remote_ip, uint16_t remote_port, uint16_t local_port) {
	// 特定的查找机制
	xtcp_t* tcp, * end;
	xtcp_t* founded_tcp = (xtcp_t*)0;

	for (tcp = tcp_socket, end = &tcp_socket[XTCP_CFG_MAX_TCP]; tcp < end; tcp++) {
		if (tcp->state == XTCP_STATE_FREE) {// 空闲跳
			continue;
		}
		if (tcp->local_port != local_port) {// 本地端口不同跳
			continue;
		}
		if (xipaddr_is_equal(remote_ip, &tcp->remote_ip) && (remote_port == tcp->remote_port)) {// 本地ip比较 已在ip层输入中判断
			return tcp;// 所有端口和地址一样 则返回
		}
		if (tcp->state == XTCP_STATE_LISTEN) {//本地端口相同（结合原理讲）状态机的事情
			founded_tcp = tcp;
		}
	}
	return founded_tcp;
}

static xnet_err_t tcp_send_reset(uint32_t remote_ack, uint16_t local_port, xipaddr_t* remote_ip, uint16_t remote_port) {
	xnet_packet_t* packet = xnet_alloc_for_send(sizeof(xtcp_hdr_t));//无数据，包头大小就行
	xtcp_hdr_t* tcp_hdr = (xtcp_hdr_t*)packet->data;

	tcp_hdr->src_port = swap_order16(local_port);
	tcp_hdr->dest_port = swap_order16(remote_port);
	tcp_hdr->seq = 0;
	tcp_hdr->ack = swap_order32(remote_ack);
	tcp_hdr->hdr_flags.all = 0;
	tcp_hdr->hdr_flags.hdr_len = sizeof(xtcp_hdr_t) / 4; //四字节为一单位
	tcp_hdr->hdr_flags.flags = XTCP_FLAG_RST | XTCP_FLAG_ACK;
	tcp_hdr->hdr_flags.all = swap_order16(tcp_hdr->hdr_flags.all);
	tcp_hdr->window = 0;
	tcp_hdr->checksum = 0;
	tcp_hdr->urgent_ptr = 0;
	tcp_hdr->checksum = checksum_peso(&netif_ipaddr, remote_ip, XNET_PROTOCOL_TCP, (uint16_t*)packet->data, packet->size);
	tcp_hdr->checksum = tcp_hdr->checksum ? tcp_hdr->checksum : 0xFFFF;
	return xip_out(XNET_PROTOCOL_TCP, remote_ip, packet);
}


static xnet_err_t tcp_send(xtcp_t* tcp, uint8_t flags) {
	xnet_packet_t* packet;
	xtcp_hdr_t* tcp_hdr;
	xnet_err_t err;
	uint16_t data_size = tcp_buf_wait_send_count(&tcp->tx_buf);//取一下 有多少数据要发
	uint16_t opt_size = (flags & XTCP_FLAG_SYN) ? 4 : 0;//为发送mms

	// 发送时候 要看一下对方的window大小
	if (tcp->remote_win) {
		data_size = min(data_size, tcp->remote_win);//接收端的窗口大小对比 
		data_size = min(data_size, tcp->remote_mss);//ip分片的对比 防止分片 分片就略过了不写了
		if (data_size + opt_size > XTCP_DATA_MAX_SIZE) {	// 如果大 就小一点 保证不会超出以太网边界
			data_size = XTCP_DATA_MAX_SIZE - opt_size;
		}
	}
	else {//窗口大小为0
		data_size = 0;	//不发数据 发ack
	}


	//packet = xnet_alloc_for_send(opt_size + sizeof(xtcp_hdr_t));
	packet = xnet_alloc_for_send(data_size + opt_size + sizeof(xtcp_hdr_t));	// 数据考虑进去
	tcp_hdr = (xtcp_hdr_t*)packet->data;
	tcp_hdr->src_port = swap_order16(tcp->local_port);
	tcp_hdr->dest_port = swap_order16(tcp->remote_port);
	tcp_hdr->seq = swap_order32(tcp->next_seq);
	tcp_hdr->ack = swap_order32(tcp->ack);
	tcp_hdr->hdr_flags.all = 0;
	tcp_hdr->hdr_flags.hdr_len = (opt_size + sizeof(xtcp_hdr_t)) / 4; //四字节为一单位
	tcp_hdr->hdr_flags.flags = flags;
	tcp_hdr->hdr_flags.all = swap_order16(tcp_hdr->hdr_flags.all);
	tcp_hdr->window = swap_order16(tcp_buf_free_count(&tcp->rx_buf));//还剩下的窗口大小
	tcp_hdr->checksum = 0;
	tcp_hdr->urgent_ptr = 0;
	// 如果是握手报文 判断SYN是否置1 （为mms而加的吧）
	if (flags & XTCP_FLAG_SYN) {
		uint8_t* opt_data = packet->data + sizeof(xtcp_hdr_t);	// 指向选项数据
		opt_data[0] = XTCP_KIND_MSS;
		opt_data[1] = 4;//长度 4字节
		*(uint16_t*)(opt_data + 2) = swap_order16(XTCP_MSS_DEFAULT);

	}

	//校验和计算之前 把数据读出来
	tcp_buf_read_for_send(&tcp->tx_buf, packet->data + opt_size + sizeof(xtcp_hdr_t), data_size);
	tcp_hdr->checksum = checksum_peso(&netif_ipaddr, &tcp->remote_ip, XNET_PROTOCOL_TCP, (uint16_t*)packet->data, packet->size);
	tcp_hdr->checksum = tcp_hdr->checksum ? tcp_hdr->checksum : 0xFFFF;

	err = xip_out(XNET_PROTOCOL_TCP, &tcp->remote_ip, packet);
	if (err < 0) return err;

	// 已经发送 要减小一下对方窗口 接收滑动窗口 已发 对方接收要减小
	tcp->remote_win -= data_size;
	tcp->next_seq += data_size;
	tcp_buf_add_unacked_count(&tcp->tx_buf, data_size);

	if (flags & (XTCP_FLAG_SYN | XTCP_FLAG_FIN)) {
		tcp->next_seq++;
	}
	return XNET_ERR_OK;
}

static void tcp_read_mss(xtcp_t* tcp, xtcp_hdr_t* tcp_hdr) {
	uint16_t opt_len = tcp_hdr->hdr_flags.hdr_len * 4 - sizeof(xtcp_hdr_t);

	if (opt_len == 0) {
		tcp->remote_mss = XTCP_MSS_DEFAULT;
	}
	else {
		uint8_t* opt_data = (uint8_t*)tcp_hdr + sizeof(xtcp_hdr_t);
		uint8_t* opt_end = opt_data + opt_len;
		while ((*opt_data != XTCP_KIND_END) && (opt_data < opt_end)) {
			if ((*opt_data++ == XTCP_KIND_MSS) && (*opt_data++ == 4)) {
				tcp->remote_mss = swap_order16(*(uint16_t*)opt_data);
				return;
			}
		}
	}
}

static void tcp_process_acccept(xtcp_t* listen_tcp, xipaddr_t* remote_ip, xtcp_hdr_t* tcp_hdr) {
	uint16_t hdr_flags = tcp_hdr->hdr_flags.all;

	if (hdr_flags & XTCP_FLAG_SYN) {
		xnet_err_t err;
		uint32_t ack = tcp_hdr->seq + 1;

		xtcp_t* new_tcp = tcp_alloc();
		if (!new_tcp) return;

		new_tcp->state = XTCP_STATE_SYN_RECVD;
		new_tcp->local_port = listen_tcp->local_port;
		//new_tcp->remote_port = listen_tcp->remote_port;
		//new_tcp->remote_port = tcp_hdr->src_port;
		new_tcp->handler = listen_tcp->handler;
		//new_tcp->remote_port = listen_tcp->remote_port;
		new_tcp->remote_port = tcp_hdr->src_port;
		new_tcp->remote_ip.addr = remote_ip->addr;
		new_tcp->ack = ack;
		new_tcp->unacked_seq = new_tcp->next_seq = tcp_get_init_seq();
		new_tcp->remote_win = tcp_hdr->window;

		tcp_read_mss(new_tcp, tcp_hdr);

		err = tcp_send(new_tcp, XTCP_FLAG_SYN | XTCP_FLAG_ACK);
		if (err < 0) {
			tcp_free(new_tcp);
			return;
		}
	}
	else {
		tcp_send_reset(tcp_hdr->seq, listen_tcp->local_port, remote_ip, tcp_hdr->src_port);
	}
}

// tcp输入
void xtcp_in(xipaddr_t* remote_ip, xnet_packet_t* packet) {
	xtcp_hdr_t* tcp_hdr = (xtcp_hdr_t*)packet->data;
	uint16_t pre_checksum;
	xtcp_t* tcp;
	uint16_t read_size;

	if (packet->size < sizeof(xtcp_hdr_t)) {
		return;
	}

	pre_checksum = tcp_hdr->checksum;
	tcp_hdr->checksum = 0;
	if (pre_checksum != 0) {
		uint16_t checksum = checksum_peso(remote_ip, &netif_ipaddr, XNET_PROTOCOL_TCP, (uint16_t*)tcp_hdr, packet->size);//???
		// 如果是0 校验和取反，防止不进行校验和的计算
		checksum = (checksum == 0) ? 0xFFFF : checksum;
		if (checksum != pre_checksum) {
			return;
		}
	}

	tcp_hdr->src_port = swap_order16(tcp_hdr->src_port);
	tcp_hdr->dest_port = swap_order16(tcp_hdr->dest_port);
	tcp_hdr->hdr_flags.all = swap_order16(tcp_hdr->hdr_flags.all);
	tcp_hdr->seq = swap_order32(tcp_hdr->seq);
	tcp_hdr->ack = swap_order32(tcp_hdr->ack);
	tcp_hdr->window = swap_order16(tcp_hdr->window);

	// 找一个控制块去处理数据包
	tcp = tcp_find(remote_ip, tcp_hdr->src_port, tcp_hdr->dest_port);
	if (tcp == (xtcp_t*)0) {// 如果没找到控制块可以处理告诉对方 类似UDP的icmp
		//发送复位
		tcp_send_reset(tcp_hdr->seq + 1, tcp_hdr->dest_port, remote_ip, tcp_hdr->src_port);
		return;
	}
	tcp->remote_win = tcp_hdr->window;

	if (tcp->state == XTCP_STATE_LISTEN) {
		// 判断是不是连接请求数据包，创建新的TCP控制块 去处理请求
		tcp_process_acccept(tcp, remote_ip, tcp_hdr);
		return;
	}

	// 你发送的seq 和 我希望接受的ack 不同就丢包
	if (tcp_hdr->seq != tcp->ack) {
		tcp_send_reset(tcp_hdr->seq + 1, tcp_hdr->dest_port, remote_ip, tcp_hdr->src_port);
		return;
	}
	remove_header(packet, tcp_hdr->hdr_flags.hdr_len * 4);
	switch (tcp->state) {
	case XTCP_STATE_SYN_RECVD:
		if (tcp_hdr->hdr_flags.flags & XTCP_FLAG_ACK) {//接收到ACK包
			tcp->unacked_seq++;
			tcp->state = XTCP_STATE_ESTABLISHED;//下次来包就走XTCP_STATE_ESTABLISHED了
			tcp->handler(tcp, XTCP_CONN_CONNECTED);//告诉http服务器已经建立连接
		}
		break;
	case XTCP_STATE_ESTABLISHED:
		if (tcp_hdr->hdr_flags.flags & (XTCP_FLAG_ACK | XTCP_FLAG_FIN)) {
			// 接收数据
			if (tcp_hdr->hdr_flags.flags & XTCP_FLAG_ACK) {	//判断里边有无数据
				// 通过序列号判断
				if ((tcp->unacked_seq < tcp_hdr->ack) && (tcp_hdr->ack <= tcp->next_seq)) { //P30 24min
					uint16_t curr_ack_size = tcp_hdr->ack - tcp->unacked_seq;
					tcp_buf_add_acked_count(&tcp->tx_buf, curr_ack_size);
					tcp->unacked_seq += curr_ack_size;
				}
			}

			read_size = tcp_recv(tcp, (uint8_t)tcp_hdr->hdr_flags.flags, packet->data, packet->size);

			//printf("connection ok");
			if (tcp_hdr->hdr_flags.flags & (XTCP_FLAG_FIN)) {	//对方想和我断开连接		//被动关闭
				tcp->state = XTCP_STATE_LAST_ACK;	// tcp控制块应该模拟的是状态机 nb
				tcp->ack++;	// +1是希望下一次接收的序号 XTCP_FLAG_FIN占一个序号 同时意味前面你发的包我已经收到
				tcp_send(tcp, XTCP_FLAG_FIN | XTCP_FLAG_ACK);	// 省略数据传输，直接第三次握手
			}
			else if (read_size) {
				tcp_send(tcp, XTCP_FLAG_ACK);
				tcp->handler(tcp, XTCP_CONN_DATA_RECV);
			}
			else if (tcp_buf_wait_send_count(&tcp->tx_buf)) {//不是结束 看看有没有数据要发
				tcp_send(tcp, XTCP_FLAG_ACK);
			}
		}
		break;
	case XTCP_STATE_FIN_WAIT_1:	//主动关闭后的进入，等待对方的响应 
		if ((tcp_hdr->hdr_flags.flags & (XTCP_FLAG_FIN | XTCP_FLAG_ACK)) == (XTCP_FLAG_FIN | XTCP_FLAG_ACK)) {	// 对方四次挥手中的二三次合并
			tcp_free(tcp);
		}
		else if (tcp_hdr->hdr_flags.flags & XTCP_FLAG_ACK) {	// 对方四次挥手中的 第二次挥手
			tcp->state = XTCP_STATE_FIN_WAIT_2;
		}
		break;
	case XTCP_STATE_FIN_WAIT_2:
		// 这里不考虑数据接收了
		if (tcp_hdr->hdr_flags.flags & XTCP_FLAG_FIN) {	// 第三次
			tcp->ack++;
			tcp_send(tcp, XTCP_FLAG_ACK);
			tcp_free(tcp);
		}
		break;
	case XTCP_STATE_LAST_ACK:
		if (tcp_hdr->hdr_flags.flags & (XTCP_FLAG_ACK)) {//简单点，直接释放掉了，不写进入CLOSE状态了
			tcp->handler(tcp, XTCP_CONN_CLOSED);
			tcp_free(tcp);
		}
		break;
	}
}

// 初始化函数
void xtcp_init(void) {
	// 清空
	memset(tcp_socket, 0, sizeof(tcp_socket));
}


// 定义打开接口 和udp一样 传入回调函数 传入回调函数
xtcp_t* xtcp_open(xtcp_handler_t handler) {
	xtcp_t* tcp = tcp_alloc();//分配一个空闲tcp块
	if (!tcp) return (xtcp_t*)0;//分配失败

	tcp->state = XTCP_STATE_CLOSED;// tcp内部协议状态机的close
	tcp->handler = handler;
	return tcp;
}

// 绑定 和udp一样
xnet_err_t xtcp_bind(xtcp_t* tcp, uint16_t local_port) {

	xtcp_t* curr, * end;
	for (curr = tcp_socket, end = &tcp_socket[XTCP_CFG_MAX_TCP]; curr < end; curr++)
	{
		if ((curr != tcp) && (curr->local_port == local_port)) {
			return XNET_ERR_BINDED;
		}
	}
	tcp->local_port = local_port;
	return XNET_ERR_OK;
}
// 监听
xnet_err_t xtcp_listen(xtcp_t* tcp) {
	tcp->state = XTCP_STATE_LISTEN;
	return XNET_ERR_OK;
}

//发送函数 返回多少数据量
int xtcp_write(xtcp_t* tcp, uint8_t* data, uint16_t size) {
	int sended_count;
	if ((tcp->state != XTCP_STATE_ESTABLISHED)) { // 判断状态，不是连接建立状态就发送不成
		return -1;
	}
	sended_count = tcp_buf_write(&tcp->tx_buf, data, size);//缓存写入函数  希望写size 但实际大小有限sended_count
	if (sended_count) {
		tcp_send(tcp, XTCP_FLAG_ACK);
	}
	return sended_count;//没完全写完就交给应用程序去判断了
}

int xtcp_read(xtcp_t* tcp, uint8_t* data, uint16_t size) {
	// 读取 从接收缓存中读
	return tcp_buf_read(&tcp->rx_buf, data, size);
}

// 关闭
xnet_err_t xtcp_close(xtcp_t* tcp) {
	xnet_err_t err;
	if (tcp->state == XTCP_STATE_ESTABLISHED) {	// 主动关闭连接
		err = tcp_send(tcp, XTCP_FLAG_FIN | XTCP_FLAG_ACK);//正常应该是只有FIN
		//err = tcp_send(tcp, XTCP_FLAG_FIN);//正常应该是只有FIN
		if (err < 0) return err;
		tcp->state = XTCP_STATE_FIN_WAIT_1;
	}
	else {	// 如果是其它状态 就简单点直接释放了
		tcp_free(tcp);
	}
	return XNET_ERR_OK;
}