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

#include <stdlib.h>
#include <stddef.h>
#include "MQTTClient.h"
#include "MQTTPacket.h"
#include "iot_platform.h"
#include "iot_net.h"
#include "iot_list.h"
#include "iot_timer.h"
#include "trace.h"
#include "error_handler.h"

//MC, MQTT client


/* MQTT client version number */
#define MC_MQTT_VERSION                    (4)

/* maximum length of topic name in byte */
#define MC_TOPIC_NAME_MAX_LEN              (64)

/* maximum MQTT packet-id */
#define MC_PACKET_ID_MAX                   (65535)

/* maximum number of simultaneously invoke subscribe request */
#define MC_SUB_REQUEST_NUM_MAX             (10)

/* Minimum interval of MQTT reconnect in millisecond */
#define MC_RECONNECT_INTERVAL_MIN_MS       (1000)

/* Maximum interval of MQTT reconnect in millisecond */
#define MC_RECONNECT_INTERVAL_MAX_MS       (60000)

/* Minimum timeout interval of MQTT request in millisecond */
#define MC_REQUEST_TIMEOUT_MIN_MS          (500)

/* Maximum timeout interval of MQTT request in millisecond */
#define MC_REQUEST_TIMEOUT_MAX_MS          (5000)

/* Default timeout interval of MQTT request in millisecond */
#define MC_REQUEST_TIMEOUT_DEFAULT_MS      (2000)


typedef enum {
    MC_CONNECTION_ACCEPTED = 0,
    MC_CONNECTION_REFUSED_UNACCEPTABLE_PROTOCOL_VERSION = 1,
    MC_CONNECTION_REFUSED_IDENTIFIER_REJECTED = 2,
    MC_CONNECTION_REFUSED_SERVER_UNAVAILABLE = 3,
    MC_CONNECTION_REFUSED_BAD_USERDATA = 4,
    MC_CONNECTION_REFUSED_NOT_AUTHORIZED = 5
} MC_connect_ack_code_t;


/* State of MQTT client */
typedef enum {
    MC_STATE_INVALID = 0,                    //MQTT in invalid state
    MC_STATE_INITIALIZED = 1,                //MQTT in initializing state
    MC_STATE_CONNECTED = 2,                  //MQTT in connected state
    MC_STATE_DISCONNECTED = 3,               //MQTT in disconnected state
    MC_STATE_DISCONNECTED_RECONNECTING = 4,  //MQTT in reconnecting state
} MC_state_t;


typedef enum MQTT_NODE_STATE {
    MC_NODE_STATE_NORMANL = 0,
    MC_NODE_STATE_INVALID,
} MC_node_t;


/* Handle structure of subscribed topic */
typedef struct {
    const char *topic_filter;
    iot_mqtt_event_handle_t handle;
} MC_topic_handle_t;


/* Information structure of subscribed topic */
typedef struct SUBSCRIBE_INFO {
    enum msgTypes       type;           //type, (sub or unsub)
    uint16_t            msg_id;         //packet id of subscribe(unsubcribe)
    iot_timer_t        sub_start_time; //start time of subscribe request
    MC_node_t          node_state;     //state of this node
    MC_topic_handle_t  handler;        //handle of topic subscribed(unsubcribed)
    uint16_t            len;            //length of subscribe message
    unsigned char       *buf;           //subscribe message
}MC_subsribe_info_t, *MC_subsribe_info_pt;


/* Information structure of published topic */
typedef struct REPUBLISH_INFO {
    iot_timer_t      pub_start_time;   //start time of publish request
    MC_node_t        node_state;       //state of this node
    uint16_t          msg_id;           //packet id of publish
    uint32_t          len;              //length of publish message
    unsigned char    *buf;              //publish message
} MC_pub_info_t, *MC_pub_info_pt;


/* Reconnected parameter of MQTT client */
typedef struct {
    iot_timer_t       reconnect_next_time;         //the next time point of reconnect
    uint32_t           reconnect_time_interval_ms;  //time interval of this reconnect
} MC_reconnect_param_t;


/* structure of MQTT client */
typedef struct Client {
    void *                          lock_generic;                            //generic lock
    uint32_t                        packet_id;                               //packet id
    uint32_t                        request_timeout_ms;                      //request timeout in millisecond
    uint32_t                        buf_size_send;                           //send buffer size in byte
    uint32_t                        buf_size_read;                           //read buffer size in byte
    char                           *buf_send;                                //pointer of send buffer
    char                           *buf_read;                                //pointer of read buffer
    MC_topic_handle_t              sub_handle[MC_SUB_NUM_MAX];             //array of subscribe handle
    iot_network_pt                ipstack;                                 //network parameter
    iot_timer_t                    next_ping_time;                          //next ping time
    int                             ping_mark;                               //flag of ping
    MC_state_t                     client_state;                            //state of MQTT client
    MC_reconnect_param_t           reconnect_param;                         //reconnect parameter
    MQTTPacket_connectData          connect_data;                            //connection parameter
    list_t                         *list_pub_wait_ack;                       //list of wait publish ack
    list_t                         *list_sub_wait_ack;                       //list of subscribe or unsubscribe ack
    void *                          lock_list_pub;                           //lock of list of wait publish ack
    void *                          lock_list_sub;                           //lock of list of subscribe or unsubscribe ack
    void *                          lock_write_buf;                          //lock of write
    iot_mqtt_event_handle_t       handle_event;                            //event handle
}MC_client_t, *MC_client_pt;


static int MC_send_packet(MC_client_t *c, char *buf, int length, iot_timer_t *timer);
static MC_state_t MC_get_client_state(MC_client_t *pClient);
static void MC_set_client_state(MC_client_t *pClient, MC_state_t newState);
static int MC_keepalive_sub(MC_client_t *pClient);
static void MC_disconnect_callback(MC_client_t *pClient) ;
static bool MC_check_state_normal(MC_client_t *c);
static int MC_handle_reconnect(MC_client_t *pClient);
static void MC_reconnect_callback(MC_client_t *pClient);
static int MC_push_pubInfo_to(MC_client_t *c, int len, unsigned short msgId, list_node_t **node);
static int MC_push_subInfo_to(MC_client_t *c, int len, unsigned short msgId, enum msgTypes type, MC_topic_handle_t *handler,
                    list_node_t **node);
static int MC_check_handle_is_identical(MC_topic_handle_t *messageHandlers1, MC_topic_handle_t *messageHandler2);


typedef enum {
    TOPIC_NAME_TYPE = 0,
    TOPIC_FILTER_TYPE
} MC_topic_type_t;


//check rule whether is valid or not
static int MC_check_rule(char *iterm, MC_topic_type_t type)
{
    if (NULL == iterm) {
        TRACE_ERROR("iterm is NULL");
        return FAIL_RETURN;
    }

    int i = 0;
    int len = strlen(iterm);
    for (i = 0; i < len; i++) {
        if (TOPIC_FILTER_TYPE == type) {
            if ('+' == iterm[i] || '#' == iterm[i]) {
                if (1 != len) {
                    TRACE_ERROR("the character # and + is error");
                    return FAIL_RETURN;
                }
            }
        } else {
            if ('+' == iterm[i] || '#' == iterm[i]) {
                TRACE_ERROR("has character # and + is error");
                return FAIL_RETURN;
            }
        }

        if (iterm[i] < 32 || iterm[i] >= 127) {
            return FAIL_RETURN;
        }
    }
    return SUCCESS_RETURN;
}


//Check topic name
//0, topic name is valid; NOT 0, topic name is invalid
static int MC_check_topic(const char *topicName, MC_topic_type_t type)
{
  return SUCCESS_RETURN;

    if (NULL == topicName || '/' != topicName[0]) {
        return FAIL_RETURN;
    }

    if (strlen(topicName) > MC_TOPIC_NAME_MAX_LEN) {
        TRACE_ERROR("len of topicName exceeds 64");
        return FAIL_RETURN;
    }

    int mask = 0;
    char topicString[MC_TOPIC_NAME_MAX_LEN];
    memset(topicString, 0x0, MC_TOPIC_NAME_MAX_LEN);
    strncpy(topicString, topicName, MC_TOPIC_NAME_MAX_LEN);

    char *delim = "/";
    char *iterm = NULL;
    iterm = strtok(topicString, delim);

    if (SUCCESS_RETURN != MC_check_rule(iterm, type)) {
        TRACE_ERROR("run iot_check_rule error");
        return FAIL_RETURN;
    }

    for (;;) {
        iterm = strtok(NULL, delim);

        if (iterm == NULL) {
            break;
        }

        //The character '#' is not in the last
        if (1 == mask) {
            TRACE_ERROR("the character # is error");
            return FAIL_RETURN;
        }

        if (SUCCESS_RETURN != MC_check_rule(iterm, type)) {
            TRACE_ERROR("run iot_check_rule error");
            return FAIL_RETURN;
        }

        if (iterm[0] == '#') {
            mask = 1;
        }
    }

    return SUCCESS_RETURN;
}


//Send keepalive packet
static int MQTTKeepalive(MC_client_t *pClient)
{
    /* there is no ping outstanding - send ping packet */
    iot_timer_t timer;
    iot_timer_init(&timer);
    iot_timer_countdown(&timer, 1000);
    int len = 0;
    int rc = 0;

    iot_platform_mutex_lock(pClient->lock_write_buf);
    len = MQTTSerialize_pingreq((unsigned char *)pClient->buf_send, pClient->buf_size_send);
    if (len <= 0) {
        iot_platform_mutex_unlock(pClient->lock_write_buf);
        TRACE_ERROR("Serialize ping request is error");
        return MQTT_PING_PACKET_ERROR;
    }

    rc = MC_send_packet(pClient, pClient->buf_send, len, &timer);
    if (SUCCESS_RETURN != rc) {
        iot_platform_mutex_unlock(pClient->lock_write_buf);
        /*ping outstanding, then close socket unsubscribe topic and handle callback function*/
        TRACE_ERROR("ping outstanding is error,result = %d", rc);
        return MQTT_NETWORK_ERROR;
    }
    iot_platform_mutex_unlock(pClient->lock_write_buf);

    return SUCCESS_RETURN;
}


//MQTT send connect packet
int MQTTConnect(MC_client_t *pClient)
{
    MQTTPacket_connectData *pConnectParams = &pClient->connect_data;
    iot_timer_t connectTimer;
    int len = 0;
    int rc = 0;

    iot_platform_mutex_lock(pClient->lock_write_buf);
    if ((len = MQTTSerialize_connect((unsigned char *)pClient->buf_send, pClient->buf_size_send, pConnectParams)) <= 0) {
        iot_platform_mutex_unlock(pClient->lock_write_buf);
        TRACE_ERROR("Serialize connect packet failed,len = %d", len);
        return MQTT_CONNECT_PACKET_ERROR;
    }

    /* send the connect packet*/
    iot_timer_init(&connectTimer);
    iot_timer_countdown(&connectTimer, pClient->request_timeout_ms);
    if ((rc = MC_send_packet(pClient, pClient->buf_send, len, &connectTimer)) != SUCCESS_RETURN) {
        iot_platform_mutex_unlock(pClient->lock_write_buf);
        TRACE_ERROR("send connect packet failed");
        return MQTT_NETWORK_ERROR;
    }
    iot_platform_mutex_unlock(pClient->lock_write_buf);

    return SUCCESS_RETURN;
}


//MQTT send publish packet
int MQTTPublish(MC_client_t *c, const char *topicName, iot_mqtt_topic_info_pt topic_msg)

{
    iot_timer_t timer;
    int32_t lefttime = 0;
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicName;
    int len = 0;

    iot_timer_init(&timer);
    iot_timer_countdown(&timer, c->request_timeout_ms);

    iot_platform_mutex_lock(c->lock_write_buf);
    len = MQTTSerialize_publish((unsigned char *)c->buf_send,
                    c->buf_size_send,
                    0,
                    topic_msg->qos,
                    topic_msg->retain,
                    topic_msg->packet_id,
                    topic,
                    (unsigned char *)topic_msg->payload,
                    topic_msg->payload_len);
    if (len <= 0) {
        iot_platform_mutex_unlock(c->lock_write_buf);
        TRACE_ERROR("MQTTSerialize_publish is error, len=%d, buf_size=%u, payloadlen=%u",
                        len,
                        c->buf_size_send,
                        topic_msg->payload_len);
        return MQTT_PUBLISH_PACKET_ERROR;
    }

    list_node_t *node = NULL;

    //If the QOS >1, push the information into list of wait publish ACK
    if (topic_msg->qos > IOT_MQTT_QOS0) {
        //push into list
        if (SUCCESS_RETURN != MC_push_pubInfo_to(c, len, topic_msg->packet_id, &node)) {
            TRACE_ERROR("push publish into to pubInfolist failed!");
            iot_platform_mutex_unlock(c->lock_write_buf);
            return MQTT_PUSH_TO_LIST_ERROR;
        }
    }

    //send the publish packet
    if (MC_send_packet(c, c->buf_send, len, &timer) != SUCCESS_RETURN) {
        if (topic_msg->qos > IOT_MQTT_QOS0) {
            //If failed, remove from list
            iot_platform_mutex_lock(c->lock_list_pub);
            list_remove(c->list_pub_wait_ack, node);
            iot_platform_mutex_unlock(c->lock_list_pub);
        }

        iot_platform_mutex_unlock(c->lock_write_buf);
        return MQTT_NETWORK_ERROR;
    }

    iot_platform_mutex_unlock(c->lock_write_buf);
    return SUCCESS_RETURN;
}


//MQTT send publish ACK
static int MQTTPuback(MC_client_t *c, unsigned int msgId, enum msgTypes type)
{
    int rc = 0;
    int len = 0;
    iot_timer_t timer;
    iot_timer_init(&timer);
    iot_timer_countdown(&timer, c->request_timeout_ms);

    iot_platform_mutex_lock(c->lock_write_buf);
    if (type == PUBACK) {
        len = MQTTSerialize_ack((unsigned char *)c->buf_send, c->buf_size_send, PUBACK, 0, msgId);
    } else if (type == PUBREC) {
        len = MQTTSerialize_ack((unsigned char *)c->buf_send, c->buf_size_send, PUBREC, 0, msgId);
    } else if (type == PUBREL) {
        len = MQTTSerialize_ack((unsigned char *)c->buf_send, c->buf_size_send, PUBREL, 0, msgId);
    } else {
        iot_platform_mutex_unlock(c->lock_write_buf);
        return MQTT_PUBLISH_ACK_TYPE_ERROR;
    }

    if (len <= 0) {
        iot_platform_mutex_unlock(c->lock_write_buf);
        return MQTT_PUBLISH_ACK_PACKET_ERROR;
    }

    rc = MC_send_packet(c, c->buf_send, len, &timer);
    if (rc != SUCCESS_RETURN) {
        iot_platform_mutex_unlock(c->lock_write_buf);
        return MQTT_NETWORK_ERROR;
    }

    iot_platform_mutex_unlock(c->lock_write_buf);
    return SUCCESS_RETURN;
}


//MQTT send subscribe packet
static int MQTTSubscribe(MC_client_t *c, const char *topicFilter, iot_mqtt_qos_t qos, unsigned int msgId,
                iot_mqtt_event_handle_func_fpt messageHandler, void *pcontext)
{
    int rc = 0;
    int len = 0;
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicFilter;

    iot_timer_t timer;
    iot_timer_init(&timer);
    iot_timer_countdown(&timer, c->request_timeout_ms);

    iot_platform_mutex_lock(c->lock_write_buf);

    len = MQTTSerialize_subscribe((unsigned char *)c->buf_send, c->buf_size_send, 0, (unsigned short)msgId, 1, &topic, (int *)&qos);
    if (len <= 0) {
        iot_platform_mutex_unlock(c->lock_write_buf);
        return MQTT_SUBSCRIBE_PACKET_ERROR;
    }

    MC_topic_handle_t handler = {topicFilter, {messageHandler, pcontext}};

    list_node_t *node = NULL;


    /*
     * NOTE: It prefer to push the element into list and then remove it when send failed,
     *       because some of extreme cases
     * */

    //push the element to list of wait subscribe ACK
    if (SUCCESS_RETURN != MC_push_subInfo_to(c, len, msgId, SUBSCRIBE, &handler, &node)) {
        TRACE_ERROR("push publish into to pubInfolist failed!");
        iot_platform_mutex_unlock(c->lock_write_buf);
        return MQTT_PUSH_TO_LIST_ERROR;
    }

    if ((rc = MC_send_packet(c, c->buf_send, len, &timer)) != SUCCESS_RETURN) { // send the subscribe packet
        //If send failed, remove it
        iot_platform_mutex_lock(c->lock_list_sub);
        list_remove(c->list_sub_wait_ack, node);
        iot_platform_mutex_unlock(c->lock_list_sub);
        iot_platform_mutex_unlock(c->lock_write_buf);
        TRACE_ERROR("run sendPacket error!");
        return MQTT_NETWORK_ERROR;
    }

    iot_platform_mutex_unlock(c->lock_write_buf);
    return SUCCESS_RETURN;
}


//MQTT send unsubscribe packet
static int MQTTUnsubscribe(MC_client_t *c, const char *topicFilter, unsigned int msgId)
{
    iot_timer_t timer;
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicFilter;
    int len = 0;
    int rc = 0;

    iot_timer_init(&timer);
    iot_timer_countdown(&timer, c->request_timeout_ms);

    iot_platform_mutex_lock(c->lock_write_buf);

    if ((len = MQTTSerialize_unsubscribe((unsigned char *)c->buf_send, c->buf_size_send, 0, (unsigned short)msgId, 1, &topic)) <= 0) {
        iot_platform_mutex_unlock(c->lock_write_buf);
        return MQTT_UNSUBSCRIBE_PACKET_ERROR;
    }

    MC_topic_handle_t handler = {topicFilter, {NULL, NULL}};

    //push into list
    list_node_t *node = NULL;
    if (SUCCESS_RETURN != MC_push_subInfo_to(c, len, msgId, UNSUBSCRIBE, &handler, &node)) {
        TRACE_ERROR("push publish into to pubInfolist failed!");
        iot_platform_mutex_unlock(c->lock_write_buf);
        return MQTT_PUSH_TO_LIST_ERROR;
    }

    if ((rc = MC_send_packet(c, c->buf_send, len, &timer)) != SUCCESS_RETURN) { // send the subscribe packet
        //remove from list
        iot_platform_mutex_lock(c->lock_list_sub);
        list_remove(c->list_sub_wait_ack, node);
        iot_platform_mutex_unlock(c->lock_list_sub);
        iot_platform_mutex_unlock(c->lock_write_buf);
        return MQTT_NETWORK_ERROR;
    }

    iot_platform_mutex_unlock(c->lock_write_buf);

    return SUCCESS_RETURN;
}


//MQTT send disconnect packet
static int MQTTDisconnect(MC_client_t *c)
{
    int rc = FAIL_RETURN;
    iot_timer_t timer;     // we might wait for incomplete incoming publishes to complete

    iot_platform_mutex_lock(c->lock_write_buf);
    int len = MQTTSerialize_disconnect((unsigned char *)c->buf_send, c->buf_size_send);

    iot_timer_init(&timer);
    iot_timer_countdown(&timer, c->request_timeout_ms);

    if (len > 0) {
        rc = MC_send_packet(c, c->buf_send, len, &timer);           // send the disconnect packet
    }

    iot_platform_mutex_unlock(c->lock_write_buf);

    return rc;
}


//remove the list element specified by @msgId from list of wait publish ACK
//return: 0, success; NOT 0, fail;
static int MC_mask_pubInfo_from(MC_client_t *c, uint16_t msgId)
{
    iot_platform_mutex_lock(c->lock_list_pub);
    if (c->list_pub_wait_ack->len) {
        list_iterator_t *iter;
        list_node_t *node = NULL;
        MC_pub_info_t *repubInfo = NULL;

        if (NULL == (iter = list_iterator_new(c->list_pub_wait_ack, LIST_TAIL))) {
            iot_platform_mutex_unlock(c->lock_list_pub);
            return SUCCESS_RETURN;
        }


        for (;;) {
            node = list_iterator_next(iter);

            if (NULL == node) {
                break;
            }

            repubInfo = (MC_pub_info_t *) node->val;
            if (NULL == repubInfo) {
                TRACE_ERROR("node's value is invalid!");
                continue;
            }

            if (repubInfo->msg_id == msgId) {
                repubInfo->node_state = MC_NODE_STATE_INVALID; //mark as invalid node
            }
        }

        list_iterator_destroy(iter);
    }
    iot_platform_mutex_unlock(c->lock_list_pub);

    return SUCCESS_RETURN;
}


//push the wait element into list of wait publish ACK
//return: 0, success; NOT 0, fail;
static int MC_push_pubInfo_to(MC_client_t *c, int len, unsigned short msgId, list_node_t **node)
{
    if ((len < 0) || (len > c->buf_size_send)) {
        TRACE_ERROR("the param of len is error!")
        return FAIL_RETURN;
    }

    iot_platform_mutex_lock(c->lock_list_pub);

    if (c->list_pub_wait_ack->len >= MC_REPUB_NUM_MAX) {
        iot_platform_mutex_unlock(c->lock_list_pub);
        TRACE_ERROR("more than %u elements in republish list. List overflow!", c->list_pub_wait_ack->len);
        return FAIL_RETURN;
    }

    MC_pub_info_t *repubInfo = (MC_pub_info_t *)iot_platform_malloc(sizeof(MC_pub_info_t) + len);
    if (NULL == repubInfo) {
        iot_platform_mutex_unlock(c->lock_list_pub);
        TRACE_ERROR("run iot_memory_malloc is error!");
        return FAIL_RETURN;
    }

    repubInfo->node_state = MC_NODE_STATE_NORMANL;
    repubInfo->msg_id = msgId;
    repubInfo->len = len;
    iot_timer_start(&repubInfo->pub_start_time);
    repubInfo->buf = (char *)repubInfo + sizeof(MC_pub_info_t);

    memcpy(repubInfo->buf, c->buf_send, len);

    *node = list_node_new(repubInfo);
    if (NULL == *node) {
        iot_platform_mutex_unlock(c->lock_list_pub);
        TRACE_ERROR("run list_node_new is error!");
        return FAIL_RETURN;
    }

    list_rpush(c->list_pub_wait_ack, *node);

    iot_platform_mutex_unlock(c->lock_list_pub);

    return SUCCESS_RETURN;
}


//push the wait element into list of wait subscribe(unsubscribe) ACK
//return: 0, success; NOT 0, fail;
static int MC_push_subInfo_to(MC_client_t *c, int len, unsigned short msgId, enum msgTypes type, MC_topic_handle_t *handler,
                    list_node_t **node)
{
    iot_platform_mutex_lock(c->lock_list_sub);

    if (c->list_sub_wait_ack->len >= MC_SUB_REQUEST_NUM_MAX) {
        iot_platform_mutex_unlock(c->lock_list_sub);
        TRACE_ERROR("number of subInfo more than max!,size = %d", c->list_sub_wait_ack->len);
        return FAIL_RETURN;
    }

    MC_subsribe_info_t *subInfo = (MC_subsribe_info_t *)iot_platform_malloc(sizeof(MC_subsribe_info_t) + len);
    if (NULL == subInfo) {
        iot_platform_mutex_unlock(c->lock_list_sub);
        TRACE_ERROR("run iot_memory_malloc is error!");
        return FAIL_RETURN;
    }

    subInfo->node_state = MC_NODE_STATE_NORMANL;
    subInfo->msg_id = msgId;
    subInfo->len = len;
    iot_timer_start(&subInfo->sub_start_time);
    subInfo->type = type;
    subInfo->handler = *handler;
    subInfo->buf = (unsigned char *)subInfo + sizeof(MC_subsribe_info_t);

    memcpy(subInfo->buf, c->buf_send, len);

    *node = list_node_new(subInfo);
    if (NULL == *node) {
        iot_platform_mutex_unlock(c->lock_list_sub);
        TRACE_ERROR("run list_node_new is error!");
        return FAIL_RETURN;
    }

    list_rpush(c->list_sub_wait_ack, *node);

    iot_platform_mutex_unlock(c->lock_list_sub);

    return SUCCESS_RETURN;
}


//remove the list element specified by @msgId from list of wait subscribe(unsubscribe) ACK
//and return message handle by @messageHandler
//return: 0, success; NOT 0, fail;
static int MC_mask_subInfo_from(MC_client_t *c, unsigned int msgId, MC_topic_handle_t *messageHandler)
{
    iot_platform_mutex_lock(c->lock_list_sub);
    if (c->list_sub_wait_ack->len) {
        list_iterator_t *iter; 
        list_node_t *node = NULL;
        MC_subsribe_info_t *subInfo = NULL;

        if (NULL == (iter = list_iterator_new(c->list_sub_wait_ack, LIST_TAIL))){
            iot_platform_mutex_lock(c->lock_list_sub);
            return SUCCESS_RETURN;
        }

        for (;;) {
            node = list_iterator_next(iter);
            if (NULL == node) {
                break;
            }

            subInfo = (MC_subsribe_info_t *) node->val;
            if (NULL == subInfo) {
                TRACE_ERROR("node's value is invalid!");
                continue;
            }

            if (subInfo->msg_id == msgId) {
                *messageHandler = subInfo->handler; //return handle
                subInfo->node_state = MC_NODE_STATE_INVALID; //mark as invalid node
            }
        }

        list_iterator_destroy(iter);
    }
    iot_platform_mutex_unlock(c->lock_list_sub);

    return SUCCESS_RETURN;
}


//get next packet-id
static int MC_get_next_packetid(MC_client_t *c)
{
    unsigned int id = 0;
    iot_platform_mutex_lock(c->lock_generic);
    c->packet_id = (c->packet_id == MC_PACKET_ID_MAX) ? 1 : c->packet_id + 1;
    id = c->packet_id;
    iot_platform_mutex_unlock(c->lock_generic);

    return id;
}


//send packet
static int MC_send_packet(MC_client_t *c, char *buf, int length, iot_timer_t *time)
{
    int rc = FAIL_RETURN;
    int sent = 0;

    while (sent < length && !iot_timer_is_expired(time)) {
        rc = c->ipstack->write(c->ipstack, &buf[sent], length, iot_timer_left(time));
        if (rc < 0) { // there was an error writing the data
            break;
        }
        sent += rc;
    }

    if (sent == length) {
        rc = SUCCESS_RETURN;
    } else {
        rc = MQTT_NETWORK_ERROR;
    }
    return rc;
}


//decode packet
static int MC_decode_packet(MC_client_t *c, int *value, int timeout)
{
    char i;
    //unsigned char j = 1;
    int multiplier = 1;
    int len = 0;
    const int MAX_NO_OF_REMAINING_LENGTH_BYTES = 4;

    *value = 0;
    do {
        int rc = MQTTPACKET_READ_ERROR;

        if (++len > MAX_NO_OF_REMAINING_LENGTH_BYTES) {
            return MQTTPACKET_READ_ERROR; /* bad data */
        }

        rc = c->ipstack->read(c->ipstack, &i, 1, timeout);
        if (rc != 1) {
            return MQTT_NETWORK_ERROR;
        }
        //i = c->buf_read[j++];
        *value += (i & 127) * multiplier;
        multiplier *= 128;
    } while ((i & 128) != 0);

    return len;
}


//read packet
static int MC_read_packet(MC_client_t *c, iot_timer_t *timer, unsigned int *packet_type)
{
    MQTTHeader header = {0};
    int len = 0;
    int rem_len = 0;
    int rc = 0;

    /* 1. read the header byte.  This has the packet type in it */
    rc = c->ipstack->read(c->ipstack, c->buf_read, 1, iot_timer_left(timer));
    if (0 == rc) { //timeout
        *packet_type = 0;
        return SUCCESS_RETURN;
    } else if (1 != rc) {
        TRACE_DEBUG("mqtt read error, rc=%d", rc);
        return FAIL_RETURN;
    }

    len = 1;

    /* 2. read the remaining length.  This is variable in itself */
    if ((rc = MC_decode_packet(c, &rem_len, iot_timer_left(timer))) < 0) {
        TRACE_ERROR("decodePacket error,rc = %d", rc);
        return rc;
    }

    len += MQTTPacket_encode((unsigned char *)c->buf_read + 1, rem_len); /* put the original remaining length back into the buffer */

    /*Check if the received data length exceeds mqtt read buffer length*/
    if ((rem_len > 0) && ((rem_len + len) > c->buf_size_read)) {
        TRACE_ERROR("mqtt read buffer is too short, mqttReadBufLen : %u, remainDataLen : %d", c->buf_size_read, rem_len);
        int needReadLen = c->buf_size_read - len;
        if (c->ipstack->read(c->ipstack, c->buf_read + len, needReadLen, iot_timer_left(timer)) != needReadLen) {
            TRACE_ERROR("mqtt read error");
            return FAIL_RETURN;
        }

        /* drop data whitch over the length of mqtt buffer*/
        int remainDataLen = rem_len - needReadLen;
        char *remainDataBuf = iot_platform_malloc(remainDataLen + 1);
        if (!remainDataBuf) {
            TRACE_ERROR("malloc remain buffer failed");
            return FAIL_RETURN;
        }

        if (c->ipstack->read(c->ipstack, remainDataBuf, remainDataLen, iot_timer_left(timer)) != remainDataLen) {
            TRACE_ERROR("mqtt read error");
            iot_platform_free(remainDataBuf);
            remainDataBuf = NULL;
            return FAIL_RETURN;
        }

        iot_platform_free(remainDataBuf);
        remainDataBuf = NULL;

        return FAIL_RETURN;

    }

    /* 3. read the rest of the buffer using a callback to supply the rest of the data */
    if (rem_len > 0 && (c->ipstack->read(c->ipstack, c->buf_read + len, rem_len, iot_timer_left(timer)) != rem_len)) {
        TRACE_ERROR("mqtt read error");
        return FAIL_RETURN;
    }

    header.byte = c->buf_read[0];
    *packet_type = header.bits.type;
    return SUCCESS_RETURN;
}


//check whether the topic is matched or not
static char MC_is_topic_matched(char *topicFilter, MQTTString *topicName)
{
    char *curf = topicFilter;
    char *curn = topicName->lenstring.data;
    char *curn_end = curn + topicName->lenstring.len;

    while (*curf && curn < curn_end) {
        if (*curn == '/' && *curf != '/') {
            break;
        }

        if (*curf != '+' && *curf != '#' && *curf != *curn) {
            break;
        }

        if (*curf == '+') {
            // skip until we meet the next separator, or end of string
            char *nextpos = curn + 1;
            while (nextpos < curn_end && *nextpos != '/') {
                nextpos = ++curn + 1;
            }
        } else if (*curf == '#') {
            curn = curn_end - 1;    // skip until end of string
        }
        curf++;
        curn++;
    }

    return (curn == curn_end) && (*curf == '\0');
}


//deliver message
static void MC_deliver_message(MC_client_t *c, MQTTString *topicName, iot_mqtt_topic_info_pt topic_msg)
{
    int i, flag_matched = 0;

    topic_msg->ptopic = topicName->lenstring.data;
    topic_msg->topic_len = topicName->lenstring.len;

    // we have to find the right message handler - indexed by topic
    iot_platform_mutex_lock(c->lock_generic);
    for (i = 0; i < MC_SUB_NUM_MAX; ++i) {

        if ((c->sub_handle[i].topic_filter != 0)
             && (MQTTPacket_equals(topicName, (char *)c->sub_handle[i].topic_filter)
                 || MC_is_topic_matched((char *)c->sub_handle[i].topic_filter, topicName))) {
            TRACE_DEBUG("topic be matched");

            MC_topic_handle_t msg_handle = c->sub_handle[i];
            iot_platform_mutex_unlock(c->lock_generic);

            if (NULL != msg_handle.handle.h_fp) {
                iot_mqtt_event_msg_t msg;
                msg.event_type = IOT_MQTT_EVENT_PUBLISH_RECVEIVED;
                msg.msg = (void *)topic_msg;

                msg_handle.handle.h_fp(msg_handle.handle.pcontext, c, &msg);
                flag_matched = 1;
            }

            iot_platform_mutex_lock(c->lock_generic);
        }
    }

    iot_platform_mutex_unlock(c->lock_generic);

    if (0 == flag_matched) {
        TRACE_DEBUG("NO matching any topic, call default handle function");

        if (NULL != c->handle_event.h_fp) {
            iot_mqtt_event_msg_t msg;

            msg.event_type = IOT_MQTT_EVENT_PUBLISH_RECVEIVED;
            msg.msg = topic_msg;

            c->handle_event.h_fp(c->handle_event.pcontext, c, &msg);
        }
    }
}


//handle CONNACK packet received from remote MQTT broker
static int MC_handle_recv_CONNACK(MC_client_t *c)
{
    int rc = SUCCESS_RETURN;
    unsigned char connack_rc = 255;
    char sessionPresent = 0;
    if (MQTTDeserialize_connack((unsigned char *)&sessionPresent, &connack_rc, (unsigned char *)c->buf_read, c->buf_size_read) != 1) {
        TRACE_ERROR("connect ack is error");
        return MQTT_CONNECT_ACK_PACKET_ERROR;
    }

    switch (connack_rc) {
        case MC_CONNECTION_ACCEPTED:
            rc = SUCCESS_RETURN;
            break;
        case MC_CONNECTION_REFUSED_UNACCEPTABLE_PROTOCOL_VERSION:
            rc = MQTT_CONANCK_UNACCEPTABLE_PROTOCOL_VERSION_ERROR;
            break;
        case MC_CONNECTION_REFUSED_IDENTIFIER_REJECTED:
            rc = MQTT_CONNACK_IDENTIFIER_REJECTED_ERROR;
            break;
        case MC_CONNECTION_REFUSED_SERVER_UNAVAILABLE:
            rc = MQTT_CONNACK_SERVER_UNAVAILABLE_ERROR;
            break;
        case MC_CONNECTION_REFUSED_BAD_USERDATA:
            rc = MQTT_CONNACK_BAD_USERDATA_ERROR;
            break;
        case MC_CONNECTION_REFUSED_NOT_AUTHORIZED:
            rc = MQTT_CONNACK_NOT_AUTHORIZED_ERROR;
            break;
        default:
            rc = MQTT_CONNACK_UNKNOWN_ERROR;
            break;
    }

    return rc;
}


//handle PUBACK packet received from remote MQTT broker
static int MC_handle_recv_PUBACK(MC_client_t *c)
{
    unsigned short mypacketid;
    unsigned char dup = 0;
    unsigned char type = 0;

    if (MQTTDeserialize_ack(&type, &dup, &mypacketid, (unsigned char *)c->buf_read, c->buf_size_read) != 1) {
        return MQTT_PUBLISH_ACK_PACKET_ERROR;
    }

    (void)MC_mask_pubInfo_from(c, mypacketid);

    if (NULL != c->handle_event.h_fp) {
        //call callback function to notify that PUBLISH is successful.
        if (NULL != c->handle_event.h_fp) {
            iot_mqtt_event_msg_t msg;
            msg.event_type = IOT_MQTT_EVENT_PUBLISH_SUCCESS;
            msg.msg = (void *)mypacketid;
            c->handle_event.h_fp(c->handle_event.pcontext, c, &msg);
        }
    }

    return SUCCESS_RETURN;
}


//handle SUBACK packet received from remote MQTT broker
static int MC_handle_recv_SUBACK(MC_client_t *c)
{
    unsigned short mypacketid;
    int i, count = 0, grantedQoS = -1;
    if (MQTTDeserialize_suback(&mypacketid, 1, &count, &grantedQoS, (unsigned char *)c->buf_read, c->buf_size_read) != 1) {
        TRACE_ERROR("Sub ack packet error");
        return MQTT_SUBSCRIBE_ACK_PACKET_ERROR;
    }

    if (grantedQoS == 0x80) {
        iot_mqtt_event_msg_t msg;

        TRACE_ERROR("MQTT SUBSCRIBE failed, ack code is 0x80");

        msg.event_type = IOT_MQTT_EVENT_SUBCRIBE_NACK;
        msg.msg = (void *)mypacketid;
        c->handle_event.h_fp(c->handle_event.pcontext, c, &msg);

        return MQTT_SUBSCRIBE_ACK_FAILURE;
    }

    MC_topic_handle_t messagehandler;
    memset(&messagehandler, 0, sizeof(MC_topic_handle_t));
    (void)MC_mask_subInfo_from(c, mypacketid, &messagehandler);

    if ((NULL == messagehandler.handle.h_fp) || (NULL == messagehandler.topic_filter)) {
        return MQTT_SUB_INFO_NOT_FOUND_ERROR;
    }


    for (i = 0; i < MC_SUB_NUM_MAX; ++i) {
        /*If subscribe the same topic and callback function, then ignore*/
        if ((NULL != c->sub_handle[i].topic_filter)
            && (0 == MC_check_handle_is_identical(&c->sub_handle[i], &messagehandler))) {
            //if subscribe a identical topic and relate callback function, then ignore this subscribe.
            return SUCCESS_RETURN;
        }
    }

    /*Search a free element to record topic and related callback function*/
    iot_platform_mutex_lock(c->lock_generic);
    for (i = 0 ; i < MC_SUB_NUM_MAX; ++i) {
        if (NULL == c->sub_handle[i].topic_filter) {
            c->sub_handle[i].topic_filter = messagehandler.topic_filter;
            c->sub_handle[i].handle.h_fp = messagehandler.handle.h_fp;
            c->sub_handle[i].handle.pcontext = messagehandler.handle.pcontext;
            iot_platform_mutex_unlock(c->lock_generic);

            //call callback function to notify that SUBSCRIBE is successful.
            if (NULL != c->handle_event.h_fp) {
                iot_mqtt_event_msg_t msg;
                msg.event_type = IOT_MQTT_EVENT_SUBCRIBE_SUCCESS;
                msg.msg = (void *)mypacketid;
                c->handle_event.h_fp(c->handle_event.pcontext, c, &msg);
            }

            return SUCCESS_RETURN;
        }
    }

    iot_platform_mutex_unlock(c->lock_generic);
    /*Not free element be found*/
    TRACE_ERROR("NOT more @sub_handle space!");
    return FAIL_RETURN;
}


//handle PUBLISH packet received from remote MQTT broker
static int MC_handle_recv_PUBLISH(MC_client_t *c)
{
    int result = 0;
    MQTTString topicName;
    iot_mqtt_topic_info_t topic_msg;

    memset(&topic_msg, 0x0, sizeof(iot_mqtt_topic_info_t));
    memset(&topicName, 0x0, sizeof(MQTTString));

    if (1 != MQTTDeserialize_publish((unsigned char *)&topic_msg.dup,
                                (int *)&topic_msg.qos,
                                (unsigned char *)&topic_msg.retain,
                                (unsigned short *)&topic_msg.packet_id,
                                &topicName,
                                (unsigned char **)&topic_msg.payload,
                                (int *)&topic_msg.payload_len,
                                (unsigned char *)c->buf_read,
                                c->buf_size_read)) {
        return MQTT_PUBLISH_PACKET_ERROR;
    }

    topic_msg.ptopic = NULL;
    topic_msg.topic_len = 0;

    TRACE_DEBUG("deliver msg");
    MC_deliver_message(c, &topicName, &topic_msg);
    TRACE_DEBUG("end of delivering msg");

    if (topic_msg.qos == IOT_MQTT_QOS0) {
        return SUCCESS_RETURN;
    } else if (topic_msg.qos == IOT_MQTT_QOS1) {
        result = MQTTPuback(c, topic_msg.packet_id, PUBACK);
    } else if (topic_msg.qos == IOT_MQTT_QOS2) {
        result = MQTTPuback(c, topic_msg.packet_id, PUBREC);
    } else {
        TRACE_ERROR("Invalid QOS, QOSvalue = %d", topic_msg.qos);
        return MQTT_PUBLISH_QOS_ERROR;
    }

    return result;
}


//handle UNSUBACK packet received from remote MQTT broker
static int MC_handle_recv_UNSUBACK(MC_client_t *c)
{
    unsigned short i, mypacketid = 0;  // should be the same as the packetid above

    if (MQTTDeserialize_unsuback(&mypacketid, (unsigned char *)c->buf_read, c->buf_size_read) != 1) {

        return MQTT_UNSUBSCRIBE_ACK_PACKET_ERROR;
    }

    MC_topic_handle_t messageHandler;
    (void)MC_mask_subInfo_from(c, mypacketid, &messageHandler);

    /* Remove from message handler array */
    iot_platform_mutex_lock(c->lock_generic);
    for (i = 0; i < MC_SUB_NUM_MAX; ++i) {
        if ((c->sub_handle[i].topic_filter != NULL)
            && (0 == MC_check_handle_is_identical(&c->sub_handle[i], &messageHandler))) {
            memset(&c->sub_handle[i], 0, sizeof(MC_topic_handle_t));

            /* NOTE: in case of more than one register(subscribe) with different callback function,
             *       so we must keep continuously searching related message handle. */
       }
    }

    if (NULL != c->handle_event.h_fp) {
        iot_mqtt_event_msg_t msg;
        msg.event_type = IOT_MQTT_EVENT_UNSUBCRIBE_SUCCESS;
        msg.msg = (void *)mypacketid;

        c->handle_event.h_fp(c->handle_event.pcontext, c, &msg);
    }

    iot_platform_mutex_unlock(c->lock_generic);
    return SUCCESS_RETURN;
}


//wait CONNACK packet from remote MQTT broker
static int MC_wait_CONNACK(MC_client_t *c)
{
    unsigned int packetType = 0;
    int rc = 0;
    iot_timer_t timer;
    iot_timer_init(&timer);
    iot_timer_countdown(&timer, c->connect_data.keepAliveInterval * 1000);

    do {
        // read the socket, see what work is due
        rc = MC_read_packet(c, &timer, &packetType);
        if (rc != SUCCESS_RETURN) {
            TRACE_ERROR("readPacket error,result = %d", rc);
            return MQTT_NETWORK_ERROR;
        }

    } while (packetType != CONNACK);

    rc = MC_handle_recv_CONNACK(c);
    if (SUCCESS_RETURN != rc) {
        TRACE_ERROR("recvConnackProc error,result = %d", rc);
    }

    return rc;
}


//MQTT cycle to handle packet from remote broker
static int MC_cycle(MC_client_t *c, iot_timer_t *timer)
{
    unsigned int packetType;
    int rc = SUCCESS_RETURN;

    MC_state_t state = MC_get_client_state(c);
    if (state != MC_STATE_CONNECTED) {
        TRACE_DEBUG("state = %d", state);
        return MQTT_STATE_ERROR;
    }

    // read the socket, see what work is due
    rc = MC_read_packet(c, timer, &packetType);
    if (rc != SUCCESS_RETURN) {
        MC_set_client_state(c, MC_STATE_DISCONNECTED);
        TRACE_DEBUG("readPacket error,result = %d", rc);
        return MQTT_NETWORK_ERROR;
    }

    if (MQTT_CPT_RESERVED == packetType) {
        //TRACE_DEBUG("wait data timeout");
        return SUCCESS_RETURN;
    }

    //receive any data to renew ping_timer
    iot_timer_countdown(&c->next_ping_time, c->connect_data.keepAliveInterval * 1000);

    //clear ping mark when any data received from MQTT broker
    iot_platform_mutex_lock(c->lock_generic);
    c->ping_mark = 0;
    iot_platform_mutex_unlock(c->lock_generic);

    switch (packetType) {
        case CONNACK: {
            TRACE_DEBUG("CONNACK");
            break;
        }
        case PUBACK: {
            rc = MC_handle_recv_PUBACK(c);

            if (SUCCESS_RETURN != rc) {
                TRACE_ERROR("recvPubackProc error,result = %d", rc);
            }

            break;
        }
        case SUBACK: {
            rc = MC_handle_recv_SUBACK(c);
            if (SUCCESS_RETURN != rc) {
                TRACE_ERROR("recvSubAckProc error,result = %d", rc);
            }
            TRACE_DEBUG("SUBACK");
            break;
        }
        case PUBLISH: {
            rc = MC_handle_recv_PUBLISH(c);
            if (SUCCESS_RETURN != rc) {
                TRACE_ERROR("recvPublishProc error,result = %d", rc);
            }
            TRACE_DEBUG("PUBLISH");
            break;
        }
        case UNSUBACK: {
            rc = MC_handle_recv_UNSUBACK(c);
            if (SUCCESS_RETURN != rc) {
                TRACE_ERROR("recvUnsubAckProc error,result = %d", rc);
            }
            break;
        }
        case PINGRESP: {
            rc = SUCCESS_RETURN;
            TRACE_INFO("receive ping response!");
            break;
        }
        default:
            TRACE_ERROR("INVALID TYPE");
            return FAIL_RETURN;
    }

    return rc;
}


//check MQTT client is in normal state.
//0, in abnormal state; 1, in normal state.
static bool MC_check_state_normal(MC_client_t *c)
{
    if (MC_get_client_state(c) == MC_STATE_CONNECTED) {
        return 1;
    }

    return 0;
}


//return: 0, identical; NOT 0, different.
static int MC_check_handle_is_identical(MC_topic_handle_t *messageHandlers1, MC_topic_handle_t *messageHandler2)
{
    int topicNameLen = strlen(messageHandlers1->topic_filter);

    if (topicNameLen != strlen(messageHandler2->topic_filter)) {
        return 1;
    }

    if (0 != strncmp(messageHandlers1->topic_filter, messageHandler2->topic_filter, topicNameLen)) {
        return 1;
    }

    if (messageHandlers1->handle.h_fp != messageHandler2->handle.h_fp) {
        return 1;
    }

    //context must be identical also.
    if (messageHandlers1->handle.pcontext != messageHandler2->handle.pcontext) {
        return 1;
    }

    return 0;
}


//subscribe
static iot_err_t MC_subscribe(MC_client_t *c,
                        const char *topicFilter,
                        iot_mqtt_qos_t qos,
                        iot_mqtt_event_handle_func_fpt topic_handle_func,
                        void *pcontext)
{
    if (NULL == c || NULL == topicFilter) {
        return NULL_VALUE_ERROR;
    }

    int rc = FAIL_RETURN;

    if (!MC_check_state_normal(c)) {
        TRACE_ERROR("mqtt client state is error,state = %d", MC_get_client_state(c));
        return MQTT_STATE_ERROR;
    }

    if (0 != MC_check_topic(topicFilter, TOPIC_FILTER_TYPE)) {
        TRACE_ERROR("topic format is error,topicFilter = %s", topicFilter);
        return MQTT_TOPIC_FORMAT_ERROR;
    }

    unsigned int msgId = MC_get_next_packetid(c);
    rc = MQTTSubscribe(c, topicFilter, qos, msgId, topic_handle_func, pcontext);
    if (rc != SUCCESS_RETURN) {
        if (rc == MQTT_NETWORK_ERROR) {
            MC_set_client_state(c, MC_STATE_DISCONNECTED);
        }

        TRACE_ERROR("run MQTTSubscribe error");
        return rc;
    }

    TRACE_INFO("mqtt subscribe success,topic = %s!", topicFilter);
    return msgId;
}


//unsubscribe
static iot_err_t MC_unsubscribe(MC_client_t *c, const char *topicFilter)
{
    if (NULL == c || NULL == topicFilter) {
        return NULL_VALUE_ERROR;
    }

    if (0 != MC_check_topic(topicFilter, TOPIC_FILTER_TYPE)) {
        TRACE_ERROR("topic format is error,topicFilter = %s", topicFilter);
        return MQTT_TOPIC_FORMAT_ERROR;
    }

    int rc = FAIL_RETURN;

    if (!MC_check_state_normal(c)) {
        TRACE_ERROR("mqtt client state is error,state = %d", MC_get_client_state(c));
        return MQTT_STATE_ERROR;
    }

    unsigned int msgId = MC_get_next_packetid(c);

    rc = MQTTUnsubscribe(c, topicFilter, msgId);
    if (rc != SUCCESS_RETURN) {
        if (rc == MQTT_NETWORK_ERROR) { // send the subscribe packet
            MC_set_client_state(c, MC_STATE_DISCONNECTED);
        }

        TRACE_ERROR("run MQTTUnsubscribe error!");
        return rc;
    }

    TRACE_INFO("mqtt unsubscribe success,topic = %s!", topicFilter);
    return msgId;
}


//publish
static iot_err_t MC_publish(MC_client_t *c, const char *topicName, iot_mqtt_topic_info_pt topic_msg)
{
    uint16_t msg_id = 0;

    if (NULL == c || NULL == topicName || NULL == topic_msg) {
        return NULL_VALUE_ERROR;
    }

    if (0 != MC_check_topic(topicName, TOPIC_NAME_TYPE)) {
        TRACE_ERROR("topic format is error,topicFilter = %s", topicName);
        return MQTT_TOPIC_FORMAT_ERROR;
    }

    int rc = FAIL_RETURN;

    if (!MC_check_state_normal(c)) {
        TRACE_ERROR("mqtt client state is error,state = %d", MC_get_client_state(c));
        return MQTT_STATE_ERROR;
    }

    if (topic_msg->qos == IOT_MQTT_QOS1 || topic_msg->qos == IOT_MQTT_QOS2) {
        msg_id = MC_get_next_packetid(c);
        topic_msg->packet_id = msg_id;
    }

    rc = MQTTPublish(c, topicName, topic_msg);
    if (rc != SUCCESS_RETURN) { // send the subscribe packet
        if (rc == MQTT_NETWORK_ERROR) {
            MC_set_client_state(c, MC_STATE_DISCONNECTED);
        }
        TRACE_ERROR("MQTTPublish is error, rc = %d", rc);
        return rc;
    }

    return msg_id;
}


//get state of MQTT client
static MC_state_t MC_get_client_state(MC_client_t *pClient)
{
    IOT_FUNC_ENTRY;

    MC_state_t state;
    iot_platform_mutex_lock(pClient->lock_generic);
    state = pClient->client_state;
    iot_platform_mutex_unlock(pClient->lock_generic);

    return state;
}


//set state of MQTT client
static void MC_set_client_state(MC_client_t *pClient, MC_state_t newState)
{
    IOT_FUNC_ENTRY;

    iot_platform_mutex_lock(pClient->lock_generic);
    pClient->client_state = newState;
    iot_platform_mutex_unlock(pClient->lock_generic);
}


//set MQTT connection parameter
static int MC_set_connect_params(MC_client_t *pClient, MQTTPacket_connectData *pConnectParams)
{
    IOT_FUNC_ENTRY;

    if (NULL == pClient || NULL == pConnectParams) {
        IOT_FUNC_EXIT_RC(NULL_VALUE_ERROR);
    }

    memcpy(pClient->connect_data.struct_id, pConnectParams->struct_id, 4);
    pClient->connect_data.struct_version = pConnectParams->struct_version;
    pClient->connect_data.MQTTVersion = pConnectParams->MQTTVersion;
    pClient->connect_data.clientID = pConnectParams->clientID;
    pClient->connect_data.cleansession = pConnectParams->cleansession;
    pClient->connect_data.willFlag = pConnectParams->willFlag;
    pClient->connect_data.username = pConnectParams->username;
    pClient->connect_data.password = pConnectParams->password;
    memcpy(pClient->connect_data.will.struct_id, pConnectParams->will.struct_id, 4);
    pClient->connect_data.will.struct_version = pConnectParams->will.struct_version;
    pClient->connect_data.will.topicName = pConnectParams->will.topicName;
    pClient->connect_data.will.message = pConnectParams->will.message;
    pClient->connect_data.will.qos = pConnectParams->will.qos;
    pClient->connect_data.will.retained = pConnectParams->will.retained;

    if (pConnectParams->keepAliveInterval < KEEP_ALIVE_INTERVAL_DEFAULT_MIN) {
        pClient->connect_data.keepAliveInterval = KEEP_ALIVE_INTERVAL_DEFAULT_MIN;
    } else if (pConnectParams->keepAliveInterval > KEEP_ALIVE_INTERVAL_DEFAULT_MAX) {
        pClient->connect_data.keepAliveInterval = KEEP_ALIVE_INTERVAL_DEFAULT_MAX;
    } else {
        pClient->connect_data.keepAliveInterval = pConnectParams->keepAliveInterval;
    }

    IOT_FUNC_EXIT_RC(SUCCESS_RETURN);
}


//Initialize MQTT client
static iot_err_t MC_init(MC_client_t *pClient, iot_mqtt_param_t *pInitParams)
{
    IOT_FUNC_ENTRY;
    int rc = FAIL_RETURN;

    if ((NULL == pClient) || (NULL == pInitParams)) {
        IOT_FUNC_EXIT_RC(NULL_VALUE_ERROR);
    }

    memset(pClient, 0x0, sizeof(MC_client_t));

    MQTTPacket_connectData connectdata = MQTTPacket_connectData_initializer;

    connectdata.MQTTVersion = MC_MQTT_VERSION;
    connectdata.keepAliveInterval = pInitParams->keepalive_interval_ms / 1000;

    connectdata.clientID.cstring = (char *)pInitParams->client_id;
    connectdata.username.cstring = (char *)pInitParams->user_name;
    connectdata.password.cstring = (char *)pInitParams->password;


    memset(pClient->sub_handle, 0, MC_SUB_NUM_MAX * sizeof(MC_topic_handle_t));

    pClient->packet_id = 0;
    pClient->lock_generic = iot_platform_mutex_create();
    pClient->lock_list_sub = iot_platform_mutex_create();
    pClient->lock_list_pub = iot_platform_mutex_create();

    if (pInitParams->request_timeout_ms < MC_REQUEST_TIMEOUT_MIN_MS
        || pInitParams->request_timeout_ms > MC_REQUEST_TIMEOUT_MAX_MS) {

        pClient->request_timeout_ms = MC_REQUEST_TIMEOUT_DEFAULT_MS;
    } else {
        pClient->request_timeout_ms = pInitParams->request_timeout_ms;
    }

    pClient->buf_send = pInitParams->pwrite_buf;
    pClient->buf_size_send = pInitParams->write_buf_size;
    pClient->buf_read = pInitParams->pread_buf;
    pClient->buf_size_read = pInitParams->read_buf_size;

    pClient->handle_event.h_fp = pInitParams->handle_event.h_fp;
    pClient->handle_event.pcontext = pInitParams->handle_event.pcontext;

    /* Initialize reconnect parameter */
    pClient->reconnect_param.reconnect_time_interval_ms = MC_RECONNECT_INTERVAL_MIN_MS;

    pClient->list_pub_wait_ack = list_new();
    pClient->list_pub_wait_ack->free = iot_platform_free;
    pClient->list_sub_wait_ack = list_new();
    pClient->list_sub_wait_ack->free = iot_platform_free;

    pClient->lock_write_buf = iot_platform_mutex_create();


    /* Initialize MQTT connect parameter */
    rc = MC_set_connect_params(pClient, &connectdata);
    if (SUCCESS_RETURN != rc) {
        MC_set_client_state(pClient, MC_STATE_INVALID);
        IOT_FUNC_EXIT_RC(rc);
    }

    iot_timer_init(&pClient->next_ping_time);
    iot_timer_init(&pClient->reconnect_param.reconnect_next_time);

    pClient->ipstack = (iot_network_pt)iot_platform_malloc(sizeof(iot_network_t));
    if (NULL == pClient->ipstack) {
        TRACE_ERROR("malloc Network failed");
        IOT_FUNC_EXIT_RC(FAIL_RETURN);
    }
    memset(pClient->ipstack, 0x0, sizeof(iot_network_t));

    rc = iot_net_init(pClient->ipstack, pInitParams->host, pInitParams->port, pInitParams->pub_key);
    if (SUCCESS_RETURN != rc) {
        MC_set_client_state(pClient, MC_STATE_INVALID);
        IOT_FUNC_EXIT_RC(rc);
    }

    MC_set_client_state(pClient, MC_STATE_INITIALIZED);
    TRACE_INFO("MQTT init success!");
    IOT_FUNC_EXIT_RC(SUCCESS_RETURN);
}


//remove node of list of wait subscribe ACK, which is in invalid state or timeout
static int MQTTSubInfoProc(MC_client_t *pClient)
{
    int rc = SUCCESS_RETURN;

    iot_platform_mutex_lock(pClient->lock_list_sub);
    do {
        if (0 == pClient->list_sub_wait_ack->len) {
            break;
        }

        list_iterator_t *iter;
        list_node_t *node = NULL;
        list_node_t *tempNode = NULL;
        uint16_t packet_id = 0;
        enum msgTypes msg_type;

        if (NULL == (iter = list_iterator_new(pClient->list_sub_wait_ack, LIST_TAIL))) {
            TRACE_ERROR("new list failed");
            iot_platform_mutex_lock(pClient->lock_list_sub);
            return SUCCESS_RETURN;
        }

        for (;;) {
            node = list_iterator_next(iter);

            if (NULL != tempNode) {
                list_remove(pClient->list_sub_wait_ack, tempNode);
                tempNode = NULL;
            }

            if (NULL == node) {
                break; //end of list
            }

            MC_subsribe_info_t *subInfo = (MC_subsribe_info_t *) node->val;
            if (NULL == subInfo) {
                TRACE_ERROR("node's value is invalid!");
                tempNode = node;
                continue;
            }

            //remove invalid node
            if (MC_NODE_STATE_INVALID == subInfo->node_state) {
                tempNode = node;
                continue;
            }

            if (MC_get_client_state(pClient) != MC_STATE_CONNECTED) {
                continue;
            }

            //check the request if timeout or not
            if (iot_timer_spend(&subInfo->sub_start_time) <= (pClient->request_timeout_ms * 2)) {
                //continue to check the next node
                continue;
            }

            /* When arrive here, it means timeout to wait ACK */
            packet_id = subInfo->msg_id;
            msg_type = subInfo->type;

            iot_platform_mutex_unlock(pClient->lock_list_sub);

            //Wait MQTT SUBSCRIBE ACK timeout
            if (NULL != pClient->handle_event.h_fp) {
                iot_mqtt_event_msg_t msg;

                if (SUBSCRIBE == msg_type) {
                    //subscribe timeout
                    msg.event_type = IOT_MQTT_EVENT_SUBCRIBE_TIMEOUT;
                    msg.msg = (void *)packet_id;
                } else /*if (UNSUBSCRIBE == msg_type)*/ {
                    //unsubscribe timeout
                    msg.event_type = IOT_MQTT_EVENT_UNSUBCRIBE_TIMEOUT;
                    msg.msg = (void *)packet_id;
                }

                pClient->handle_event.h_fp(pClient->handle_event.pcontext, pClient, &msg);
            }

            iot_platform_mutex_lock(pClient->lock_list_sub);

            tempNode = node;
        }

        list_iterator_destroy(iter);

    } while (0);

    iot_platform_mutex_unlock(pClient->lock_list_sub);

    return rc;
}


static void MC_keepalive(MC_client_t *pClient)
{
    int rc = 0;

    /*Periodic sending ping packet to detect whether the network is connected*/
    MC_keepalive_sub(pClient);

    MC_state_t currentState = MC_get_client_state(pClient);
    do {
        /*if Exceeds the maximum delay time, then return reconnect timeout*/
        if (MC_STATE_DISCONNECTED_RECONNECTING == currentState) {
            /*Reconnection is successful, Resume regularly ping packets*/
            iot_platform_mutex_lock(pClient->lock_generic);
            pClient->ping_mark = 0;
            iot_platform_mutex_unlock(pClient->lock_generic);
            rc = MC_handle_reconnect(pClient);
            if (SUCCESS_RETURN != rc) {
                TRACE_DEBUG("reconnect network fail, rc = %d", rc);
            } else {
                TRACE_INFO("network is reconnected!");
                MC_reconnect_callback(pClient);
                pClient->reconnect_param.reconnect_time_interval_ms = MC_RECONNECT_INTERVAL_MIN_MS;
            }

            break;
        }

        /*If network suddenly interrupted, stop pinging packet, try to reconnect network immediately*/
        if (MC_STATE_DISCONNECTED == currentState) {
            TRACE_ERROR("network is disconnected!");
            MC_disconnect_callback(pClient);

            pClient->reconnect_param.reconnect_time_interval_ms = MC_RECONNECT_INTERVAL_MIN_MS;
            iot_timer_countdown(&(pClient->reconnect_param.reconnect_next_time),
                        pClient->reconnect_param.reconnect_time_interval_ms);

            pClient->ipstack->disconnect(pClient->ipstack);
            MC_set_client_state(pClient, MC_STATE_DISCONNECTED_RECONNECTING);
            break;
        }

    } while (0);
}


//republish
static int MQTTRePublish(MC_client_t *c, char *buf, int len)
{
    iot_timer_t timer;
    iot_timer_init(&timer);
    iot_timer_countdown(&timer, c->request_timeout_ms);

    iot_platform_mutex_lock(c->lock_write_buf);

    if (MC_send_packet(c, buf, len, &timer) != SUCCESS_RETURN) {
        iot_platform_mutex_unlock(c->lock_write_buf);
        return MQTT_NETWORK_ERROR;
    }

    iot_platform_mutex_unlock(c->lock_write_buf);
    return SUCCESS_RETURN;
}


//remove node of list of wait publish ACK, which is in invalid state or timeout
static int MQTTPubInfoProc(MC_client_t *pClient)
{
    int rc = 0;
    MC_state_t state = MC_STATE_INVALID;

    iot_platform_mutex_lock(pClient->lock_list_pub);
    do {
        if (0 == pClient->list_pub_wait_ack->len) {
            break;
        }

        list_iterator_t *iter;
        list_node_t *node = NULL;
        list_node_t *tempNode = NULL;

        if (NULL == (iter = list_iterator_new(pClient->list_pub_wait_ack, LIST_TAIL))){
            TRACE_ERROR("new list failed");
            break; 
        }  

        for (;;) {
            node = list_iterator_next(iter);

            if (NULL != tempNode) {
                list_remove(pClient->list_pub_wait_ack, tempNode);
                tempNode = NULL;
            }

            if (NULL == node) {
                break; //end of list
            }

            MC_pub_info_t *repubInfo = (MC_pub_info_t *) node->val;
            if (NULL == repubInfo) {
                TRACE_ERROR("node's value is invalid!");
                tempNode = node;
                continue;
            }

            //remove invalid node
            if (MC_NODE_STATE_INVALID == repubInfo->node_state) {
                tempNode = node;
                continue;
            }

            state = MC_get_client_state(pClient);
            if (state != MC_STATE_CONNECTED) {
                continue;
            }

            //check the request if timeout or not
            if (iot_timer_spend(&repubInfo->pub_start_time) <= (pClient->request_timeout_ms * 2)) {
                continue;
            }

            //If wait ACK timeout, republish
            iot_platform_mutex_unlock(pClient->lock_list_pub);
            rc = MQTTRePublish(pClient, repubInfo->buf, repubInfo->len);
            iot_timer_start(&repubInfo->pub_start_time);
            iot_platform_mutex_lock(pClient->lock_list_pub);

            if (MQTT_NETWORK_ERROR == rc) {
                MC_set_client_state(pClient, MC_STATE_DISCONNECTED);
                break;
            }
        }

        list_iterator_destroy(iter);

    } while (0);

    iot_platform_mutex_unlock(pClient->lock_list_pub);

    return SUCCESS_RETURN;
}


//connect
static int MC_connect(MC_client_t *pClient)
{
    IOT_FUNC_ENTRY;
    int rc = FAIL_RETURN;

    if (NULL == pClient) {
        IOT_FUNC_EXIT_RC(NULL_VALUE_ERROR);
    }

    /*Establish TCP or TLS connection*/
    rc = pClient->ipstack->connect(pClient->ipstack);
    if (SUCCESS_RETURN != rc) {
        pClient->ipstack->disconnect(pClient->ipstack);
        TRACE_ERROR("TCP or TLS Connection failed");

        if (ERROR_CERTIFICATE_EXPIRED == rc) {
            TRACE_ERROR("certificate is expired!");
            return ERROR_CERT_VERIFY_FAIL;
        } else {
            return MQTT_NETWORK_CONNECT_ERROR;
        }
    }

    TRACE_DEBUG("start MQTT connection with parameters: clientid=%s, username=%s, password=%s",
            pClient->connect_data.clientID.cstring,
            pClient->connect_data.username.cstring,
            pClient->connect_data.password.cstring);

    rc = MQTTConnect(pClient);
    if (rc  != SUCCESS_RETURN) {
        pClient->ipstack->disconnect(pClient->ipstack);
        TRACE_ERROR("send connect packet failed");
        return rc;
    }

    if (SUCCESS_RETURN != MC_wait_CONNACK(pClient)) {
        (void)MQTTDisconnect(pClient);
        pClient->ipstack->disconnect(pClient->ipstack);
        TRACE_ERROR("wait connect ACK timeout, or receive a ACK indicating error!");
        return MQTT_CONNECT_ERROR;
    }

    MC_set_client_state(pClient, MC_STATE_CONNECTED);

    iot_timer_countdown(&pClient->next_ping_time, pClient->connect_data.keepAliveInterval * 1000);

    TRACE_INFO("mqtt connect success!");
    return SUCCESS_RETURN;
}


static int MC_attempt_reconnect(MC_client_t *pClient)
{
    IOT_FUNC_ENTRY;

    int rc;

    TRACE_INFO("reconnect params:MQTTVersion =%d clientID =%s keepAliveInterval =%d username = %s",
                   pClient->connect_data.MQTTVersion,
                   pClient->connect_data.clientID.cstring,
                   pClient->connect_data.keepAliveInterval,
                   pClient->connect_data.username.cstring);

    /* Ignoring return code. failures expected if network is disconnected */
    rc = MC_connect(pClient);

    if (SUCCESS_RETURN != rc) {
        TRACE_ERROR("run iot_mqtt_connect() error!");
        return rc;
    }

    return SUCCESS_RETURN;
}


//reconnect
static int MC_handle_reconnect(MC_client_t *pClient)
{
    IOT_FUNC_ENTRY;

    if (NULL == pClient) {
        return NULL_VALUE_ERROR;
    }

    if (!iot_timer_is_expired(&(pClient->reconnect_param.reconnect_next_time))) {
        /* Timer has not expired. Not time to attempt reconnect yet. Return attempting reconnect */
        return FAIL_RETURN;
    }

    TRACE_INFO("start reconnect");
#if 0
    //REDO AUTH before each reconnection
    if (0 != iot_auth(iot_get_device_info(), iot_get_user_info())) {
        TRACE_ERROR("run iot_auth() error!\n");
        return -1;
    }

    int rc = FAIL_RETURN;
    rc = MC_attempt_reconnect(pClient);
    if (SUCCESS_RETURN == rc) {
        MC_set_client_state(pClient, MC_STATE_CONNECTED);
        return SUCCESS_RETURN;
    } else {
        /*if reconnect network failed, then increase currentReconnectWaitInterval,
        ex: init currentReconnectWaitInterval=1s,  reconnect failed then 2s .4s. 8s*/
        if (MC_RECONNECT_INTERVAL_MAX_MS > pClient->reconnect_param.reconnect_time_interval_ms) {
            pClient->reconnect_param.reconnect_time_interval_ms *= 2;
        } else {
            pClient->reconnect_param.reconnect_time_interval_ms = MC_RECONNECT_INTERVAL_MAX_MS;
        }
    }

    iot_timer_countdown(&(pClient->reconnect_param.reconnect_next_time),
                pClient->reconnect_param.reconnect_time_interval_ms);

    TRACE_ERROR("mqtt reconnect failed rc = %d", rc);
#endif
    return SUCCESS_RETURN;
}


//disconnect
static int MC_disconnect(MC_client_t *pClient)
{
    if (NULL == pClient) {
        IOT_FUNC_EXIT_RC(NULL_VALUE_ERROR);
    }

    if (!MC_check_state_normal(pClient)) {
        return SUCCESS_RETURN;
    }

    IOT_FUNC_ENTRY;

    (void)MQTTDisconnect(pClient);

    /*close tcp/ip socket or free tls resources*/
    pClient->ipstack->disconnect(pClient->ipstack);

    MC_set_client_state(pClient, MC_STATE_INITIALIZED);

    TRACE_INFO("mqtt disconnect!");
    return SUCCESS_RETURN;
}



static void MC_disconnect_callback(MC_client_t *pClient)
{
    IOT_FUNC_ENTRY;

    if (NULL != pClient->handle_event.h_fp) {
        iot_mqtt_event_msg_t msg;
        msg.event_type = IOT_MQTT_EVENT_DISCONNECT;
        msg.msg = NULL;

        pClient->handle_event.h_fp(pClient->handle_event.pcontext,
                                   pClient,
                                   &msg);
    }
}


//release MQTT resource
static int MC_release(MC_client_t *pClient)
{
    IOT_FUNC_ENTRY;

    if (NULL == pClient) {
        IOT_FUNC_EXIT_RC(NULL_VALUE_ERROR);
    }

    //iot_delete_thread(pClient);
    iot_platform_msleep(100);

    MC_disconnect(pClient);
    MC_set_client_state(pClient, MC_STATE_INVALID);
    iot_platform_msleep(100);

    iot_platform_mutex_destroy(pClient->lock_generic);
    iot_platform_mutex_destroy(pClient->lock_list_sub);
    iot_platform_mutex_destroy(pClient->lock_list_pub);
    iot_platform_mutex_destroy(pClient->lock_write_buf);

    list_destroy(pClient->list_pub_wait_ack);
    list_destroy(pClient->list_sub_wait_ack);

    if (NULL != pClient->ipstack) {
        iot_platform_free(pClient->ipstack);
    }

    TRACE_INFO("mqtt release!");
    IOT_FUNC_EXIT_RC(SUCCESS_RETURN);
}


static void MC_reconnect_callback(MC_client_t *pClient)
{
    IOT_FUNC_ENTRY;

    /*handle callback function*/
    if (NULL != pClient->handle_event.h_fp) {
        iot_mqtt_event_msg_t msg;
        msg.event_type = IOT_MQTT_EVENT_RECONNECT;
        msg.msg = NULL;

        pClient->handle_event.h_fp(pClient->handle_event.pcontext,
                                   pClient,
                                   &msg);
    }
}


static int MC_keepalive_sub(MC_client_t *pClient)
{
    IOT_FUNC_ENTRY;

    int rc = SUCCESS_RETURN;

    if (NULL == pClient) {
        return NULL_VALUE_ERROR;
    }

    /*if in disabled state, without having to send ping packets*/
    if (!MC_check_state_normal(pClient)) {
        return SUCCESS_RETURN;
    }
 
    /*if there is no ping_timer timeout, then return success*/
    if (!iot_timer_is_expired(&pClient->next_ping_time)) {
        return SUCCESS_RETURN;
    }

    //update to next time sending MQTT keep-alive
    iot_timer_countdown(&pClient->next_ping_time, pClient->connect_data.keepAliveInterval * 1000);

    rc = MQTTKeepalive(pClient);
    if (SUCCESS_RETURN != rc) {
        if (rc == MQTT_NETWORK_ERROR) {
            MC_set_client_state(pClient, MC_STATE_DISCONNECTED);
        }
        TRACE_ERROR("ping outstanding is error,result = %d", rc);
        return rc;
    }

    TRACE_INFO("send MQTT ping...");

    iot_platform_mutex_lock(pClient->lock_generic);
    pClient->ping_mark = 1;
    iot_platform_mutex_unlock(pClient->lock_generic);

    return SUCCESS_RETURN;
}



/************************  Public Interface ************************/
void *iot_mqtt_construct(iot_mqtt_param_t *pInitParams)
{
    iot_err_t err;
    MC_client_t *pclient = (MC_client_t *)iot_platform_malloc(sizeof(MC_client_t));
    if (NULL == pclient) {
        TRACE_ERROR("not enough memory.")
        return NULL;
    }

    err = MC_init( pclient, pInitParams );
    if (SUCCESS_RETURN != err) {
        iot_platform_free(pclient);
        return NULL;
    }

    err = MC_connect(pclient);
    if (SUCCESS_RETURN != err) {
        MC_release(pclient);
        iot_platform_free(pclient);
        return NULL;
    }

    return pclient;
}


int iot_mqtt_deconstruct(void *handle)
{
    if (NULL == handle) {
        return NULL_VALUE_ERROR;
    }

    MC_release((MC_client_t *)handle);

    iot_platform_free(handle);

    return SUCCESS_RETURN;
}


void iot_mqtt_yield(void *handle, int timeout_ms)
{
    int rc = SUCCESS_RETURN;
    MC_client_t *pClient = (MC_client_t *)handle;
    iot_timer_t time;

    iot_timer_init(&time);
    iot_timer_countdown(&time, timeout_ms);

    do {
        /*acquire package in cycle, such as PINGRESP  PUBLISH*/
        rc = MC_cycle(pClient, &time);
        if (SUCCESS_RETURN == rc) {
            //check list of wait publish ACK to remove node that is ACKED or timeout
            MQTTPubInfoProc(pClient);

            //check list of wait subscribe(or unsubscribe) ACK to remove node that is ACKED or timeout
            MQTTSubInfoProc(pClient);
        }

        //Keep MQTT alive or reconnect if connection abort.
        MC_keepalive(pClient);

    } while (!iot_timer_is_expired(&time) && (SUCCESS_RETURN == rc));
}


//check whether MQTT connection is established or not.
bool iot_mqtt_check_state_normal(void *handle)
{
    return MC_check_state_normal((MC_client_t *)handle);
}


int32_t iot_mqtt_subscribe(void *handle,
                const char *topic_filter,
                iot_mqtt_qos_t qos,
                iot_mqtt_event_handle_func_fpt topic_handle_func,
                void *pcontext)
{
    return MC_subscribe((MC_client_t *)handle, topic_filter, qos, topic_handle_func, pcontext);
}


int32_t iot_mqtt_unsubscribe(void *handle, const char *topic_filter)
{
    return MC_unsubscribe((MC_client_t *)handle, topic_filter);
}


int32_t iot_mqtt_publish(void *handle, const char *topic_name, iot_mqtt_topic_info_pt topic_msg)
{
    return MC_publish((MC_client_t *)handle, topic_name, topic_msg);
}


