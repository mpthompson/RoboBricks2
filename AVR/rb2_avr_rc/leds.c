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
#include <stdint.h>
#include "avrx.h"
#include "leds.h"

static uint8_t leds_on;
static uint8_t leds_blinking;
static uint8_t leds_count;

// Declare the control block to blink LEDs.
TimerControlBlock blink_timer;

// Declare the LEDs mutex.
AVRX_MUTEX(leds_mutex);

void leds_set(uint8_t leds)
// Set the indicated LEDs.
{
    // Mask out the unused bits.
    leds &= 0x3f;

    // Get exclusive access to the LEDs.
    AvrXWaitSemaphore(&leds_mutex);

    // Set the leds that should be on.
    leds_on |= leds;

    // These leds should no longer be blinking.
    leds_blinking &= ~leds;

    // Clear the LEDs.
    PORTC &= ~0x3f;

    // Show the LEDs that should be on.
    PORTC |= leds_on;

    // Also turn on LEDs that are blinking.
    if (leds_count & 0x01) PORTC |= leds_blinking;

    // Give up exclusive access to the LEDs.
    AvrXSetSemaphore(&leds_mutex);
}


void leds_blink(uint8_t leds)
// Blink the indicated LEDs.
{
    // Mask out the unused bits.
    leds &= 0x3f;

    // Get exclusive access to the LEDs.
    AvrXWaitSemaphore(&leds_mutex);

    // Set the leds that should blink.
    leds_blinking |= leds;

    // These leds should no longer be on.
    leds_on &= ~leds;

    // Clear the LEDs.
    PORTC &= ~0x3f;

    // Show the LEDs that should be on.
    PORTC |= leds_on;

    // Also turn of LEDs that are blinking.
    if (leds_count & 0x01) PORTC |= leds_blinking;

    // Give up exclusive access to the LEDs.
    AvrXSetSemaphore(&leds_mutex);
}


void leds_reset(uint8_t leds)
// Reset the indicated LEDs.
{
    // Mask out the unused bits.
    leds &= 0x3f;

    // Get exclusive access to the LEDs.
    AvrXWaitSemaphore(&leds_mutex);

    // These leds should no longer be on.
    leds_on &= ~leds;

    // These leds should no longer be blinking.
    leds_blinking &= ~leds;

    // Clear the LEDs.
    PORTC &= ~0x3f;

    // Show the LEDs that should be on.
    PORTC |= leds_on;

    // Also turn of LEDs that are blinking.
    if (leds_count & 0x01) PORTC |= leds_blinking;

    // Give up exclusive access to the LEDs.
    AvrXSetSemaphore(&leds_mutex);
}


NAKEDFUNC(leds_task)
// Task to process LEDs.
{
    // Initialize the state.
    leds_on = 0;
    leds_blinking = 0;
    leds_count = 0;

    // Set port C to output.
    DDRC |= 0x3f;

    // Clear all LEDs.
    PORTC &= ~0x3f;

    // Prime the LEDs mutex.
    AvrXSetSemaphore(&leds_mutex);

    // Loop to periodically update the blinking LEDs.
    for (;;)
    {
        // Delay for 250 milliseconds.
        AvrXDelay(&blink_timer, 250);

        // Get exclusive access to the LEDs.
        AvrXWaitSemaphore(&leds_mutex);

        // Increment the LED count.
        ++leds_count;

        // Clear the LEDs.
        PORTC &= ~0x3f;

        // Show the LEDs that should be on.
        PORTC |= leds_on;

        // Also turn of LEDs that are blinking.
        if (leds_count & 0x01) PORTC |= leds_blinking;

        // Give up exclusive access to the LEDs.
        AvrXSetSemaphore(&leds_mutex);
    }
}


