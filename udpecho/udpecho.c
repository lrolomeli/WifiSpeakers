/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
#include "udpecho.h"
#include "lwip/opt.h"

#if LWIP_NETCONN

#include "lwip/api.h"
#include "lwip/sys.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MK64F12.h"
#include "fsl_dac.h"
#include "fsl_pit.h"

void background(unsigned short int * buffer);

unsigned short int bufferA[400];
unsigned short int bufferB[400];

unsigned short int * back_buffer = bufferA;
unsigned short int * active_buffer = bufferB;

unsigned char empty_buffer = 0;

static unsigned short index = 0;

void PIT0_IRQHandler()
{
	PIT_ClearStatusFlags(PIT, kPIT_Chnl_0, kPIT_TimerFlag);

	/* DAC value to be send */
	DAC_SetBufferValue(DAC0, 0U, (uint16_t) active_buffer[index]);

	if(index == 399)
	{
		index = 0;
		empty_buffer = 1;
	}
	else
	{
		index++;
	}

}

static void server_thread(void *arg)
{

	struct netconn *conn;
	struct netbuf *buf;

	unsigned short int *msg;

	uint16_t len;

	LWIP_UNUSED_ARG(arg);
	conn = netconn_new(NETCONN_UDP);
	netconn_bind(conn, IP_ADDR_ANY, 50001);
	//LW_IP_ERROR("udpecho: invalid conn", (conn != NULL), return;);

	while (1)
	{

		netconn_bind(conn, IP_ADDR_ANY, 50001);
		netconn_recv(conn, &buf);
		netbuf_data(buf, (void**) &msg, &len);
		background(msg);
		netbuf_delete(buf);

	}

}

void udpecho_init(void)
{

	sys_thread_new("server", server_thread, NULL, 300, 1);

}

void background(unsigned short int * buffer)
{
	unsigned short i;
	static unsigned char buffer_ptr = 0;

	/* Fills the back buffer while foreground sends to DAC */
	for (i = 0; i < 400; i++)
	{
		back_buffer[i] = buffer[i];
	}

	if(empty_buffer)
	{

		empty_buffer = 0;

		if(buffer_ptr)
		{
			buffer_ptr = 0;
			active_buffer = bufferA;
			back_buffer = bufferB;
		}

		else
		{
			buffer_ptr = 1;
			active_buffer = bufferB;
			back_buffer = bufferA;
		}

	}

}


#endif /* LWIP_NETCONN */
