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

int lan_do_one_packet(void *in, unsigned char action, char *kv, int kv_len)
{
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

int cloud_do_one_packet(void *in, unsigned char action, char *kv, int kv_len)
{
	cloud_st	*cloud = NULL;
	log_debug(_log, "this is one packet from mqtt, action: %d", action);

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

		case ACTION_PUSH_OTA:
			cloud = (cloud_st *)in;
			log_debug(_log, "this is push ota msg from m2m");
			check_ota(cloud);
			log_debug(_log, "get your ota file url: %s", cloud->ota_info);
			//do you ota here
			break;
	}

	return 0;
}

int main(int argc, char **argv)
{
#ifdef USE_SSL
	SSL_library_init();
#endif

	int			i, max_sock, error_num, new_client, report_time;
	struct 		timeval select_timeval;
	char		send_buf[512];
	fd_set		readfds;
	
	//API:	define these
	con_st		con;		//config info
	cloud_st	*cloud;		//cloud handle
	lan_st		*lan;		//lan handle
	
	//API:	read confile info from GAgent_Lite.xml
	parse_config(&con, "doc/GAgent_Lite.xml");

	signal(SIGPIPE, SIG_IGN);
	_log = log_init("doc/running_log.txt", 1000000, 2, 1);
	log_debug(_log, "Running......");

	//API:	init cloud and lan using the config VAR
	cloud = cloud_init(&con, "00163E002C7C", "doc/GAgent_Lite.xml");
	lan = lan_init(&con);
	if(cloud == NULL || lan == NULL) return -1;
	
	//check ota when reboot;
	/*
	if(check_ota(cloud) == 0){
		log_debug(_log, "need ota, file url: %s", cloud->ota_info);
		//do ota here;
	}
	*/

	//API:	cloud API, will connect mqtt
//	memcpy(cloud->mqtt_server, "sandbox.gizwits.com", strlen("sandbox.gizwits.com"));
//	cloud->mqtt_port = G_M2M_PORT;
	cloud_mqtt_connect(cloud, con.did, con.passcode);
	
	report_time = time(NULL);
	while(1){
		check_time_task(cloud, lan);  //mqtt heart ping
		if(time(NULL) - report_time > 3){
			memset(send_buf, 0, 512);
			snprintf(send_buf, 512, "this is test data, now time: %d", (int)time(NULL));
			//log_debug(_log, "now will report status: %s", send_buf);
			//API:	cloud API, will send you data to MQTT Server
			cloud_send_data(cloud, ACTION_TRANS_SEND, (char *)send_buf, strlen(send_buf));
			report_time = time(NULL);
		}		

		select_timeval.tv_sec = 1;
		select_timeval.tv_usec = 0;

		FD_ZERO(&readfds);
		if(cloud->mqtt_fd > 0){
			FD_SET(cloud->mqtt_fd, &readfds);
			max_sock = cloud->mqtt_fd;
		}
		for(i=0; i<MAX_CLIENT; i++){
			if(lan->client_fd[i] <= 0) continue;
			FD_SET(lan->client_fd[i], &readfds);
			if(lan->client_fd[i] > max_sock) max_sock = lan->client_fd[i];
		}
		FD_SET(lan->udp_server, &readfds);
		if(max_sock < lan->udp_server) max_sock = lan->udp_server;
		FD_SET(lan->tcp_server, &readfds);
		if(max_sock < lan->tcp_server) max_sock = lan->tcp_server;

		if(select(max_sock+1, &readfds, NULL, NULL, &select_timeval) <= 0){
			//log_debug(_log, "server: no select");
			continue;
		}
		
		error_num = 0;
		log_debug(_log, "server: get one select issue");
		if(FD_ISSET(cloud->mqtt_fd, &readfds)){ 
			log_debug(_log, "cloud->mqtt_fd can read");
			//API:	cloud API, parse the data from MQTT server
			error_num = cloud_do_mqtt(cloud);
		}
		if(FD_ISSET(lan->udp_server, &readfds)){
			log_debug(_log, "lan->udp_server can read");
			//API:	lan API, return discovery info
			error_num = lan_do_udp(lan);
		}
		if(FD_ISSET(lan->tcp_server, &readfds)){
			log_debug(_log, "lan->tcp_server can read");
			new_client = accept(lan->tcp_server, NULL, NULL);
			for(i=0; i<MAX_CLIENT; i++){
				if(lan->client_fd[i] == 0){
					lan->client_fd[i] = new_client;
					log_debug(_log, "get one client place: %d, fd: %d", i, lan->client_fd[i]);
					break;
				}
			}
			if(i == MAX_CLIENT) log_debug(_log, "too many client");
		}
		for(i=0; i<MAX_CLIENT; i++){
			if(FD_ISSET(lan->client_fd[i], &readfds)){
				log_debug(_log, "lan->client_fd[%d]: %d can read", i, lan->client_fd[i]);
				//API, lan API, parse the data from local APP
				error_num = lan_do_tcp(lan, i);
				if(error_num) break;
			}
		}
		
		check_error_num(error_num, cloud, lan);
	}
	
	return 0;
}


