
#ifndef _TCP_TASK_H_
#define _TCP_TASK_H_

#define  TCP_SEND_MESSAGE_TIMEOUT  2000



void tcp_recv_task(void *pvParameters);
void tcp_send_task(void *pvParameters);

#endif /* _TCP_TASK_H_ */
