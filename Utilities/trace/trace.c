#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "trace.h"
#include "uart4.h"

trace_level_t g_iotLogLevel = TRACE_LEVEL_DEBUG;


trace_err_t trace_error_table[]={
  {HAL_OK, "HAL_OK"},
  {HAL_ERROR, "HAL_ERROR"},
  {HAL_BUSY, "HAL_BUSY"},
  {HAL_TIMEOUT, "HAL_TIMEOUT"}
};


void set_trace_level(trace_level_t iotLogLevel)
{
    g_iotLogLevel = iotLogLevel;
}

trace_level_t get_trace_level()
{
    return g_iotLogLevel;
}

extern __IO ITStatus Uart4TxReady;
extern __IO ITStatus Uart4RxReady;
extern UART_HandleTypeDef Uart4Handle;

int fputc(int ch, FILE *f)
{
  while (Uart4TxReady == SET)
  {
  } 
  HAL_UART_Transmit_IT(&Uart4Handle, (uint8_t*)&ch, 1);
  /*## Wait for the end of the transfer ###################################*/   
  while (Uart4TxReady != SET)
  {
  } 
   /* Reset transmission flag */
  Uart4TxReady = RESET;
  return (ch);
}


int fgetc(FILE * f)
{
    uint8_t  ch;
    
    HAL_UART_Receive_IT(&Uart4Handle, &ch, 1);    	
    while (Uart4RxReady != SET)
    {
    } 
    printf("%c",ch);  // send to tx
    Uart4RxReady = RESET;	
    return ch;
    
}



