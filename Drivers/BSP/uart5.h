#ifndef __UART5_H
#define __UART5_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "nmealib.h"
   
#define UART5_RXBUFFERSIZE        512

/* Definition for UART5 clock resources */
#define UART5_CLK_ENABLE()              __HAL_RCC_UART5_CLK_ENABLE();
#define UART5_RX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOD_CLK_ENABLE()
#define UART5_TX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOC_CLK_ENABLE() 

#define UART5_FORCE_RESET()             __HAL_RCC_UART5_FORCE_RESET()
#define UART5_RELEASE_RESET()           __HAL_RCC_UART5_RELEASE_RESET()

/* Definition for UART5 Pins */
#define UART5_TX_PIN                    GPIO_PIN_12
#define UART5_TX_GPIO_PORT              GPIOC  
#define UART5_TX_AF                     GPIO_AF8_UART5
#define UART5_RX_PIN                    GPIO_PIN_2
#define UART5_RX_GPIO_PORT              GPIOD 
#define UART5_RX_AF                     GPIO_AF8_UART5


/* Exported functions ------------------------------------------------------- */
void Uart5_config(void);
void Uart5_SendMsg(uint8_t* , size_t);
size_t Uart5_RcvMsg(uint8_t **);
void Uart5_MspInit(void);
void Uart5_MspDeInit(void);
extern void HAL_UART5_TxCpltCallback(UART_HandleTypeDef *);
extern void HAL_UART5_RxCpltCallback(UART_HandleTypeDef *);
void UART5_StartRecv(void);
#endif /* __UART5_H */

