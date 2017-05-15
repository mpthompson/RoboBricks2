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
#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include "avrx.h"
#include "uio.h"
#include "usart.h"

// Note: Assuming globals are zeroed.

// LEDs state.
static uint8_t uio_leds_on;
static uint8_t uio_leds_blinking;

// Buttons state.
static uint8_t uio_buttons_buffer;

// RC state.
static int8_t uio_rc_chan1;
static int8_t uio_rc_chan2;

// Task control.
AVRX_MUTEX(uio_mutex);
AVRX_TIMER(uio_buttons_timeout);

void uio_leds_set(uint8_t leds)
// Set the indicated LEDs.
{
    // Get exclusive access to the user I/O data.
    AvrXWaitSemaphore(&uio_mutex);

    // Set the leds that should be on.
    uio_leds_on |= leds;

    // These leds should no longer be blinking.
    uio_leds_blinking &= ~leds;

    // Release exclusive access to the user I/O data.
    AvrXSetSemaphore(&uio_mutex);
}


void uio_leds_blink(uint8_t leds)
// Blink the indicated LEDs.
{
    // Get exclusive access to the user I/O data.
    AvrXWaitSemaphore(&uio_mutex);

    // Set the leds that should blink.
    uio_leds_blinking |= leds;

    // These leds should no longer be on.
    uio_leds_on &= ~leds;

    // Release exclusive access to the user I/O data.
    AvrXSetSemaphore(&uio_mutex);
}


void uio_leds_reset(uint8_t leds)
// Reset the indicated LEDs.
{
    // Get exclusive access to the user I/O data.
    AvrXWaitSemaphore(&uio_mutex);

    // These leds should no longer be on.
    uio_leds_on &= ~leds;

    // These leds should no longer be blinking.
    uio_leds_blinking &= ~leds;

    // Release exclusive access to the user I/O data.
    AvrXSetSemaphore(&uio_mutex);
}

INTERFACE void AvrXSetObjectSemaphore(pMutex);
INTERFACE void AvrXWaitObjectSemaphore(pMutex);
INTERFACE Mutex AvrXTestObjectSemaphore(pMutex);

uint8_t uio_next_button(uint16_t timeout)
// Get the next button or BUTTON_NONE if timeout expired.  A timeout of
// zero indicates that no timeout should be used.
{
    uint8_t rv = BUTTON_NONE;

    // Is a button already waiting?
    if ((AvrXTestObjectSemaphore((pMutex) &uio_buttons_timeout) != SEM_DONE) && (timeout > 0))
    {
        // Start the timer.
        AvrXStartTimer(&uio_buttons_timeout, timeout);

        // Wait for timer or signal to wake up task.
        AvrXWaitTimer(&uio_buttons_timeout);

        // If we cancel the timer it will return non-zero if the timer
        // is still on the queue.  This indicates we were woken by a signal
        // rather than the expiration of the timer and a button is ready.
        if (AvrXCancelTimer(&uio_buttons_timeout) != 0) rv = uio_buttons_buffer;

        // Reset the timer semaphore after being canceled.
        uio_buttons_timeout.semaphore = SEM_PEND;
    }
    else
    {
        // Wait for the button signal without timeout.
        AvrXWaitObjectSemaphore((pMutex) &uio_buttons_timeout);

        // Return the button.
        rv = uio_buttons_buffer;
    }

    return rv;
}


void uio_get_rc(int8_t *chan1, int8_t *chan2)
// Get the RC channel 1 (left/right) and channel 3 (forwards/backwards) values.
{
    // Return the channel 1 and channel 3 values.
    if (chan1) *chan1 = uio_rc_chan1;
    if (chan2) *chan2 = uio_rc_chan2;
}


void uio_init(void)
// User I/O module init function.
{
    // Prime the mutex.
    AvrXSetSemaphore(&uio_mutex);
}


static void uio_send_leds(uint8_t command, uint8_t leds)
// Sends the LEDs to the user I/O module.
{
    // Send the set LEDs command.
    usart_xmit_discard_echo(command);

    // Get the response.
    usart_recv();

    // Send the LEDs to be set.
    usart_xmit_discard_echo(leds);

    // Get the response.
    usart_recv();
}


void uio_update(void)
// User I/O module update function.
{
    static uint16_t response;

    // Grab access to the USART.
    usart_grab_access();

    // Select the I/O module.
    usart_xmit_discard_echo(0x0130);

    // Get the response.
    response = usart_recv();

    // Validate the response.
    if (response == 0x00A5)
    {
        // Get exclusive access to the user I/O data.
        AvrXWaitSemaphore(&uio_mutex);

        // Send the set LEDs command.
        uio_send_leds(0x0001, uio_leds_on);

        // Send the set LEDs command.
        uio_send_leds(0x0002, uio_leds_blinking);

        // Send the set LEDs command.
        uio_send_leds(0x0003, ~(uio_leds_on | uio_leds_blinking));

        // Release exclusive access to the user I/O data.
        AvrXSetSemaphore(&uio_mutex);

        // Get the button state.
        usart_xmit_discard_echo(0x0004);

        // Receive the response which is the button press.
        response = usart_recv();

        // Did we receive a button press?
        if ((response > 0) && (response < 6))
        {
            // Buffer the button.
            uio_buttons_buffer = (uint8_t) response;

            // Signal the next button.
            AvrXSetObjectSemaphore((pMutex) &uio_buttons_timeout);
        }

        // IMU RC Channel 1

        // Get the channel 1 byte.
        usart_xmit_discard_echo(0x05);

        // Receive the channel 1 byte.
        uio_rc_chan1 = (int8_t) usart_recv();

        // IMU RC Channel 2

        // Get the channel 3 byte.
        usart_xmit_discard_echo(0x06);

        // Receive the channel 2 byte.
        uio_rc_chan2 = (int8_t) usart_recv();
    }

    // Release access to the USART.
    usart_release_access();
}


