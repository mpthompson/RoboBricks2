/*
    Copyright (c) 2007 Michael P. Thompson <mpthompson@gmail.com>

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    $Id$
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include "avrx.h"
#include "config.h"
#include "rb2.h"
#include "usart.h"

#define LED PORTC
#define LEDDDR DDRC

// Declare the control blocks needed for timers.
TimerControlBlock   timer1;
TimerControlBlock   timer2;

// External tasks.
AVRX_GCC_TASK(rb2_task, 100, 2);

AVRX_SIGINT(SIG_OUTPUT_COMPARE0A)
// Timer 0 Compare Match A Interrupt Handler
//
// This is the system tick handler.
//
// Prototypical Interrupt handler:
// . Switch to kernel context
// . handle interrupt
// . switch back to interrupted context.
{
    // Switch to kernel stack/context.
    IntProlog();                

    // Call time queue manager.
    AvrXTimerHandler();

    // Return to tasks
    Epilog();
}


AVRX_GCC_TASKDEF(task1, 8, 3)
// Task 1 simply flashes the light off for 1/5 second and 
// then on for 4/5th for a 1 second cycle time.
{
    // Loop forever.
    while (1)
    {
        // Delay for 800 ms.
        AvrXStartTimer(&timer1, 800);
        AvrXWaitTimer(&timer1);

        // Switch LED on.
        LED = LED ^ 0x01;

        // Delay for 200 ms.
        AvrXStartTimer(&timer1, 200);
        AvrXWaitTimer(&timer1);

        // Switch LED off.
        LED = LED ^ 0x01;
    }
}


AVRX_GCC_TASKDEF(task2, 8, 2)
// Task 2 cycles the light on/off over 4 seconds.  It uses a simplified
// form of the delay API.
{
    while (1)
    {
        // Two second delay.
        AvrXDelay(&timer2, 2000);

        LED = LED ^ 0x02;
    }
}


int main(void)                 // Main runs under the AvrX Stack
{
    AvrXSetKernelStack(0);

    // Initialize the USART.
    usart_init();

    // Enable sleep mode.
    SMCR = (1<<SE);

    // Initialize the system tick timer.
    TCNT0 = 0;
    OCR0A = CPUCLK / 256 / TICKRATE;                            // Set tick rate.
    TCCR0A = ((1<<WGM01) | (0<<WGM00));                         // Normal port operation. CTC mode.
    TCCR0B = ((0<<WGM02) | (1<<CS02) | (0<<CS01) | (0<<CS00));  // Clk/256.
    TIMSK0 = ((0<<OCIE0B) | (1<<OCIE0A) | (0<<TOIE0));          // Interrupt on output compare match A.

    LEDDDR = 0xFF;
    LED   = 0x00;

    AvrXRunTask(TCB(task1));
    AvrXRunTask(TCB(task2));
    AvrXRunTask(TCB(rb2_task));

    // Need for access to semaphore.
    AvrXSetSemaphore(&EEPromMutex);

    // Switch from AvrX Stack to first task.
    Epilog();
    while(1);
}


