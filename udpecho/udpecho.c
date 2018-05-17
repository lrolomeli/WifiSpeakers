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
#include "tcpecho/tcpecho.h"
#if LWIP_NETCONN

#include "lwip/api.h"
#include "lwip/sys.h"
#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MK64F12.h"
#include "fsl_dac.h"
#include "fsl_pit.h"

#define BUFFER_SIZE 400
#define PACKAGE_PER_SEC 109
#define HALF_OFFSET				0x7FF
//#define STORE_SIZE				5
#define STORE_SIZE				4
enum{
	LOWPASS = 1, HIGHPASS, BANDPASS, ALL
};

void background(unsigned short int * buffer);
void filter(unsigned char filter_type);

//const static float as_low[STORE_SIZE] = {1, 1.4911, 1.197, 0.445, 0.0694};
const static float as_low[STORE_SIZE] = {1, 1.1029, 0.6595, 0.1276};
//const static float bs_low[STORE_SIZE] = {0.2626, 1.05064, 1.5759, 1.0506, 0.2626};
const static float bs_low[STORE_SIZE] = {0.3612, 1.0837, 1.0837, 0.3612};
//const static float as_high[STORE_SIZE] = {1, -2.1284, 1.9524, -0.8442, 0.1439};
const static float as_high[STORE_SIZE] = {1, -1.57894, 1.01207, -0.2271};
//const static float bs_high[STORE_SIZE] = {0.3793, -1.5172, 2.2759, -1.5172, 0.3793};
const static float bs_high[STORE_SIZE] = {0.4772, -1.4318, 1.4318, -0.4772};
const static float bs_band[7] = {0.1370, 0, -0.4109, 0, 0.4109, 0, -0.1370};
const static float as_band[7] = {1.0000, -0.5398, 0.3368, -0.2006, 0.3750, -0.0687, 0.0212};



static float x[7] = {0};
static float y[7] = {0};
static float yout = 0;
static float yout1 = 0;
static float yout2 = 0;
static float yout3 = 0;

static float amp1 = 0;
static float amp2 = 0;
static float amp3 = 0;

unsigned short int bufferA[BUFFER_SIZE];
unsigned short int bufferB[BUFFER_SIZE];

unsigned short int * back_buffer = bufferA;
unsigned short int * active_buffer = bufferB;

volatile unsigned short index = 0;
unsigned char package_counter = 0;
unsigned char quality = 0;
volatile unsigned char buffer_ptr = 0;
static unsigned char filter_type = 0;

/**********************************************
 *	PIT IRQ HANDLER
 *
 * Basically to have more power and less time
 * wasted, we are doing almost every decision
 * to play on Digital Analog Converter
 * specific samples here in the Periodic
 * Interrupt Timer Handler.
 *
 * Down here it is explained every statement
 * and why it is not in a task.
 *
 *********************************************/
void PIT0_IRQHandler()
{


	if(PIT_GetStatusFlags(PIT, kPIT_Chnl_0))
	{
		/* Clear PIT Hardware flag */
		PIT_ClearStatusFlags(PIT, kPIT_Chnl_0, kPIT_TimerFlag);

		/* Once index get to BUFFER_SIZE, it means the buffer is empty or we should not
		 * send more values through the digital analog converter. */
		if(index < BUFFER_SIZE)
		{
			filter(filter_type);

			/* DAC value to be send */
			//DAC_SetBufferValue(DAC0, 0U, (uint16_t) yout);
			index++;
		}

		/* Instead we should be sending zeros or something more valid
		 * as a value equals to the average of the 12 bit DAC */
		else
		{

			/* This code here works as a ping-pong buffer alternating
			 * pointers to buffers when active buffer is empty*/
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

			/* at this point buffer should be full again */
			index=0;
		}
	}

	if(PIT_GetStatusFlags(PIT, kPIT_Chnl_1))
	{
		PIT_ClearStatusFlags(PIT, kPIT_Chnl_1, kPIT_TimerFlag);
		quality = (unsigned char)(((float)package_counter / (float)PACKAGE_PER_SEC) * 100);
		package_counter = 0;
	}
}

/**********************************************
 *	UDP SERVER TASK
 *
 *
 *
 *********************************************/
static void server_thread(void *arg)
{

	struct netconn *conn;
	struct netbuf *buf;

	unsigned short int *msg;
	static unsigned char port_switch = 1;

	uint16_t len;

	LWIP_UNUSED_ARG(arg);
	conn = netconn_new(NETCONN_UDP);
	//LW_IP_ERROR("udpecho: invalid conn", (conn != NULL), return;);
	netconn_bind(conn, IP_ADDR_ANY, 50001);
	PIT_StartTimer(PIT, kPIT_Chnl_1);
	while (1)
	{
		if (get_port_flag())
		{
			set_port_flag();
			if (port_switch)
			{
				netconn_bind(conn, IP_ADDR_ANY, 50001);
				port_switch = 0;
			}
			else
			{
				netconn_bind(conn, IP_ADDR_ANY, 50003);
				port_switch = 1;
			}
		}
		netconn_recv(conn, &buf);
		package_counter++;
		netbuf_data(buf, (void**) &msg, &len);
		background(msg);
		netbuf_delete(buf);
	}
}

/**********************************************
 *	Creation of the server task in UDP
 *
 *********************************************/
void udpecho_init(void)
{

	sys_thread_new("server", server_thread, NULL, 200, 2);

}

/**********************************************
 *	Fills the buffer every time a UDP package
 *	enters and interrupts.
 *
 *********************************************/
void background(unsigned short int * buffer)
{
	unsigned short fill_index;

	/* Fills the back buffer while foreground sends to DAC */
	for (fill_index = 0; fill_index < BUFFER_SIZE; fill_index++)
	{
		back_buffer[fill_index] = buffer[fill_index];
	}

}

/**********************************************
 *	Returns the value percentage previously
 *	calculated every second in channel 1 PIT0
 *	HANDLER.
 *
 *********************************************/
unsigned char get_quality(void)
{

	return quality;

}

void filter(unsigned char filter_type)
{
	unsigned char i = 0;

	for(i = 6; i > 0; i--)
	{
		x[i]= x[i-1];
	}

	x[0] = active_buffer[index] - HALF_OFFSET;

		for(i = 0; i < 3; i++)
		{
			yout1 += (bs_low[i]*x[i]) - (as_low[i+1]*y[i]);

		}
		yout1 = yout1 * amp1;
		yout1 *= .55;

		for(i = 0; i < 3; i++)
		{
			yout2 += (bs_high[i]*x[i]) - (as_high[i+1]*y[i]);

		}
		yout2 = yout2 * amp2;
		yout2 *= .30;

		for(i = 0; i < 6; i++)
		{
			yout3 += (bs_band[i]*x[i]) - (as_band[i+1]*y[i]);
		}
		yout3 = yout3 * amp3;
		yout3 *= .55;

	yout = yout1 + yout2 + yout3;

	yout += HALF_OFFSET;

	/* DAC value to be send */
	DAC_SetBufferValue(DAC0, 0U, (uint16_t) yout);

	for(i = 6; i > 0; i--)
	{
		y[i]= y[i-1];
	}

	y[0] = yout;
	yout = 0;
}

void setfilter(void)
{
	filter_type = (filter_type == ALL) ? 0 : filter_type + 1;
}

void volumefreq1(void)
{

	amp1 += 0.33;
	if(amp1 > 1)
	{
		amp1 = 0;
	}
}

void volumefreq2(void)
{

	amp2 += 0.33;
	if(amp2 > 1)
	{
		amp2 = 0;
	}
}

void volumefreq3(void)
{

	amp3 += 0.33;
	if(amp3 > 1)
	{
		amp3 = 0;
	}
}

#endif /* LWIP_NETCONN */
