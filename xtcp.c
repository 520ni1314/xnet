#include "xtcp.h"
#include <stdlib.h>

#define XTCP_DATA_MAX_SIZE					(XNET_CFG_PACKET_MAX_SIZE - sizeof(xether_hdr_t) - sizeof(xip_hdr_t) - sizeof(xtcp_hdr_t))

extern const xipaddr_t netif_ipaddr;
static xtcp_t tcp_socket[XTCP_CFG_MAX_TCP];							// TCP���ӿ�


//��ʼ��
static void tcp_buf_init(xtcp_buf_t* tcp_buf) {
	tcp_buf->tail = tcp_buf->next = tcp_buf->front = 0;///��ָ��0��Ԫ
	tcp_buf->data_count = tcp_buf->unacked_cout = 0;//���ݵ����� �ѷ���ŶΪȷ�ϵ�����
}

// ��ѯ ���е����� ���������п��е���������ɫ�飩
static uint16_t tcp_buf_free_count(xtcp_buf_t* tcp_buf) {
	//return XTCP_CFG_RTX_BUF_SIZE - tcp_buf->data_count - tcp_buf->unacked_cout;
	return XTCP_CFG_RTX_BUF_SIZE - tcp_buf->data_count;//data��Ϊ�ѷ���δȷ�Ϻʹ����͵��ܺ�
}

//ȡһ�µ�ǰ���ж������ݴ���
static uint16_t tcp_buf_wait_send_count(xtcp_buf_t* tcp_buf) {
	return tcp_buf->data_count - tcp_buf->unacked_cout;
}

//���� ��ȷ�ϵ������� tailǰ��
static void tcp_buf_add_acked_count(xtcp_buf_t* tcp_buf, uint16_t size) {
	tcp_buf->tail += size;
	if (tcp_buf->tail >= XTCP_CFG_RTX_BUF_SIZE) {
		tcp_buf->tail = 0;
	}
	tcp_buf->data_count -= size;
	//tcp_buf->unacked_cout += size;	//ֻ����һ�䣿����
	tcp_buf->unacked_cout -= size;	//ֻ����һ�䣿����
}

//���� δȷ�ϵ������� 
static void tcp_buf_add_unacked_count(xtcp_buf_t* tcp_buf, uint16_t size) {
	/*tcp_buf->tail += size;
	if (tcp_buf->tail >= XTCP_CFG_RTX_BUF_SIZE) {
		tcp_buf->tail = 0;
	}
	tcp_buf->data_count -= size;
	tcp_buf->unacked_cout -= size;*/			//ȫ��ע��
	tcp_buf->unacked_cout += size;
}
//����д�뺯��
static uint16_t tcp_buf_write(xtcp_buf_t* tcp_buf, uint8_t* from, uint16_t size) {
	int i;
	size = min(size, tcp_buf_free_count(tcp_buf));

	for (i = 0; i < size; i++) {
		tcp_buf->data[tcp_buf->front++] = *from++;
		if (tcp_buf->front >= XTCP_CFG_RTX_BUF_SIZE) { //�ƹ���
			tcp_buf->front = 0;
		}
	}
	tcp_buf->data_count += size;
	return size;
}


//��ȡ���� Ϊ�˷���
static uint16_t tcp_buf_read_for_send(xtcp_buf_t* tcp_buf, uint8_t* to, uint16_t size) {
	int i;
	uint16_t wait_send_count = tcp_buf->data_count - tcp_buf->unacked_cout;
	size = min(size, wait_send_count);

	for (i = 0; i < size; i++) {
		*to++ = tcp_buf->data[tcp_buf->next++];
		if (tcp_buf->next >= XTCP_CFG_RTX_BUF_SIZE) { //���������ƹ���
			tcp_buf->next = 0;
		}
	}
	//tcp_buf->data_count += size; �����������ñ� ֻ�Ǳ�Ϊ�ѷ���δȷ��
	return size;
}

// ��ȡtcp�����е����� �յ���δ��ȡ�Ĳ���
static uint16_t tcp_buf_read(xtcp_buf_t* tcp_buf, uint8_t* to, uint16_t size) {
	int i;
	size = min(size, tcp_buf->data_count);
	for (i = 0; i < size; i++) {
		*to++ = tcp_buf->data[tcp_buf->tail++];
		if (tcp_buf->tail >= XTCP_CFG_RTX_BUF_SIZE) {	// ���������
			tcp_buf->tail = 0;
		}
	}

	tcp_buf->data_count -= size;
	return size;
}

// �����ݴ�tcp��ȡ�� ������ջ���
static uint16_t tcp_recv(xtcp_t* tcp, uint8_t flags, uint8_t* from, uint16_t size) {
	uint16_t read_size = tcp_buf_write(&tcp->rx_buf, from, size);
	tcp->ack += read_size;
	//if (flags & (XTCP_FLAG_FIN | XTCP_FLAG_ACK)) { // ?????????????
	if (flags & (XTCP_FLAG_FIN | XTCP_FLAG_SYN)) {
		tcp->ack++;
	}
	return read_size;
}

// �������tcp���ƿ麯��
static xtcp_t* tcp_alloc(void) {
	// �ҵ����أ�û�ҵ����ؿ�
	xtcp_t* tcp, *end;
	for (tcp = tcp_socket, end = tcp_socket + XTCP_CFG_MAX_TCP; tcp < end; tcp++) {
		if (tcp->state == XTCP_STATE_FREE) {
			tcp->state = XTCP_STATE_CLOSED;
			tcp->local_port = 0;
			tcp->remote_port = 0;
			tcp->remote_ip.addr = 0;//addr���㸳ֵ��
			tcp->handler = (xtcp_handler_t)0;
			tcp->remote_win = XTCP_MSS_DEFAULT;
			tcp->remote_mss = XTCP_MSS_DEFAULT;
			tcp->next_seq = tcp->unacked_seq = tcp_get_init_seq();
			tcp->ack = 0;
			tcp_buf_init(&tcp->tx_buf);//��tcp����ʱ�����ϻ����ʼ��
			tcp_buf_init(&tcp->rx_buf);//��tcp����ʱ�����ϻ����ʼ��
			return tcp;
		}
	}
	return (xtcp_t*)0;
}

// ���� �ͷź���
static void tcp_free(xtcp_t* tcp) {
	tcp->state = XTCP_STATE_FREE;
}

static xtcp_t* tcp_find(xipaddr_t* remote_ip, uint16_t remote_port, uint16_t local_port) {
	// �ض��Ĳ��һ���
	xtcp_t* tcp, * end;
	xtcp_t* founded_tcp = (xtcp_t*)0;

	for (tcp = tcp_socket, end = &tcp_socket[XTCP_CFG_MAX_TCP]; tcp < end; tcp++) {
		if (tcp->state == XTCP_STATE_FREE) {// ������
			continue;
		}
		if (tcp->local_port != local_port) {// ���ض˿ڲ�ͬ��
			continue;
		}
		if (xipaddr_is_equal(remote_ip, &tcp->remote_ip) && (remote_port == tcp->remote_port)) {// ����ip�Ƚ� ����ip���������ж�
			return tcp;// ���ж˿ں͵�ַһ�� �򷵻�
		}
		if (tcp->state == XTCP_STATE_LISTEN) {//���ض˿���ͬ�����ԭ����״̬��������
			founded_tcp = tcp;
		}
	}
	return founded_tcp;
}

static xnet_err_t tcp_send_reset(uint32_t remote_ack, uint16_t local_port, xipaddr_t* remote_ip, uint16_t remote_port) {
	xnet_packet_t* packet = xnet_alloc_for_send(sizeof(xtcp_hdr_t));//�����ݣ���ͷ��С����
	xtcp_hdr_t* tcp_hdr = (xtcp_hdr_t*)packet->data;

	tcp_hdr->src_port = swap_order16(local_port);
	tcp_hdr->dest_port = swap_order16(remote_port);
	tcp_hdr->seq = 0;
	tcp_hdr->ack = swap_order32(remote_ack);
	tcp_hdr->hdr_flags.all = 0;
	tcp_hdr->hdr_flags.hdr_len = sizeof(xtcp_hdr_t) / 4; //���ֽ�Ϊһ��λ
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
	uint16_t data_size = tcp_buf_wait_send_count(&tcp->tx_buf);//ȡһ�� �ж�������Ҫ��
	uint16_t opt_size = (flags & XTCP_FLAG_SYN) ? 4 : 0;//Ϊ����mms

	// ����ʱ�� Ҫ��һ�¶Է���window��С
	if (tcp->remote_win) {
		data_size = min(data_size, tcp->remote_win);//���ն˵Ĵ��ڴ�С�Ա� 
		data_size = min(data_size, tcp->remote_mss);//ip��Ƭ�ĶԱ� ��ֹ��Ƭ ��Ƭ���Թ��˲�д��
		if (data_size + opt_size > XTCP_DATA_MAX_SIZE) {	// ����� ��Сһ�� ��֤���ᳬ����̫���߽�
			data_size = XTCP_DATA_MAX_SIZE - opt_size;
		}
	}
	else {//���ڴ�СΪ0
		data_size = 0;	//�������� ��ack
	}


	//packet = xnet_alloc_for_send(opt_size + sizeof(xtcp_hdr_t));
	packet = xnet_alloc_for_send(data_size + opt_size + sizeof(xtcp_hdr_t));	// ���ݿ��ǽ�ȥ
	tcp_hdr = (xtcp_hdr_t*)packet->data;
	tcp_hdr->src_port = swap_order16(tcp->local_port);
	tcp_hdr->dest_port = swap_order16(tcp->remote_port);
	tcp_hdr->seq = swap_order32(tcp->next_seq);
	tcp_hdr->ack = swap_order32(tcp->ack);
	tcp_hdr->hdr_flags.all = 0;
	tcp_hdr->hdr_flags.hdr_len = (opt_size + sizeof(xtcp_hdr_t)) / 4; //���ֽ�Ϊһ��λ
	tcp_hdr->hdr_flags.flags = flags;
	tcp_hdr->hdr_flags.all = swap_order16(tcp_hdr->hdr_flags.all);
	tcp_hdr->window = swap_order16(tcp_buf_free_count(&tcp->rx_buf));//��ʣ�µĴ��ڴ�С
	tcp_hdr->checksum = 0;
	tcp_hdr->urgent_ptr = 0;
	// ��������ֱ��� �ж�SYN�Ƿ���1 ��Ϊmms���ӵİɣ�
	if (flags & XTCP_FLAG_SYN) {
		uint8_t* opt_data = packet->data + sizeof(xtcp_hdr_t);	// ָ��ѡ������
		opt_data[0] = XTCP_KIND_MSS;
		opt_data[1] = 4;//���� 4�ֽ�
		*(uint16_t*)(opt_data + 2) = swap_order16(XTCP_MSS_DEFAULT);

	}

	//У��ͼ���֮ǰ �����ݶ�����
	tcp_buf_read_for_send(&tcp->tx_buf, packet->data + opt_size + sizeof(xtcp_hdr_t), data_size);
	tcp_hdr->checksum = checksum_peso(&netif_ipaddr, &tcp->remote_ip, XNET_PROTOCOL_TCP, (uint16_t*)packet->data, packet->size);
	tcp_hdr->checksum = tcp_hdr->checksum ? tcp_hdr->checksum : 0xFFFF;

	err = xip_out(XNET_PROTOCOL_TCP, &tcp->remote_ip, packet);
	if (err < 0) return err;

	// �Ѿ����� Ҫ��Сһ�¶Է����� ���ջ������� �ѷ� �Է�����Ҫ��С
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

// tcp����
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
		// �����0 У���ȡ������ֹ������У��͵ļ���
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

	// ��һ�����ƿ�ȥ�������ݰ�
	tcp = tcp_find(remote_ip, tcp_hdr->src_port, tcp_hdr->dest_port);
	if (tcp == (xtcp_t*)0) {// ���û�ҵ����ƿ���Դ�����߶Է� ����UDP��icmp
		//���͸�λ
		tcp_send_reset(tcp_hdr->seq + 1, tcp_hdr->dest_port, remote_ip, tcp_hdr->src_port);
		return;
	}
	tcp->remote_win = tcp_hdr->window;

	if (tcp->state == XTCP_STATE_LISTEN) {
		// �ж��ǲ��������������ݰ��������µ�TCP���ƿ� ȥ��������
		tcp_process_acccept(tcp, remote_ip, tcp_hdr);
		return;
	}

	// �㷢�͵�seq �� ��ϣ�����ܵ�ack ��ͬ�Ͷ���
	if (tcp_hdr->seq != tcp->ack) {
		tcp_send_reset(tcp_hdr->seq + 1, tcp_hdr->dest_port, remote_ip, tcp_hdr->src_port);
		return;
	}
	remove_header(packet, tcp_hdr->hdr_flags.hdr_len * 4);
	switch (tcp->state) {
	case XTCP_STATE_SYN_RECVD:
		if (tcp_hdr->hdr_flags.flags & XTCP_FLAG_ACK) {//���յ�ACK��
			tcp->unacked_seq++;
			tcp->state = XTCP_STATE_ESTABLISHED;//�´���������XTCP_STATE_ESTABLISHED��
			tcp->handler(tcp, XTCP_CONN_CONNECTED);//����http�������Ѿ���������
		}
		break;
	case XTCP_STATE_ESTABLISHED:
		if (tcp_hdr->hdr_flags.flags & (XTCP_FLAG_ACK | XTCP_FLAG_FIN)) {
			// ��������
			if (tcp_hdr->hdr_flags.flags & XTCP_FLAG_ACK) {	//�ж������������
				// ͨ�����к��ж�
				if ((tcp->unacked_seq < tcp_hdr->ack) && (tcp_hdr->ack <= tcp->next_seq)) { //P30 24min
					uint16_t curr_ack_size = tcp_hdr->ack - tcp->unacked_seq;
					tcp_buf_add_acked_count(&tcp->tx_buf, curr_ack_size);
					tcp->unacked_seq += curr_ack_size;
				}
			}

			read_size = tcp_recv(tcp, (uint8_t)tcp_hdr->hdr_flags.flags, packet->data, packet->size);

			//printf("connection ok");
			if (tcp_hdr->hdr_flags.flags & (XTCP_FLAG_FIN)) {	//�Է�����ҶϿ�����		//�����ر�
				tcp->state = XTCP_STATE_LAST_ACK;	// tcp���ƿ�Ӧ��ģ�����״̬�� nb
				tcp->ack++;	// +1��ϣ����һ�ν��յ���� XTCP_FLAG_FINռһ����� ͬʱ��ζǰ���㷢�İ����Ѿ��յ�
				tcp_send(tcp, XTCP_FLAG_FIN | XTCP_FLAG_ACK);	// ʡ�����ݴ��䣬ֱ�ӵ���������
			}
			else if (read_size) {
				tcp_send(tcp, XTCP_FLAG_ACK);
				tcp->handler(tcp, XTCP_CONN_DATA_RECV);
			}
			else if (tcp_buf_wait_send_count(&tcp->tx_buf)) {//���ǽ��� ������û������Ҫ��
				tcp_send(tcp, XTCP_FLAG_ACK);
			}
		}
		break;
	case XTCP_STATE_FIN_WAIT_1:	//�����رպ�Ľ��룬�ȴ��Է�����Ӧ 
		if ((tcp_hdr->hdr_flags.flags & (XTCP_FLAG_FIN | XTCP_FLAG_ACK)) == (XTCP_FLAG_FIN | XTCP_FLAG_ACK)) {	// �Է��Ĵλ����еĶ����κϲ�
			tcp_free(tcp);
		}
		else if (tcp_hdr->hdr_flags.flags & XTCP_FLAG_ACK) {	// �Է��Ĵλ����е� �ڶ��λ���
			tcp->state = XTCP_STATE_FIN_WAIT_2;
		}
		break;
	case XTCP_STATE_FIN_WAIT_2:
		// ���ﲻ�������ݽ�����
		if (tcp_hdr->hdr_flags.flags & XTCP_FLAG_FIN) {	// ������
			tcp->ack++;
			tcp_send(tcp, XTCP_FLAG_ACK);
			tcp_free(tcp);
		}
		break;
	case XTCP_STATE_LAST_ACK:
		if (tcp_hdr->hdr_flags.flags & (XTCP_FLAG_ACK)) {//�򵥵㣬ֱ���ͷŵ��ˣ���д����CLOSE״̬��
			tcp->handler(tcp, XTCP_CONN_CLOSED);
			tcp_free(tcp);
		}
		break;
	}
}

// ��ʼ������
void xtcp_init(void) {
	// ���
	memset(tcp_socket, 0, sizeof(tcp_socket));
}


// ����򿪽ӿ� ��udpһ�� ����ص����� ����ص�����
xtcp_t* xtcp_open(xtcp_handler_t handler) {
	xtcp_t* tcp = tcp_alloc();//����һ������tcp��
	if (!tcp) return (xtcp_t*)0;//����ʧ��

	tcp->state = XTCP_STATE_CLOSED;// tcp�ڲ�Э��״̬����close
	tcp->handler = handler;
	return tcp;
}

// �� ��udpһ��
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
// ����
xnet_err_t xtcp_listen(xtcp_t* tcp) {
	tcp->state = XTCP_STATE_LISTEN;
	return XNET_ERR_OK;
}

//���ͺ��� ���ض���������
int xtcp_write(xtcp_t* tcp, uint8_t* data, uint16_t size) {
	int sended_count;
	if ((tcp->state != XTCP_STATE_ESTABLISHED)) { // �ж�״̬���������ӽ���״̬�ͷ��Ͳ���
		return -1;
	}
	sended_count = tcp_buf_write(&tcp->tx_buf, data, size);//����д�뺯��  ϣ��дsize ��ʵ�ʴ�С����sended_count
	if (sended_count) {
		tcp_send(tcp, XTCP_FLAG_ACK);
	}
	return sended_count;//û��ȫд��ͽ���Ӧ�ó���ȥ�ж���
}

int xtcp_read(xtcp_t* tcp, uint8_t* data, uint16_t size) {
	// ��ȡ �ӽ��ջ����ж�
	return tcp_buf_read(&tcp->rx_buf, data, size);
}

// �ر�
xnet_err_t xtcp_close(xtcp_t* tcp) {
	xnet_err_t err;
	if (tcp->state == XTCP_STATE_ESTABLISHED) {	// �����ر�����
		err = tcp_send(tcp, XTCP_FLAG_FIN | XTCP_FLAG_ACK);//����Ӧ����ֻ��FIN
		//err = tcp_send(tcp, XTCP_FLAG_FIN);//����Ӧ����ֻ��FIN
		if (err < 0) return err;
		tcp->state = XTCP_STATE_FIN_WAIT_1;
	}
	else {	// ���������״̬ �ͼ򵥵�ֱ���ͷ���
		tcp_free(tcp);
	}
	return XNET_ERR_OK;
}