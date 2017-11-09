#ifndef _LAN_H__
#define _LAN_H__

#include "tool.h"
#include "libemqtt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CLIENT			8

typedef struct _lan_st lan_st;

struct _lan_st
{
	con_st	*con;
	int	udp_server;
	int	tcp_server;
	int	client_fd[MAX_CLIENT];
	char	recv_buf[BUF_LEN];
	int	recv_len;
	char	send_buf[BUF_LEN];
	int	send_len;
	char	passcode[32];

	int	sn;
	void	*in;
};

lan_st	*lan_init(con_st *con);
int	lan_do_tcp(lan_st *lan, int i);
int	lan_do_udp(lan_st *lan);
int     lan_send_data(lan_st *lan, char action, char *buf, int len);
#ifdef __cplusplus
}
#endif
#endif
