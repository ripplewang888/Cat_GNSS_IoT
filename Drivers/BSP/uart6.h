#ifndef __UART6_H
#define __UART6_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
   
#define UART6_RXBUFFERSIZE        1024

/* Definition for UART6 clock resources */
#define UART6_CLK_ENABLE()              __HAL_RCC_USART6_CLK_ENABLE();
#define UART6_RX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOC_CLK_ENABLE()
#define UART6_TX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOG_CLK_ENABLE() 

#define UART6_FORCE_RESET()             __HAL_RCC_USART6_FORCE_RESET()
#define UART6_RELEASE_RESET()           __HAL_RCC_USART6_RELEASE_RESET()

/* Definition for UART6 Pins */
#define UART6_RTS_PIN                   GPIO_PIN_12
#define UART6_RTS_GPIO_PORT             GPIOG  
#define UART6_RTS_AF                    GPIO_AF8_USART6
#define UART6_CTS_PIN                   GPIO_PIN_13
#define UART6_CTS_GPIO_PORT             GPIOG  
#define UART6_CTS_AF                    GPIO_AF8_USART6
#define UART6_TX_PIN                    GPIO_PIN_14
#define UART6_TX_GPIO_PORT              GPIOG  
#define UART6_TX_AF                     GPIO_AF8_USART6
#define UART6_RX_PIN                    GPIO_PIN_7
#define UART6_RX_GPIO_PORT              GPIOC 
#define UART6_RX_AF                     GPIO_AF8_USART6


/* Exported functions ------------------------------------------------------- */
void Uart6_config(void);
void Uart6_SendMsg(uint8_t*, size_t);
size_t Uart6_RcvMsg(uint8_t **);
void UART6_StartRecv(void);
void Uart6_MspInit(void);
void Uart6_MspDeInit(void);
void HAL_UART6_TxCpltCallback(UART_HandleTypeDef *Uart6Handle);
void HAL_UART6_RxCpltCallback(UART_HandleTypeDef *Uart6Handle);

#endif /* __UART6_H */

