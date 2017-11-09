#include <string.h>
#include "lm61.h"
#include "trace.h"
#include "uart6.h"
#include "error_handler.h"
#include "cmsis_os.h"
#include "TCPClient.h"



static uint8_t recvBuffer[RECV_BUFFERSIZE];
static uint8_t hexCharBuffer[RECV_BUFFERSIZE];
static uint8_t cmdBuffer[RECV_BUFFERSIZE];

extern TCP_client_t *pclient;    


static lm61_rc_t check_status(char *status);
static lm61_rc_t at_common(char *cmdSend);
static lm61_rc_t at_common2(char *cmdSend, char *str);
static lm61_rc_t at_common3(char *cmdSend, char *str);
static lm61_rc_t at_common_transparent_result_action(char *cmdSend);

static void char_to_hex(char* des, char *src,int length);
static int hex_to_char(char *buff, const char *src, int len);


/*
LM61 entero into transparent mode.
Give ip, address. auto create, open ,init socket.
*/
lm61_rc_t LM61_start_transparent_mode(uint8_t *ip, uint8_t *port) {
    lm61_rc_t ret;
    char data[100];
    int i =0;

    //IP
    HAL_Delay(1000);
    memset(data, 0, sizeof(data));
    sprintf((char*)data,LM61_AT_SEVER_IP,ip);
    if ( CAT1_STATUS_SUCCESS != LM61_sendAT_with_times(data, 5,NULL)){
        TRACE_INFO("配置IP失败. ");
    }else{
        TRACE_INFO("配置IP成功. ");
    }


   //port
   HAL_Delay(1000);
   memset(data, 0, sizeof(data));
   sprintf((char*)data,LM61_AT_SERVER_PORT,port);
   if ( CAT1_STATUS_SUCCESS != LM61_sendAT_with_times(data, 5,NULL)){
       TRACE_INFO("配置port失败. ");
   }else{
       TRACE_INFO("配置port成功. ");
   }


   
#if 0 
   // transparent buad rate
   delay(1000);
   memset(data, 0, sizeof(data));
   sprintf((char*)data,LM61_AT_TRANSPARENT_BAUDRATE,115200);
   if ( CAT1_STATUS_SUCCESS != LM61_sendAT_with_times(data, 5,NULL)){
       TRACE_INFO("配置透传波特率失败. ");
   }else{
       TRACE_INFO("配置透传波特率成功. ");
   }

    // AT buad rate
    memset(data, 0, sizeof(data));
    sprintf((char*)data,LM61_AT_BAUDRATE,115200);
    ret = at_common(data);
    if ( CAT1_STATUS_SUCCESS != ret) {
        return ret;
    }




#endif

   //start transparent mode.
   if ( CAT1_STATUS_SUCCESS != LM61_sendAT_with_times(LM61_AT_TRANSPARENT_START, 5,NULL)){
       TRACE_INFO("开启透传模式失败.");
   }else{
       TRACE_INFO("开启透传模式成功.");
   }


   HAL_Delay(3000);

    TCP_set_client_state(pclient, TCP_STATE_CONNECTED);

    return CAT1_STATUS_SUCCESS;

}



//关闭透传模式
lm61_rc_t LM61_close_transparent_mode() {

   //close transparent mode.
   LM61_sendAT_with_times(LM61_AT_TRANSPARENT_END, 5,NULL);

       TRACE_INFO("关闭透传模式成功.");
    HAL_Delay(1000);
    return CAT1_STATUS_SUCCESS;

}



/* 
AT^SISS=0,srvType,STREAM
OK
AT^SISS=0,address,10.0.0.10
OK
AT^SISS=0,port,6800
OK
AT^SICI=0
OK
AT^SISO=0
OK
AT^SISW=0,helloworld
OK 	
AT^SISR=0
^SISR:HelloCustomer
AT^SISC=0
OK 	
*/
lm61_rc_t LM61_socketOpen(char *ip, uint16_t port)
{
  lm61_rc_t ret;
  char data1[100];
  char data2[100];

  sprintf((char*)data1,LM61_AT_SEVER_IP,ip);
  sprintf((char*)data2,LM61_AT_SERVER_PORT,port);

  ret = at_common(LM61_AT_SRVTYPE);
  if ( CAT1_STATUS_SUCCESS != ret) {
      return ret;
  }

  ret = at_common(data1);
  if ( CAT1_STATUS_SUCCESS != ret) {
      return ret;
  }

  ret = at_common(data2);
  if ( CAT1_STATUS_SUCCESS != ret) {
      return ret;
  }

  ret = at_common(LM61_AT_INIT);
  if ( CAT1_STATUS_SUCCESS != ret) {
      return ret;
  }  

  ret = at_common(LM61_AT_OPEN);
  if ( CAT1_STATUS_SUCCESS != ret) {
      return ret;
  } 

  return CAT1_STATUS_SUCCESS;
}

lm61_rc_t LM61_socketClose(void)
{
  int i = 0;
  lm61_rc_t ret;

  do
  {
    ret = at_common(LM61_AT_CLOSE);
    osDelay(LM61_DELAY_1S);
    i++;
  }while(ret != CAT1_STATUS_SUCCESS && i<10);

  if(i>=10){
    TRACE_ERROR("-------!!! LM61_healthCheck Failed-------");
    return CAT1_STATUS_FAILED;
  }	

  return CAT1_STATUS_SUCCESS;	
}

lm61_rc_t LM61_socketRead(uint8_t** pRecvBuf, int *pLen, int timeout_ms) 
{
  char *token = NULL;
	char *str = NULL;

  memset(recvBuffer, 0x00, RECV_BUFFERSIZE);
  /* read mqtt data from uart. then put uart data into ringbuffer */
  LM61_RecvData("AT^SISHR=0\r\n", recvBuffer, timeout_ms);

  if(AT_BASE_LENGTH >= strlen((char *)recvBuffer))
    return CAT1_STATUS_FAILED;

  /***** has ^SISHR: or not ******/
  str = strstr((char *)recvBuffer, "AT^SISHR=0");
  if(str == NULL)
    return CAT1_STATUS_FAILED;

  /***** Socket not connected ******/
  str = strstr((char *)recvBuffer, "Socket");   
  if(str != NULL)
    ERR_HANDLER_ASSERT("Socket not connected");

  /***** when ERROR, the dog timer will restart the whole system ******/
  str = strstr((char *)recvBuffer, "ERROR");   
  if( NULL != str)
    ERR_HANDLER_ASSERT("ERROR");

  token = strtok((char *)recvBuffer, "\r\n");
  //  char *atCmd = token;
  if (token != NULL) {
    token = strtok(NULL, ":");
  }
  //  char *echo=token;
  if (token != NULL) {
    token = strtok(NULL, "\r\n");
  }

  char *data = token;
  if (token != NULL) {
    token = strtok(NULL, "\r\n");
  }
  char *status = token;
  if (!memcmp(status,"OK",sizeof("OK")-1)) {
    char_to_hex((char *)recvBuffer,(char *)data,strlen((char *)data));
    *pRecvBuf = recvBuffer;
    *pLen = strlen((char *)data) / 2;
    return CAT1_STATUS_SUCCESS;
  }
  return CAT1_STATUS_FAILED;
}

lm61_rc_t LM61_socketWrite(uint8_t* buffer, int len, int timeout_ms)
{
  //TRACE_INFO("iot_platform_tcp_write");
  //printf("--------HEX------------\r\n");
  //for(int i = 0; i < len; i++) printf("0x%x ",buffer[i]);
  //printf("\r\n-------------------------\r\n");

  hex_to_char((char *)hexCharBuffer,(char *)buffer,len);
  sprintf((char *)cmdBuffer,"AT^SISH=0,%s",(char *)hexCharBuffer);

  memset(recvBuffer, 0x00, RECV_BUFFERSIZE);
  return LM61_sendData(cmdBuffer, recvBuffer, timeout_ms);
}

lm61_rc_t LM61_transparent_Read(uint8_t* readBuffer,int * len)
{
    
     uint8_t *readBuff=NULL;
      *len = Uart6_RcvMsg(&readBuff);
      if(*len > 0){
        //strncpy((char *)cmdRecv, (char *)readBuff, len);
        TRACE_DEBUG("<<<<<< Receive[%d]\r\n%s",*len, readBuffer);
        memcpy((char*)readBuffer,(char*)readBuff, *len);
        //printf("--------HEX------------\r\n");
        //for(int i = 0; i < len; i++) printf("0x%x ",(*cmdRecv)[i]);
        //printf("\r\n-------------------------\r\n");    
      }

      return CAT1_STATUS_SUCCESS;

}



lm61_rc_t LM61_transparent_Write(uint8_t* buffer, int len, int timeout_ms){
    //hex_to_char((char *)hexCharBuffer,(char *)buffer,len);
    //sprintf((char *)cmdBuffer,"%s>",(char *)hexCharBuffer);
    buffer[len] = '>';
    //buffer[len+1] = '\n';
    //TRACE_INFO("buffer = %s", buffer);
    return LM61_transparent_mode_sendData(buffer, len+1, timeout_ms);

}


lm61_rc_t LM61_healthCheck(void)
{
  int i = 0;
  lm61_rc_t ret;

  TRACE_INFO("+------------------+");
  TRACE_INFO("| LM61_healthCheck |");
  TRACE_INFO("+------------------+");


  
  do
  {
    ret = at_common(LM61_AT_AT);
    HAL_Delay(LM61_DELAY_1S);
    i++;
    TRACE_DEBUG("i=%d", i);
  }while(ret != CAT1_STATUS_SUCCESS && i<10);

  if(i>=10){
    TRACE_INFO("-------!!! LM61_healthCheck Failed-------");
    return CAT1_STATUS_FAILED;
  }

  TRACE_INFO("-------LM61_healthCheck passed-------");
  return CAT1_STATUS_SUCCESS;
}


lm61_rc_t LM61_sendAT(uint8_t *cmdSend , uint8_t *cmdRecv,int timeoutMs)
{
  uint32_t curTime =0;
  size_t len = 0;
  uint8_t *readBuff=NULL;

  TRACE_DEBUG(">>>>>> sendCmd : %s",cmdSend);
  //Uart6_SendMsg(cmdSend,strlen((char*)cmdSend));

  Uart6_SendMsg(cmdSend,strlen((char*)cmdSend));

  
  while (timeoutMs > curTime) {
    HAL_Delay(1000);
    curTime  +=1000;
    len = Uart6_RcvMsg(&readBuff);
    if(len > 0){
      strncpy((char*)cmdRecv, (char*)readBuff, len);
      TRACE_DEBUG("<<<<<< Receive[%d]\r\n%s",len, cmdRecv);
      //printf("--------HEX------------\r\n");
      //for(int i = 0; i < len; i++) printf("0x%x ",(*cmdRecv)[i]);
      //printf("\r\n-------------------------\r\n");
      return CAT1_STATUS_SUCCESS;
    }
    else {
      continue;
    }
	}
  TRACE_DEBUG("!!!Receive Timeout!!!");
  return CAT1_STATUS_TIMEOUT;
}

lm61_rc_t LM61_transparent_mode_sendData(uint8_t *sendData , int  len,int timeoutMs)
{
	uint32_t lastTime;
	uint32_t curTime;
	uint8_t *readBuff;

	TRACE_DEBUG("sendData=%s , len =%d", sendData, len);
	Uart6_SendMsg(sendData,len);
	HAL_Delay(500);


	return  CAT1_STATUS_TIMEOUT;
}





lm61_rc_t LM61_sendData(uint8_t *cmdSend , uint8_t *cmdRecv,int timeoutMs)
{
  uint32_t lastTime;
  uint32_t curTime;
  size_t len = 0;
  uint8_t *readBuff;

  TRACE_DEBUG(">>>>>> sendCmd : %s",cmdSend);
  Uart6_SendMsg(cmdSend,strlen((char*)cmdSend));

  lastTime = osKernelSysTick();
  curTime = osKernelSysTick();
  osDelay(LM61_DELAY_1S);
  
  while (timeoutMs > (curTime - lastTime)) {
    curTime = osKernelSysTick();
    if (curTime < lastTime) {
      lastTime = curTime;
    }
    

    len = Uart6_RcvMsg(&readBuff);
    TRACE_DEBUG("len = %d", len);
#if 1 
    if(len > 0){
      strncpy((char *)cmdRecv, (char *)readBuff, len);
      TRACE_DEBUG("<<<<<< Receive[%d]\r\n%s",len, cmdRecv);
      //printf("--------HEX------------\r\n");
      //for(int i = 0; i < len; i++) printf("0x%x ",(*cmdRecv)[i]);
      //printf("\r\n-------------------------\r\n");
      if(NULL != strstr((char*)cmdRecv, "T^SISH=0")){
        return CAT1_STATUS_SUCCESS;
      }
      else
        continue;
    } 
    else {
      continue;
    }
#endif    

  }
  return CAT1_STATUS_TIMEOUT;
}


lm61_rc_t LM61_RecvData(uint8_t *cmdSend , uint8_t *cmdRecv,int timeoutMs)
{
  uint32_t lastTime;
  uint32_t curTime;
  size_t len = 0;
  uint8_t *readBuff;

  TRACE_DEBUG(">>>>>> RecvCmd : %s",cmdSend);
  Uart6_SendMsg(cmdSend,strlen((char*)cmdSend));

  lastTime = osKernelSysTick();
  curTime = osKernelSysTick();
  osDelay(LM61_DELAY_1S);
  
  while (timeoutMs > (curTime - lastTime)) {
    curTime = osKernelSysTick();
    if (curTime < lastTime) {
      lastTime = curTime;
    }

    len = Uart6_RcvMsg(&readBuff);
    if(len > 0){
      strncpy((char *)cmdRecv, (char *)readBuff, len);
      TRACE_DEBUG("<<<<<< Receive[%d]\r\n%s",len, cmdRecv);
      //printf("--------HEX------------\r\n");
      //for(int i = 0; i < len; i++) printf("0x%x ",(*cmdRecv)[i]);
      //printf("\r\n-------------------------\r\n");	
      if(NULL != strstr((char*)cmdRecv, "AT^SISHR=0")) 
        return CAT1_STATUS_SUCCESS;
       else 
        continue;
      
    } else {
      continue;
    }

  }
  return CAT1_STATUS_TIMEOUT;
}

lm61_rc_t LM61_reset(void)
{
  //LM61_healthCheck();

  return at_common(LM61_AT_RESET);
}

lm61_rc_t LM61_get_manufacturer(char *manufacturer)
{
  return at_common2(LM61_AT_MANUFACTUREER, manufacturer);
}

lm61_rc_t LM61_get_revision(char * revision)
{
  return at_common2(LM61_AT_REVISION, revision);
}

lm61_rc_t LM61_get_IMEI(char * IMEI)
{
  return at_common2(LM61_AT_IMEI, IMEI);
}

lm61_rc_t LM61_get_IMSI(char * IMSI)
{
  return at_common2(LM61_AT_IMSI, IMSI);
}

lm61_rc_t  LM61_get_SQ(char *SQ)
{
  return at_common2(LM61_AT_CSQ, SQ);
}

lm61_rc_t LM61_get_local_ip(char * IP)
{
  return at_common3(LM61_AT_CGCONTRDP, IP);
}

/*
Send AT comand command.
times:  max send times,  until commmand send success.
matchstr: send until match  str success.

*/
lm61_rc_t  LM61_sendAT_with_times(char *cmdSend, int times,  char * matchstr)
{

    lm61_rc_t ret;
    char *token = NULL;
    uint8_t  i =0;
    
    do
    {
        HAL_Delay(200);
        TRACE_DEBUG("times i = %d, cmdSend =%s\n",i+1,cmdSend);
        memset(recvBuffer, 0x00, RECV_BUFFERSIZE);
        ret = LM61_sendAT((uint8_t *)cmdSend, recvBuffer, LM61_AT_TIMEOUT);
        if (ret == CAT1_STATUS_TIMEOUT) {
            ret =CAT1_STATUS_TIMEOUT;
            i++;
            continue;
        } 
        else if (ret == CAT1_STATUS_SUCCESS) {
            if( NULL  == strstr((char *)recvBuffer, "OK")){
                ret =CAT1_STATUS_FAILED;
                return ret;
            }
            
            //if matchstr is not null, judge if exists match string.
            if(matchstr != NULL){
                if(NULL  == strstr((char *)recvBuffer, matchstr)){
                    ret = CAT1_STATUS_RESULT_ERROR;
                    i++;
                    continue;
                }

                //get receive buffer. see if something else to do.
                matchstr = (char *)recvBuffer;
                //strcpy(matchstr, recvBuffer);
                TRACE_INFO("%s", recvBuffer);
            }
            ret = CAT1_STATUS_SUCCESS;
            return ret;
      }

    }while(i<times);

    if(i>=times){
    TRACE_DEBUG("command %s failed. ret=%d\n",cmdSend,ret);
    return ret;
    }
    return CAT1_STATUS_SUCCESS;
}



static lm61_rc_t at_common_transparent_result_action(char *cmdSend)
    {
      lm61_rc_t ret;
      char *token = NULL;
    
      TRACE_DEBUG("cmdSend =%s\n",cmdSend);
      memset(recvBuffer, 0x00, RECV_BUFFERSIZE);
      ret = LM61_sendAT((uint8_t *)cmdSend, recvBuffer, LM61_AT_TIMEOUT);
      TRACE_DEBUG("cmdSend = %s\n",cmdSend);
      if (ret == CAT1_STATUS_TIMEOUT) {
        return ret;
      } 
      else if (ret == CAT1_STATUS_SUCCESS) {
        token = strtok((char *)recvBuffer, "\r\n");
        if( NULL  != strstr(token, "OK")){
               return  CAT1_STATUS_SUCCESS;
        }
      }
        return CAT1_STATUS_FAILED;
    }





/* 1 return */
static lm61_rc_t at_common(char *cmdSend)
{
  lm61_rc_t ret;
  char *token = NULL;

  TRACE_DEBUG("cmdSend =%s\n",cmdSend);
  memset(recvBuffer, 0x00, RECV_BUFFERSIZE);
  ret = LM61_sendAT((uint8_t *)cmdSend, recvBuffer, LM61_AT_TIMEOUT);
  TRACE_DEBUG("cmdSend = %s\n",cmdSend);
  if (ret == CAT1_STATUS_TIMEOUT) {
    return ret;
  } 
  else if (ret == CAT1_STATUS_SUCCESS) {
    token = strtok((char *)recvBuffer, "\r\n");
    char *atCmd = token;
    if (token != NULL) {
      token = strtok(NULL, "\r\n");
    }
    char *status = token;
    TRACE_DEBUG("atCmd[%s],status[%s]\n", atCmd, status);
    return check_status(status);
  }
	return CAT1_STATUS_FAILED;
}

/* 2 lines return */
static lm61_rc_t at_common2(char *cmdSend, char *str)
{
  lm61_rc_t ret;
  char *token = NULL;

  memset(recvBuffer, 0x00, RECV_BUFFERSIZE);
  ret = LM61_sendAT((uint8_t *)cmdSend, recvBuffer, LM61_AT_TIMEOUT);
  if (ret == CAT1_STATUS_TIMEOUT) {
    return ret;
  } 
  else if (ret == CAT1_STATUS_SUCCESS) {
    token = strtok((char *)recvBuffer, "\r\n");
    char *atCmd = token;
    if (token != NULL) {
      token = strtok(NULL, "\r\n");
    }
    char *Cmddata=token;
    if (token != NULL) {
      token = strtok(NULL, "\r\n");
    }
    char *status = token;
    strcpy(str, Cmddata);
    TRACE_DEBUG("atCmd[%s] Cmddata[%s] status[%s]",atCmd,Cmddata, status);
    return check_status(status);
  }
	return CAT1_STATUS_FAILED;
}

/* 3 lines return */
static lm61_rc_t at_common3(char *cmdSend, char *str)
{
  lm61_rc_t ret;
  char *token = NULL;

  memset(recvBuffer, 0x00, RECV_BUFFERSIZE);
  ret = LM61_sendAT((uint8_t *)cmdSend, recvBuffer, LM61_AT_TIMEOUT);
  if (ret == CAT1_STATUS_TIMEOUT) {
    return ret;
  } 
  else if (ret == CAT1_STATUS_SUCCESS) {
    token = strtok((char *)recvBuffer, "\r\n");
    char *atCmd = token;
    if (token != NULL) {
      token = strtok(NULL, "\r\n");
    }
    char *cmd = token;
    if (token != NULL) {
      token = strtok(NULL, "\r\n");
    }
    char *Cmddata=token;
    if (token != NULL) {
      token = strtok(NULL, "\r\n");
    }
    char *status = token;
    strcpy(str,cmd);
    TRACE_DEBUG("atCmd[%s] cmd[%s] Cmddata[%s] status[%s]",atCmd,cmd,Cmddata, status);
    return check_status(status);
  }
	return CAT1_STATUS_FAILED;
}


static lm61_rc_t check_status(char *status)
{
   if (!memcmp(status,LM61_RECV_OK,sizeof(LM61_RECV_OK)-1)) {
     return CAT1_STATUS_SUCCESS;
   } 
   else if (!memcmp(status,LM61_RECV_ERR,sizeof(LM61_RECV_ERR)-1)) {
     return CAT1_STATUS_FAILED;
   } 
   else {
     return CAT1_STATUS_INVALID_PARMS;
   }
}

static void char_to_hex(char* des, char *src,int length)
{
  int i,j;

  for(j=0; j<length; j++)
  {
    *des=0;
    for (i = 0; i < 2; i++)
    {
      *des = *des<<4;
      if (*src >= '0' && *src <= '9')
      {
        *des += *src++ - '0';							
        //*des += *src++ - 0x30;
      }
      else if (*src >= 'A' && *src <= 'F')
      {
        *des += *src++ - 'A' + 10;
        //*des += *src++ - 0x37;
      }
      else if (*src >= 'a' && *src <= 'f')
      {
        *des += *src++ - 'a' + 10;
        //*des += *src++ - 0x37;
      }
    }
    des++;
  }
}


static int hex_to_char(char *buff, const char *src, int len)
{
  int rc=0;
  unsigned char temp,value;
  char * dest;
  dest=buff;
  while(len)
  { 
    temp = (unsigned char )*src++;
    value = temp &0x0f;
    temp = temp >>4;
    if(temp < 10)
      *dest++ = temp + 0x30; 
    else
      *dest++ = temp + 0x37;  //why is 0x31 because of lacking of 0x40
    if(value < 10)
      *dest++ = value + 0x30;
    else
      *dest++ = value + 0x37;
    len--;  
  } 
  *dest++ ='\r';
  *dest++ ='\n';
  *dest ='\0';

  return rc;
}


