
#ifndef _IOT_NET_H_
#define _IOT_NET_H_
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>


/**
 * @brief The structure of network connection(TCP or SSL).
 *   The user has to allocate memory for this structure.
 */

struct mqtt_network;
typedef struct mqtt_network iot_network_t, *iot_network_pt;

struct mqtt_network {
    const char *pHostAddress;
    uint16_t port;
    uint16_t ca_crt_len;

    /**< NULL, TCP connection; NOT NULL, SSL connection */
    const char *ca_crt;

    /**< connection handle: 0, NOT connection; NOT 0, handle of the connection */
    uintptr_t handle;

    /**< Read data from server function pointer. */
    int (*read)(iot_network_pt, char *, uint32_t, uint32_t);

    /**< Send data to server function pointer. */
    int (*write)(iot_network_pt, const uint8_t *, uint32_t, uint32_t);

    /**< Disconnect the network */
    int (*disconnect)(iot_network_pt);

    /**< Establish the network */
    int (*connect)(iot_network_pt);
};


int iot_net_read(iot_network_pt pNetwork, char *buffer, uint32_t len, uint32_t timeout_ms);
int iot_net_write(iot_network_pt pNetwork, const uint8_t *buffer, uint32_t len, uint32_t timeout_ms);
int iot_net_disconnect(iot_network_pt pNetwork);
int iot_net_connect(iot_network_pt pNetwork);
int iot_net_init(iot_network_pt pNetwork, const char *host, uint16_t port, const char *ca_crt);

#endif /* _IOT_NET_H_ */
