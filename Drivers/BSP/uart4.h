#ifndef __UART4_H
#define __UART4_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
   
#define UART4_RXBUFFERSIZE        512

/* Definition for UART4 clock resources */
#define UART4_CLK_ENABLE()              __HAL_RCC_UART4_CLK_ENABLE();
#define UART4_RX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOC_CLK_ENABLE()
#define UART4_TX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOC_CLK_ENABLE() 

#define UART4_FORCE_RESET()             __HAL_RCC_UART4_FORCE_RESET()
#define UART4_RELEASE_RESET()           __HAL_RCC_UART4_RELEASE_RESET()

/* Definition for UART4 Pins */
#define UART4_TX_PIN                    GPIO_PIN_10
#define UART4_TX_GPIO_PORT              GPIOC  
#define UART4_TX_AF                     GPIO_AF8_UART4
#define UART4_RX_PIN                    GPIO_PIN_11
#define UART4_RX_GPIO_PORT              GPIOC 
#define UART4_RX_AF                     GPIO_AF8_UART4


/* Exported functions ------------------------------------------------------- */
void Uart4_config(void);
void Uart4_SendMsg(uint8_t* , size_t );
size_t Uart4_RcvMsg(uint8_t **);
void Uart4_MspInit(void);
void Uart4_MspDeInit(void);
void HAL_UART4_TxCpltCallback(UART_HandleTypeDef *);
void HAL_UART4_RxCpltCallback(UART_HandleTypeDef *);
void UART4_StartRecv(void);
#endif /* __UART4_H */

