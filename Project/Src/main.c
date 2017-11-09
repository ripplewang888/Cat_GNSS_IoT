#include "main.h"
#include "error_handler.h"
#include "rcc.h"
#include "uart4.h"
#include "uart5.h"
#include "uart6.h"
#include "lm61.h"
#include "trace.h"
#include <string.h>
#include "tcp_task.h"
#include "cmsis_os.h"
#include "TCPClient.h"




static osThreadId MQTTThreadHandle;
static osThreadId TCPRecvHandle;
static osThreadId TCPSendHandle;

#define MSG_LEN_MAX         (1024)
//#define RELEASE_VERSION

//ripple added start for task{
#define START_TASK_PRIO		1
#define START_STK_SIZE 		4096  
TaskHandle_t StartTask_Handler;
void start_task(void *pvParameters);

#define TASK1_TASK_PRIO		10
#define TASK1_STK_SIZE 		1024  
TaskHandle_t Task1Task_Handler;

#define TASK2_TASK_PRIO		9
#define TASK2_STK_SIZE 		2048  
TaskHandle_t Task2Task_Handler;



//ripple added end for task}



/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static void prvSetupHardware( void );



uint8_t *msg_buf = NULL, *msg_readbuf = NULL;
TCP_client_t *pclient=NULL;    //will initialize as TCP_client_t



void device_check_and_init(){

    iot_TCP_param_t  tcp_params;
    //char *msg_buf = NULL, *msg_readbuf = NULL;
    uint8_t *readBuff=NULL;
    int len = 0;
    int total =0;
    uint8_t  *buff = NULL;
    uint8_t  ip[50];
    uint8_t  port[50];
   


//    if (NULL == (msg_buf = (uint8_t *)iot_platform_malloc(MSG_LEN_MAX))) {
//        TRACE_DEBUG("tcp_thread_initialize not enough memory");
//        goto do_exit;
//    }

//    if (NULL == (msg_readbuf = (uint8_t *)iot_platform_malloc(MSG_LEN_MAX))) {
//        TRACE_DEBUG("tcp_thread_initialize not enough memory");
//        goto do_exit;
//    }

#if 0 
    if ( CAT1_STATUS_SUCCESS != LM61_healthCheck()) {
        TRACE_DEBUG("LM61_healthCheck failed !!!");
        goto do_exit;
    }
#endif 

   //health check.
    if ( CAT1_STATUS_SUCCESS != LM61_sendAT_with_times(LM61_AT_AT, 5, NULL)) {
        TRACE_INFO("LM61 芯片模块初始化失败..");
        goto do_exit;
    }else{
        TRACE_INFO("LM61 芯片模块初始化成功..");
    }

    //check SIM card status.
    readBuff = LM61_HAS_SIM_CARD;
    if ( CAT1_STATUS_SUCCESS != LM61_sendAT_with_times(LM61_AT_SIM_STATUS, 5, readBuff)){
        TRACE_INFO("没有检测到SIM卡");
        goto do_exit;
    }else{
        TRACE_INFO("检测SIM卡成功");
    }

    //check SIM card signal 
    readBuff = "Signal Quality";
    TRACE_INFO("开始检测信号强度...");
    if ( CAT1_STATUS_SUCCESS != LM61_sendAT_with_times(LM61_AT_SIM_SIGNAL_INTENSITY, 5, readBuff)){
        TRACE_INFO("SIM卡无信号强度");
    }

    //检测是否连接上基站
    #if 0 
    if ( CAT1_STATUS_SUCCESS != LM61_sendAT_with_times(LM61_AT_SIM_CONNECT, 5,LM61_SIM_CONNECTED_SUCCESS)){
        TRACE_INFO("SIM卡连接基站失败...");
    }
    #endif

    /* Initialize MQTT parameter */
    memset(&tcp_params, 0x0, sizeof(tcp_params));

#ifndef RELEASE_VERSION
    tcp_params.port = "60000";
    tcp_params.host = "113.59.226.244";
#endif
    //tcp_params.port = 27500;
    //tcp_params.host = "106.14.125.103";
    

    tcp_params.pread_buf = msg_readbuf;
    tcp_params.read_buf_size = MSG_LEN_MAX;
    tcp_params.pwrite_buf = msg_buf;
    tcp_params.write_buf_size = MSG_LEN_MAX;

    delay(5000);

#ifdef  RELEASE_VERSION

    //IP
    printf("开始初始化TCP客户端...");
    printf("\r\n[Tips]请输入服务器IP地址:");
    memset(ip,0,sizeof(ip));
    scanf("%s",ip);
    tcp_params.host = ip;
    printf("\r\ntcp_params.host = %s\n", tcp_params.host);
    



     //port
     printf("\r\n[Tips]请输入服务器port:");
     scanf("%s",port);
     tcp_params.port = port;
     printf("\r\ntcp_params.port = %s", (char *)tcp_params.port);
#endif
 
    //iot_tcp_client_transparent_mode(&tcp_params);
    Gizwits_init();

   

    if (NULL == pclient) {
        TRACE_DEBUG("TCP construct failed");
        goto do_exit;
    }


do_exit:
    if (NULL != msg_buf) {
        iot_platform_free(msg_buf);
    }

    if (NULL != msg_readbuf) {
        iot_platform_free(msg_readbuf);
    }

}




/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
    
    size_t len = 0;    
    uint8_t  *buff = NULL;
  
  prvSetupHardware(); 

  TRACE_INFO("开始初始化硬件....");
  HAL_Delay(10000);

  device_check_and_init();
  UART4_StartRecv();

  /* Thread TCP  definition,  send and recv*/
  //osThreadDef(TCPSend, TCP_send_thread, osPriorityNormal, 0, 1024);
  //osThreadDef(TCPRecv, TCP_recv_thread, osPriorityNormal, 0, 1024);
  

  /* Thread MQTT definition */
  //osThreadDef(MQTT, MQTT_Thread, osPriorityNormal, 0, 1024);

  /* Start MQTT thread */
  //MQTTThreadHandle = osThreadCreate(osThread(MQTT), NULL);


  /*start TCP  send and recv thread*/
  //TCPRecvHandle =  osThreadCreate(osThread(TCPSend), NULL);
  //TCPSendHandle = osThreadCreate(osThread(TCPRecv), NULL);

  xTaskCreate((TaskFunction_t )start_task,         
              (const char*    )"start_task",         
              (uint16_t       )START_STK_SIZE,      
              (void*          )NULL,                
              (UBaseType_t    )START_TASK_PRIO,     
              (TaskHandle_t*  )&StartTask_Handler);          


  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  for(;;);
}



void start_task(void *pvParameters)
{
    taskENTER_CRITICAL();        
    xTaskCreate((TaskFunction_t )tcp_send_task,             
                (const char*    )"tcp_send_task",           
                (uint16_t       )TASK1_STK_SIZE,        
                (void*          )NULL,                  
                (UBaseType_t    )TASK1_TASK_PRIO,        
                (TaskHandle_t*  )&Task1Task_Handler);   
#if 1 
    xTaskCreate((TaskFunction_t )tcp_recv_task,     
                (const char*    )"tcp_recv_task",   
                (uint16_t       )TASK2_STK_SIZE,
                (void*          )NULL,
                (UBaseType_t    )TASK2_TASK_PRIO,
                (TaskHandle_t*  )&Task2Task_Handler); 
#endif    
    vTaskDelete(StartTask_Handler);
    taskEXIT_CRITICAL();           
}




/* Private functions ---------------------------------------------------------*/
static void prvSetupHardware( void )
{
  /* STM32F4xx HAL library initialization:
       - Configure the Flash prefetch, instruction and Data caches
       - Configure the Systick to generate an interrupt each 1 msec
       - Set NVIC Group Priority to 4
       - Global MSP (MCU Support Package) initialization
     */
  HAL_Init();

  /* Configure the system clock to 168 MHz */
  Rcc_Config();
  
  Uart4_config();
  Uart5_config();
  Uart6_config();
  UART5_StartRecv();
  UART6_StartRecv();
}




/**
  * @}
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *UartHandle)
{
   // HAL_UART4_RxCpltCallback(UartHandle);
  /* Set transmission flag: transfer complete */
  switch((unsigned int)UartHandle->Instance)
  {
    case (unsigned int)UART4:
      HAL_UART4_RxCpltCallback(UartHandle);
      break;
    
    case (unsigned int)UART5:
      HAL_UART5_RxCpltCallback(UartHandle);
      break;
    case (unsigned int)USART6:
      HAL_UART6_RxCpltCallback(UartHandle);
      break;
    
    default:
      break;
  }
    
}

/**
  * @}
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *UartHandle)
{
  //  HAL_UART4_TxCpltCallback(UartHandle);
  /* Set transmission flag: transfer complete */
  switch((unsigned int)UartHandle->Instance)
  {
    case (unsigned int)UART4:
      HAL_UART4_TxCpltCallback(UartHandle);
      break;
     
    case (unsigned int)UART5:
      HAL_UART5_TxCpltCallback(UartHandle);
      break;
   
    case (unsigned int)USART6:
      HAL_UART6_TxCpltCallback(UartHandle);
      break;
    
    default:
      break;
  }
    
}
void vApplicationMallocFailedHook( void )
{
	/* vApplicationMallocFailedHook() will only be called if
	configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
	function that will get called if a call to pvPortMalloc() fails.
	pvPortMalloc() is called internally by the kernel whenever a task, queue,
	timer or semaphore is created.  It is also called by various parts of the
	demo application.  If heap_1.c or heap_2.c are used, then the size of the
	heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
	FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
	to query the size of free heap space that remains (although it does not
	provide information on how the remaining heap might be fragmented). */
	taskDISABLE_INTERRUPTS();
	ERR_HANDLER_ASSERT("vApplicationMallocFailedHook");
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
	/* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
	to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
	task.  It is essential that code added to this hook function never attempts
	to block in any way (for example, call xQueueReceive() with a block time
	specified, or call vTaskDelay()).  If the application makes use of the
	vTaskDelete() API function (as this demo application does) then it is also
	important that vApplicationIdleHook() is permitted to return to its calling
	function, because it is the responsibility of the idle task to clean up
	memory allocated by the kernel to any task that has since been deleted. */
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	taskDISABLE_INTERRUPTS();
	ERR_HANDLER_ASSERT("vApplicationStackOverflowHook");
}


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
