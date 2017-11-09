#ifndef _TRACE_H_
#define _TRACE_H_

#include <stdio.h>
#include <stdlib.h>
#include "stm32f4xx_hal.h"

typedef enum IOT_LOG_LEVEL {
    TRACE_LEVEL_DEBUG = 0,
    TRACE_LEVEL_INFO,
    TRACE_LEVEL_WARN,
    TRACE_LEVEL_ERROR,
    TRACE_LEVEL_FATAL,
    TRACE_LEVEL_NONE,
} trace_level_t;

typedef struct
{
  HAL_StatusTypeDef status;
  char* errstr;
}trace_err_t;

extern trace_level_t g_iotLogLevel;
extern trace_err_t trace_error_table[];


void set_trace_level(trace_level_t iotLogLevel);


#define TRACE_DEBUG(format, ...) \
    {\
        if(g_iotLogLevel <= TRACE_LEVEL_DEBUG)\
        {\
            printf("[debug] %s:%d %s()| "format"\r\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);\
            fflush(stdout);\
        }\
    }

#define TRACE_INFO(format, ...) \
    {\
        if(g_iotLogLevel <= TRACE_LEVEL_INFO)\
        {\
            printf("[info] | "format"\r\n", ##__VA_ARGS__);\
            fflush(stdout);\
        }\
    }

#define TRACE_WARN(format, ...) \
    {\
        if(g_iotLogLevel <= TRACE_LEVEL_WARN)\
        {\
            printf("[warn] %s:%d %s()| "format"\r\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);\
            fflush(stdout);\
        }\
    }

#define TRACE_ERROR(format,...) \
    {\
        if(g_iotLogLevel <= TRACE_LEVEL_ERROR)\
        {\
            printf("[error] %s:%d %s()| "format"\r\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);\
            fflush(stdout);\
        }\
    }

#define TRACE_FATAL(format, ...) \
    {\
        if(g_iotLogLevel <= TRACE_LEVEL_FATAL)\
        {\
            printf("[notice] %s:%d %s()| "format"\r\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);\
            fflush(stdout);\
        }\
    }

#endif
