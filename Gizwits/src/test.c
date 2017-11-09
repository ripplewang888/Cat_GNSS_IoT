#include "../include/tool.h"
#include "../include/cloud.h"
#include "../include/lan.h"

//globle log handle
log_st		*_log;

//check socket fd, if read error, will close it and reconnect
int check_error_num(int error_num, cloud_st *cloud, lan_st *lan)
{
	int		i;

	if(error_num == UDP_RECV_ERROR){
		log_debug(_log, "udp recv error");
		close(lan->udp_server);
		lan->udp_server = 0;
	}
	
	for(i=0; i<MAX_CLIENT; i++){
		if(error_num == TCP_RECV_ERROR_BASE+i){
			log_debug(_log, "tcp client[%d] fd:%d error", i, lan->client_fd[i]);
			close(lan->client_fd[i]);
			lan->client_fd[i] = 0;
		}
	}

	if(error_num == cloud->mqtt_fd){
		close(cloud->mqtt_fd);
		cloud->mqtt_fd = 0;
	}
	return 0;
}

//some job need to do timed
int check_time_task(cloud_st *cloud, lan_st *lan)
{
	int		now_time;

	if(cloud == NULL || lan == NULL) return -1;

	now_time = time(NULL);
	if((now_time - cloud->mqtt_ping_time) > 40){
		cloud_mqtt_ping(cloud);
		cloud->mqtt_ping_time = now_time;
	}

	return 0;
}

int lan_do_one_packet(void *in, char action, char *kv, int kv_len)
{
	if(in == NULL) return -1;
	log_debug(_log, "this is one packet from lan(app), action: %d", action);

	switch(action){
		case ACTION_CONTROL:
			log_debug(_log, "please control your device here, and then report the new status, both app and mqtt");
			//cloud_send_data(device->cloud, ACTION_REPORT_STATUS, buf, len);
			//lan_send_data(device->lan, ACTION_REPORT_STATUS, buf, len);
			break;
		
		case ACTION_READ_STATUS:
			log_debug(_log, "please update your device status into data buf and send back to app");		
			//lan_send_data(device->lan, ACTION_READ_STATUS_ACK, buf, len);
			break;

		case ACTION_TRANS_RECV:
			log_debug(_log, "this is your raw data from app");
			break;
	}

	return 0;
}

int cloud_do_one_packet(void *in, char action, char *kv, int kv_len)
{
	if(in == NULL) return -1;
	log_debug(_log, "this is one packet from matt, action: %d", action);

	switch(action){
		case ACTION_CONTROL:
			log_debug(_log, "please control your device here, and then report the new status, both app and mqtt");
			//cloud_send_data(device->cloud, ACTION_REPORT_STATUS, buf, len);
			//lan_send_data(device->lan, ACTION_REPORT_STATUS, buf, len);
			break;
		
		case ACTION_READ_STATUS:
			log_debug(_log, "please update your device status into data buf and send back to mqtt");		
			//cloud_send_data(device->cloud, ACTION_READ_STATUS_ACK, buf, len);
			break;

		case ACTION_TRANS_RECV:
			log_debug(_log, "this is your raw data from mqtt");
			break;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int			i, max_sock, error_num, new_client, report_time, one_packet_len;
	struct 		timeval select_timeval;
	char		send_buf[512], one_packet[BUF_LEN];
	short		cmd;
	fd_set		readfds;
	
	//API:	define these
	con_st		con;		//config info
	cloud_st	*cloud;		//cloud handle
	
	//API:	read confile info from GAgent_Lite.xml
	parse_config(&con, "doc/GAgent_Lite.xml");

	signal(SIGPIPE, SIG_IGN);
	_log = log_init("doc/", 1000000, 2, 1);
	log_debug(_log, "Running......");

	//API:	init cloud and lan using the config VAR
	cloud = cloud_init(&con, "doc/GAgent_Lite.xml");
	
	memset(send_buf, 0, 512);
	snprintf(send_buf, 512, "%s", "30 79 00 20 61 70 70 32 64 65 76 2F 50 5A 34 73 43 65 47 4C 6A 53 64 5A 4B 66 34 70 44 70 55 78 34 4D 2F 30 00 00 00 03 52 00 00 90 05 7B 22 6D 73 67 49 44 22 3A 31 2C 22 72 65 73 75 6C 74 22 3A 7B 22 63 6F 64 65 22 3A 33 30 32 38 2C 22 64 61 74 61 22 3A 7B 7D 2C 22 6D 73 67 22 3A 22 44 65 76 20 69 73 20 61 6C 72 65 61 64 79 20 72 65 67 69 73 74 65 72 65 64 22 7D 7D ");
	StrToHex(cloud->recv_buf, send_buf, 123);

	while(get_one_cloud_packet(cloud, one_packet, &one_packet_len, &cmd) == 0){
                memset(send_buf, 0, 512);
                HexToStr((unsigned char *)send_buf, (unsigned char *)one_packet, one_packet_len);
                log_debug(_log, "get one_packet, cmd: %d, bytes: %s", cmd, send_buf);
	}


	return 0;
}


