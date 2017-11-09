#include "iot_platform.h"
#include "iot_timer.h"

void iot_timer_start(iot_timer_t *timer)
{
    timer->time = iot_platform_time_get_ms();
}

uint32_t iot_timer_spend(iot_timer_t *start)
{
    uint32_t now, res;

    now = iot_platform_time_get_ms();
    res = now - start->time;
    return res;
}

uint32_t iot_timer_left(iot_timer_t *end)
{
    uint32_t now, res;

    if (iot_timer_is_expired(end)) {
        return 0;
    }

    now = iot_platform_time_get_ms();
    res = end->time - now;
    return res;
}

uint32_t iot_timer_is_expired(iot_timer_t *timer)
{
    uint32_t cur_time = iot_platform_time_get_ms();

    /*
     *  WARNING: Do NOT change the following code until you know exactly what it do!
     *
     *  check whether it reach destination time or not.
     */
    if ((cur_time - timer->time) < (UINT32_MAX / 2)) {
        return 1;
    } else {
        return 0;
    }
}

void iot_timer_init(iot_timer_t *timer)
{
    timer->time = 0;
}

void iot_timer_countdown(iot_timer_t *timer, uint32_t millisecond)
{
    //ALIOT_ASSERT(millisecond < (UINT32_MAX / 2), "time should NOT exceed UINT32_MAX/2!");
    timer->time = iot_platform_time_get_ms() + millisecond;
}

uint32_t iot_timer_get_ms(void)
{
    return iot_platform_time_get_ms();
}

