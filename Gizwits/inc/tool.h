#ifndef _TOOL_H__
#define _TOOL_H__

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>

#define BUF_LEN 				512 
#define ACTION_CONTROL			1
#define ACTION_READ_STATUS		2	
#define ACTION_READ_STATUS_ACK	3
#define ACTION_REPORT_STATUS	4
#define ACTION_TRANS_RECV		5
#define ACTION_TRANS_SEND		6
#define	ACTION_PUSH_OTA			254	

#define HEAD_LEN                5       
#define UDP_RECV_ERROR          199
#define TCP_RECV_ERROR_BASE     100
#define LOG_IP_BUF_LENGTH		16

#define HARD_VERSION			LINUX_LI
#define SOFT_VERSION			04030000
typedef struct _log_st	log_st;
typedef struct _con_st	con_st;
typedef unsigned char 	uint8;
typedef unsigned int	uint32;
typedef unsigned short int uint16;

struct _con_st
{
	char	mac[32];
	char	did[32];
	char	passcode[16];
	char	pk[48];
	char	pk_secret[48];
	char	hard_version[16];
	char	soft_version[16];
}; 

struct _log_st
{
	char	path[128];
	FILE	*fp;
	int	fd;
	int	size;
	int	level;
	int	num;
};

/* num == 0, log will auto create if larger than size, path is the pwd,
 * num != 0, log will interchange between path.new and path.bak if larger than size, path is the full path of log file */
log_st	*log_init(char *path, int size, int level, int num);
void	log_write(log_st *log, const char *msg, ...);
void	log_debug(log_st *log, const char *msg, ...);
void	log_checksize(log_st *log);
void	log_close(log_st *log);
int		readn(int fd, void *vptr, size_t n);
int		writen(int fd, const void *vptr, size_t n);
int     sock_connect_by_domain(char *domain, int port);
int     sock_connect_by_ip(char *ip, int port);
int		sock_disconnect(int fd);
int		set_sock_time(int fd, int read_sec, int write_sec);
int     create_listen(int port);
int		udp_wirte(int fd, int port, char *buf, int len);
int		create_read_udp(int read_port);
void 	StrToHex(unsigned char *pbDest, unsigned char *pbSrc, int nLen);
void 	HexToStr(unsigned char *pbDest, unsigned char *pbSrc, int nLen);
int     hex_string_to_10(char *string, int len);
void	StrToHex_noSpace(char *pbDest, char *pbSrc, int nLen);

#define MAX_BUF_LEN     1024
#define HTTP_STATUS_OK  200
#define HTTP_TIMEOUT	5

typedef enum _http_method_m {
    HTTP_POST = 1,
    HTTP_GET = 2,
    HTTP_DELETE = 3,
    HTTP_PUT = 4
} http_method_m;

static int xpg_get_ip_by_domain(const char* domain, char ip[16]);
char* http_request( char *domain, int port, int time_out_sec, http_method_m method,
                    char *dest,  char *head_custom,  char *content,
                   int *response_code, int *answer_len);

typedef struct _config_st config_st;
struct _config_st
{
	char	path[128];
	FILE	*fp;
	int		fd;
	char	*buf;
	int		size;

	char	**free_buf;
	int		free_buf_index;
	int		free_buf_size;
};

config_st *config_init(char *path);
/* string: the tag; val: default; return the tag's value */
char *get_value(config_st *config, char *string, char *val);
void config_free(config_st *config);
int lan_do_one_packet(void *in, unsigned char action, char *kv, int kv_len);
int cloud_do_one_packet(void *in, unsigned char action, char *kv, int kv_len);
static int get_ip_by_domain(const char* domain, char ip[16]);
int mqtt_length2bytes(int length, char *buf, int *buf_len);
int write_config(con_st *con, char *file_path);
int parse_config(con_st *con, char *config_file_path);
//int get_mac(char *mac, int len);
#endif
