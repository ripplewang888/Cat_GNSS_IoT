#include "uart5.h"
#include "error_handler.h"
#include <string.h>

/* Private macro -------------------------------------------------------------*/
/* Size of Reception buffer */
#define RXBUFFERSIZE                    UART5_RXBUFFERSIZE

UART_HandleTypeDef Uart5Handle;
__IO ITStatus Uart5RxReady = RESET;
__IO ITStatus Uart5TxReady = RESET;


/* Buffer used for reception */
static uint8_t aRxBuffer[RXBUFFERSIZE+1];
static uint32_t bufferPoint;
static uint32_t unReadBytes;

/* Get Uart Message Line, Parse the GGA information*/
uint8_t aRxMessageLine[64];
uint8_t aRxGGALine[64];
uint32_t pRxMessageLineStartPoint = 0;
data_store_t GPS_data;
data_store_t tmp_GPS_data;
uint32_t resolved_GPS_data_count = 0;




/*##-1- Configure the UART peripheral ######################################*/
/* Put the USART peripheral in the Asynchronous mode (UART Mode) */
/* UART1 configured as follow:
    - Word Length = 8 Bits
    - Stop Bit = One Stop bit
    - Parity = None
    - BaudRate = 9600 baud
    - Hardware flow control disabled (RTS and CTS signals) */
void Uart5_config(void)
{
  Uart5Handle.Instance          = UART5;

  Uart5Handle.Init.BaudRate     = 9600;
  Uart5Handle.Init.WordLength   = UART_WORDLENGTH_8B;
  Uart5Handle.Init.StopBits     = UART_STOPBITS_1;
  Uart5Handle.Init.Parity       = UART_PARITY_NONE;
  Uart5Handle.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  Uart5Handle.Init.Mode         = UART_MODE_TX_RX;
  Uart5Handle.Init.OverSampling = UART_OVERSAMPLING_16;
    
  if(HAL_UART_Init(&Uart5Handle) != HAL_OK)
  {
    ERR_HANDLER_ASSERT("failed!!!\r\n");
  }
  bufferPoint = 0;
  aRxBuffer[RXBUFFERSIZE] = '\0';
  unReadBytes=0;
  
}

void Uart5_SendMsg(uint8_t* sendBuff, size_t size)
{
  /*##-2- Start the transmission process #####################################*/  

  while (Uart5TxReady == SET)
  {
  } 
  if(HAL_UART_Transmit_IT(&Uart5Handle, sendBuff, size)!= HAL_OK)
  {
    ERR_HANDLER_ASSERT("failed!!!\r\n");
  }
  /*## Wait for the end of the transfer ###################################*/   
  while (Uart5TxReady != SET)
  {
  } 
   /* Reset transmission flag */
  Uart5TxReady = RESET;  


}

size_t Uart5_RcvMsg(uint8_t **pReadBuff)
{
    size_t len = 0;
    if(pReadBuff == NULL ) return 0;
    while (Uart5RxReady == SET)
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

void Uart5_MspInit(void)
{  
  GPIO_InitTypeDef  GPIO_InitStruct;
  
  /*##-1- Enable peripherals and GPIO Clocks #################################*/
  /* Enable GPIO TX/RX clock */
  UART5_TX_GPIO_CLK_ENABLE();
  UART5_RX_GPIO_CLK_ENABLE();
  /* Enable clock */
  UART5_CLK_ENABLE(); 
  
  /*##-2- Configure peripheral GPIO ##########################################*/  
  /* UART TX GPIO pin configuration  */
  GPIO_InitStruct.Pin       = UART5_TX_PIN;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FAST;
  GPIO_InitStruct.Alternate = UART5_TX_AF;
  
  HAL_GPIO_Init(UART5_TX_GPIO_PORT, &GPIO_InitStruct);
    
  /* UART RX GPIO pin configuration  */
  GPIO_InitStruct.Pin = UART5_RX_PIN;
  GPIO_InitStruct.Alternate = UART5_RX_AF;
    
  HAL_GPIO_Init(UART5_RX_GPIO_PORT, &GPIO_InitStruct);

  /*##-3- Configure the NVIC for UART ########################################*/
  /* NVIC for USART1 */
  HAL_NVIC_SetPriority(UART5_IRQn, 0, 1);
  HAL_NVIC_EnableIRQ(UART5_IRQn);
  
}

void UART5_StartRecv(void){
  HAL_UART_Receive_IT(&Uart5Handle, (uint8_t *)aRxBuffer, 1);    
}

void Uart5_MspDeInit(void)
{
  /*##-1- Reset peripherals ##################################################*/
  UART5_FORCE_RESET();
  UART5_RELEASE_RESET();

  /*##-2- Disable peripherals and GPIO Clocks #################################*/
  /* Configure UART Tx as alternate function  */
  HAL_GPIO_DeInit(UART5_TX_GPIO_PORT, UART5_TX_PIN);
  /* Configure UART Rx as alternate function  */
  HAL_GPIO_DeInit(UART5_RX_GPIO_PORT, UART5_RX_PIN);

  
  /*##-3- Disable the NVIC for UART ##########################################*/
  HAL_NVIC_DisableIRQ(UART5_IRQn);
}  

/**
  * @brief  Tx Transfer completed callback
  * @param  Uart5Handle: UART handle. 
  * @note   This example shows a simple way to report end of IT Tx transfer, and 
  *         you can add your own implementation. 
  * @retval None
  */
void HAL_UART5_TxCpltCallback(UART_HandleTypeDef *Uart5Handle)
{
  /* Set transmission flag: transfer complete */
  Uart5TxReady = SET;
}

/**
  * @brief  Rx Transfer completed callback
  * @param  Uart5Handle: UART handle
  * @note   This example shows a simple way to report end of IT Rx transfer, and 
  *         you can add your own implementation.
  * @retval None
  */
void HAL_UART5_RxCpltCallback(UART_HandleTypeDef *Uart5Handle)
{
    
  /* Set transmission flag: transfer complete */
  Uart5RxReady = SET;
    
  /*Rx Message Line Process*/ 
  if( *((uint8_t *)aRxBuffer + bufferPoint) == 0x0d || *((uint8_t *)aRxBuffer + bufferPoint) == 0x0a){
      //*((uint8_t *)aRxBuffer + bufferPoint) = 0x00;
      if(bufferPoint > pRxMessageLineStartPoint){
          memcpy(aRxMessageLine , (uint8_t *)aRxBuffer + pRxMessageLineStartPoint, bufferPoint - pRxMessageLineStartPoint + 1);
          aRxMessageLine[bufferPoint - pRxMessageLineStartPoint] = 0x00;
      }else if(bufferPoint == pRxMessageLineStartPoint){
          pRxMessageLineStartPoint = bufferPoint;
      }else{
          memcpy(aRxMessageLine, (uint8_t *)aRxBuffer + pRxMessageLineStartPoint, RXBUFFERSIZE - pRxMessageLineStartPoint);
          memcpy(aRxMessageLine + (RXBUFFERSIZE - pRxMessageLineStartPoint), aRxBuffer, bufferPoint + 1);
          aRxMessageLine[RXBUFFERSIZE - pRxMessageLineStartPoint + bufferPoint] = 0x00;
      }
      if(bufferPoint >= RXBUFFERSIZE - 1){
          pRxMessageLineStartPoint = 0;
      }else{
          pRxMessageLineStartPoint = bufferPoint + 1;
      }
      
      /*NMEA GGA parse*/
      memset(&tmp_GPS_data, 0x00, sizeof(data_store_t));
      if(nmea_parse_GGA((u8 *)aRxMessageLine, 64, false, (u8 *)&tmp_GPS_data) == true){
          memcpy(&GPS_data, &tmp_GPS_data, sizeof(data_store_t));
          resolved_GPS_data_count ++;
          memcpy(aRxGGALine, aRxMessageLine, sizeof(aRxMessageLine));
      }   
      
  }    
    
    
  if(bufferPoint >= RXBUFFERSIZE - 1)
      bufferPoint = 0;
  else
      bufferPoint ++;
  unReadBytes ++;
  HAL_UART_Receive_IT(Uart5Handle, (uint8_t *)aRxBuffer + bufferPoint, 1);
  if(unReadBytes > RXBUFFERSIZE)
      unReadBytes = RXBUFFERSIZE;
  Uart5RxReady = RESET;
}


/**
  * @brief  This function handles UART interrupt request.  
  * @param  None
  * @retval None
  * @Note   This function is redefined in "main.h" and related to DMA stream 
  *         used for USART data transmission     
  */
void UART5_IRQHandler(void)
{
  HAL_UART_IRQHandler(& Uart5Handle);
}



