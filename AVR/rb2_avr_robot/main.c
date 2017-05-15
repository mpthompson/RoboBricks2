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
#include "bootloader.h"
#include "rb2.h"
#include "usart.h"

// External tasks.
AVRX_GCC_TASK(rb2_task, 50, 1);
AVRX_GCC_TASK(control_task, 200, 2);
AVRX_GCC_TASK(ui_task, 200, 3);

AVRX_SIGINT(SIG_OUTPUT_COMPARE0)
// System tick handler.
{
    // Switch to kernel stack/context.
    IntProlog();                

    // Call time queue manager.
    AvrXTimerHandler();

    // Return to tasks
    Epilog();
}


int main(void)
// Main runs under the AvrX stack.
{
    AvrXSetKernelStack(0);

    // Initialize the USART module.
    usart_init();

    // Enable sleep mode.
    MCUCR = (1<<SE);

    // Initialize the system tick timer.
    TCNT0 = 0;
    OCR0 = CPUCLK / 256 / TICKRATE;                             // Set tick rate.
    TCCR0 = (0<<FOC0) |                                         // No force output compare.
            (1<<WGM01) | (0<<WGM00) |                           // CTC mode.
            (0<<COM01) | (0<<COM00) |                           // Normal port operation.
            (1<<CS02) | (1<<CS01) | (0<<CS00);                  // Clk/256.
    TIMSK |= (1<<OCIE0);                                        // Interrupt on output compare.

    // Need for access to EEPROM semaphore.
    AvrXSetSemaphore(&EEPromMutex);

    // Run the tasks.
    AvrXRunTask(TCB(rb2_task));
    AvrXRunTask(TCB(control_task));
    AvrXRunTask(TCB(ui_task));

    // Switch from AvrX stack to first task.
    Epilog();

    while(1);
}


