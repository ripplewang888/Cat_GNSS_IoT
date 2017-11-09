#include "../include/lan.h"

extern log_st	*_log;

lan_st	*lan_init(con_st *con)
{
    lan_st    *lan;
    
    lan = (lan_st *)malloc(sizeof(lan_st));
    if(lan == NULL) return NULL;
    memset(lan, 0, sizeof(lan_st));

	lan->con = con;

	lan->udp_server = create_read_udp(12414);
	log_debug(_log, "create lan udp server fd: %d", lan->udp_server);
	fcntl(lan->udp_server, F_SETFL, O_NONBLOCK);
	    
	lan->tcp_server = create_listen(12416);
	log_debug(_log, "create lan tcp server fd: %d", lan->tcp_server);

	lan_do_udp(lan);
	return lan;
}

int lan_do_udp(lan_st *lan)
{
	int				read_len, package_len, addrLen, flag;
	char			cmd, *index;
	struct			sockaddr addr;
    struct  		sockaddr_in addr_write;
    struct  		in_addr _addr;

	if(lan == NULL || lan->udp_server <= 0) return -1;

	flag = 0;
	addrLen = sizeof(struct sockaddr);
	read_len = recvfrom(lan->udp_server, lan->recv_buf, BUF_LEN, 0, &addr, (socklen_t *)&addrLen);
	if(read_len <= 0){
		log_debug(_log, "no data recv, will broadcast");
		flag = 1;
	}        

	//00 00 00 03 03 00 00 03
	memset(lan->send_buf, 0, BUF_LEN);
	index = lan->send_buf;
	package_len = 0;

	//head
	*index = 0x00; *(index+1) = 0x00; *(index+2) = 0x00; *(index+3) = 0x03;
	index += 4;
	
	//len
	*index = 0x0A;
	index += 1;

	//flag
	*index = 0x00;
	index += 1;

	//cmd
	*index = 0x00; 
	if(flag == 0) *(index+1) = 0x04;
	if(flag == 1) *(index+1) = 0x05;
	index += 2;

	//did_len
	*index = 0x00; *(index+1) = 0x16;
	index += 2;
	//did
	memcpy(index, lan->con->did, 22);
	index += 22;

	//mac_len
	*index = 0x00; *(index+1) = 0x0C;
	index += 2;
	//mac
	memcpy(index, lan->con->mac, 12);
	index += 12;
	
	//hard_version_len
	*index = 0x00; *(index+1) = 0x08;
	index += 2;
	//hard version
	memcpy(index, lan->con->hard_version, 8);
	index += 8;

	//pk_len
	*index = 0x00; *(index+1) = 0x20;
	index += 2;
	//pk
	memcpy(index, lan->con->pk, 32);
	index += 32;
	
	//dev_status
	index+= 8;

	package_len = index - lan->send_buf;

	//broadcast
	if(flag == 1){
		log_debug(_log, "broadcast...");
	    inet_aton("255.255.255.255", &_addr);
	    addr_write.sin_family = AF_INET;
	    addr_write.sin_port = htons(2415);
	    addr_write.sin_addr = _addr;
    	sendto(lan->udp_server, lan->send_buf, package_len, 0, (struct sockaddr *)&addr_write, sizeof(struct sockaddr));
	}	
	//discover
	else{
		log_debug(_log, "discover...");
		sendto(lan->udp_server, lan->send_buf, package_len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr));
	}
		
	return 0;
}

int lan_get_device_info(lan_st *lan)
{
	char		*index, *len_index;
	if(lan == NULL) return -1;

	memset(lan->send_buf, 0, BUF_LEN);
	index = lan->send_buf;

	//head
	*index = 0x00; *(index+1) = 0x00; *(index+2) = 0x00; *(index+3) = 0x03;
	index += 4;
	
	//len
	len_index = index;
	index += 1;
	
	//flag
	*index = 0x00;
	index += 1;

	//cmd
	*index = 0x00; *(index+1) = 0x14;
	index += 2;

	//wifi hard version
	memcpy(index, lan->con->hard_version, 8);
	index += 8;

	//wifi soft version
	memcpy(index, lan->con->soft_version, 8);
	index += 8;

	//mcu hard version
	index += 8;

	//mcu soft version
	index += 8;

	//p0 version
	index += 8;

	//remain1
	index += 8;
	
	//remain2
	*index = 0x00; *(index+1) = 0x00;
	index += 2;

	//pk_len
	*index = 0x00; *(index+1) = 0x20;
	index += 2;
	//pk
	memcpy(index, lan->con->pk, 32);
	index += 32;
	
	lan->send_len = (index - lan->send_buf);
	*len_index = lan->send_len - HEAD_LEN;

	return 0;
}

int	lan_get_passcode(lan_st *lan)
{
	int				passcode_len;
	char			*index, *len_index;
	if(lan == NULL) return -1;
	
	log_debug(_log, "in lan_get_passcode");
	memset(lan->send_buf, 0, BUF_LEN);
	index = lan->send_buf;

	//00 00 00 03 0X 00 07 00 0X 
	//head
	*index = 0x00; *(index+1) = 0x00; *(index+2) = 0x00; *(index+3) = 0x03;
	index += 4;
	
	//len
	len_index = index;
	index += 1;

	//flag
	*index = 0x00;
	index += 1;

	//cmd
	*index = 0x00; *(index+1) = 0x07;
	index += 2;

	//passcode_len
	passcode_len = strlen(lan->con->passcode);
	*index = 0x00; *(index+1) = passcode_len;
	index += 2;
	//passcode
	memcpy(index, lan->con->passcode, passcode_len);
	index += passcode_len;
	
	lan->send_len = (index - lan->send_buf);
	*len_index = lan->send_len - HEAD_LEN;
	return 0;
}

int	lan_login_device(lan_st *lan, char *packet)
{
	short			passcode_len;
	char			*index, *len_index, passcode_recv[32];
	if(lan == NULL) return -1;
	
	//00 00 00 03 0F 00 00 08 00 0A 52 4F 53 56 48 51 55 4A 4D 56
	memcpy(&passcode_len, packet+HEAD_LEN+1+2, 2);
	passcode_len = ntohs(passcode_len);
	memset(passcode_recv, 0, 32);
	memcpy(passcode_recv, packet+HEAD_LEN+1+2+2, passcode_len);
	log_debug(_log, "get passcode: %s", passcode_recv);

	memset(lan->send_buf, 0, BUF_LEN);
	index = lan->send_buf;

	//00 00 00 03 03 00 00 09 00
	//head
	*index = 0x00; *(index+1) = 0x00; *(index+2) = 0x00; *(index+3) = 0x03;
	index += 4;
	
	//len
	len_index = index;
	index += 1;

	//flag
	*index = 0x00;
	index += 1;

	//cmd
	*index = 0x00; *(index+1) = 0x09;
	index += 2;

	//result
	if(strncmp(passcode_recv, lan->con->passcode, passcode_len) == 0) *index = 0x00; 
	else *index = 1;
	log_debug(_log, "my passcode: %s, recv passcode: %s, result: %d", lan->con->passcode, passcode_recv, *index);
	index += 1;

	lan->send_len = (index - lan->send_buf);
	*len_index = lan->send_len - HEAD_LEN;
	return 0;
}

int	lan_trans_data(lan_st *lan, char *packet)
{
	char			cmd, *index, action, *kv;
	uint16_t		len, kv_len;
    	uint8_t			length_len;
	
	if(lan == NULL) return -1;
	
	index = packet;
	len = mqtt_parse_rem_len((const uint8_t *)index+3);
   	length_len = mqtt_num_rem_len_bytes((const uint8_t*)index+3);
	log_debug(_log, "in lan_trans_data");

	// 00 00 00 03 06 00 00 90 01 01 01 
	// 00 00 00 03 0A 00 00 93 00 00 00 00 01 01 01 
	cmd = *(index+4+length_len+2);
	if((unsigned char)cmd == 0x93){
		memcpy(&(lan->sn), index+4+length_len+3, 4);
		action = *(index+4+length_len+3+4);
		kv = index+4+length_len+3+4+1;
		kv_len = len-8;
	}
	else if((unsigned char)cmd == 0x90){
		action = *(index+4+length_len+3);
		kv = index+4+length_len+3+1;
		kv_len = len-4;
	}
	log_debug(_log, "get action: %d", action);
	
	if((unsigned char)action == 0x01) lan_send_data(lan, 0x00, NULL, 0);
	
	lan_do_one_packet(lan->in, action, kv, kv_len);

	return 0;
}

int 	lan_send_data(lan_st *lan, char action, char *buf, int len)
{
	int	i, length_byte_num, length_remain, write_len;
	char	*index, *len_index, length_bytes[4], debug_info[BUF_LEN];
	
	if(lan == NULL) return -1;

	memset(lan->send_buf, 0, BUF_LEN);
	index = lan->send_buf;
	//head
	*index = 0x00; *(index+1) = 0x00; *(index+2) = 0x00; *(index+3) = 0x03;
	index += 4;

        //get the length, 3 is the length of flag and cmd, 4 is the length of flag, cmd, action
        memset(length_bytes, 0, 4);
        if(buf && len > 0) length_remain = len+4;
        else length_remain = len+3;
        mqtt_length2bytes(length_remain, length_bytes, &length_byte_num);

	//len
	len_index = index;
	index += length_byte_num;
	//flag
	*index = 0x00;
	index += 1;

	//cmd
	*index = 0x00; *(index+1) = 0x91;
	index += 2;

	if(buf && len > 0){
		memcpy(index, &action, 1);
		index += 1;
		//data
		memcpy(index, buf, len);
		index += len;
	}
	lan->send_len = (index - lan->send_buf);
	for(i=0; i<length_byte_num; i++){
                *(len_index+i) = length_bytes[i];
        }

		for(i=0; i<MAX_CLIENT; i++){
			if(lan->client_fd[i] <= 0) continue;
			write_len = writen(lan->client_fd[i], lan->send_buf, lan->send_len);
			memset(debug_info, 0, BUF_LEN);
			HexToStr((unsigned char *)debug_info, (unsigned char *)lan->send_buf, lan->send_len>512?512:lan->send_len);
			log_debug(_log, "write to fd: %d, len: %d %s", lan->client_fd[i], write_len, debug_info);
		}
		memset(lan->send_buf, 0, BUF_LEN);
		lan->send_len = 0;	

	return 0;
}

int	lan_heart_beat(lan_st *lan)
{
	char			*index, *len_index;
	
	if(lan == NULL) return -1;

	memset(lan->send_buf, 0, BUF_LEN);
	index = lan->send_buf;

	//00 00 00 03 02 00 00 16
	//head
	*index = 0x00; *(index+1) = 0x00; *(index+2) = 0x00; *(index+3) = 0x03;
	index += 4;
	
	//len
	len_index = index;
	index += 1;

	//flag
	*index = 0x00;
	index += 1;

	//cmd
	*index = 0x00; *(index+1) = 0x16;
	index += 2;

	lan->send_len = (index - lan->send_buf);
	*len_index = lan->send_len - HEAD_LEN;

	return 0;
}

int get_one_packet(lan_st *lan, char *packet, int *packet_len)
{
	char			*index;
	uint16_t		len;
    	uint8_t			length_len;
	
	if(lan == NULL || packet == NULL || packet_len == NULL || lan->recv_len <= 0) return -1;

	index = lan->recv_buf;

	len = mqtt_parse_rem_len((const uint8_t *)index+3);
    	length_len = mqtt_num_rem_len_bytes((const uint8_t*)index+3);

	*packet_len = 4+len+length_len;
	memset(packet, 0, BUF_LEN);
	memcpy(packet, index, *packet_len);

	memcpy(lan->recv_buf, lan->recv_buf+*packet_len, lan->recv_len-*packet_len);
	lan->recv_len -= *packet_len;
	
	return 0;
}

int lan_do_tcp(lan_st *lan, int i)
{
	int		read_len, write_len, error_num, one_packet_len;
	short		cmd;
	char		debug_info[BUF_LEN], one_packet[BUF_LEN];

	if(lan == NULL || i < 0) return -1;

	memset(lan->recv_buf, 0, BUF_LEN);
	read_len = read(lan->client_fd[i], lan->recv_buf, BUF_LEN);
	lan->recv_len = read_len;
	if(read_len <= 0) return TCP_RECV_ERROR_BASE+i;

	memset(debug_info, 0, BUF_LEN);
	HexToStr((unsigned char *)debug_info, (unsigned char *)lan->recv_buf, lan->recv_len>512?512:lan->recv_len);
	log_debug(_log, "read from client: %d, len: %d %s", lan->client_fd[i], lan->recv_len, debug_info);

	
	//00 00 00 03 04 00 00 90 02 
	//00 00 00 03 04 00 00 90 02 
	//00 00 00 03 03 00 00 15 
	//00 00 00 03 08 00 00 93 00 00 00 04 02 
	//00 00 00 03 03 00 00 13
	//00 00 00 03 06 00 00 90 01 01 00
	//00 00 00 03 03 00 00 03
	while(get_one_packet(lan, one_packet, &one_packet_len) == 0){	
		memset(debug_info, 0, BUF_LEN);
		HexToStr((unsigned char *)debug_info, (unsigned char *)one_packet, one_packet_len>512?512:one_packet_len);
		log_debug(_log, "get one_packet: %s", debug_info);
	
		error_num = 0;	
		memcpy(&cmd, one_packet+6, 2);
		cmd = ntohs(cmd);
		switch(cmd){
			case 0x06:
				error_num = lan_get_passcode(lan);
				break;
			case 0x08:
				error_num = lan_login_device(lan, one_packet);
				break;
			case 0x93:
			case 0x90:
				error_num = lan_trans_data(lan, one_packet);
				break;
			case 0x13:
				error_num = lan_get_device_info(lan);
				break;
			case 0x15:
				error_num = lan_heart_beat(lan);
				break;
			
			default:
				break;
		}
	
		if(error_num > 0) return error_num;
		write_len = writen(lan->client_fd[i], lan->send_buf, lan->send_len);
		memset(debug_info, 0, BUF_LEN);
		HexToStr((unsigned char *)debug_info, (unsigned char *)lan->send_buf, lan->send_len>512?512:lan->send_len);
		log_debug(_log, "write to fd: %d, len: %d %s", lan->client_fd[i], write_len, debug_info);
		memset(lan->send_buf, 0, BUF_LEN);
		lan->send_len = 0;
	}
	return 0;
}

