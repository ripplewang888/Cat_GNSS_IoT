

#ifndef _IOT_TIMER_H_
#define _IOT_TIMER_H_


typedef struct {
    uint32_t time;
} iot_timer_t;


void iot_timer_start(iot_timer_t *timer);

uint32_t iot_timer_spend(iot_timer_t *start);

uint32_t iot_timer_left(iot_timer_t *end);

uint32_t iot_timer_is_expired(iot_timer_t *timer);

void iot_timer_init(iot_timer_t *timer);

void iot_timer_countdown(iot_timer_t *timer, uint32_t millisecond);

uint32_t iot_timer_get_ms(void);

#endif /* _IOT_TIMER_H_ */
