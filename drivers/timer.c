/*
    drivers/timer.c: Openchronos TA0 timer driver

    Copyright (C) 2012 Angelo Arrifano <miknix@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/*	  Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/


	  Redistribution and use in source and binary forms, with or without
	  modification, are permitted provided that the following conditions
	  are met:

	    Redistributions of source code must retain the above copyright
	    notice, this list of conditions and the following disclaimer.

	    Redistributions in binary form must reproduce the above copyright
	    notice, this list of conditions and the following disclaimer in the
	    documentation and/or other materials provided with the
	    distribution.

	    Neither the name of Texas Instruments Incorporated nor the names of
	    its contributors may be used to endorse or promote products derived
	    from this software without specific prior written permission.

	  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
	  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
	  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
	  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
	  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
	  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "timer.h"

/* HARDWARE TIMER ASSIGNMENT:
	 TA0CCR0: 100ms timer (TODO)
	 TA0CCR1: Unused
	 TA0CCR2: Unused
	 TA0CCR3: programable timer (TODO)
	 TA0CCR4: delay timer
	OVERFLOW: 1Hz timer */

/* source is 32kHz with /2 divider */
#define TIMER0_FREQ 16000

static volatile uint8_t delay_finished;

/* The callback list for 1Hz timer */
struct cbList {
	void (*fn)(void);
	struct cbList *next;
};
static struct cbList *timer0_1hz_queue;

/* this function setups a 1Hz timer ticked every overflow interrupt */
void timer0_init(void)
{
	/* Enable overflow interrupts */
	TA0CTL |= TAIE;

	/* select external 32kHz source, /2 divider, continous mode */
	TA0CTL |= TASSEL__ACLK | ID__2 | MC__CONTINOUS;

	/* Set queue to empty */
	timer0_1hz_queue = NULL;
}

/* This function was based on original Texas Instruments implementation,
   see LICENSE-TI for more information. */
void timer0_delay(uint16_t duration)
{
	uint16_t ticks = (TIMER0_FREQ / 1000) * duration;
	
	delay_finished = 0;

	/* Set next CCR match */
	TA0CCR4 = TA0R + ticks;

	/* enable interrupt */
	TA0CCTL4 |= CCIE;

	/* Wait for interrupt */
	while (1) {
		/* enter low power mode */
		_BIS_SR(LPM3_bits + GIE);
		__no_operation();

#ifdef USE_WATCHDOG
		/* Service watchdog */
		WDTCTL = WDTPW + WDTIS__512K + WDTSSEL__ACLK + WDTCNTCL;
#endif

		/* The interrupt routine sets delay_finished to signal us
		   that a interrupt happened */
		if (delay_finished == 1)
			break;
	}

	/* disable interrupt */
	TA0CCTL4 |= CCIE;
}

void timer0_1hz_register(void (*callback)(void))
{
	struct cbList **p = &timer0_1hz_queue;

	while (*p) {
		p = &(*p)->next;
	}

	*p = malloc(sizeof(struct cbList));
	(*p)->next = NULL;
	(*p)->fn = callback;
}

void timer0_1hz_unregister(void (*callback)(void))
{
	struct cbList *p = timer0_1hz_queue, *pp = NULL;

	while (p) {
		if (p->fn == callback) {
			if (!pp)
				timer0_1hz_queue = p->next;
			else
				pp->next = p->next;

			free(p);
		}

		pp = p;
		p = p->next;
	}
}

/* interrupt vector for CCR0 */
__attribute__((interrupt(TIMER0_A0_VECTOR)))
void timer0_A0_ISR(void)
{
	
}

/* interrupt vector for CCR1-4 and overflow */
__attribute__((interrupt(TIMER0_A1_VECTOR)))
void timer0_A1_ISR(void)
{
	/* reading TA0IV automatically resets the interrupt flag */
	uint8_t flag = TA0IV;
	
	/* delay timer */
	if (flag == TA0IV_TA0CCR4) {
		delay_finished = 1;
		return;
	}

	/* 1Hz timer, nothing to do yet */
	if (flag == TA0IV_TA0IFG) {
		/* go through the callback queue and call the functions */
		struct cbList *p = timer0_1hz_queue;

		while (p) {
			p->fn();
			p = p->next;
		}
	}
}

