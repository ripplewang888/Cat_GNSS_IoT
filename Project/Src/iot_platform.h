
#ifndef _IOT_PLATFORM_H_
#define _IOT_PLATFORM_H_
#include "iot_net.h"

void *iot_platform_mutex_create(void);
void iot_platform_mutex_destroy(void *mutex);
void iot_platform_mutex_lock(void *mutex);
void iot_platform_mutex_unlock(void *mutex);
void *iot_platform_malloc(uint32_t size);
void iot_platform_free(void *ptr);
uint32_t iot_platform_time_get_ms(void);
void iot_platform_msleep(uint32_t ms);

int iot_platform_tcp_read(uintptr_t fd, uint8_t* buffer, int len, int timeout_ms);
int iot_platform_tcp_write(uintptr_t fd, uint8_t* buffer, int len, int timeout_ms);
uintptr_t iot_platform_tcp_establish(const char *host, uint16_t port);
void iot_platform_tcp_destroy(uintptr_t fd);
int iot_platform_ssl_read(uintptr_t handle, char *buf, int len, int timeout_ms);
int iot_platform_ssl_write(uintptr_t handle, const char *buf, int len, int timeout_ms);
int32_t iot_platform_ssl_destroy(uintptr_t handle);
uintptr_t iot_platform_ssl_establish(const char *host,
                uint16_t port,
                const char *ca_crt,
                size_t ca_crt_len);

#endif /* _IOT_PLATFORM_H_ */
