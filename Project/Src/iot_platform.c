#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "iot_platform.h"
#include "uart6.h"
#include "trace.h"
#include "lm61.h"
#include "cmsis_os.h"
#include "error_handler.h"

#define IOT_MIN(x,y)  ((x) < (y) ? (x) : (y))

typedef struct
{
	uint32_t size;
	uint32_t head;
	uint32_t tail;
	uint8_t *buffer;
} iot_ring_buffer_t;

#define MQTT_RING_BUFFER_LEN 1024
static iot_ring_buffer_t  mqtt_ring_buffer;
static uint8_t mqtt_ring_data[MQTT_RING_BUFFER_LEN] = {0};

void *iot_platform_mutex_create(void)
{
  return xSemaphoreCreateMutex();
}

void iot_platform_mutex_destroy(void *mutex)
{
  (void) vSemaphoreDelete(mutex);
}

void iot_platform_mutex_lock(void *mutex)
{
  if( xSemaphoreTake( mutex, portMAX_DELAY ) != pdTRUE )
  {
    ERR_HANDLER_ASSERT("xSemaphoreTake failed!!!");
  }
}

void iot_platform_mutex_unlock(void *mutex)
{
  if( xSemaphoreGive( mutex ) != pdTRUE )
  {
    ERR_HANDLER_ASSERT("xSemaphoreGive failed!!!");
  }

}

void *iot_platform_malloc(uint32_t size)
{
    return malloc(size);
}

void iot_platform_free(void *ptr)
{
    free(ptr);
}

uint32_t iot_platform_time_get_ms(void)
{
  return osKernelSysTick();
}

void iot_platform_msleep(uint32_t ms)
{
  osDelay(ms);
}

/*
	Initializes the size of the ring buffer
*/
void iot_ring_buffer_init( iot_ring_buffer_t *ring_buffer, uint8_t *buffer, uint32_t size )
{
    ring_buffer->buffer     = buffer;
    ring_buffer->size       = size;
    ring_buffer->head       = 0;
    ring_buffer->tail       = 0;
}

/*
	the size of ring buffer is already in use
*/
uint32_t iot_ring_buffer_used_space( iot_ring_buffer_t *ring_buffer )
{
	uint32_t head_to_end = ring_buffer->size - ring_buffer->head;
	return ((head_to_end + ring_buffer->tail) % ring_buffer->size);
}


/*
	read mqtt data from ring buffer
*/
uint8_t iot_ring_buffer_read(iot_ring_buffer_t *ring_buffer, uint8_t *data, uint32_t bytes_consume)
{
	uint32_t head_to_end = ring_buffer->size - ring_buffer->head;
	if(bytes_consume < head_to_end)
	{
		memcpy(data, &(ring_buffer->buffer[ring_buffer->head]), bytes_consume);
	}
	else
	{
		memcpy(data, &(ring_buffer->buffer[ring_buffer->head]), head_to_end);
		memcpy((data + head_to_end), ring_buffer->buffer, (bytes_consume - head_to_end));
	}
	ring_buffer->head = (ring_buffer->head + bytes_consume) % ring_buffer->size;
	return 0;
}

/*
	store mqtt data into global ring buffer
*/
uint32_t iot_ring_buffer_write( iot_ring_buffer_t *ring_buffer, const uint8_t *data, uint32_t data_length )
{
	uint32_t tail_to_end = ring_buffer->size - ring_buffer->tail;

	/* Calculate the maximum amount we can copy */
	uint32_t amount_to_copy = IOT_MIN(data_length, (ring_buffer->tail == ring_buffer->head) ? ring_buffer->size : (tail_to_end + ring_buffer->head) % ring_buffer->size);

	/* Copy as much as we can until we fall off the end of the buffer */
	memcpy(&ring_buffer->buffer[ring_buffer->tail], data, IOT_MIN(amount_to_copy, tail_to_end));

	/* Check if we have more to copy to the front of the buffer */
	if (tail_to_end < amount_to_copy)
	{
		memcpy(ring_buffer->buffer, data + tail_to_end, amount_to_copy - tail_to_end);
	}

	/* Update the tail */
	ring_buffer->tail = (ring_buffer->tail + amount_to_copy) % ring_buffer->size;

	return amount_to_copy;
}

int iot_platform_tcp_read(uintptr_t fd, uint8_t* buffer, int len, int timeout_ms) 
{
  int rc = 0;
  uint8_t *cmdRecv = NULL;
  int socketReadLen;
  uint32_t dataLen, readLen;

  dataLen = iot_ring_buffer_used_space(&mqtt_ring_buffer);
  if(dataLen)
  {
    // read mqtt data from ringbuffer
    readLen = (dataLen > len) ? len : dataLen;
    iot_ring_buffer_read(&mqtt_ring_buffer, buffer, readLen);
    return (int)readLen;
  }

  if (CAT1_STATUS_SUCCESS == LM61_socketRead(&cmdRecv, &socketReadLen, timeout_ms)) {
    memcpy(mqtt_ring_data, cmdRecv, socketReadLen);
    iot_ring_buffer_init(&mqtt_ring_buffer, mqtt_ring_data, socketReadLen);
    iot_ring_buffer_read(&mqtt_ring_buffer, buffer, len);
    return len;
  }
  return rc;
}



int iot_platform_tcp_write(uintptr_t fd, uint8_t* buffer, int len, int timeout_ms) 
{
  LM61_socketWrite(buffer, len, timeout_ms);
  return len;
}

uintptr_t iot_platform_tcp_establish(const char *host, uint16_t port)
{
  int rc = 1;
  if (LM61_socketOpen((char *)host,port) != CAT1_STATUS_SUCCESS )  {
    ERR_HANDLER_ASSERT("CAT1 Init failed!!!\r\n");
  }  
  return (uintptr_t)rc;  
}

void iot_platform_tcp_destroy(uintptr_t fd) 
{
  LM61_socketClose();
}

int iot_platform_ssl_read(uintptr_t handle, char *buf, int len, int timeout_ms)
{
  return 0;
}

int iot_platform_ssl_write(uintptr_t handle, const char *buf, int len, int timeout_ms)
{
  return 0;
}

int32_t iot_platform_ssl_destroy(uintptr_t handle)
{
  return 0;
}

uintptr_t iot_platform_ssl_establish(const char *host,
                uint16_t port,
                const char *ca_crt,
                size_t ca_crt_len)
{
  return 0;
}

