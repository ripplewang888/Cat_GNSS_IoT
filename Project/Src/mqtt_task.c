#include <string.h>
#include "uart6.h"
#include "uart4.h"
#include "trace.h"
#include "lm61.h"
#include "iot_net.h"
#include "iot_device.h"
#include "MQTTClient.h"
#include "mqtt_task.h"
#include "error_handler.h"
#include "cmsis_os.h"

#define MQTT_CLIENT_ID     "pub_16101705" 
//NOTE!!! you must define the following topic in IOT consle before running this sample.
#define TOPIC_DATA            "topic1"

#define MSG_LEN_MAX         (1024)

static void event_handle(void *pcontext, void *pclient, iot_mqtt_event_msg_pt msg)
{
    uint32_t packet_id = (uint32_t)msg->msg;
    iot_mqtt_topic_info_pt topic_info = (iot_mqtt_topic_info_pt)msg->msg;

    switch (msg->event_type)
    {
    case IOT_MQTT_EVENT_UNDEF:
        TRACE_INFO("undefined event occur.");
        break;

    case IOT_MQTT_EVENT_DISCONNECT:
        TRACE_INFO("MQTT disconnect.");
        break;

    case IOT_MQTT_EVENT_RECONNECT:
        TRACE_INFO("MQTT reconnect.");
        break;

    case IOT_MQTT_EVENT_SUBCRIBE_SUCCESS:
        TRACE_INFO("subscribe success, packet-id=%u", packet_id);
        break;

    case IOT_MQTT_EVENT_SUBCRIBE_TIMEOUT:
        TRACE_INFO("subscribe wait ack timeout, packet-id=%u", packet_id);
        break;

    case IOT_MQTT_EVENT_SUBCRIBE_NACK:
        TRACE_INFO("subscribe nack, packet-id=%u", packet_id);
        break;

    case IOT_MQTT_EVENT_UNSUBCRIBE_SUCCESS:
        TRACE_INFO("unsubscribe success, packet-id=%u", packet_id);
        break;

    case IOT_MQTT_EVENT_UNSUBCRIBE_TIMEOUT:
        TRACE_INFO("unsubscribe timeout, packet-id=%u", packet_id);
        break;

    case IOT_MQTT_EVENT_UNSUBCRIBE_NACK:
        TRACE_INFO("unsubscribe nack, packet-id=%u", packet_id);
        break;

    case IOT_MQTT_EVENT_PUBLISH_SUCCESS:
        TRACE_INFO("publish success, packet-id=%u", packet_id);
        break;

    case IOT_MQTT_EVENT_PUBLISH_TIMEOUT:
        TRACE_INFO("publish timeout, packet-id=%u", packet_id);
        break;

    case IOT_MQTT_EVENT_PUBLISH_NACK:
        TRACE_INFO("publish nack, packet-id=%u", packet_id);
        break;

    case IOT_MQTT_EVENT_PUBLISH_RECVEIVED:
        TRACE_INFO("topic message arrived but without any related handle: topic=%.*s, topic_msg=%.*s",
                topic_info->topic_len,
                topic_info->ptopic,
                topic_info->payload_len,
                topic_info->payload);
        break;

    default:
        TRACE_INFO("Should NOT arrive here.");
        break;
    }
}


static void iot_mqtt_msg_publish(void *pclient)
{
  int rc = 0, msg_len;
  iot_mqtt_topic_info_t topic_msg;
  char msg_pub[128];

  uint8_t *newStr = NULL;
  size_t len = 0;	 
  len = Uart4_RcvMsg(&newStr);
  if(len == 0){
	  return;
  }  
	
	Uart4_SendMsg(newStr, len);

  /* Initialize topic information */
  memset(&topic_msg, 0x0, sizeof(iot_mqtt_topic_info_t));
  strcpy(msg_pub, "message: hello! start!");
  
  topic_msg.qos = IOT_MQTT_QOS0;
  topic_msg.retain = 0;
  topic_msg.dup = 0;
  topic_msg.payload = (void *)msg_pub;
  topic_msg.payload_len = strlen(msg_pub);

  /* Generate topic message */
  if(len > MSG_LEN_MAX) {
	msg_len = snprintf(msg_pub, MSG_LEN_MAX, "%s", newStr);
  }
  else {
  	msg_len = snprintf(msg_pub, len, "%s", newStr);
  }
  
  if (msg_len < 0) {
      TRACE_DEBUG("Error occur! Exit program");
      return;
  }
  
  topic_msg.payload = (void *)msg_pub;
  topic_msg.payload_len = msg_len;
  
  rc = iot_mqtt_publish(pclient, TOPIC_DATA, &topic_msg);
  if (rc < 0) {
      TRACE_DEBUG("error occur when publish");
      return;
  }
  TRACE_INFO("packet-id=%u, publish topic msg=%s", (uint32_t)rc, msg_pub);
}

int mqtt_task(void)
{
    int rc = 0;
    void *pclient;
    iot_user_info_pt puser_info;
    iot_mqtt_param_t mqtt_params;
    char *msg_buf = NULL, *msg_readbuf = NULL;

    if (NULL == (msg_buf = (char *)iot_platform_malloc(MSG_LEN_MAX))) {
        TRACE_DEBUG("not enough memory");
        rc = -1;
        goto do_exit;
    }

    if (NULL == (msg_readbuf = (char *)iot_platform_malloc(MSG_LEN_MAX))) {
        TRACE_DEBUG("not enough memory");
        rc = -1;
        goto do_exit;
    }

    if ( CAT1_STATUS_SUCCESS != LM61_healthCheck()) {
        TRACE_DEBUG("LM61_healthCheck failed !!!");
        rc = -1;
        goto do_exit;
    }

    TRACE_INFO("+--------------------------------+");
    TRACE_INFO("| Wait for LM61 Init(About 15 s) |");
    TRACE_INFO("+--------------------------------+");
    osDelay(15000);

    /* Initialize device info */
    iot_device_init();

    puser_info = iot_get_user_info();

    /* Initialize MQTT parameter */
    memset(&mqtt_params, 0x0, sizeof(mqtt_params));

    mqtt_params.port = 1883;
    mqtt_params.host = "120.55.163.126";
    mqtt_params.client_id = MQTT_CLIENT_ID;
    mqtt_params.user_name = puser_info->user_name;
    mqtt_params.password = puser_info->password;
    mqtt_params.pub_key = puser_info->pubKey;

    mqtt_params.request_timeout_ms = 2000;
    mqtt_params.clean_session = 0;
    mqtt_params.keepalive_interval_ms = 60000;
    mqtt_params.pread_buf = msg_readbuf;
    mqtt_params.read_buf_size = MSG_LEN_MAX;
    mqtt_params.pwrite_buf = msg_buf;
    mqtt_params.write_buf_size = MSG_LEN_MAX;

    mqtt_params.handle_event.h_fp = event_handle;
    mqtt_params.handle_event.pcontext = NULL;


    /* Construct a MQTT client with specify parameter */
    pclient = iot_mqtt_construct(&mqtt_params);
    if (NULL == pclient) {
        TRACE_DEBUG("MQTT construct failed");
        rc = -1;
        goto do_exit;
    }

    iot_platform_msleep(1000);

    TRACE_INFO("+-------------------------+");
    TRACE_INFO("| Ready to send msg Y*_*Y |");
    TRACE_INFO("+-------------------------+");

    for(;;){
        iot_mqtt_msg_publish(pclient);
        /* handle the MQTT packet received from TCP or SSL connection */
        iot_mqtt_yield(pclient, 200);
        iot_platform_msleep(1000);
    }

    iot_platform_msleep(200);

    iot_mqtt_deconstruct(pclient);

do_exit:
    if (NULL != msg_buf) {
        iot_platform_free(msg_buf);
    }

    if (NULL != msg_readbuf) {
        iot_platform_free(msg_readbuf);
    }

    return rc;
}


