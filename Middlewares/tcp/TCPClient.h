/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander/Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/

#ifndef _IOT_TCP_CLIENT_H_
#define _IOT_TCP_CLIENT_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iot_platform.h"
#include "iot_timer.h"


/* maximum number of successful subscribe */
#define TCP_SUB_NUM_MAX                     (10)

/* maximum republish elements in list */
#define TCP_REPUB_NUM_MAX                   (20)



typedef enum {

    /* Undefined event */
    IOT_TCP_EVENT_UNDEF = 0,

    /* TCP disconnect event */
    IOT_TCP_EVENT_DISCONNECT = 1,

    /* TCP reconnect event */
    IOT_TCP_EVENT_RECONNECT = 2,


    /* A ACK to the specific publish which specify by packet-id be received */
    IOT_TCP_EVENT_PUBLISH_SUCCESS = 3,

    /* No ACK to the specific publish which specify by packet-id be received in timeout period */
    IOT_TCP_EVENT_PUBLISH_TIMEOUT = 4,


    /* TCP packet published from TCP remote broker be received */
    IOT_TCP_EVENT_PUBLISH_RECVEIVED = 5,

} iot_TCP_event_type_t;


/* topic information */
typedef struct {
    uint16_t packet_id;
    uint8_t qos;
    uint8_t dup;
    uint8_t retain;
    uint16_t topic_len;
    uint16_t payload_len;
    const char *ptopic;
    const char *payload;
} iot_TCP_topic_info_t, *iot_TCP_topic_info_pt;





/* TCP client version number */
#define TCP_TCP_VERSION                    (4)

/* maximum length of topic name in byte */
#define TCP_TOPIC_NAME_MAX_LEN              (64)

/* maximum TCP packet-id */
#define TCP_PACKET_ID_MAX                   (65535)

/* maximum number of simultaneously invoke subscribe request */
#define TCP_SUB_REQUEST_NUM_MAX             (10)

/* Minimum interval of TCP reconnect in millisecond */
#define TCP_RECONNECT_INTERVAL_MIN_MS       (1000)

/* Maximum interval of TCP reconnect in millisecond */
#define TCP_RECONNECT_INTERVAL_MAX_MS       (60000)

/* Minimum timeout interval of TCP request in millisecond */
#define TCP_REQUEST_TIMEOUT_MIN_MS          (500)

/* Maximum timeout interval of TCP request in millisecond */
#define TCP_REQUEST_TIMEOUT_MAX_MS          (5000)

/* Default timeout interval of TCP request in millisecond */
#define TCP_REQUEST_TIMEOUT_DEFAULT_MS      (2000)


typedef enum {
    TCP_CONNECTION_ACCEPTED = 0,
    TCP_CONNECTION_REFUSED_UNACCEPTABLE_PROTOCOL_VERSION = 1,
    TCP_CONNECTION_REFUSED_IDENTIFIER_REJECTED = 2,
    TCP_CONNECTION_REFUSED_SERVER_UNAVAILABLE = 3,
    TCP_CONNECTION_REFUSED_BAD_USERDATA = 4,
    TCP_CONNECTION_REFUSED_NOT_AUTHORIZED = 5
} TCP_connect_ack_code_t;


/* State of TCP client */
typedef enum {
    TCP_STATE_INVALID = 0,                    //TCP in invalid state
    TCP_STATE_INITIALIZED = 1,                //TCP in initializing state
    TCP_STATE_CONNECTED = 2,                  //TCP in connected state
    TCP_STATE_DISCONNECTED = 3,               //TCP in disconnected state
    TCP_STATE_DISCONNECTED_RECONNECTING = 4,  //TCP in reconnecting state
} TCP_state_t;


typedef enum TCP_NODE_STATE {
    TCP_NODE_STATE_NORMANL = 0,
    TCP_NODE_STATE_INVALID,
} TCP_node_t;


/* Information structure of published topic */
typedef struct REPUBLISH_INFO {
    iot_timer_t      pub_start_time;   //start time of publish request
    TCP_node_t        node_state;       //state of this node
    uint16_t          msg_id;           //packet id of publish
    uint32_t          len;              //length of publish message
    unsigned char    *buf;              //publish message
} TCP_pub_info_t, *TCP_pub_info_pt;


/* Reconnected parameter of TCP client */
typedef struct {
    iot_timer_t       reconnect_next_time;         //the next time point of reconnect
    uint32_t           reconnect_time_interval_ms;  //time interval of this reconnect
} TCP_reconnect_param_t;


/* structure of TCP client */
typedef struct Client {
    uint8_t                        *port;                   /* Specify TCP broker port */
    uint8_t                       * host;                   /* Specify TCP broker host */
    uint32_t                        buf_size_read;                           // bytes read from server
    uint32_t                        buf_size_write;                           //bytes send to server
    uint8_t                        *buf_send;                                //pointer of send buffer
    uint8_t                        *buf_read;                                //pointer of read buffer
    iot_network_pt               ipstack;                                 //network parameter
    TCP_state_t                   client_state;                            //state of TCP client
    TCP_reconnect_param_t           reconnect_param;                         //reconnect parameter
    void *                           uart_locker;                          // uart locker, for read and write, can not send AT command at the same time
}TCP_client_t, *TCP_client_pt;


 TCP_state_t TCP_get_client_state(TCP_client_t *pClient);
 void TCP_set_client_state(TCP_client_t *pClient, TCP_state_t newState);
 bool TCP_check_state_normal(TCP_client_t *c);


typedef enum {
    TOPIC_NAME_TYPE = 0,
    TOPIC_FILTER_TYPE
} TCP_topic_type_t;



/* The structure of TCP initial parameter */
typedef struct {

    uint8_t                  *port;                   /* Specify TCP broker port */
    uint8_t                 *host;                   /* Specify TCP broker host */

    /* Specify TCP transport channel and key.
     * If the value is NULL, it means that use TCP channel,
     * If the value is NOT NULL, it means that use SSL/TLS channel and
     *   @pub_key point to the CA certification */
    const char                 *pub_key;

    char                       *pwrite_buf;               /* Specify write-buffer */
    uint32_t                    write_buf_size;           /* Specify size of write-buffer in byte */
    char                       *pread_buf;                /* Specify read-buffer */
    uint32_t                    read_buf_size;            /* Specify size of read-buffer in byte */

} iot_TCP_param_t, *iot_TCP_param_pt;



/**
 * @brief Deconstruct the TCP client
 *        This function disconnect TCP connection and release the related resource.
 *
 * @param handle, specify the TCP client.
 *
 * @return 0, deconstruct success; -1, deconstruct failed.
 */





/**
 * @brief check whether TCP connection is established or not.
 *
 * @param handle, specify the TCP client.
 *
 * @return true, TCP in normal state; false, TCP in abnormal state.
 */
bool iot_TCP_check_state_normal(void *handle);



void delay(unsigned int nDelay);
int  iot_tcp_client_transparent_mode(iot_TCP_param_t *pInitParams);




#if defined(__cplusplus)
}
#endif

#endif
