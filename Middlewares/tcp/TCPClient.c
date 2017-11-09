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
#include "TCPClient.h"

#include "iot_platform.h"
#include "iot_net.h"
#include "iot_list.h"
#include "iot_timer.h"
#include "trace.h"
#include "error_handler.h"


extern TCP_client_t *pclient;    //will initialize as TCP_client_t


// TCP client
//6144*6
void delay(unsigned int nDelay){
    unsigned int i,j,k;
    for ( i=0;i<nDelay;i++ )
        for ( j=0;j<36864;j++ )
             k++;
}



//send packet
/*
static int TCP_send_packet(TCP_client_t *c, char *buf, int length, iot_timer_t *time)
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
        rc = TCP_NETWORK_ERROR;
    }
    return rc;
}

*/


//check TCP client is in normal state.
//
bool TCP_check_state_normal(TCP_client_t *c)
{
    if(c == NULL){
        TRACE_ERROR("% TCP client is null");
    }
    if ( c->client_state != TCP_STATE_CONNECTED) {
        TRACE_DEBUG("tcp  client state is wrong, state %d", c->client_state);
        return FALSE;
    }

    return TRUE;
}


//get state of TCP client
 TCP_state_t TCP_get_client_state(TCP_client_t *pClient)
{
    IOT_FUNC_ENTRY;
    TCP_state_t state;
    state = pClient->client_state;

    return state;
}


//set state of TCP client
 void TCP_set_client_state(TCP_client_t *pClient, TCP_state_t newState)
{
    IOT_FUNC_ENTRY;

    pClient->client_state = newState;
}











/*
This function to init  TCP client transparent mode
need ,host ,port
*/
int  iot_tcp_client_transparent_mode(iot_TCP_param_t *pInitParams){
    int rc  = SUCCESS_RETURN;
  
    pclient = (TCP_client_t *)iot_platform_malloc(sizeof(TCP_client_t));
    if (NULL == pclient) {
          TRACE_ERROR(" TCP client transparent mode malloc memory failed.")
          return rc  ;
      }
    memset(pclient, 0x0, sizeof(TCP_client_t));


    pclient->host = pInitParams ->host;
    pclient->port = pInitParams->port ;
    pclient->buf_send = pInitParams->pwrite_buf;
    pclient->buf_size_write = pInitParams->write_buf_size;
    pclient->buf_read = pInitParams->pread_buf;
    pclient->buf_size_read = pInitParams->read_buf_size;
    pclient->uart_locker =iot_platform_mutex_create();

    TRACE_DEBUG("pclient host =%s, port =%s", pclient->host, pclient->port);
    LM61_start_transparent_mode(pclient->host, pclient->port);

    return rc  ;
}







//check whether TCP connection is established or not.
bool iot_TCP_check_state_normal(void *handle)
{
    return TCP_check_state_normal((TCP_client_t *)handle);
}


