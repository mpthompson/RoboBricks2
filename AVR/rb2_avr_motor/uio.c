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

// Keys state.
static uint8_t uio_buttons_buffer;

// Task control.
AVRX_MUTEX(uio_leds_mutex);
AVRX_MUTEX(uio_buttons_signal);
AVRX_TIMER(uio_buttons_timeout);
AVRX_TIMER(uio_timer);


void uio_leds_set(uint8_t leds)
// Set the indicated LEDs.
{
    // Get exclusive access to the LEDs.
    AvrXWaitSemaphore(&uio_leds_mutex);

    // Set the leds that should be on.
    uio_leds_on |= leds;

    // These leds should no longer be blinking.
    uio_leds_blinking &= ~leds;

    // Give up exclusive access to the LEDs.
    AvrXSetSemaphore(&uio_leds_mutex);
}


void uio_leds_blink(uint8_t leds)
// Blink the indicated LEDs.
{
    // Get exclusive access to the LEDs.
    AvrXWaitSemaphore(&uio_leds_mutex);

    // Set the leds that should blink.
    uio_leds_blinking |= leds;

    // These leds should no longer be on.
    uio_leds_on &= ~leds;

    // Give up exclusive access to the LEDs.
    AvrXSetSemaphore(&uio_leds_mutex);
}


void uio_leds_reset(uint8_t leds)
// Reset the indicated LEDs.
{
    // Get exclusive access to the LEDs.
    AvrXWaitSemaphore(&uio_leds_mutex);

    // These leds should no longer be on.
    uio_leds_on &= ~leds;

    // These leds should no longer be blinking.
    uio_leds_blinking &= ~leds;

    // Give up exclusive access to the LEDs.
    AvrXSetSemaphore(&uio_leds_mutex);
}


static void uio_send_leds(uint8_t command, uint8_t leds)
// Sends the LEDs to the user I/O module.
{
    // Send the set LEDs command.
    usart_xmit(command);

    // Discard the echo.
    usart_recv(250);

    // Get the response.
    usart_recv(250);

    // Send the LEDs to be set.
    usart_xmit(leds);

    // Discard the echo.
    usart_recv(250);

    // Get the response.
    usart_recv(250);
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




NAKEDFUNC(uio_task)
// Task for user I/O module.
{
    uint16_t response;

    // Prime the LEDs mutex.
    AvrXSetSemaphore(&uio_leds_mutex);

    // Loop to periodically to update the button state.
    for (;;)
    {
        // Delay for 100 milliseconds.
        AvrXDelay(&uio_timer, 100);

        // Grab access to the USART.
        usart_grab_access();

        // Select the I/O module.
        usart_xmit(0x0130);

        // Discard the echo.
        usart_recv(250);

        // Get the response.
        response = usart_recv(250);

        // Validate the response.
        if (response == 0x00A5)
        {
            // Send the set LEDs command.
            uio_send_leds(0x0001, uio_leds_on);

            // Send the set LEDs command.
            uio_send_leds(0x0002, uio_leds_blinking);

            // Send the set LEDs command.
            uio_send_leds(0x0003, ~(uio_leds_on | uio_leds_blinking));

            // Get the button state.
            usart_xmit(0x0004);

            // Discard the echo.
            usart_recv(250);

            // Receive the response which is the button press.
            response = usart_recv(250);

            // Did we receive a button press?
            if ((response > 0) && (response < 6))
            {
                // Buffer the button.
                uio_buttons_buffer = (uint8_t) response;

                // Signal the next button.
                AvrXSetObjectSemaphore((pMutex) &uio_buttons_timeout);
            }
        }

        // Release access to the USART.
        usart_release_access();
    }
}


