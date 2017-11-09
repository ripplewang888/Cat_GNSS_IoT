#include <string.h>
#include "uart6.h"
#include "uart4.h"
#include "trace.h"
#include "lm61.h"
#include "iot_net.h"
#include "iot_device.h"
#include "TCPClient.h"
#include "TCP_task.h"
#include "error_handler.h"
#include "nmealib.h"
#include "cmsis_os.h"
#include "tool.h"
#include "cloud.h"

//NOTE!!! you must define the following topic in IOT consle before running this sample.
#define MSG_LEN_MAX         (1024)

extern TCP_client_t *pclient;
extern data_store_t GPS_data;
extern uint32_t resolved_GPS_data_count;
extern con_st      global_con;
extern cloud_st    *global_cloud;     

int check_time_task(cloud_st *cloud)
{
	TickType_t		now_time;
       static  TickType_t  xLastWakeTime;
      
	if(cloud == NULL ) return -1;

	now_time = HAL_GetTick();
	if((now_time -xLastWakeTime) > 120000){
            TRACE_INFO("\r\nnow_time = %d, xLastWakeTime =%d\r\n", now_time,xLastWakeTime);
		cloud_mqtt_ping(cloud);
             xLastWakeTime = now_time;
	}

	return 0;
}




static int  iot_TCP_msg_send( TCP_client_t * pclient)
{
    int rc = SUCCESS_RETURN;
    int sent = 0;
    iot_timer_t timer;
    int32_t lefttime = 0;

    if (!TCP_check_state_normal(pclient)) {
        return TCP_STATE_ERROR;
    }

   
    iot_timer_init(&timer);
    iot_timer_countdown(&timer, 1000);
    while (sent < pclient->buf_size_write) {
        rc = pclient->ipstack->write(pclient->ipstack, &pclient->buf_send[sent], pclient->buf_size_write, iot_timer_left(&timer));
        if (rc < 0) { // there was an error writing the data
            break;
        }
        sent += rc;
    }

    if (sent == pclient->buf_size_write) {
        rc = SUCCESS_RETURN;
    } else {
        TRACE_DEBUG("tcp send message bytes  num error.");
        rc = TCP_NETWORK_ERROR;
    }
    return rc;

}


/*
TCP send task thread.
receive uart4 mesage.  Then forward to LM61.
*/

void tcp_send_task(void *pvParameters)
{
    int rc = 0, msg_len;

    size_t len = 0;    
    uint8_t  *buff = NULL;
    char msg_pub[128];

    TRACE_INFO("开始发送任务..\n");
    
    while(1){
    /*check bufffer malloc. And receive data*/
    if( pclient !=NULL){
        memset(  pclient->buf_send, 0 , MSG_LEN_MAX);
        //vTaskDelay(500);
    }

    //mqtt heart beat
    //此处先用delay实现，数据通了以后换成timer
    //HAL_Delay(3000);
    check_time_task(global_cloud);  //mqtt heart ping
    //cloud_send_data   发送数据到server端
#if 0 
    if(time(NULL) - report_time > 3){
        memset(send_buf, 0, 512);
        snprintf(send_buf, 512, "this is test data, now time: %d", (int)time(NULL));
        //log_debug(_log, "now will report status: %s", send_buf);
        //API:  cloud API, will send you data to MQTT Server
        cloud_send_data(cloud, ACTION_TRANS_SEND, (char *)send_buf, strlen(send_buf));
        report_time = time(NULL);
    }       
#endif

    //暂时屏蔽发送数据任务，等mqtt client 稳定上线后，需要进一步考虑
#if 0 
    
    len = Uart4_RcvMsg(&buff);
    if(len == 0){

#ifdef  GPS_ENABLE        
        len = Uart5_RcvMsg(&buff);
        if(len == 0){
            vTaskDelay(500);
            continue ;
        } 
       TRACE_INFO("buff=%s",buff);
       //parse GPS data
       msg_len = snprintf(msg_pub, sizeof(msg_pub), "##300 %s##301 %s##302 %s##303 %s##304 %s", 
                   str_GGA_time(&GPS_data),
                   str_GGA_latitude(&GPS_data),
                   str_GGA_longitude(&GPS_data),
                   str_GGA_AMSL(&GPS_data),
                   str_GGA_HDOP(&GPS_data));

        TRACE_INFO("msg_pub=%s", msg_pub);
        len =0; 
#endif
        vTaskDelay(500);
        continue ;
    } 

    
    memcpy((char*)pclient->buf_send,(char*)buff, len);
    TRACE_INFO("receive len = %d, buf_send =%s", len, pclient->buf_send);
    pclient->buf_size_write  = len;

    //send to debug console
    //Uart4_SendMsg(pclient->buf_send, len);
    //LM61_transparent_Write(pclient->buf_send, pclient->buf_size_write+1, TCP_SEND_MESSAGE_TIMEOUT);
    
    //iot_platform_mutex_lock(pclient->uart_locker);
    LM61_transparent_Write(pclient->buf_send, pclient->buf_size_write, TCP_SEND_MESSAGE_TIMEOUT);
    //iot_platform_mutex_unlock(pclient->uart_locker);
    //memset(pclient->buf_send, 0 ,MSG_LEN_MAX);
     /*when use uart6 to send message. In order to prevent uart6 resource  confilicts. Need to locker.
        After send AT command done.  unlocker it*/
    //iot_platform_mutex_lock(pclient->uart_locker);
    // iot_TCP_msg_send(pclient);
    //iot_platform_mutex_unlock(pclient->uart_locker);

#endif
        vTaskDelay(500);



    }
    
}


/*
   receive LM61 message, then forward to UART4
*/

void tcp_recv_task(void *pvParameters)
{

    int socketReadLen = 0;
    size_t len = 0;    
    uint8_t *readBuff=NULL;

    
    TRACE_INFO("开始接收任务..\n");

    while(1){
        
        cloud_do_mqtt(global_cloud);
#if 0 
        TCP_state_t state = TCP_get_client_state(pclient);
        if (state != TCP_STATE_CONNECTED) {
            TRACE_DEBUG("state = %d", state);
            vTaskDelay(600);
            continue ;
        }

        len = Uart6_RcvMsg(&readBuff);
        //TRACE_DEBUG("readBuff = %s", readBuff);
        if(len == 0){
            vTaskDelay(600);
            continue ;
        } 

        if(len > 0){
          memcpy((char*)pclient->buf_read,(char*)readBuff, len);
          TRACE_DEBUG("pclient->buf_read=%s, len= %d",pclient->buf_read, len);
          Uart4_SendMsg(pclient->buf_read, len);
          memset(pclient->buf_read, 0 ,MSG_LEN_MAX);
        }

        //iot_platform_mutex_lock(pclient->uart_locker);
        //iot_platform_mutex_unlock(pclient->uart_locker);
        //send to  uart4
        socketReadLen = 0;

#endif        
        vTaskDelay(600);
    }



}

