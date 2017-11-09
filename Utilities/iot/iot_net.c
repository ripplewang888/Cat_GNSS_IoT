#include <string.h>
#include "iot_net.h"
#include "iot_platform.h"
#include "trace.h"


/*** TCP connection ***/
int read_tcp(iot_network_pt pNetwork, char *buffer, uint32_t len, uint32_t timeout_ms)
{
    return iot_platform_tcp_read(pNetwork->handle, (uint8_t *)buffer, len, timeout_ms);
}

static int write_tcp(iot_network_pt pNetwork, const char *buffer, uint32_t len, uint32_t timeout_ms)
{
    return iot_platform_tcp_write(pNetwork->handle, (uint8_t *)buffer, len, timeout_ms);
}

static int disconnect_tcp(iot_network_pt pNetwork)
{
    if ( 0 == pNetwork->handle) {
        return -1;
    }

    iot_platform_tcp_destroy(pNetwork->handle);
    pNetwork->handle = 0;
    return 0;
}


static int connect_tcp(iot_network_pt pNetwork)
{
    if (NULL == pNetwork) {
        TRACE_ERROR("network is null");
        return 1;
    }

    pNetwork->handle = iot_platform_tcp_establish(pNetwork->pHostAddress, pNetwork->port);
    if ( 0 == pNetwork->handle ) {
        return -1;
    }

    return 0;
}


/*** SSL connection ***/

static int read_ssl(iot_network_pt pNetwork, char *buffer, uint32_t len, uint32_t timeout_ms)
{
    if (NULL == pNetwork) {
        TRACE_ERROR("network is null");
        return -1;
    }

    return iot_platform_ssl_read(pNetwork->handle, buffer, len, timeout_ms);
}

static int write_ssl(iot_network_pt pNetwork, const char *buffer, uint32_t len, uint32_t timeout_ms)
{
    if (NULL == pNetwork) {
        TRACE_ERROR("network is null");
        return -1;
    }

    return iot_platform_ssl_write(pNetwork->handle, buffer, len, timeout_ms);
}

static int disconnect_ssl(iot_network_pt pNetwork)
{
    if (NULL == pNetwork) {
        TRACE_ERROR("network is null");
        return -1;
    }

    iot_platform_ssl_destroy(pNetwork->handle);
    pNetwork->handle = 0;

    return 0;
}

static int connect_ssl(iot_network_pt pNetwork)
{
    if (NULL == pNetwork) {
        TRACE_ERROR("network is null");
        return 1;
    }

    if (0 != (pNetwork->handle = (intptr_t)iot_platform_ssl_establish(
                                            pNetwork->pHostAddress,
                                            pNetwork->port,
                                            pNetwork->ca_crt,
                                            pNetwork->ca_crt_len + 1))) {
        return 0;
    } else {
        //TODO SHOLUD not remove this handle space
        // The space will be freed by calling disconnect_ssl()
        //aliot_memory_free((void *)pNetwork->handle);
        return -1;
    }
}



/****** network interface ******/

int iot_net_read(iot_network_pt pNetwork, char *buffer, uint32_t len, uint32_t timeout_ms)
{
    if (NULL == pNetwork->ca_crt) { //TCP connection
        return read_tcp(pNetwork, buffer, len, timeout_ms);
    } else { //SSL connection
        return read_ssl(pNetwork, buffer, len, timeout_ms);
    }
}


int iot_net_write(iot_network_pt pNetwork, const uint8_t *buffer, uint32_t len, uint32_t timeout_ms)
    
{
    if (NULL == pNetwork->ca_crt) { //TCP connection
        return write_tcp(pNetwork, buffer, len, timeout_ms);
    } else { //SSL connection
        return write_ssl(pNetwork, buffer, len, timeout_ms);
    }
}


int iot_net_disconnect(iot_network_pt pNetwork)
{
    if (NULL == pNetwork->ca_crt) { //TCP connection
        return disconnect_tcp(pNetwork);
    } else { //SSL connection
        return disconnect_ssl(pNetwork);
    }
}


int iot_net_connect(iot_network_pt pNetwork)
{
    if (NULL == pNetwork->ca_crt) { //TCP connection
        return connect_tcp(pNetwork);
    } else { //SSL connection
        return connect_ssl(pNetwork);
    }
}


int iot_net_init(iot_network_pt pNetwork, const char *host, uint16_t port, const char *ca_crt)
{
    pNetwork->pHostAddress = host;
    pNetwork->port = port;
    pNetwork->ca_crt = ca_crt;
    if (NULL == ca_crt) {
        pNetwork->ca_crt_len = 0;
    } else {
        pNetwork->ca_crt_len = strlen(ca_crt);
    }

    pNetwork->handle = 0;
    pNetwork->read = iot_net_read;
    pNetwork->write = iot_net_write;
    pNetwork->disconnect = iot_net_disconnect;
    pNetwork->connect = iot_net_connect;

    return 0;
}
