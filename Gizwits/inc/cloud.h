#ifndef _CLOUD_H__
#define _CLOUD_H__

#include "tool.h"
#include "libemqtt.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_SSL
#include "g_ssl.h"
#endif

/*
#define G_SERVICE_DOMAIN        "iotopenapi.ffan.com"
#define G_SERVICE_PORT          80
#define G_M2M_DOMAIN            "iotm2m.ffan.com"
#define G_M2M_PORT              10003
#define G_M2M_SSL_PORT          10103
*/


// 短整型大小端互换
#define BigLittleSwap16(A)  ((((uint16)(A) & 0xff00) >> 8) | \
                            (((uint16)(A) & 0x00ff) << 8))
 // 长整型大小端互换
 
#define BigLittleSwap32(A)  ((((uint32)(A) & 0xff000000) >> 24) | \
                            (((uint32)(A) & 0x00ff0000) >> 8) | \
                            (((uint32)(A) & 0x0000ff00) << 8) | \
                            (((uint32)(A) & 0x000000ff) << 24))



//119.29.47.111
#define G_SERVICE_DOMAIN 	"api.gizwits.com"
#define G_SERVICE_DOMAIN_IP 	"119.29.47.111"
#define G_SERVICE_PORT		80
//203.195.142.207
#define G_M2M_DOMAIN		"sandbox.gizwits.com"
#define G_M2M_DOMAIN_IP		"203.195.142.207"
#define G_M2M_PORT			1883
#define G_M2M_SSL_PORT		8883

#define DID_LENGTH			22
typedef struct _cloud_st cloud_st;

struct _cloud_st
{
	con_st		*con;
	mqtt_broker_handle_t	broker;
	
	char		mqtt_server[128];
	int		mqtt_port;
	int		mqtt_fd;
	int		mqtt_ping_time;
#ifdef USE_SSL
	sslConnection_st	*ssl_mqtt;
#endif	
	char		recv_buf[BUF_LEN];
	int		recv_len;
	char		send_buf[BUF_LEN];
	int		send_len;

	char	ota_info[BUF_LEN];
	void		*in;
	int		sn;
};

cloud_st	*cloud_init(con_st *con, char *mac, char *file_path);
int		cloud_do_mqtt(cloud_st *cloud);
int		cloud_mqtt_connect(cloud_st *cloud, char *did, char *passcode);
int         cloud_mqtt_reconnect(cloud_st *cloud, char *did, char *passcode);
int		cloud_mqtt_ping(cloud_st *cloud);
int		cloud_register(cloud_st *cloud, char *did);
int		cloud_register_safe(cloud_st *cloud, char *did);
int		cloud_provision(cloud_st *cloud);
int		cloud_provision_safe(cloud_st *cloud);
int		cloud_send_data(cloud_st *cloud, char action, char *buf, int len);
int		check_ota(cloud_st *cloud);
int		cloud_push_ota(cloud_st *cloud, char *packet);
unsigned short  htons(unsigned short h);
unsigned short  ntohs(unsigned short  n);


#ifdef __cplusplus
}
#endif
#endif
