#ifdef ANDROID_PLATFORM
#include "tool.h"
#include<Android/log.h>
#else
#include "tool.h"
#endif
#include "stm32f4xx_hal.h"
#include "tcp_task.h"
#include "lm61.h"
#include "stm32f4xx.h"
#include <string.h>
#include <stdio.h>
#include "trace.h"

#define BUF_SIZE 1024

int udp_wirte(int fd, int port, char *buf, int len)
{
#if 0 
    struct  sockaddr_in addr_write;
    struct  in_addr addr;

    inet_aton("255.255.255.255", &addr);
    addr_write.sin_family = AF_INET;
    addr_write.sin_port = htons(port);
    addr_write.sin_addr = addr;

    return sendto(fd, buf, len, 0, (struct sockaddr *)&addr_write, sizeof(struct sockaddr));
#endif	
	   return 0;
}

int create_read_udp(int read_port)
{
#if 0 
    int     	sock, cflags, mass;
    struct  	sockaddr_in addr_read;
    const int   opt = 1;

    bzero(&addr_read, sizeof(struct sockaddr_in));
    addr_read.sin_family = AF_INET;
    addr_read.sin_addr.s_addr = htonl(INADDR_ANY);  
    addr_read.sin_port = htons(read_port);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    cflags = fcntl(sock,F_GETFL,0);
    fcntl(sock,F_SETFL, cflags|O_NONBLOCK);
    mass=1;

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &mass, sizeof(mass));
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(opt));
    while(bind(sock,(struct sockaddr *)&(addr_read), sizeof(struct sockaddr_in))){
    	printf("bind error, sleep 1, try again");
    	sleep(1);
    }

    return sock;
#endif
		return 0;
}

void StrToHex(unsigned char *pbDest, unsigned char *pbSrc, int nLen)
{
    char h1,h2;
    unsigned char s1,s2;
    int i;

    for (i=0; i<nLen; i++){
        h1 = pbSrc[3*i];
        h2 = pbSrc[3*i+1];

        s1 = toupper(h1) - 0x30;
        if (s1 > 9) s1 -= 7;

        s2 = toupper(h2) - 0x30;
        if (s2 > 9) s2 -= 7;

        pbDest[i] = s1*16 + s2;
    }
}

void HexToStr(unsigned char *pbDest, unsigned char *pbSrc, int nLen)
{
    char    ddl,ddh;
    int i;

    for (i=0; i<nLen; i++){
        ddh = 48 + pbSrc[i] / 16;
        ddl = 48 + pbSrc[i] % 16;
        if (ddh > 57) ddh = ddh + 7;
        if (ddl > 57) ddl = ddl + 7;
        pbDest[i*3] = ddh;
        pbDest[i*3+1] = ddl;
        pbDest[i*3+2] = ' ';
    }

    pbDest[nLen*3] = '\0';
}

int set_sock_time(int fd, int read_sec, int write_sec)
{
#if 0 
	struct  timeval send_timeval;
	struct  timeval recv_timeval;

	if(fd <= 0) return -1;

	send_timeval.tv_sec = write_sec<0?0:write_sec;
	send_timeval.tv_usec = 0;
	recv_timeval.tv_sec = read_sec<0?0:read_sec;;
	recv_timeval.tv_usec = 0;

	if(setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &send_timeval, sizeof(send_timeval)) == -1){
		return -1;
	}

	if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeval, sizeof(recv_timeval)) == -1){
		return -1;
	}
#endif
	return 0;
}



/*
  readn 需要实现从 socket fd里读取数据
  vptr 是返回的数据
  n :   要读取的数据长度
  返回值: 实际读取的长度
*/
int readn(int fd, void *vptr, size_t n)
{
    int  nleft;
    int  nread;
    char    *ptr;
    int  times=0;
    	
    ptr = (char *)vptr;
    nleft = n;
    while (nleft > 0) {
       HAL_Delay(600);
    	LM61_transparent_Read((uint8_t*) ptr, &nread);
    	nleft -= nread;
    	ptr   += nread;
       if(times >=2)   //未读取到数据
           break;
       times ++;
    }
    return(n - nleft);
}       


/*
fd :  暂时没用
vptr:  要写入的数据
n: 要写入的数据长度
 返回值: 实际写入数据的长度
*/
int     writen(int fd, const void *vptr, size_t n)
{

    LM61_transparent_Write((uint8_t*)vptr,n, TCP_SEND_MESSAGE_TIMEOUT);
    return n;
}	     

int hex_string_to_10(char *string, int len)
{
	char    *p, *q;
	int     result, i;

	if(string == NULL || len <= 0) return 0;

	p = string;
	q = p + strlen(p)-1;
	i = 0;
	result = 0;

	while(q && q >= p){
		if(*q == 'a' || *q == 'A') *q = 10;
		else if(*q == 'b' || *q == 'B') *q = 11;
		else if(*q == 'c' || *q == 'C') *q = 12;
		else if(*q == 'd' || *q == 'D') *q = 13;
		else if(*q == 'e' || *q == 'E') *q = 14;
		else if(*q == 'f' || *q == 'F') *q = 15;
		else *q -= 48;

		result += *q * (int)pow(16, i);
		i++;
		q--;
	}

	return result;
}


/*
此处需要修改,有ip和port,发起socket连接，
但是LM61并没有socket返回的fd
*/
int sock_connect_by_ip(char *ip, int port)
{
    uint8_t   portBuf[10];
    sprintf(portBuf, "%d",port);
    LM61_start_transparent_mode(ip,portBuf);    
    return 0;
}

/*
根据域名发起 socket连接， 因为暂时没有域名解析函数
所以写的固定IP。 gizwits在不同的城市，域名解析出来的IP不一样
以后可能需要加上域名解析函数
//119.29.47.111
#define G_SERVICE_DOMAIN 	"api.gizwits.com"
#define G_SERVICE_PORT		80
*/
int sock_connect_by_domain(char *domain, int port)
{
    //socket连接,连接成功后直接开启透传模式
    //若需要传输数据，则调用LM61_transparent_Write
    uint8_t   portBuf[10];
    sprintf(portBuf, "%d",port);
    TRACE_INFO("IP=%s , port=%s\n", domain,portBuf);
    LM61_start_transparent_mode(domain,portBuf);    
    return 0;
}

int sock_disconnect(int fd)
{
    if (CAT1_STATUS_SUCCESS == LM61_close_transparent_mode())
        return 0;
    else 
        return -1;
}


void StrToHex_noSpace(char *pbDest, char *pbSrc, int nLen)
{
    char h1,h2;
    char s1,s2;
    int i;

    for (i=0; i<nLen; i++)
    {   
        h1 = pbSrc[2*i];
        h2 = pbSrc[2*i+1];

        s1 = toupper(h1) - 0x30;
        if (s1 > 9)
        s1 -= 7;

        s2 = toupper(h2) - 0x30;
        if (s2 > 9)
        s2 -= 7;

        pbDest[i] = s1*16 + s2; 
    }   
}

#if 0 
config_st *config_init(char *path)
{
	config_st	*config;
	int	size, len_total;
	struct stat stat_buf;

	if(path == NULL) return NULL;
	if((config = (config_st *)malloc(sizeof(config_st))) == NULL) return NULL;
	memset(config, 0, sizeof(config_st));

	strncpy(config->path, path, 128);
	if((config->fd = open(path, O_RDONLY)) == -1){
		free(config);
		return NULL;
	}

	memset(&stat_buf, 0, sizeof(struct stat));
        fstat(config->fd, &stat_buf);
	size = stat_buf.st_size + 1;

	if((config->buf = (char *)malloc(size)) == NULL){
		free(config);
		return NULL;
	}
	memset(config->buf, 0, size);

	len_total = read(config->fd, config->buf, size);
	if(len_total < 0){
		free(config->buf);
		free(config);
		return NULL;
	}

	config->size = len_total;
	config->free_buf_size = size/10;
	config->free_buf_index = 0;

	config->free_buf = (char **)malloc(sizeof(char *) * config->free_buf_size);
	memset(config->free_buf, 0, sizeof(char *) * config->free_buf_size);

	return config;
}



char *get_value(config_st *config, char *string, char *val)
{
	int	value_size, fail;
	char	*p, *q, *dot, *tag_head, *tag_end, *value, *new_tag_head, *new_tag_end;
	char	tag_head_str[256], tag_end_str[256], sub_str[250], check_str[256];

	if(config == NULL || string == NULL || config->buf == NULL || config->size == 0) return val;

	p = string;
	tag_head = config->buf;
	tag_end = tag_head + strlen(tag_head);
	fail = 0;
	while(p){
		memset(sub_str, 0, 250);
		if((dot = strchr(p, '.')) == NULL){
			strcpy(sub_str, p);
			p = NULL;
		}
		else{
			strncpy(sub_str, p, (dot-p)>250 ? 250:(dot-p));
			p = dot+1;
		}
		
		
		memset(tag_head_str, 0, 256);
		memset(tag_end_str, 0, 256);
		snprintf(tag_head_str, 256, "<%s>", sub_str);
		snprintf(tag_end_str, 256, "</%s>", sub_str);
		
		if((new_tag_head = (char *)strstr(tag_head, tag_head_str)) == NULL || 
			(new_tag_end = (char *)strstr(tag_head, tag_end_str)) == NULL){
			return val;
		}
	
		if(new_tag_head < tag_head || new_tag_end > tag_end){
			return val;
		}
		else{
			tag_head = new_tag_head;
			tag_end  = new_tag_end;
		}

	}

	memset(check_str, 0, 256);
	tag_head += strlen(tag_head_str);
	if((p = strchr(tag_head, '<')) == NULL) return val;
	if((q = strchr(p, '>')) == NULL) return val;
	strncpy(check_str, p, q+1-p);
	if(strcmp(check_str, tag_end_str)) return val;

	tag_end -= 1;
	while(*tag_head == ' ' && tag_head <= tag_end) tag_head++;
	while(*tag_end == ' ' && tag_head <= tag_end) tag_end--;
	if(tag_head > tag_end) return val;

	value_size = (tag_end - tag_head)+1;
	value = (char *)malloc(value_size+1);
	memset(value, 0, value_size+1);
	memcpy(value, tag_head, value_size);

	if(config->free_buf_index >= config->free_buf_size){
		config->free_buf = (char **)realloc(config->free_buf, config->free_buf_size+1024);
		memset(&(config->free_buf[config->free_buf_index]), 0, 
			config->free_buf_size+1024-config->free_buf_index);
	}

	config->free_buf[config->free_buf_index] = value;
	config->free_buf_index++;

	return value;
}

void config_free(config_st *config)
{
	int	i;

	if(config == NULL) return ;
	if(config->fd){
		close(config->fd);
		config->fd = 0;
	}

	for(i=0; i<config->free_buf_index; i++){
		if(config->free_buf[i]) free(config->free_buf[i]);
	}

	free(config->free_buf);
	if(config->buf) free(config->buf);

	free(config);

	return ;
}

#endif

static int get_ip_by_domain(const char* domain, char ip[16])
{
#if 0 	
    struct hostent *answer;

    if (NULL == domain || '\0' == domain[0]) return -1;

    answer = gethostbyname(domain);
    if (NULL == answer || '\0' == answer->h_addr_list[0]) {
        printf("gethostbyname %s failed: %s\n", domain, hstrerror(h_errno));
        return -1;
    } else {
        inet_ntop(AF_INET, (answer->h_addr_list)[0], ip, 16);
    }
#endif
    return 0;
}

char* http_request( char *domain, int port, int time_out_sec, http_method_m method,
                    char *dest,  char *head_custom,  char *content,
                   int *response_code, int *answer_len)
{
    int ret = 0;
    int len = 0;
    int head_len = 0;
    int body_len = 0;
    int fd = 0;
    int index = 0;
    int remain_length = 0;
    int is_transfer_encoding = 0;
    char *p_start = NULL;
    char *p_end = NULL;
    char *answer = NULL;
    char ip[16] = { 0 };
    char buf[MAX_BUF_LEN] = { 0 };
    
    if (NULL == domain || NULL == dest || port < 0 ||
        NULL == response_code || NULL == answer_len) return NULL;
    
    *response_code = 0;
    body_len = (int)(content ? strlen(content) : 0);
    snprintf(buf, sizeof(buf), "%s %s HTTP/1.1\r\n"
                 "language: zh-CN\r\n"
                 "User-Agent: XPGWiFiSDK (v%s)\r\n"
                 "Host: %s:%d\r\n"
                 "Connection: keep-alive\r\n"
                 "%s"
                 "Content-Length: %d\r\n"
                 "\r\n",
                 (HTTP_POST == method) ? "POST" : (HTTP_GET == method) ? "GET" : (HTTP_PUT == method) ? "PUT" : "DELETE",
                 dest, "1.1.6.15042922", domain, port, head_custom ? head_custom : "", body_len);
    head_len = (int)strlen(buf);
    
    if (get_ip_by_domain(domain, ip)) {
        printf("get_ip_by_domain failed, domain:%s\n", domain);
    } else {
        fd = sock_connect_by_ip(ip, port);
        if (fd > 0) {
            ret = writen(fd, buf, head_len);
            if (ret != head_len) {
                printf("writen to fd %d failed, expect %d, return %d, %s\n", fd, head_len, ret, strerror(errno));
            } else {
                //printf("Send http head: %s", buf);
                ret = writen(fd, content, body_len);
                if (ret != body_len) {
                    printf("writen to fd %d failed, expect %d, return %d, %s\n", fd, body_len, ret,
                           strerror(errno));
                } else {
                    //printf("Send http body: %s\n", content);
                    memset(buf, 0, sizeof(buf));
                    head_len = 0;
                    while (!strstr(buf, "\r\n\r\n")) {
                        // ripple, 此处读取http reques的返回值
                        //需要修改一下
                        //ret = (int)read(fd, buf + head_len, sizeof(buf) - head_len - 1);
                        head_len += ret;
                        if (ret <= 0 || head_len >= sizeof(buf) - 1) {
                            break;
                        }
                    }
                    
                    if (ret > 0) {
                        //printf("Http response: %s\n", buf);
                        
                        p_start = strstr(buf, " ");
                        p_end = strstr(++p_start, " ");
                        if (p_end) {
                        	p_end[0] = '\0';
                        }
                        *response_code = atoi(p_start);
                        if  (p_end) {
                            p_end[0] = ' ';
                        }
                        
                        p_start = strstr(buf, "Content-Length:");
                        if (p_start) {
                            p_end = strstr(p_start, "\r\n");
                            if (p_end) {
                                p_end[0] = '\0';
                                len = atoi(p_start + strlen("Content-Length:"));
                                p_end[0] = '\r';
                                p_end = strstr(p_end, "\r\n\r\n");
                                if (p_end) {
                                    p_end += strlen("\r\n\r\n");
                                }
                            }
                        }
                        
                        p_start = strstr(buf, "Transfer-Encoding:");
                        if (p_start) {
                            is_transfer_encoding = 1;
                            p_end = strstr(p_start, "\r\n\r\n");
                            if (p_end) {
                                p_start = p_end + strlen("\r\n\r\n");
                                p_end = strstr(p_start, "\r\n");
                                if (p_end) {
                                    p_end[0] = '\0';
                                    sscanf(p_start, "%x", &len);
                                    p_end += strlen("\r\n");
                                }
                            }
                        }
                    } else {
                        printf("read failed, return %d, %s\n", ret, strerror(errno));
                    }
                    
                    if (len > 0 && p_end) {
                        //printf("Http Content-Length or first ChunckedSize: %d\n", len);
                        
                        *answer_len = len;
                        remain_length = (int)(len - (head_len - (p_end - buf)));
                        answer = (char *) malloc(len + 1);
                        if (answer) {
                            memset(answer, 0, len + 1);
                            if (remain_length > 0) {
                                memcpy(answer, p_end, head_len - (p_end - buf));
                                //ripple, 返回值判断这部分需要修改
                                ret = readn(fd, answer + head_len - (p_end - buf), remain_length);
                                if (ret != remain_length) {
                                    free(answer);
                                    answer = NULL;
                                    printf("readn return %d, expect %d\n", ret, remain_length);
                                } else {
                                    if (is_transfer_encoding) {
                                        memset(buf, 0, sizeof(buf));
                                        while (index < sizeof(buf) && 1 == readn(fd, buf + index, 1)) {
                                            ++index;
                                            
                                            if ((p_end = strstr(buf + 1, "\r\n"))) {
                                                p_end[0] = '\0';
                                                sscanf(buf + strlen("\r\n"), "%x", &remain_length);
                                                if (0 >= remain_length) {
                                                    break;
                                                }
                                                *answer_len += remain_length;
                                                answer = (char *)realloc(answer, len + remain_length + 1);
                                                if (answer) {
                                                    answer[len + remain_length] = '\0';
                                                    ret = readn(fd, answer + len, remain_length);
                                                    if (ret != remain_length) {
                                                        printf("readn return %d, expect %d\n", ret, remain_length);
                                                        free(answer);
                                                        answer = NULL;
                                                        break;
                                                    }
                                                    else
                                                    {
                                                        len += remain_length;
                                                    }
                                                } else {
                                                    printf("realloc a size of %d space failed\n", len + remain_length + 1);
                                                    break;
                                                }
                                                
                                                index = 0;
                                                memset(buf, 0, sizeof(buf));
                                            }
                                        }
                                    }
                                }
                            } else {
                                memcpy(answer, p_end, len);
                            }
                        } else {
                            printf("malloc a size of %d space failed\n", len + 1);
                        }
                    }
                }
            }
            //close(fd);
        } else {
            printf("socket_connect_by_ip %s:%d failed, return %d\n", ip, port, fd);
        }
    }
    
    if (!answer && len != 0) {
        *answer_len = 0;
        *response_code = 0;
    }
    
    return answer;
}

int mqtt_length2bytes(int length, char *buf, int *buf_len)
{
        int     val, digit, i;
        
        val = length;
    
	i = 0;
    	do {
        	digit = val % 128;
        	val = val / 128;
         	if (val > 0) digit = digit | 0x80;
        
        	buf[i] = digit;
		i++;
   	}while (val > 0);
	*buf_len = i;

	return 0;
}

//read config
int parse_config(con_st *con, char *config_file_path)
{
#if 0 	
    config_st       *config_file;
    if(con == NULL || config_file_path == NULL) return 1;

    if((config_file = config_init(config_file_path)) == NULL){
        printf("cann't open config file at %s...\n", config_file_path);
        return 1;
    }

    snprintf(con->mac, 16, "%s", get_value(config_file, "gagent.mac", ""));
    snprintf(con->did, 32, "%s", get_value(config_file, "gagent.did", ""));
    snprintf(con->passcode, 16, "%s", get_value(config_file, "gagent.passcode", ""));
    snprintf(con->pk, 48, "%s", get_value(config_file, "gagent.pk", ""));
    snprintf(con->pk_secret, 48, "%s", get_value(config_file, "gagent.pk_secret", ""));
    snprintf(con->hard_version, 16, "%s", get_value(config_file, "gagent.hard_version", ""));
    snprintf(con->soft_version, 16, "%s", get_value(config_file, "gagent.soft_version", ""));

    config_free(config_file);
#endif
    return 0;
}

int write_config(con_st *con, char *file_path)
{
#if  0	
    FILE    *fp;
    if(con == NULL || file_path == NULL) return -1; 

    fp = fopen(file_path, "w");
    if(fp == NULL) return -1; 

    fprintf(fp, "<gagent>\n");
    fprintf(fp, "\t<mac> %s </mac>\n", con->mac);
    fprintf(fp, "\t<did> %s </did>\n", con->did);
    fprintf(fp, "\t<passcode> %s </passcode>\n", con->passcode);
    fprintf(fp, "\t<pk> %s </pk>\n", con->pk);
    fprintf(fp, "\t<pk_secret> %s </pk_secret>\n", con->pk_secret);
    fprintf(fp, "\t<hard_version> %s </hard_version>\n", con->hard_version);
    fprintf(fp, "\t<soft_version> %s </soft_version>\n", con->soft_version);
    fprintf(fp, "</gagent>\n");

    fclose(fp);
#endif
	return 0;
}




