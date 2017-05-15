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

    This task module polls the User I/O board for button input and to
    update the LEDs.  All buttons are processed in other tasks.
*/

#include <stdint.h>
#include <avr/io.h>
#include "avrx.h"
#include "encoder.h"
#include "usart.h"

// Task control.
AVRX_MUTEX(encoder_mutex);

// Note: Assuming globals are zeroed.
static int16_t prev_left_encoder;
static int16_t prev_right_encoder;
static int16_t left_encoder_delta;
static int16_t right_encoder_delta;
static int32_t left_encoder_pos;
static int32_t right_encoder_pos;

void encoder_init(void)
// Initialize the encoder module.
{
    // Prime the encoder mutex.
    AvrXSetSemaphore(&encoder_mutex);

    // Grab access to the USART.
    usart_grab_access();

    // Select the Shaft2-D module.
    usart_xmit_discard_echo(0x0105);

    // Get and validate the response.
    if (usart_recv() == 0x00A5)
    {
        // Clear the rotation count for each shaft.
        usart_xmit_discard_echo(0x0001);
    }

    // Release access to the USART.
    usart_release_access();
}


void encoder_update(void)
// Update values from the encoder module.
{
    int16_t left_encoder = 0;
    int16_t right_encoder = 0;

    // Grab access to the USART.
    usart_grab_access();

    // Select the Shaft2-D module.
    usart_xmit_discard_echo(0x0105);

    // Get and validate the response.
    if (usart_recv() == 0x00A5)
    {
        // Latch the current rotation count for both shafts.
        usart_xmit_discard_echo(0x0000);

        // Get left encoder high byte.
        usart_xmit_discard_echo(0x0002);

        // Receive left encoder high byte.
        left_encoder = usart_recv();

        // Get left encoder low byte.
        usart_xmit_discard_echo(0x0004);

        // Receive left encoder low byte.
        left_encoder = (left_encoder << 8) | usart_recv();

        // Get right encoder high byte.
        usart_xmit_discard_echo(0x0003);

        // Receive right encoder high byte.
        right_encoder = usart_recv();

        // Get right encoder low byte.
        usart_xmit_discard_echo(0x0004);

        // Receive right encoder low byte.
        right_encoder = (right_encoder << 8) | usart_recv();
    }

    // Release access to the USART.
    usart_release_access();

    // We invert the position of the left motor to account for the
    // fact that the wheels are geomtrically opposed to each other
    // and we want the positive encoder value to move in the forward 
    // direction.
    left_encoder = -left_encoder;

    // Get exclusive access to the encoder values.
    AvrXWaitSemaphore(&encoder_mutex);

    // Determine the encoder deltas.
    left_encoder_delta = left_encoder - prev_left_encoder;
    right_encoder_delta = right_encoder - prev_right_encoder;

    // Add the deltas to the encoder positions.
    left_encoder_pos += left_encoder_delta;
    right_encoder_pos += right_encoder_delta;

    // Update the previous positions.
    prev_left_encoder = left_encoder;
    prev_right_encoder = right_encoder;

    // Release exclusive access to the encoder values.
    AvrXSetSemaphore(&encoder_mutex);
}


void encoder_get_positions(int32_t *left_pos, int32_t *right_pos)
// Get the encoder postion from last update.  We use 32 bit integers to
// keep track of large movements over long time spans.
{
    // Get exclusive access to the encoder values.
    AvrXWaitSemaphore(&encoder_mutex);

    // Get encoder postions.
    *left_pos = left_encoder_pos;
    *right_pos = right_encoder_pos;

    // Release exclusive access to the encoder values.
    AvrXSetSemaphore(&encoder_mutex);
}


void encoder_get_deltas(int16_t *left_delta, int16_t *right_delta)
// Get the encoder deltas between updates.  We assume that the 
// delta between updates can fit within a signed 16 bit quantity.
{
    // Get exclusive access to the encoder values.
    AvrXWaitSemaphore(&encoder_mutex);

    // Get encoder postions.
    *left_delta = left_encoder_delta;
    *right_delta = right_encoder_delta;

    // Release exclusive access to the encoder values.
    AvrXSetSemaphore(&encoder_mutex);
}

