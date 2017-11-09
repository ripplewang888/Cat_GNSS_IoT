#include "uart4.h"
#include "error_handler.h"

/* Private macro -------------------------------------------------------------*/

/* Size of Reception buffer */
#define RXBUFFERSIZE                    UART4_RXBUFFERSIZE
  
/* Exported macro ------------------------------------------------------------*/
/*#define COUNTOF(__BUFFER__)   (sizeof(__BUFFER__) / sizeof(*(__BUFFER__)))*/
/* Private variables ---------------------------------------------------------*/
/* UART handler declaration */
UART_HandleTypeDef Uart4Handle;
__IO ITStatus Uart4RxReady = RESET;
__IO ITStatus Uart4TxReady = RESET;


/* Buffer used for reception */
static uint8_t aRxBuffer[RXBUFFERSIZE+1];
static uint32_t bufferPoint;
static uint32_t unReadBytes;
//此处为了区分刚开始接收输入信息和 串口一直循环接收信息
// 如果uart4_recv_start_flag = 1  了，那么表示uart4 开始了一直循环接收模式
//即 uart的回调函数里面会调用HAL_UART_Receive_IT
uint8_t  uart4_recv_start_flag =0;  




/*##-1- Configure the UART peripheral ######################################*/
/* Put the USART peripheral in the Asynchronous mode (UART Mode) */
/* UART1 configured as follow:
    - Word Length = 8 Bits
    - Stop Bit = One Stop bit
    - Parity = None
    - BaudRate = 9600 baud
    - Hardware flow control disabled (RTS and CTS signals) */
void Uart4_config(void)
{
  Uart4Handle.Instance          = UART4;

  Uart4Handle.Init.BaudRate     = 115200;
  Uart4Handle.Init.WordLength   = UART_WORDLENGTH_8B;
  Uart4Handle.Init.StopBits     = UART_STOPBITS_1;
  Uart4Handle.Init.Parity       = UART_PARITY_NONE;
  Uart4Handle.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  Uart4Handle.Init.Mode         = UART_MODE_TX_RX;
  Uart4Handle.Init.OverSampling = UART_OVERSAMPLING_16;
    
  if(HAL_UART_Init(&Uart4Handle) != HAL_OK)
  {
    ERR_HANDLER_ASSERT("failed!!!\r\n");
  }
  bufferPoint = 0;
  aRxBuffer[RXBUFFERSIZE] = '\0';
  unReadBytes=0;
  
}

void Uart4_SendMsg(uint8_t* sendBuff, size_t size)
{
  /*##-2- Start the transmission process #####################################*/  

  while (Uart4TxReady == SET)
  {
  } 
  if(HAL_UART_Transmit_IT(&Uart4Handle, sendBuff, size)!= HAL_OK)
  {
    ERR_HANDLER_ASSERT("failed!!!\r\n");
  }
  /*## Wait for the end of the transfer ###################################*/   
  while (Uart4TxReady != SET)
  {
  } 
   /* Reset transmission flag */
  Uart4TxReady = RESET;  


}

size_t Uart4_RcvMsg(uint8_t **pReadBuff)
{
    size_t len = 0;
    if(pReadBuff == NULL ) return 0;
    while (Uart4RxReady == SET)
    {
    }    
    if(unReadBytes > bufferPoint){        
        len = unReadBytes - bufferPoint;
        *pReadBuff =  (uint8_t *)aRxBuffer + (bufferPoint + RXBUFFERSIZE - unReadBytes);
    }
    else{
        len=unReadBytes;
        *pReadBuff =  (uint8_t *)aRxBuffer + (bufferPoint - unReadBytes);
    }
    unReadBytes -= len;
    return len;
}

void Uart4_MspInit(void)
{  
  GPIO_InitTypeDef  GPIO_InitStruct;
  
  /*##-1- Enable peripherals and GPIO Clocks #################################*/
  /* Enable GPIO TX/RX clock */
  UART4_TX_GPIO_CLK_ENABLE();
  UART4_RX_GPIO_CLK_ENABLE();
  /* Enable clock */
  UART4_CLK_ENABLE(); 
  
  /*##-2- Configure peripheral GPIO ##########################################*/  
  /* UART TX GPIO pin configuration  */
  GPIO_InitStruct.Pin       = UART4_TX_PIN;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FAST;
  GPIO_InitStruct.Alternate = UART4_TX_AF;
  
  HAL_GPIO_Init(UART4_TX_GPIO_PORT, &GPIO_InitStruct);
    
  /* UART RX GPIO pin configuration  */
  GPIO_InitStruct.Pin = UART4_RX_PIN;
  GPIO_InitStruct.Alternate = UART4_RX_AF;
    
  HAL_GPIO_Init(UART4_RX_GPIO_PORT, &GPIO_InitStruct);

  /*##-3- Configure the NVIC for UART ########################################*/
  /* NVIC for USART1 */
  HAL_NVIC_SetPriority(UART4_IRQn, 0, 1);
  HAL_NVIC_EnableIRQ(UART4_IRQn);
  
  
}

void UART4_StartRecv(void){
    uart4_recv_start_flag = 1;
    HAL_UART_Receive_IT(&Uart4Handle, (uint8_t *)aRxBuffer, 1);
}

void Uart4_MspDeInit(void)
{
  /*##-1- Reset peripherals ##################################################*/
  UART4_FORCE_RESET();
  UART4_RELEASE_RESET();

  /*##-2- Disable peripherals and GPIO Clocks #################################*/
  /* Configure UART Tx as alternate function  */
  HAL_GPIO_DeInit(UART4_TX_GPIO_PORT, UART4_TX_PIN);
  /* Configure UART Rx as alternate function  */
  HAL_GPIO_DeInit(UART4_RX_GPIO_PORT, UART4_RX_PIN);

  
  /*##-3- Disable the NVIC for UART ##########################################*/
  HAL_NVIC_DisableIRQ(UART4_IRQn);
}  

/**
  * @brief  Tx Transfer completed callback
  * @param  Uart4Handle: UART handle. 
  * @note   This example shows a simple way to report end of IT Tx transfer, and 
  *         you can add your own implementation. 
  * @retval None
  */
void HAL_UART4_TxCpltCallback(UART_HandleTypeDef *UartHandle)
{
  /* Set transmission flag: transfer complete */
  Uart4TxReady = SET;
}


/**
  * @brief  Rx Transfer completed callback
  * @param  UartHandle: UART handle
  * @note   This example shows a simple way to report end of IT Rx transfer, and 
  *         you can add your own implementation.
  * @retval None
  */
void HAL_UART4_RxCpltCallback(UART_HandleTypeDef *UartHandle)
{
  /* Set transmission flag: transfer complete */
  Uart4RxReady = SET;
  if(uart4_recv_start_flag == 1 )
  {
      if(bufferPoint >= RXBUFFERSIZE - 1)
          bufferPoint = 0;
      else
          bufferPoint ++;
      unReadBytes ++;
      HAL_UART_Receive_IT(UartHandle, (uint8_t *)aRxBuffer + bufferPoint, 1);
      if(unReadBytes > RXBUFFERSIZE)
          unReadBytes = RXBUFFERSIZE;
      Uart4RxReady = RESET;

  }
}








/**
  * @brief  This function handles UART interrupt request.  
  * @param  None
  * @retval None
  * @Note   This function is redefined in "main.h" and related to DMA stream 
  *         used for USART data transmission     
  */
void UART4_IRQHandler(void)
{
  HAL_UART_IRQHandler(&Uart4Handle);
}

