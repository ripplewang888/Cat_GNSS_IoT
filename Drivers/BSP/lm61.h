#ifndef __LM61_H
#define __LM61_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"


/*send to device*/
#define LM61_AT_AT                  "AT\r\n"
#define LM61_AT_MANUFACTUREER       "AT+CGMI\n"
#define LM61_AT_REVISION            "AT+CGMR\n"
#define LM61_AT_IMEI                "AT+CGSN\n"
#define LM61_AT_IMSI                "AT+CIMI\n"
#define LM61_AT_CGCONTRDP           "AT+CGCONTRDP\n"
#define LM61_AT_CSQ                 "AT%CSQ\n"
#define LM61_AT_SRVTYPE             "AT^SISS=0,srvType,Socket\n"
#define LM61_AT_INIT                "AT^SICI=0\n"
#define LM61_AT_OPEN                "AT^SISO=0\n"
#define LM61_AT_CLOSE               "AT^SISC=0\n"
#define LM61_AT_RECEIVE             "AT^SISR=0\n"
#define LM61_AT_SEVER_IP            "AT^SISS=0,address,%s\n"
#define LM61_AT_TRANSPARENT_BAUDRATE         "AT^IPR=%d\n"
#define LM61_AT_BAUDRATE         "AT+IPR=%d\n"
#define LM61_AT_TRANSPARENT_START   "AT^SISD=1\n"
#define LM61_AT_TRANSPARENT_END       "++++++>\n"

#define LM61_AT_SERVER_PORT         "AT^SISS=0,port,%s\n"
#define LM61_AT_RESET               "AT%EXE=\"reboot\"\n"

#define LM61_AT_SIM_STATUS    "AT%STATUS=\"USIM\"\r\n"

#define LM61_AT_SIM_SIGNAL_INTENSITY   "AT%MEAS=\"8\"\r\n"

#define LM61_AT_SIM_CONNECT            "AT+CGATT?\n"



/*recv from device*/
#define LM61_RECV_OK                "OK"
#define LM61_RECV_ERR               "ERROR"
#define LM61_HAS_SIM_CARD    "REAL USIM"
#define LM61_SIM_CONNECTED_SUCCESS "CGATT: 1"

#define LM61_AT_TIMEOUT             30000
#define LM61_DELAY_1S               1000

/* Size of Reception buffer */
#define RECV_BUFFERSIZE             1024
#define AT_BASE_LENGTH 27


typedef enum LM61_RETRUN_CODE{
	CAT1_STATUS_SUCCESS       =  0,
	CAT1_STATUS_FAILED        =  1,
	CAT1_STATUS_INVALID_PARMS =  2,
	CAT1_STATUS_TIMEOUT       =  3,
	CAT1_STATUS_RESULT_ERROR =4,

} lm61_rc_t;

lm61_rc_t LM61_socketOpen(char *ip, uint16_t port);
lm61_rc_t LM61_socketClose(void);
lm61_rc_t LM61_socketRead(uint8_t** pRecvBuf, int *pLen, int timeout_ms);
lm61_rc_t LM61_socketWrite(uint8_t* buffer, int len, int timeout_ms);
lm61_rc_t LM61_healthCheck(void);
lm61_rc_t LM61_sendAT(uint8_t *cmdSend , uint8_t *cmdRecv,int timeoutMs);
lm61_rc_t LM61_sendData(uint8_t *cmdSend , uint8_t *cmdRecv,int timeoutMs);
lm61_rc_t LM61_RecvData(uint8_t *cmdSend , uint8_t *cmdRecv,int timeoutMs);
lm61_rc_t LM61_reset(void);
lm61_rc_t LM61_get_manufacturer(char *manufacturer);
lm61_rc_t LM61_get_revision(char * revision);
lm61_rc_t LM61_get_IMEI(char * IMEI);
lm61_rc_t LM61_get_IMSI(char * IMSI);
lm61_rc_t LM61_get_SQ(char *SQ);
lm61_rc_t LM61_get_local_ip(char * IP);
lm61_rc_t LM61_start_transparent_mode(uint8_t *ip, uint8_t  *port);
lm61_rc_t LM61_transparent_Write(uint8_t* buffer, int len, int timeout_ms);
lm61_rc_t LM61_transparent_Read(uint8_t* readBuffer,int * len);
lm61_rc_t  LM61_sendAT_with_times(char *cmdSend, int times,  char * matchstr);
lm61_rc_t LM61_transparent_mode_sendData(uint8_t *cmdSend , int  len,int timeoutMs);



#endif /* __LM61_H */

