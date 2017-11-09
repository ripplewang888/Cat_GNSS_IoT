
#include <string.h>
#include "error_handler.h"
#include "trace.h"
#include "iot_device.h"


static iot_user_info_t iot_user_info;
static iot_device_info_t iot_device_info;


int iot_device_init(void)
{
    memset(&iot_device_info, 0x0, sizeof(iot_device_info_t));
    memset(&iot_user_info, 0x0, sizeof(iot_user_info_t));

    TRACE_INFO("device init success!");
    return SUCCESS_RETURN;
}


int32_t iot_set_device_info(
            const char *product_key,
            const char *device_name,
            const char *device_secret)
{
    int ret;
    TRACE_DEBUG("start to set device info!");
    memset(&iot_device_info, 0x0, sizeof(iot_device_info));

    strncpy(iot_device_info.product_key, product_key, PRODUCT_KEY_LEN);
    strncpy(iot_device_info.device_name, device_name, DEVICE_NAME_LEN);
    strncpy(iot_device_info.device_secret, device_secret, DEVICE_SECRET_LEN);

    //construct device-id(@product_key+@device_name)
    ret = snprintf(iot_device_info.device_id, DEVICE_ID_LEN, "%s.%s", product_key, device_name);
    if ((ret < 0) || (ret >= DEVICE_ID_LEN)) {
        TRACE_ERROR("set device info failed");
        return FAIL_RETURN;
    }

    TRACE_DEBUG("set device info successfully!");
    return SUCCESS_RETURN;
}

iot_device_info_pt iot_get_device_info(void)
{
    return &iot_device_info;
}


iot_user_info_pt iot_get_user_info(void)
{
    return &iot_user_info;
}


