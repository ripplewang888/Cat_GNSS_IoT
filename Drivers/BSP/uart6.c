#include <string.h>
#include "uart6.h"
#include "error_handler.h"
#include "trace.h"

/* Private macro -------------------------------------------------------------*/
/* Size of Reception buffer */
#define RXBUFFERSIZE                    UART6_RXBUFFERSIZE

UART_HandleTypeDef Uart6Handle;
__IO ITStatus Uart6RxReady = RESET;
__IO ITStatus Uart6TxReady = RESET;


/* Buffer used for reception */
static uint8_t aRxBuffer[RXBUFFERSIZE+1];
static uint32_t bufferPoint;
static uint32_t unReadBytes;

/*##-1- Configure the UART peripheral ######################################*/
/* Put the USART peripheral in the Asynchronous mode (UART Mode) */
/* UART1 configured as follow:
    - Word Length = 8 Bits
    - Stop Bit = One Stop bit
    - Parity = None
    - BaudRate = 9600 baud
    - Hardware flow control disabled (RTS and CTS signals) */
void Uart6_config(void)
{
  Uart6Handle.Instance          = USART6;

  Uart6Handle.Init.BaudRate     = 115200;
  Uart6Handle.Init.WordLength   = UART_WORDLENGTH_8B;
  Uart6Handle.Init.StopBits     = UART_STOPBITS_1;
  Uart6Handle.Init.Parity       = UART_PARITY_NONE;
  Uart6Handle.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  Uart6Handle.Init.Mode         = UART_MODE_TX_RX;
  Uart6Handle.Init.OverSampling = UART_OVERSAMPLING_16;
    
  if(HAL_UART_Init(&Uart6Handle) != HAL_OK)
  {
    ERR_HANDLER_ASSERT("failed!!!\r\n");
  }
  bufferPoint = 0;
  aRxBuffer[RXBUFFERSIZE] = '\0';
  unReadBytes=0;
  
}

void Uart6_SendMsg(uint8_t* sendBuff, size_t size)
{
  /*##-2- Start the transmission process #####################################*/  

  while (Uart6TxReady == SET)
  {
  } 
  
  if(HAL_UART_Transmit_IT(&Uart6Handle, sendBuff, size)!= HAL_OK)
  {
    
    ERR_HANDLER_ASSERT("failed!!!\r\n"); 
  }
  
  /*## Wait for the end of the transfer ###################################*/   
  while (Uart6TxReady != SET)
  {
  } 
   /* Reset transmission flag */
  Uart6TxReady = RESET;  


}

size_t Uart6_RcvMsg(uint8_t **pReadBuff)
{
    size_t len = 0;
    if(pReadBuff == NULL ) return 0;
    while (Uart6RxReady == SET)
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


void Uart6_MspInit(void)
{  
  GPIO_InitTypeDef  GPIO_InitStruct;
  
  /*##-1- Enable peripherals and GPIO Clocks #################################*/
  /* Enable GPIO TX/RX clock */
  UART6_TX_GPIO_CLK_ENABLE();
  UART6_RX_GPIO_CLK_ENABLE();
  /* Enable clock */
  UART6_CLK_ENABLE(); 
  
  /*##-2- Configure peripheral GPIO ##########################################*/  
  /* UART TX GPIO pin configuration  */
  GPIO_InitStruct.Pin       = UART6_TX_PIN;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FAST;
  GPIO_InitStruct.Alternate = UART6_TX_AF;
  
  HAL_GPIO_Init(UART6_TX_GPIO_PORT, &GPIO_InitStruct);
    
  /* UART RX GPIO pin configuration  */
  GPIO_InitStruct.Pin = UART6_RX_PIN;
  GPIO_InitStruct.Alternate = UART6_RX_AF;
    
  HAL_GPIO_Init(UART6_RX_GPIO_PORT, &GPIO_InitStruct);

#if 0    
  /* UART RTS GPIO pin configuration  */
  GPIO_InitStruct.Pin = UART6_RTS_PIN;
  GPIO_InitStruct.Alternate = UART6_RTS_AF;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FAST;    
  HAL_GPIO_Init(UART6_RTS_GPIO_PORT, &GPIO_InitStruct);


  /* UART CTS GPIO pin configuration  */
  GPIO_InitStruct.Pin       = UART6_CTS_PIN;
  GPIO_InitStruct.Alternate = UART6_CTS_AF;
  
  HAL_GPIO_Init(UART6_CTS_GPIO_PORT, &GPIO_InitStruct);
#endif
  
/* NVIC for USART1 */
  HAL_NVIC_SetPriority(USART6_IRQn, 0, 1);
  HAL_NVIC_EnableIRQ(USART6_IRQn);
  
}
void UART6_StartRecv(void){
  HAL_UART_Receive_IT(&Uart6Handle, (uint8_t *)aRxBuffer, 1);    
  //TRACE_DEBUG("aRxBuffer = %s", aRxBuffer);
}

void Uart6_MspDeInit(void)
{
  /*##-1- Reset peripherals ##################################################*/
  UART6_FORCE_RESET();
  UART6_RELEASE_RESET();

  /*##-2- Disable peripherals and GPIO Clocks #################################*/
  /* Configure UART Tx as alternate function  */
  HAL_GPIO_DeInit(UART6_TX_GPIO_PORT, UART6_TX_PIN);
  /* Configure UART Rx as alternate function  */
  HAL_GPIO_DeInit(UART6_RX_GPIO_PORT, UART6_RX_PIN);
  
  /*##-3- Disable the NVIC for UART ##########################################*/
  HAL_NVIC_DisableIRQ(USART6_IRQn);
}  

/**
  * @brief  Tx Transfer completed callback
  * @param  Uart6Handle: UART handle. 
  * @note   This example shows a simple way to report end of IT Tx transfer, and 
  *         you can add your own implementation. 
  * @retval None
  */
void HAL_UART6_TxCpltCallback(UART_HandleTypeDef *Uart6Handle)
{
  /* Set transmission flag: transfer complete */
  Uart6TxReady = SET;
}

/**
  * @brief  Rx Transfer completed callback
  * @param  Uart6Handle: UART handle
  * @note   This example shows a simple way to report end of IT Rx transfer, and 
  *         you can add your own implementation.
  * @retval None
  */
void HAL_UART6_RxCpltCallback(UART_HandleTypeDef *Uart6Handle)
{
   int rc;
  /* Set transmission flag: transfer complete */
  Uart6RxReady = SET;
  if(bufferPoint >= RXBUFFERSIZE - 1)
      bufferPoint = 0;
  else
      bufferPoint ++;
  unReadBytes ++;
  HAL_UART_Receive_IT(Uart6Handle, (uint8_t *)aRxBuffer + bufferPoint, 1);
  if(unReadBytes > RXBUFFERSIZE)
      unReadBytes = RXBUFFERSIZE;
  Uart6RxReady = RESET;
}


/**
  * @brief  This function handles UART interrupt request.  
  * @param  None
  * @retval None
  * @Note   This function is redefined in "main.h" and related to DMA stream 
  *         used for USART data transmission     
  */
void USART6_IRQHandler(void)
{
  HAL_UART_IRQHandler(& Uart6Handle);
}



