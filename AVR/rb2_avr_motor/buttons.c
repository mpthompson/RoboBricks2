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
#include "buttons.h"
#include "click.h"

// Buttons mutex.
AVRX_MUTEX(buttons_mutex);

// Buttons timer control block.
TimerControlBlock buttons_timer;

static uint8_t buttons_head = 0;
static uint8_t buttons_tail = 0;
static uint8_t buttons_queue[4];

uint8_t buttons_get(void)
// De-queue the next pending button.
{
    uint8_t button = BUTTON_NONE;

    // Get exclusive access to the button queue.
    AvrXWaitSemaphore(&buttons_mutex);

    // Is there a button in the queue?
    if (buttons_tail != buttons_head)
    {
        // Get the button from the queue.
        button = buttons_queue[buttons_tail];

        // Increment the tail of the queue.
        buttons_tail = (buttons_tail + 1) & 0x03;
    }

    // Give up exclusive access to the button queue.
    AvrXSetSemaphore(&buttons_mutex);

    return button;
}


static void buttons_put(uint8_t button)
// Queue up the button pressed.
{
    uint8_t next_head;

    // Get exclusive access to the button queue.
    AvrXWaitSemaphore(&buttons_mutex);

    // Determine the next head within the queue.
    next_head = (buttons_head + 1) & 0x03;

    // Have we wrapped the queue?
    if (next_head != buttons_tail)
    {
        // Yes. Add the button to the queue.
        buttons_queue[buttons_head] = button;

        // Increment the head of the queue.
        buttons_head = next_head;
    }

    // Give up exclusive access to the button queue.
    AvrXSetSemaphore(&buttons_mutex);
}


NAKEDFUNC(buttons_task)
// Task to process buttons.
{
    uint8_t current_sample;
    static uint8_t vcount_bit1 = 0;
    static uint8_t vcount_bit0 = 0;
    static uint8_t previous_state = 0;
    static uint8_t previous_sample = 0;
    static uint8_t buttons_pressed = 0;
    static uint8_t debounced_state = 0;

    // Initialize the hardware. Set port D to inputs with internal pull-up resistors.
    DDRD &= ~((1<<DDD2) | (1<<DDD3) | (1<<DDD4) | (1<<DDD5) | (1<<DDD6));
    PORTD |= ((1<<PD2) | (1<<PD3) | (1<<PD4) | (1<<PC5) | (1<<PC6));

    // Prime the button queue mutex.
    AvrXSetSemaphore(&buttons_mutex);

    // Loop to periodically to update the button state.
    for (;;)
    {
        // Delay for 5 milliseconds.
        AvrXDelay(&buttons_timer, 5);

        // Read the current button sample.
        current_sample = ~PIND & 0x7c;

        // Decrement the vertical counter and reset if button state changes
        // The counter starts at 0 then counts 3, 2, 1 and back to zero at 
        // which point it sets the appropriate debounced state bit.

        // These two lines reset the counter if the current sample 
        // indicates a stage change from the debounced state.
        vcount_bit1 &= ~(current_sample ^ previous_sample);
        vcount_bit0 &= ~(current_sample ^ previous_sample);
        previous_sample = current_sample;

        // These two lines implement a 2-bit vertical backwards counter.  
        // vcount_bit1 is the high bit and vcount_bit0 is the low bit.
        vcount_bit1 ^= ~vcount_bit0;
        vcount_bit0 = ~vcount_bit0;

        // If the backwards counter has reached zero we change the values of 
        // the debounced state to current sample.  This is done by first
        // clearing the debounced_state and then setting it to current_sample.
        debounced_state &= (vcount_bit1 | vcount_bit0);
        debounced_state |= (current_sample & ~(vcount_bit0 | vcount_bit1));

        // Reset the buttons pressed.
        buttons_pressed = 0;

        // Capture when buttons have been pressed.
        buttons_pressed |= (debounced_state & ~previous_state);
        previous_state = debounced_state;

        // Should we click?
        if (buttons_pressed) click();

        // Queue up the buttons that have changed.
        if (buttons_pressed & 0x04) buttons_put(BUTTON_UP);
        if (buttons_pressed & 0x08) buttons_put(BUTTON_RIGHT);
        if (buttons_pressed & 0x10) buttons_put(BUTTON_LEFT);
        if (buttons_pressed & 0x20) buttons_put(BUTTON_CENTER);
        if (buttons_pressed & 0x40) buttons_put(BUTTON_DOWN);
    }
}


