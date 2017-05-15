/*
    Copyright (c) 2013 Michael P. Thompson <mpthompson@gmail.com>

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
#include "avrx.h"
#include "config.h"
#include "click.h"

// The click event.
AVRX_MUTEX(click_event);

// The click timer.
AVRX_TIMER(click_timer);

void click(void)
// Click the speaker.
{
    // Indicate a click event.
    AvrXSetSemaphore(&click_event);
}


NAKEDFUNC(click_task)
// Task to process click sounds.
{
    // Initialize hardware.  Bit 1 on port B to output.
    DDRB = (1<<DDB1);
    PORTB = ~(1<<PB1);

    // Initialize the clock to generate the click.
    TCNT1 = 0;                                                  // Zero the count.
    OCR1A = CPUCLK / 256 / TICKRATE;                            // Set tick rate.
    TIMSK1 = 0;                                                 // No interrupts.
    TCCR1C = 0;
    TCCR1A = 0;
    TCCR1B = 0;

    // Loop to wait for click events.
    for (;;)
    {
        // Wait for a click event.
        AvrXWaitSemaphore(&click_event);

        // Enable the timer with a 1khz sound.
        TCNT1 = 0;
        TCCR1A = ((0<<COM1A1) | (1<<COM1A0) |                       // Toggle OC1A on compare match.
                  (0<<COM1B1) | (0<<COM1B0) |                       // OC1B disconnected.
                  (0<<WGM11) | (0<<WGM10));                         // CTC mode.
        TCCR1B = ((0<<WGM13) | (1<<WGM12) |                         // CTC mode.
                  (1<<CS12) | (0<<CS11) | (0<<CS10));               // Clk/256.

        // Delay for 20 milliseconds.
        AvrXDelay(&click_timer, 20);

        // Disable the timer.
        TCCR1B = 0;
    }
}


