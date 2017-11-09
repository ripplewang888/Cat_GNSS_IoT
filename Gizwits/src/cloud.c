#include "cloud.h"
#include "aes.h"
#include "trace.h"
#include "tool.h"
#include <time.h>

extern log_st *_log;

#define RIPPLE_DEBUG

//global variable, gizwits的配置和 cloud操作句柄
con_st      global_con;
cloud_st    *global_cloud;      //cloud handle



// 本机大端返回1，小端返回0
int checkCPUendian()
{
       union{
              unsigned long int i;
              unsigned char s[4];
       }c;
 
       c.i = 0x12345678;
       return (0x12 == c.s[0]);
}



cloud_st	*cloud_init(con_st *con, char *mac, char *file_path)
{
	int			need_write_config;
	cloud_st	*cloud = NULL;
	
	cloud = (cloud_st *)malloc(sizeof(cloud_st));
	if(cloud == NULL) {
            TRACE_INFO("cloud malloc failed,cloud is null");        
            return NULL;
	}

	memset(cloud, 0, sizeof(cloud_st));

	cloud->con = con;
       //ripple,此处应该是在mqtt  heart报文有需求
       //考虑其他方式实现
	//cloud->mqtt_ping_time = time(NULL);

	need_write_config = 0;
	if(cloud->con->passcode[0] == '\0'){
		need_write_config = 1;
		memset(cloud->con->passcode, 0, 16);
		memcpy(cloud->con->passcode, cloud->con->pk, 10);
	}

      //配置文件cloud->con->mac 值为空,或者配置文件mac和给定mac不一样
	if(mac != NULL && mac[0] != '\0' && (cloud->con->mac[0] == '\0' || strcmp(cloud->con->mac, mac))){
		TRACE_INFO("mac changed will register a new one");
		need_write_config = 1;
		memset(cloud->con->did, 0, 32);
		memset(cloud->con->mac, 0, 32);
		memcpy(cloud->con->mac, mac, strlen(mac)>32?32:strlen(mac));
	}

      //did通过pk 注册获得
	if(cloud->con->did[0] == '\0'){
		need_write_config = 1;
		memset(cloud->con->did, 0, 32);
               TRACE_INFO("ripple----> in cloud register.");
		cloud_register_safe(cloud, cloud->con->did);
	}

      //ripple, 做完文件系统，需要填充write_config
	if(need_write_config == 1){
		TRACE_INFO("need write config");
		write_config(cloud->con, file_path);	
	}

	cloud_provision_safe(cloud);

	TRACE_INFO("mac: %s, passcode: %s, did: %s, pk: %s, pk_secret: %s, hard_version: %s, soft_version: %s", 
		cloud->con->mac, cloud->con->passcode, cloud->con->did, cloud->con->pk, cloud->con->pk_secret, cloud->con->hard_version, cloud->con->soft_version);	

	return cloud;
}




int	cloud_register_safe(cloud_st *cloud, char *did)
{
	int				i, http_fd, len, ContentLen, totalLen, write_len, read_len, chunked;
	char			*index_body;
	unsigned char	Content[128], pksecHex[48], pksecStr[48], encryptData[128], byteData[4], encryptDataStr[256], urldata[256], decryptData[256], pStr1[16], pStr2[16];
	unsigned char	*pPksecHex, *pPKSec;

       //因为LM61模块没有把socket 的fd给我们， 所以此处无法获得
       http_fd = sock_connect_by_domain(G_SERVICE_DOMAIN_IP, G_SERVICE_PORT);

	memset(Content, 0, 128);
	memset(pksecHex, 0, 48);
	memset(pksecStr, 0, 48);
	memset(encryptData, 0, 128);
	memset(byteData, 0, 2);
	memset(encryptDataStr, 0, 256);
	memset(urldata, 0, 256);
	memset(pStr1, 0, 16);
	memset(pStr2, 0, 16);

	memcpy(pStr1, "/dev/", strlen("/dev/"));
	memcpy(pStr2, "/device", strlen("/device"));
	memcpy(pksecStr, cloud->con->pk_secret, strlen(cloud->con->pk_secret));
	pPKSec = pksecStr;
	pPksecHex = pksecHex; 
     	
	snprintf((char *)Content, 128, "mac=%s&passcode=%s&type=%s", cloud->con->mac, cloud->con->passcode, "normal");
	aesInit();
	StrToHex_noSpace((char *)pPksecHex, (char *)pPKSec, strlen((char *)pPKSec));

	TRACE_INFO( "Content: %s, pksecret(string):%s", Content, pPKSec);
       len = 0;
	len = (int)aesECB128Encrypt(Content, encryptData, (uint8 *)pPksecHex, strlen((const char *)Content));
	TRACE_INFO("encryptDatalen:%d", len);
	if(len > 0){
	    for(i = 0; i < len; i++){
			memset(byteData, 0, 4);
			snprintf((char *)byteData, 4, "%02x", encryptData[i]&0xff);
			memcpy(&encryptDataStr[i*2], byteData, strlen((const char *)byteData));
	    }
	}

	TRACE_INFO( "encryptDataStr:%s\n", encryptDataStr);
	aesDestroy();

	memcpy(&urldata[0], pStr1, strlen((const char *)pStr1));
	memcpy(&urldata[strlen((const char *)pStr1)], cloud->con->pk, 32);
	memcpy(&urldata[strlen((const char *)pStr1)+32], pStr2, strlen((const char *)pStr2));       
    
	ContentLen = strlen((char *)encryptDataStr) + strlen("data=");
	memset(cloud->send_buf, 0, BUF_LEN);

      //ripple, 这里Host是填写的域名,G_SERVICE_DOMAIN,
      //如果连接不成功， 考虑换成IP
	snprintf(cloud->send_buf, BUF_LEN, 
		  "POST %s HTTP/1.1\r\n"
		  "Host:%s\r\n"
		  "Content-Length:%d\r\n"
		  "Content-Type: application/x-www-form-urlencoded\r\n\r\n"
		  "data=%s",
			urldata, G_SERVICE_DOMAIN, ContentLen, encryptDataStr);
    
	cloud->send_len = strlen(cloud->send_buf);
    write_len = writen(http_fd, cloud->send_buf, cloud->send_len);
	TRACE_INFO( "write to fd: %d, len: %d\r\n%s", http_fd, write_len, cloud->send_buf);

	//set_sock_time(http_fd, 3, 3);
	memset(cloud->recv_buf, 0, BUF_LEN);
	cloud->recv_len = readn(http_fd, cloud->recv_buf, BUF_LEN);

	TRACE_INFO( "[ripple Debug] :get response, len: %d\r\n%s", cloud->recv_len, cloud->recv_buf);
	if(strstr(cloud->recv_buf, "Transfer-Encoding: chunked")) chunked = 1;
	else chunked = 0;

	aesInit();
	index_body = strstr(cloud->recv_buf, "\r\n\r\n");
	if(index_body == NULL){
		TRACE_DEBUG( "no body");
		sock_disconnect(http_fd);
		return -1;
	}

	index_body += 4;
	if(chunked == 1){
		index_body = strstr(index_body, "\r\n");
		index_body += 2;
	}
	*(index_body+64) = '\0';
	if(strlen(index_body) != 64) return -1;

	StrToHex_noSpace((char *)encryptDataStr, (char *)index_body, strlen((char *)index_body));
	len = (int)aesECB128Decrypt(encryptDataStr, decryptData, (uint8 *)pPksecHex, strlen((char *)index_body)/2);
	aesDestroy();
	memset(decryptData+strlen("did=")+DID_LENGTH,0,(128 - strlen("did=")-DID_LENGTH));
	TRACE_INFO("ripple----->decryptData: %s", decryptData);

	memset(did, 0, 32);
	memcpy(did, decryptData+4, strlen((const char *)decryptData)-4);
	//sock_disconnect(http_fd);
	return 0;
}

int cloud_register(cloud_st *cloud, char *did)
{
	int 	http_fd, write_len;
	char	content[256], *index;

	http_fd = sock_connect_by_domain(G_SERVICE_DOMAIN_IP, G_SERVICE_PORT);
	if(http_fd <= 0){
		TRACE_DEBUG( "connect G_SERVICE_DOMAIN error");
		return -1;
	}
	TRACE_DEBUG( "connect G_SERVICE_DOMAIN success, fd: %d", http_fd);

	memset(content, 0, 256);
    	sprintf(content,"passcode=%s&mac=%s&product_key=%s", cloud->con->passcode, cloud->con->mac, cloud->con->pk);
	
	memset(cloud->send_buf, 0, BUF_LEN);
	snprintf(cloud->send_buf, BUF_LEN, 
              "POST /dev/devices HTTP/1.1\r\n"
              "Host:%s\r\n"
              "Content-Length:%d\r\n"
              "Content-Type: application/x-www-form-urlencoded\r\n\r\n%s", G_SERVICE_DOMAIN, (int)strlen((char *)content), content);
	cloud->send_len = strlen(cloud->send_buf);	

	write_len = writen(http_fd, cloud->send_buf, cloud->send_len);
	TRACE_DEBUG( "register send to server:\n%s", cloud->send_buf);
	
	set_sock_time(http_fd, 3, 3);
	memset(cloud->recv_buf, 0, BUF_LEN);
	cloud->recv_len = readn(http_fd, cloud->recv_buf, BUF_LEN);
	TRACE_DEBUG( "get response, len: %d\r\n%s", cloud->recv_len, cloud->recv_buf);

	memset(did, 0, 32);
	index = strstr(cloud->recv_buf, "\r\n\r\n");
	if(index == NULL){
		TRACE_DEBUG( "register error");
		return -1;
	}
	index += 4;
	index = strstr(index, "did=");
	if(index == NULL){
		TRACE_DEBUG( "register error");
		return -1;
	}

	index += 4;
	if(index) memcpy(did, index, 22);
	//close(http_fd);
	return 0;
}

int cloud_provision(cloud_st *cloud)
{
	int 	http_fd, write_len;
	char	*index1, *index2, port[16];

	http_fd = sock_connect_by_domain(G_SERVICE_DOMAIN_IP, G_SERVICE_PORT);
	if(http_fd <= 0){
		TRACE_DEBUG( "connect G_SERVICE_DOMAIN error");
		return -1;
	}
	TRACE_DEBUG( "connect G_SERVICE_DOMAIN success, fd: %d", http_fd);

	memset(cloud->send_buf, 0, BUF_LEN);
	snprintf(cloud->send_buf, BUF_LEN, 
              "GET /dev/devices/%s HTTP/1.1\r\n"
              "Host:%s\r\n"
              "Cache-Control: no-cache\r\n"
              "Content-Type: application/x-www-form-urlencoded\r\n\r\n", cloud->con->did, G_SERVICE_DOMAIN);
	cloud->send_len = strlen(cloud->send_buf);	

	write_len = writen(http_fd, cloud->send_buf, cloud->send_len);
	TRACE_DEBUG( "provision send to server:\n%s", cloud->send_buf);
	
	set_sock_time(http_fd, 3, 3);
	memset(cloud->recv_buf, 0, BUF_LEN);
	cloud->recv_len = readn(http_fd, cloud->recv_buf, BUF_LEN);
	TRACE_DEBUG( "get response, len: %d\r\n%s", cloud->recv_len, cloud->recv_buf);
	
	//host=sandbox.gizwits.com&port=1883&server_ts=1481943906&log_host=119.29.48.145&log_port=1884&biz_log=0&sys_log=0
	index1 = strstr(cloud->recv_buf, "\r\n\r\n");
	if(index1 == NULL){
		TRACE_DEBUG( "provision error\r\n");
		return -1;
	}
	index1 += 4;
	index1 = strstr(index1, "host=");
	if(index1){
		index1 += 5;
		index2 = strchr(index1, '&');
		memset(cloud->mqtt_server, 0, 128);
		memcpy(cloud->mqtt_server, index1, index2-index1);
	
		index1 = strstr(index1, "port=");
		index1 += 5;
		index2 = strchr(index1, '&');
		memset(port, 0, 16);
		memcpy(port, index1, index2-index1);	
		cloud->mqtt_port = atoi(port);
	}
	TRACE_DEBUG( "provision success, get domain: %s, port: %d", cloud->mqtt_server, cloud->mqtt_port);	
	//close(http_fd);
	return 0;
}

int cloud_provision_safe(cloud_st *cloud)
{
	int				i, http_fd, len, ContentLen, totalLen, write_len, read_len;
	char			*index_body, *index_tail;
	unsigned char	pksecHex[32], pksecStr[48], encryptData[128], byteData[4], encryptDataStr[256], urldata[256];
	unsigned char	decryptData[256], pStr1[16], pStr2[16], pStr3[16];
	unsigned char	*pPksecHex, *pPKSec;


	http_fd = sock_connect_by_domain(G_SERVICE_DOMAIN_IP, G_SERVICE_PORT);
       //因为获取不到socket fd, 所以没有判断意义
//	if(http_fd <= 0){
//		TRACE_DEBUG( "connect G_SERVICE_DOMAIN error");
//		return -1;
//	}
	//TRACE_DEBUG( "connect G_SERVICE_DOMAIN success, fd: %d", http_fd);

	memset(pksecHex, 0, 32);
	memset(pksecStr, 0, 48);
	memset(encryptData, 0, 128);
	memset(byteData, 0, 4);
	memset(encryptDataStr, 0, 256);
	memset(urldata, 0, 256);
	memset(pStr1, 0, 16);
	memset(pStr2, 0, 16);
	memset(pStr3, 0, 16);

	memcpy(pStr1, "/dev/", strlen("/dev/"));
	memcpy(pStr2, "/device", strlen("/device"));
    memcpy(pStr3, "?did=", strlen("?did="));;
	memcpy(pksecStr, cloud->con->pk_secret, strlen(cloud->con->pk_secret));
	pPKSec = pksecStr;
	pPksecHex = pksecHex; 
     	
	aesInit();
	StrToHex_noSpace((char *)pPksecHex, (char *)pPKSec, strlen((char *)pPKSec));

    TRACE_DEBUG( "did: %s, pksecret(string):%s\n", cloud->con->did, pPKSec);
    len = (int)aesECB128Encrypt((uint8 *)cloud->con->did, encryptData, (uint8 *)pPksecHex, strlen((const char *)(cloud->con->did)));
	TRACE_DEBUG( "encryptDatalen:%d", len);
	if(len > 0){
        for(i = 0; i < len; i++){
			memset(byteData, 0, 4);
			snprintf((char *)byteData, 4, "%02x", encryptData[i]&0xff);
			memcpy(&encryptDataStr[i*2], byteData, strlen((const char *)byteData));
        }
	}
    aesDestroy();
    memcpy(&urldata[0], pStr1, strlen((const char *)pStr1));
    memcpy(&urldata[strlen((const char *)pStr1)], cloud->con->pk, strlen(cloud->con->pk));
    memcpy(&urldata[strlen((const char *)pStr1)+strlen(cloud->con->pk)], pStr2, strlen((const char *)pStr2));
    memcpy(&urldata[strlen((const char *)pStr1)+strlen(cloud->con->pk)+strlen((const char *)pStr2)], pStr3, strlen((const char *)pStr3));
    memcpy(&urldata[strlen((const char *)pStr1)+strlen(cloud->con->pk)+strlen((const char *)pStr2)+strlen((const char *)pStr3)], encryptDataStr, strlen((const char *)encryptDataStr));

	memset(cloud->send_buf, 0, BUF_LEN);
    //ripple, 如果连接不成功，考虑将G_SERVICE_DOMAIN换成119.29.47.111
    snprintf(cloud->send_buf, BUF_LEN,
          "GET %s HTTP/1.1\r\n"
          "Host: %s\r\n"
          "Cache-Control: no-cache\r\n"
          "Content-Type: application/x-www-form-urlencoded\r\n\r\n", urldata, G_SERVICE_DOMAIN);
	cloud->send_len = strlen(cloud->send_buf);    
	
	write_len = writen(http_fd, cloud->send_buf, cloud->send_len);
    TRACE_INFO( "provision send to server:\n%s", cloud->send_buf);
	
	//set_sock_time(http_fd, 3, 3);
	memset(cloud->recv_buf, 0, BUF_LEN);
	cloud->recv_len = readn(http_fd, cloud->recv_buf, BUF_LEN);
	TRACE_INFO( "get response, len: %d\r\n%s", cloud->recv_len, cloud->recv_buf);
	
	index_body = strstr(cloud->recv_buf, "\r\n\r\n");
	if(index_body == NULL){
		TRACE_DEBUG( "no body");
		sock_disconnect(http_fd);
		return -1;
	}
	index_body += 4;
	index_body = strstr(index_body, "\r\n");
	index_body += 2;
	index_tail = strstr(index_body, "\r\n");
	if(index_tail) *index_tail = '\0';

	StrToHex_noSpace((char *)encryptDataStr, (char *)index_body, strlen((char *)index_body));
    aesInit();
    memset(decryptData, 0, 256);
    len = (int)aesECB128Decrypt(encryptDataStr, decryptData, (uint8 *)pPksecHex, strlen((char *)index_body)/2);
    aesDestroy();
    TRACE_INFO( "decryptData: %s", decryptData);
    //host=sandbox.gizwits.com&port=1883&server_ts=1496240910&log_host=119.29.48.145&log_port=1884&biz_log=0&sys_log=0

    if(decryptData > 0){
        index_body = strstr(decryptData, "host=");
        if(index_body){
            index_body += strlen("host=");
            index_tail = strchr(index_body, '&');
            if(index_tail){
                memset(cloud->mqtt_server, 0, 128);
                memcpy(cloud->mqtt_server, index_body, (index_tail-index_body)>128?128:(index_tail-index_body));
                TRACE_INFO( "parse mqtt_server: %s", cloud->mqtt_server);
            }
        }
        index_body = strstr(decryptData, "port=");
        if(index_body){
            index_body += strlen("port=");
            index_tail = strchr(index_body, '&');
            if(index_tail){
                *index_tail = '\0';
                cloud->mqtt_port = atoi(index_body);
                TRACE_INFO( "parse mqtt_port: %d", cloud->mqtt_port);
            }
        }
    }

	sock_disconnect(http_fd);
	return 0;
}


int cloud_mqtt_ping(cloud_st *cloud)
{
    int ret = 0;

	if(cloud == NULL ||  cloud->broker.send == NULL) return -1;
#if 0     
#ifdef USE_SSL
	if(cloud->broker.socket_info == NULL){
		TRACE_DEBUG( "will reconnect mqtt");
		cloud_mqtt_connect(cloud, cloud->con->did, cloud->con->passcode);
	}
#else
	if(*(int*)(cloud->broker.socket_info) <= 0){
		TRACE_DEBUG( "will reconnect mqtt");
		cloud_mqtt_reconnect(cloud, cloud->con->did, cloud->con->passcode);
	}
#endif
#endif
    if(ret == 0)
    {
        mqtt_ping(&(cloud->broker));
        TRACE_INFO( "now ping mqtt");
    }
	return 0;
}

int get_one_cloud_packet(cloud_st *cloud, char *packet, int *packet_len, short *cmd)
{       
	int			mqtt_head_len, total_pack_len, topic_len;
    char                    *index;
    uint16_t                len;
    uint8_t                 length_len;
	char			debug_info[BUF_LEN];        

    if(cloud == NULL || packet == NULL || packet_len == NULL || cloud->recv_len <= 0 || cmd == NULL) return -1;

	memset(debug_info, 0, BUF_LEN);
    HexToStr((unsigned char *)debug_info, (unsigned char *)cloud->recv_buf, cloud->recv_len>512?512:cloud->recv_len);
    TRACE_DEBUG( "****** in get one cloud packet, len: %d, bytes: %s", cloud->recv_len, debug_info);
 
	memset(packet, 0, BUF_LEN);
	//D0 00 ,mqtt heart 
	if((unsigned char)(cloud->recv_buf[0]) == 0xD0 && (unsigned char)(cloud->recv_buf[1]) == 0x00){
		*packet_len = 2;
		memcpy(packet, cloud->recv_buf, 2);
		*cmd = 0xD0;
        	memcpy(cloud->recv_buf, cloud->recv_buf+2, cloud->recv_len-2);
        	cloud->recv_len -= 2;
		return 0;
	}
    
    //if recv len less than 27, drop it
    if(cloud->recv_len <=30){
        memset(cloud->recv_buf, 0,BUF_LEN );
        cloud->recv_len =0;
        return -1;
    }
        length_len = mqtt_num_rem_len_bytes((const uint8_t*)cloud->recv_buf);
	mqtt_head_len = 3+length_len;//30 2C 00 22        
	printf(" length_len = %d, mqtt_head_len = %d \n", length_len,mqtt_head_len);
	//ota push: 30 43 00 22 73 65 72 32 63 6C 69 5F 72 65 73 2F 5A 6F 4B 36 6D 45 62 68 33 51 4B 39 4D 57 70 70 73 36 6A 57 46 53 00 00 00 03 02 11 01 00 16 73 4D 75 59 73 44 62 34 6B 65 6E 6B 39 78 55 59 46 77 6F 77 4E 54 
	//heart beat or app online num push: 30 2C 00 22 73 65 72 32 63 6C 69 5F 72 65 73 2F 44 72 66 6F 6D 48 56 50 46 69 37 41 4C 6E 5A 72 41 36 4E 45 36 52 00 00 00 03 02 10 00 01
	index = cloud->recv_buf+mqtt_head_len;
	//ser2cli_res/DrfomHVPFi7ALnZrA6NE6R
	if(strncmp(index, "ser2cli_res", strlen("ser2cli_res")) == 0){
              printf("ripple, in case ser2cli_res\n");
		topic_len = strlen("ser2cli_res/DrfomHVPFi7ALnZrA6NE6R");
#ifdef  RIPPLE_DEBUG
                    memset(debug_info,0,BUF_LEN);
                    strncpy(debug_info,index, topic_len);
                    printf("[ripple debug]: debug_info=%s\n",debug_info);
#endif

		index += topic_len;
		memcpy(cmd, index+4, 2);
             
		printf("ripple, before ntohs, cmd=%d\n", *cmd);
		*(cmd) = ntohs(*(cmd));
		printf("ripple, after ntohs , cmd = %d\n", *cmd);
		//ripple, 此处需要添加网络字节序转换函数
		if(*cmd == 208 || *cmd == 528){    //heart or 0x210(push app online)
			len = 4;
			length_len = 0;
		}
		else if(*cmd == 529){   //ota
			len = 27;
			length_len = 0;
		}
	}
	//30 41 00 36 61 70 70 32 64 65 76 2F 59 34 78 42 76 32 76 46 33
	//30 41 00 36 61 70 70 32 64 65 76 2F 44 72 66 6F 6D 48 56 50 46 69 37 41 4C 6E 5A 72 41 36 4E 45 36 52 2F 75 73 72 36 55 
	//39 6A 74 79 49 4E 58 32 37 68 6D 77 42 47 4C 56 30 35 00 00 00 03 04 00 00 90 02
	//app2dev/DrfomHVPFi7ALnZrA6NE6R/usr6U9jtyINX27hmwBGLV05
	else if(strncmp(index, "app2dev/", strlen("app2dev/")) == 0){
        
                printf("ripple, in case app2dev\n");
		topic_len = strlen("app2dev/DrfomHVPFi7ALnZrA6NE6R/usr6U9jtyINX27hmwBGLV05");
		index += topic_len;
		if(index[0] != 0x00 || index[1] != 0x00 || index[2] != 0x00 || index[3] != 0x03){
			memset(cloud->recv_buf, 0, BUF_LEN);
			cloud->recv_len = 0;
			return 0;
		} 
        	len = mqtt_parse_rem_len((const uint8_t *)index+3);
        	length_len = mqtt_num_rem_len_bytes((const uint8_t*)index+3);
		memcpy(cmd, index+4+length_len+1, 2);
		*(cmd) = ntohs(*(cmd));
	}
	else{
		memset(cloud->recv_buf, 0, BUF_LEN);
		cloud->recv_len = 0;
		return 0;
	}
       	*packet_len = 4+len+length_len;
        memcpy(packet, index, *packet_len);
	//00 00 00 03 04 00 00 90 02
	total_pack_len = *packet_len+mqtt_head_len+topic_len;
       printf("ripple, total_pack_len =%d, packet_len=%d , mqtt_head_len=%d, topic_len = %d \n",total_pack_len, *packet_len, mqtt_head_len, topic_len);
	if(cloud->recv_len-total_pack_len <= 0){
		memset(cloud->recv_buf, 0, BUF_LEN);
		cloud->recv_len = 0;
		return 0;
	}
        memcpy(cloud->recv_buf, cloud->recv_buf+total_pack_len, cloud->recv_len-total_pack_len);
        cloud->recv_len -= total_pack_len;
        
        return 0;
}

int     cloud_trans_data(cloud_st *cloud, char *packet)
{       
        char                    cmd, *index, action, *kv;
        uint16_t                len, kv_len;
        uint8_t                 length_len;
        
        if(cloud == NULL) return -1;
        
        index = packet;
        len = mqtt_parse_rem_len((const uint8_t *)index+3);
        length_len = mqtt_num_rem_len_bytes((const uint8_t*)index+3);
        TRACE_DEBUG( "in cloud_trans_data");
        
        // 00 00 00 03 06 00 00 90 01 01 01 
        // 00 00 00 03 0A 00 00 93 00 00 00 00 01 01 01 
        cmd = *(index+4+length_len+2);
        if((unsigned char)cmd == 0x93){
                memcpy(&(cloud->sn), index+4+length_len+3, 4);
                action = *(index+4+length_len+3+4);
                kv = index+4+length_len+3+4+1;
				kv_len = len - 8;
        }
        else if((unsigned char)cmd == 0x90){
                action = *(index+4+length_len+3);
                kv = index+4+length_len+3+1;
				kv_len = len - 4;
        }
        TRACE_DEBUG( "get action: %d", action);
        
        cloud_do_one_packet(cloud->in, action, kv, kv_len);
        return 0;
}

int cloud_do_mqtt(cloud_st *cloud)
{
        int             read_len, write_len, error_num, one_packet_len;
        short           cmd;
        char            debug_info[BUF_LEN], one_packet[BUF_LEN];

        if(cloud == NULL) return -1;

        memset(cloud->recv_buf, 0, BUF_LEN);
#ifdef USE_SSL
		read_len = SSL_read(cloud->ssl_mqtt->handle, cloud->recv_buf, BUF_LEN);
        cloud->recv_len = read_len;
        if(read_len <= 0){
			sslConnectionFree(cloud->ssl_mqtt);
			cloud->ssl_mqtt = NULL;
			cloud->broker.socket_info = NULL;
			return -1;
		}
#else
		read_len = readn(cloud->mqtt_fd, cloud->recv_buf, BUF_LEN);
        cloud->recv_len = read_len;
        if(read_len <= 0){
			//close(*(int *)(cloud->broker.socket_info));
			*(int *)(cloud->broker.socket_info) = 0;
			cloud->mqtt_fd = 0;
			return -1;
		}
#endif
        memset(debug_info, 0, BUF_LEN);
        HexToStr((unsigned char *)debug_info, (unsigned char *)cloud->recv_buf, cloud->recv_len>512?512:cloud->recv_len);
        TRACE_DEBUG( "read from m2m, len: %d, bytes: %s", cloud->recv_len, debug_info);
 
        while(get_one_cloud_packet(cloud, one_packet, &one_packet_len, &cmd) == 0){
                memset(debug_info, 0, BUF_LEN);
                HexToStr((unsigned char *)debug_info, (unsigned char *)one_packet, one_packet_len>512?512:one_packet_len);
                TRACE_DEBUG( "get one_packet, cmd: %d, bytes: %s", cmd, debug_info);

		switch(cmd){
			//0x0210
			case 528:
				TRACE_DEBUG( "push app online num, forget it");
				break;
			case 144:   //0x90
			case 147:  //0x93
				TRACE_DEBUG( "90 or 93 message");
				cloud_trans_data(cloud, one_packet);
				break;
			case 529: //0x211
				TRACE_DEBUG( "push ota");
				cloud_push_ota(cloud, one_packet);
				break;
			case 208:  //0xD0
				TRACE_DEBUG( "mqtt heart beat, forget it, pk: %s", cloud->con->pk);
				break;
			default:
				TRACE_DEBUG( "other cmd: %d", cmd);
				break;
		}
	}

	return 0;
}


int send_packet(void* socket_info,  void* buf, unsigned int count)
{
#ifdef USE_SSL
	SSL	*handle;
	handle = (SSL *)socket_info;
	return sslWriten(handle, buf, count);
#else
    int fd = *((int*)socket_info);
    //return send(fd, buf, count, 0);
    return writen(fd,buf,count);
#endif
    
}

int cloud_mqtt_connect(cloud_st *cloud, char *did, char *passcode)
{
	uint16_t    msg_id, msg_id_rcv;
	int			packet_length;
	char		recv_buf[BUF_LEN], sub_string[128], ip[LOG_IP_BUF_LENGTH];

	if(cloud == NULL || did == NULL || passcode == NULL) return -1;

    memset(&(cloud->broker), 0, sizeof(mqtt_broker_handle_t));
    mqtt_init(&(cloud->broker), did);
    mqtt_init_auth(&(cloud->broker), did, passcode);

#ifdef USE_SSL
	if(cloud->mqtt_server[0]){
		TRACE_DEBUG( "cloud->mqtt_server is aviable, will connect %s", cloud->mqtt_server);
		getIPByDomain(cloud->mqtt_server, ip);
		cloud->ssl_mqtt = sslConnectByIPPort(ip, G_M2M_SSL_PORT, 5);
		TRACE_DEBUG( "get ip: %s, ssl_mqtt fd: %d", ip, cloud->ssl_mqtt->fd);
		if(cloud->ssl_mqtt == NULL){
			TRACE_DEBUG( "ssl connect server error");
			return -1;
		}
	}
	else{
		TRACE_DEBUG( "cloud->mqtt_server is null, will connect %s", G_M2M_DOMAIN);
		getIPByDomain(G_M2M_DOMAIN, ip);
		cloud->ssl_mqtt = sslConnectByIPPort(ip, G_M2M_SSL_PORT, 5);
		TRACE_DEBUG( "get ip: %s, ssl_mqtt fd: %d", ip, cloud->ssl_mqtt->fd);
		if(cloud->ssl_mqtt == NULL){
			TRACE_DEBUG( "ssl connect server error");
			return -1;
		}
	}

	set_sock_time(cloud->ssl_mqtt->fd, 2, 2);
    mqtt_set_alive(&(cloud->broker), 300);
    cloud->broker.socket_info = (void*)(cloud->ssl_mqtt->handle);
	cloud->mqtt_fd = cloud->ssl_mqtt->fd;
    cloud->broker.send = send_packet;
	TRACE_DEBUG( "device->client_id: %s\tuser: %s\tpass: %s\t", did, did, passcode);
    
	mqtt_connect(&(cloud->broker));
	packet_length = SSL_read(cloud->ssl_mqtt->handle, recv_buf, BUF_LEN);
    if(packet_length < 0){
		sslConnectionFree(cloud->ssl_mqtt);
        cloud->mqtt_fd = 0;
        cloud->ssl_mqtt = NULL;
        cloud->broker.socket_info = NULL;
        TRACE_DEBUG( "error, packet_length is %d on read packet!", packet_length);
        return -1;
    }
	TRACE_DEBUG( "mqtt_connect success, and return value");

    if(MQTTParseMessageType(recv_buf) != MQTT_MSG_CONNACK)
    {
        fprintf(stderr, "CONNACK expected!\n");
        return -2;
    }
	TRACE_DEBUG( "MQTTParseMessageType is MQTT_MSG_CONNACK");

    if(recv_buf[3] != 0x00)
    {
        fprintf(stderr, "CONNACK failed!\n");
        return -2;
    }
	TRACE_DEBUG( "MQTT_MSG_CONNACK result is success");

    memset(sub_string, 0, 128);
    snprintf(sub_string, 128, "ser2cli_res/%s", did);
    mqtt_subscribe(&(cloud->broker), sub_string, &msg_id);
    packet_length = SSL_read(cloud->ssl_mqtt->handle, recv_buf, BUF_LEN);
    if(packet_length < 0)
    {
		sslConnectionFree(cloud->ssl_mqtt);
        cloud->mqtt_fd = 0;
        cloud->ssl_mqtt = NULL;
        cloud->broker.socket_info = NULL;
        fprintf(stderr, "Error(%d) on read packet!\n", packet_length);
        return -1;
    }
	TRACE_DEBUG( "sub ser2cli_res success and return ack");
 
    if(MQTTParseMessageType(recv_buf) != MQTT_MSG_SUBACK)
    {
        fprintf(stderr, "SUBACK expected!\n");
        return -2;
    }
	TRACE_DEBUG( "MQTTParseMessageType is MQTT_MSG_SUBACK");

    msg_id_rcv = mqtt_parse_msg_id((const uint8_t *)recv_buf);
    if(msg_id != msg_id_rcv)
    {
        fprintf(stderr, "%d message id was expected, but %d message id was found!\n", msg_id, msg_id_rcv);
        return -3;
    }
	TRACE_DEBUG( "mqtt_parse_msg_id success");
	
	memset(sub_string, 0, 128);
    snprintf(sub_string, 128, "app2dev/%s/#", did);
    mqtt_subscribe(&(cloud->broker), sub_string, &msg_id);
    packet_length = SSL_read(cloud->ssl_mqtt->handle, recv_buf, BUF_LEN);
    if(packet_length < 0)
    {
		sslConnectionFree(cloud->ssl_mqtt);
        cloud->mqtt_fd = 0;
        cloud->ssl_mqtt = NULL;
        cloud->broker.socket_info = NULL;
        return -1;
    }
	TRACE_DEBUG( "sub app2dev success and return ack");
 
    if(MQTTParseMessageType(recv_buf) != MQTT_MSG_SUBACK)
    {
        fprintf(stderr, "SUBACK expected!\n");
        return -2;
    }
	TRACE_DEBUG( "MQTTParseMessageType is MQTT_MSG_SUBACK");

    msg_id_rcv = mqtt_parse_msg_id((const uint8_t *)recv_buf);
    if(msg_id != msg_id_rcv)
    {
        fprintf(stderr, "%d message id was expected, but %d message id was found!\n", msg_id, msg_id_rcv);
        return -3;
    }
	TRACE_DEBUG( "app2dev mqtt_parse_msg_id success");

#else
    if(cloud->mqtt_server[0]){
        TRACE_DEBUG( "cloud->mqtt_server is aviable, will connect %s", cloud->mqtt_server);

        //ripple 这里换成了IP, 考虑以后加上域名解析
        memset(cloud->mqtt_server, 0, sizeof(cloud->mqtt_server));
        strcpy(cloud->mqtt_server,G_M2M_DOMAIN_IP);
        
        cloud->mqtt_fd = sock_connect_by_domain(cloud->mqtt_server, cloud->mqtt_port);
    }
    else{
        TRACE_INFO( "cloud->mqtt_server is null, will connect %s", G_M2M_DOMAIN_IP);
        //ripple,  这里讲domain  G_M2M_DOMAIN 换成了IP
        cloud->mqtt_fd = sock_connect_by_domain(G_M2M_DOMAIN_IP, G_M2M_PORT);
    }
    //if(cloud->mqtt_fd <= 0) return -1;

	//set_sock_time(cloud->mqtt_fd, 1, 1);
    mqtt_set_alive(&(cloud->broker), 300);
    cloud->broker.socket_info = (void*)&(cloud->mqtt_fd);
    cloud->broker.send = send_packet;
	TRACE_INFO( "device->client_id: %s\tuser: %s\tpass: %s\t", did, did, passcode);
    
	mqtt_connect(&(cloud->broker));
       HAL_Delay(2000);
	packet_length = readn(cloud->mqtt_fd, recv_buf, BUF_LEN);
      TRACE_INFO("mqtt connect, receive = %x, %x ,%x, %x", recv_buf[0],recv_buf[1],recv_buf[2],recv_buf[3]);
    if(packet_length < 0){
        TRACE_ERROR( "error, packet_length is %d on read packet!", packet_length);
        return -1;
    }
	TRACE_INFO( "mqtt_connect success, and return value, packet_length= %d",packet_length);

    if(MQTTParseMessageType(recv_buf) != MQTT_MSG_CONNACK)
    {
        TRACE_ERROR("CONNACK expected!\n");
        return -2;
    }
	TRACE_DEBUG( "MQTTParseMessageType is MQTT_MSG_CONNACK");

    if(recv_buf[3] != 0x00)
    {
        TRACE_ERROR("CONNACK failed!\n");
        return -2;
    }
	TRACE_DEBUG( "MQTT_MSG_CONNACK result is success");

    memset(sub_string, 0, 128);
    snprintf(sub_string, 128, "ser2cli_res/%s", did);
    mqtt_subscribe(&(cloud->broker), sub_string, &msg_id);
    HAL_Delay(2000);
    packet_length = readn(cloud->mqtt_fd, recv_buf, 2048);
    if(packet_length < 0)
    {
        fprintf(stderr, "Error(%d) on read packet!\n", packet_length);
        mqtt_connect(&(cloud->broker));
        return -1;
    }
	TRACE_DEBUG( "sub ser2cli_res success and return ack");
 
    if(MQTTParseMessageType(recv_buf) != MQTT_MSG_SUBACK)
    {
        fprintf(stderr, "SUBACK expected!\n");
        return -2;
    }
	TRACE_DEBUG( "MQTTParseMessageType is MQTT_MSG_SUBACK");

    msg_id_rcv = mqtt_parse_msg_id((const uint8_t *)recv_buf);
    if(msg_id != msg_id_rcv)
    {
        fprintf(stderr, "%d message id was expected, but %d message id was found!\n", msg_id, msg_id_rcv);
        return -3;
    }
	TRACE_DEBUG( "mqtt_parse_msg_id success");
	
	memset(sub_string, 0, 128);
    snprintf(sub_string, 128, "app2dev/%s/#", did);
    mqtt_subscribe(&(cloud->broker), sub_string, &msg_id);
    HAL_Delay(2000);
    packet_length = readn(cloud->mqtt_fd, recv_buf, 2048);
    if(packet_length < 0)
    {
        fprintf(stderr, "Error(%d) on read packet!\n", packet_length);
        mqtt_connect(&(cloud->broker));
        return -1;
    }
	TRACE_DEBUG( "sub app2dev success and return ack");
 
    if(MQTTParseMessageType(recv_buf) != MQTT_MSG_SUBACK)
    {
        fprintf(stderr, "SUBACK expected!\n");
        return -2;
    }
	TRACE_DEBUG( "MQTTParseMessageType is MQTT_MSG_SUBACK");

    msg_id_rcv = mqtt_parse_msg_id((const uint8_t *)recv_buf);
    if(msg_id != msg_id_rcv)
    {
        fprintf(stderr, "%d message id was expected, but %d message id was found!\n", msg_id, msg_id_rcv);
        return -3;
    }
	TRACE_DEBUG( "app2dev mqtt_parse_msg_id success");
#endif

	return 0;
}





int cloud_mqtt_reconnect(cloud_st *cloud, char *did, char *passcode)
{
    uint16_t    msg_id, msg_id_rcv;
    int         packet_length;
    char        recv_buf[2048], sub_string[128], ip[LOG_IP_BUF_LENGTH];

    if(cloud == NULL || did == NULL || passcode == NULL) return -1;

    memset(&(cloud->broker), 0, sizeof(mqtt_broker_handle_t));
    mqtt_init(&(cloud->broker), did);
    mqtt_init_auth(&(cloud->broker), did, passcode);

#ifdef USE_SSL
    if(cloud->mqtt_server[0]){
        TRACE_DEBUG( "cloud->mqtt_server is aviable, will connect %s", cloud->mqtt_server);
        getIPByDomain(cloud->mqtt_server, ip);
        cloud->ssl_mqtt = sslConnectByIPPort(ip, G_M2M_SSL_PORT, 5);
        TRACE_DEBUG( "get ip: %s, ssl_mqtt fd: %d", ip, cloud->ssl_mqtt->fd);
        if(cloud->ssl_mqtt == NULL){
            TRACE_DEBUG( "ssl connect server error");
            return -1;
        }
    }
    else{
        TRACE_DEBUG( "cloud->mqtt_server is null, will connect %s", G_M2M_DOMAIN);
        getIPByDomain(G_M2M_DOMAIN, ip);
        cloud->ssl_mqtt = sslConnectByIPPort(ip, G_M2M_SSL_PORT, 5);
        TRACE_DEBUG( "get ip: %s, ssl_mqtt fd: %d", ip, cloud->ssl_mqtt->fd);
        if(cloud->ssl_mqtt == NULL){
            TRACE_DEBUG( "ssl connect server error");
            return -1;
        }
    }

    set_sock_time(cloud->ssl_mqtt->fd, 2, 2);
    mqtt_set_alive(&(cloud->broker), 300);
    cloud->broker.socket_info = (void*)(cloud->ssl_mqtt->handle);
    cloud->mqtt_fd = cloud->ssl_mqtt->fd;
    cloud->broker.send = send_packet;
    TRACE_DEBUG( "device->client_id: %s\tuser: %s\tpass: %s\t", did, did, passcode);
    
    mqtt_connect(&(cloud->broker));
    packet_length = SSL_read(cloud->ssl_mqtt->handle, recv_buf, BUF_LEN);
    if(packet_length < 0){
        sslConnectionFree(cloud->ssl_mqtt);
        cloud->mqtt_fd = 0;
        cloud->ssl_mqtt = NULL;
        cloud->broker.socket_info = NULL;
        TRACE_DEBUG( "error, packet_length is %d on read packet!", packet_length);
        return -1;
    }
    TRACE_DEBUG( "mqtt_connect success, and return value");

    if(MQTTParseMessageType(recv_buf) != MQTT_MSG_CONNACK)
    {
        fprintf(stderr, "CONNACK expected!\n");
        return -2;
    }
    TRACE_DEBUG( "MQTTParseMessageType is MQTT_MSG_CONNACK");

    if(recv_buf[3] != 0x00)
    {
        fprintf(stderr, "CONNACK failed!\n");
        return -2;
    }
    TRACE_DEBUG( "MQTT_MSG_CONNACK result is success");

    memset(sub_string, 0, 128);
    snprintf(sub_string, 128, "ser2cli_res/%s", did);
    mqtt_subscribe(&(cloud->broker), sub_string, &msg_id);
    packet_length = SSL_read(cloud->ssl_mqtt->handle, recv_buf, BUF_LEN);
    if(packet_length < 0)
    {
        sslConnectionFree(cloud->ssl_mqtt);
        cloud->mqtt_fd = 0;
        cloud->ssl_mqtt = NULL;
        cloud->broker.socket_info = NULL;
        fprintf(stderr, "Error(%d) on read packet!\n", packet_length);
        return -1;
    }
    TRACE_DEBUG( "sub ser2cli_res success and return ack");
 
    if(MQTTParseMessageType(recv_buf) != MQTT_MSG_SUBACK)
    {
        fprintf(stderr, "SUBACK expected!\n");
        return -2;
    }
    TRACE_DEBUG( "MQTTParseMessageType is MQTT_MSG_SUBACK");

    msg_id_rcv = mqtt_parse_msg_id((const uint8_t *)recv_buf);
    if(msg_id != msg_id_rcv)
    {
        fprintf(stderr, "%d message id was expected, but %d message id was found!\n", msg_id, msg_id_rcv);
        return -3;
    }
    TRACE_DEBUG( "mqtt_parse_msg_id success");
    
    memset(sub_string, 0, 128);
    snprintf(sub_string, 128, "app2dev/%s/#", did);
    mqtt_subscribe(&(cloud->broker), sub_string, &msg_id);
    packet_length = SSL_read(cloud->ssl_mqtt->handle, recv_buf, BUF_LEN);
    if(packet_length < 0)
    {
        sslConnectionFree(cloud->ssl_mqtt);
        cloud->mqtt_fd = 0;
        cloud->ssl_mqtt = NULL;
        cloud->broker.socket_info = NULL;
        return -1;
    }
    TRACE_DEBUG( "sub app2dev success and return ack");
 
    if(MQTTParseMessageType(recv_buf) != MQTT_MSG_SUBACK)
    {
        fprintf(stderr, "SUBACK expected!\n");
        return -2;
    }
    TRACE_DEBUG( "MQTTParseMessageType is MQTT_MSG_SUBACK");

    msg_id_rcv = mqtt_parse_msg_id((const uint8_t *)recv_buf);
    if(msg_id != msg_id_rcv)
    {
        fprintf(stderr, "%d message id was expected, but %d message id was found!\n", msg_id, msg_id_rcv);
        return -3;
    }
    TRACE_DEBUG( "app2dev mqtt_parse_msg_id success");

#else
    TRACE_INFO( "device->client_id: %s\tuser: %s\tpass: %s\t", did, did, passcode);
     mqtt_connect(&(cloud->broker));
     HAL_Delay(2000);
    packet_length = readn(cloud->mqtt_fd, recv_buf, BUF_LEN);
      TRACE_INFO("mqtt connect, receive = %x, %x ,%x, %x", recv_buf[0],recv_buf[1],recv_buf[2],recv_buf[3]);
    if(packet_length < 0){
        TRACE_DEBUG( "error, packet_length is %d on read packet!", packet_length);
        return -1;
    }
    TRACE_INFO( "mqtt_connect success, and return value, packet_length= %d",packet_length);

    if(MQTTParseMessageType(recv_buf) != MQTT_MSG_CONNACK)
    {
        fprintf(stderr, "CONNACK expected!\n");
        return -2;
    }
    TRACE_DEBUG( "MQTTParseMessageType is MQTT_MSG_CONNACK");

    if(recv_buf[3] != 0x00)
    {
        fprintf(stderr, "CONNACK failed!\n");
        return -2;
    }
    TRACE_DEBUG( "MQTT_MSG_CONNACK result is success");

    memset(sub_string, 0, 128);
    snprintf(sub_string, 128, "ser2cli_res/%s", did);
    mqtt_subscribe(&(cloud->broker), sub_string, &msg_id);
    HAL_Delay(2000);
    packet_length = readn(cloud->mqtt_fd, recv_buf, 2048);
    if(packet_length < 0)
    {
        fprintf(stderr, "Error(%d) on read packet!\n", packet_length);
        mqtt_connect(&(cloud->broker));
        return -1;
    }
    TRACE_DEBUG( "sub ser2cli_res success and return ack");
 
    if(MQTTParseMessageType(recv_buf) != MQTT_MSG_SUBACK)
    {
        fprintf(stderr, "SUBACK expected!\n");
        return -2;
    }
    TRACE_DEBUG( "MQTTParseMessageType is MQTT_MSG_SUBACK");

    msg_id_rcv = mqtt_parse_msg_id((const uint8_t *)recv_buf);
    if(msg_id != msg_id_rcv)
    {
        fprintf(stderr, "%d message id was expected, but %d message id was found!\n", msg_id, msg_id_rcv);
        return -3;
    }
    TRACE_DEBUG( "mqtt_parse_msg_id success");
    
    memset(sub_string, 0, 128);
    snprintf(sub_string, 128, "app2dev/%s/#", did);
    mqtt_subscribe(&(cloud->broker), sub_string, &msg_id);
    HAL_Delay(2000);
    packet_length = readn(cloud->mqtt_fd, recv_buf, 2048);
    if(packet_length < 0)
    {
        fprintf(stderr, "Error(%d) on read packet!\n", packet_length);
        mqtt_connect(&(cloud->broker));
        return -1;
    }
    TRACE_DEBUG( "sub app2dev success and return ack");
 
    if(MQTTParseMessageType(recv_buf) != MQTT_MSG_SUBACK)
    {
        fprintf(stderr, "SUBACK expected!\n");
        return -2;
    }
    TRACE_DEBUG( "MQTTParseMessageType is MQTT_MSG_SUBACK");

    msg_id_rcv = mqtt_parse_msg_id((const uint8_t *)recv_buf);
    if(msg_id != msg_id_rcv)
    {
        fprintf(stderr, "%d message id was expected, but %d message id was found!\n", msg_id, msg_id_rcv);
        return -3;
    }
    TRACE_DEBUG( "app2dev mqtt_parse_msg_id success");
#endif

    return 0;
}



int	cloud_send_data(cloud_st *cloud, char action, char *buf, int len)
{
	int	i, result, length_byte_num, length_remain;
	char	topic[128], send_buf[1024], debug_info[1024], length_bytes[4];
	char    *index, *len_index;
	
	if(cloud == NULL || cloud->broker.socket_info <= 0 || cloud->broker.send <= 0) return -1;
	
	memset(topic, 0, 128);
	snprintf(topic, 128, "dev2app/%s", cloud->con->did);

	memset(cloud->send_buf, 0, BUF_LEN);
	length_remain = 0;
    index = cloud->send_buf;
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
    cloud->send_len = (index - cloud->send_buf);
    for(i=0; i<length_byte_num; i++){
		*(len_index+i) = length_bytes[i];
	}
	
	memset(debug_info, 0, 1024);
	HexToStr((unsigned char *)debug_info, (unsigned char *)cloud->send_buf, cloud->send_len>512?512:cloud->send_len);
	//TRACE_DEBUG( "mqtt send: %s", debug_info);

    	result = mqtt_publish_with_qos(&(cloud->broker), topic, cloud->send_buf, cloud->send_len, 0, 0, NULL);
	if(result != 1){
		//close(*(int *)(cloud->broker.socket_info));
		*(int *)(cloud->broker.socket_info) = 0;
		return -1;
	}
	return 0;
}

int check_ota(cloud_st *cloud)
{
	int 	http_fd, write_len;
	char	*index1, *index2, port[16], content[128];

	http_fd = sock_connect_by_domain(G_SERVICE_DOMAIN_IP, G_SERVICE_PORT);
	if(http_fd <= 0){
		TRACE_DEBUG( "connect G_SERVICE_DOMAIN error");
		return -1;
	}
	//TRACE_DEBUG( "connect G_SERVICE_DOMAIN success, fd: %d", http_fd);

	memset(content, 0, 128);
	snprintf(content, 128, "passcode=%s&type=1&hard_version=%s&soft_version=%s", cloud->con->passcode,cloud->con->hard_version,cloud->con->soft_version);

	memset(cloud->send_buf, 0, BUF_LEN);
	snprintf(cloud->send_buf, BUF_LEN, 
              "POST /dev/ota/v4.1/update_and_check/%s HTTP/1.1\r\n"
              "Host:%s\r\n"
              "Content-Length:%d\r\n"
              "Cache-Control: no-cache\r\n"
              "Content-Type: application/x-www-form-urlencoded\r\n\r\n"
			  "%s", cloud->con->did, G_SERVICE_DOMAIN, (int)strlen(content), content);
	cloud->send_len = strlen(cloud->send_buf);	

	write_len = writen(http_fd, cloud->send_buf, cloud->send_len);
	TRACE_DEBUG( "provision send to server:\n%s", cloud->send_buf);
	
	set_sock_time(http_fd, 3, 3);
	memset(cloud->recv_buf, 0, BUF_LEN);
	cloud->recv_len = readn(http_fd, cloud->recv_buf, BUF_LEN);

	memset(cloud->ota_info, 0, BUF_LEN);
	TRACE_DEBUG( "get response, len: %d\r\n%s", cloud->recv_len, cloud->recv_buf);
	//soft_ver=04020021&download_url=http://api.gizwits.com/dev/ota/v4.1/download/7072
	if((index1 = strstr(cloud->recv_buf, "soft_ver="))){
		index2 = strstr(index1, "\r\n");
		if(index2){
			memcpy(cloud->ota_info, index1, index2-index1);
			TRACE_DEBUG( "get ota_info: %s", cloud->ota_info);
			memset(cloud->recv_buf, 0, BUF_LEN);
			cloud->recv_len = 0;
			return 0;
		}
	}
	else if((index1 = strstr(cloud->recv_buf, "Don't have a push rule id on device"))){
		memset(cloud->recv_buf, 0, BUF_LEN);
		cloud->recv_len = 0;
		return 9500;
	}
	else if((index1 = strstr(cloud->recv_buf, "push rule not in effect or device already upgrade"))){
		memset(cloud->recv_buf, 0, BUF_LEN);
		cloud->recv_len = 0;
		return 9502;
	}

	return -1;
}

int cloud_push_ota(cloud_st *cloud, char *packet)
{       
        char                    did[32], ota_type, *index, action, *kv;
        uint16_t                len, kv_len;
        uint8_t                 length_len;
        
        if(cloud == NULL) return -1;
        
        index = packet;
        TRACE_DEBUG( "in cloud_push_ota");
        
		//00 00 00 03 02 11 01 00 16 75 4E 4D 64 59 72 63 56 33 59 4E 36 44 5A 68 51 50 63 35 75 67 57
        ota_type = *(index+4+2);
		TRACE_DEBUG( "ota_type: %d", ota_type);
		
		len = *(index+7)*256+*(index+8);
		if(len < 0 || len >32) return -1;
		memset(did, 0, 32);
		memcpy(did, index+9, len);
        
		TRACE_DEBUG( "get ota push, did: %s", did);
        cloud_do_one_packet(cloud, ACTION_PUSH_OTA, did, len);
		return 0;
}


int cloud_do_one_packet(void *in, unsigned char action, char *kv, int kv_len)
{
	cloud_st	*cloud = NULL;
	TRACE_DEBUG( "this is one packet from mqtt, action: %d", action);

	switch(action){
		case ACTION_CONTROL:
			TRACE_DEBUG( "please control your device here, and then report the new status, both app and mqtt");
			//cloud_send_data(device->cloud, ACTION_REPORT_STATUS, buf, len);
			//lan_send_data(device->lan, ACTION_REPORT_STATUS, buf, len);
			break;
		
		case ACTION_READ_STATUS:
			TRACE_DEBUG( "please update your device status into data buf and send back to mqtt");		
			//cloud_send_data(device->cloud, ACTION_READ_STATUS_ACK, buf, len);
			break;

		case ACTION_TRANS_RECV:
			TRACE_DEBUG( "this is your raw data from mqtt");
			break;

		case ACTION_PUSH_OTA:
			cloud = (cloud_st *)in;
			TRACE_DEBUG( "this is push ota msg from m2m");
			check_ota(cloud);
			TRACE_DEBUG( "get your ota file url: %s", cloud->ota_info);
			//do you ota here
			break;
	}

	return 0;
}


 unsigned short  htons(unsigned short h)
{
       // 若本机为大端，与网络字节序同，直接返回
       // 若本机为小端，转换成大端再返回
       return checkCPUendian() ? h : BigLittleSwap16(h);
}
 
// 模拟ntohs函数，网络字节序转本机字节序
 unsigned short  ntohs(unsigned short  n)
{
       // 若本机为大端，与网络字节序同，直接返回
       // 若本机为小端，网络数据转换成小端再返回
       return checkCPUendian() ? n : BigLittleSwap16(n);
}

/*
 机智云平台的初始化,包括 client端注册，provision
 mqtt客户端连接
*/
int Gizwits_init()
{

    //ripple, 这部分为机智的参数，以后会修改成文件系统读取
    //形式

    memset(&global_con, 0, sizeof(global_con));
    
    snprintf(global_con.mac, 16, "%s", "00163E002CAC");
    snprintf(global_con.did, 32, "%s", "GdzuSRncfDPGHGo2LxwgM2");
    snprintf(global_con.passcode, 16, "%s", "0aeb9953f9");
    snprintf(global_con.pk, 48, "%s", "0aeb9953f97c49d6be32c46487e59400");
    snprintf(global_con.pk_secret, 48, "%s", "3cbbe25bd11c4d7194bb56c92be8616b");
    snprintf(global_con.hard_version, 16, "%s", "CubeEV001");
    snprintf(global_con.soft_version, 16, "%s", "04020020");

    global_cloud = cloud_init(&global_con, "00163E002CAC", "doc/GAgent_Lite.xml");
    if(global_cloud == NULL) return -1;

    //check ota when reboot;
    /*
    if(check_ota(cloud) == 0){
        TRACE_DEBUG("need ota, file url: %s", cloud->ota_info);
    //do ota here;
    }
    */

    //API:	cloud API, will connect mqtt
    //	memcpy(cloud->mqtt_server, "sandbox.gizwits.com", strlen("sandbox.gizwits.com"));
    //	cloud->mqtt_port = G_M2M_PORT;
    cloud_mqtt_connect(global_cloud, global_con.did, global_con.passcode);

    return 0;		
}


