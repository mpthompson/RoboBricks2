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

    RoboBricks2 Protocol Module
*/

#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "avrx.h"
#include "config.h"
#include "bootloader.h"
#include "buttons.h"
#include "leds.h"
#include "rb2.h"
#include "receiver.h"
#include "usart.h"

// Serial state variables.
static uint16_t rb2_data;
static uint8_t rb2_address;
static uint8_t rb2_id_index;
static uint8_t rb2_address_pending;

// The length of the following serial id string.
#define ID_LENGTH    24

// The serial string is stored in flash memory.
const uint8_t rb2_id_string[] PROGMEM = "\x10\x00\x1d\x01\x03" "\x09" "AVR-A UIO" "\x08" "Thompson";

static void rb2_xmit_data(uint16_t data)
// Send the data word.
{
    // Send the data.
    usart_xmit(data);

    // Discard echo.
    usart_recv();
}


static uint16_t rb2_recv_data(void)
// Receive the next data word.  Returns error if the data
// word is actually an address.
{
    // Do not receive more data if an address is pending.
    if (!rb2_address_pending)
    {
        // Read the serial data.
        rb2_data = usart_recv();
    }

    // Update the address pending flag.
    rb2_address_pending = ((rb2_data & 0xff00) == 0x0100) ? 1 : 0;

    // Return error if we have an address pending.
    return rb2_address_pending ? -1 : rb2_data;
}


static uint16_t rb2_recv_data_or_address(void)
// Receive the next data word.
{
    // Do not receive more data if an address is pending.
    if (!rb2_address_pending)
    {
        // Read the serial data.
        rb2_data = usart_recv();
    }

    // Reset the address pending flag.
    rb2_address_pending = 0;

    return rb2_data;
}


static void rb2_address_set(void)
//  Handle the slave address set command.
{
    uint16_t data;
    uint8_t address_update;

    // Send the response.
    rb2_xmit_data(rb2_address);

    // Wait for serial data.
    data = rb2_recv_data();

    // Make sure no error.
    if (data != -1)
    {
        // Save the address to be set.
        address_update = (uint8_t) data;

        // Send the address to be set as the response.
        rb2_xmit_data(data);

        // Wait for serial data.
        data = rb2_recv_data();

        // Make sure no error.
        if (data != -1)
        {
            // Do the two addresses match?
            if (address_update == (uint8_t) data)
            {
                // Update the serial address in memory.
                rb2_address = address_update;

                // Update the serial address in EEPROM.
                AvrXWriteEEProm((void *) 0, address_update);

                // Send the OK response.
                rb2_xmit_data(0x00A5);
            }
            else
            {
                // Send the error response.
                rb2_xmit_data(0x0000);
            }
        }
    }
}


static void rb2_set_leds(void)
//  Handle the set LEDs command.
{
    uint16_t data;

    // Send the response.
    rb2_xmit_data(0x00A5);

    // Wait for serial data.
    data = rb2_recv_data();

    // Make sure no error.
    if (data != -1)
    {
        // Set the indicated LEDs.
        leds_set((uint8_t) data);

        // Send the response.
        rb2_xmit_data(data);
    }
}



static void rb2_blink_leds(void)
//  Handle the blink LEDs command.
{
    uint16_t data;

    // Send the response.
    rb2_xmit_data(0x00A5);

    // Wait for serial data.
    data = rb2_recv_data();

    // Make sure no error.
    if (data != -1)
    {
        // Blink the indicated LEDs.
        leds_blink((uint8_t) data);

        // Send the response.
        rb2_xmit_data(data);
    }
}


static void rb2_reset_leds(void)
//  Handle the reset LEDs command.
{
    uint16_t data;

    // Send the response.
    rb2_xmit_data(0x00A5);

    // Wait for serial data.
    data = rb2_recv_data();

    // Make sure no error.
    if (data != -1)
    {
        // Reset the indicated LEDs.
        leds_reset((uint8_t) data);

        // Send the response.
        rb2_xmit_data(data);
    }
}


NAKEDFUNC(rb2_task)
// Task to process the RoboBricks2 protocol.
{
    uint16_t data;
    uint16_t result;
    uint8_t selected;

    // Initialize the serial state.
    rb2_data = -1;
    rb2_id_index = 0;
    rb2_address_pending = 0;

    // Read the serial address from EEPROM.
    rb2_address = AvrXReadEEProm((unsigned char *) 0);

    // Initial state is unselected.
    selected = 0;

    // Set the USART into address only mode.
    usart_address_only(1);

    // Loop in the unselected state.
    for (;;)
    {
        // Wait for serial data or address.
        data = rb2_recv_data_or_address();

        // Does this character select us?
        selected = (data == (0x0100 | rb2_address)) ? 1 : 0;

        // Send the response if selected.
        if (selected)
        {
            // Set the USART into address/data mode.
            usart_address_only(0);

            // Send the response.
            rb2_xmit_data(0x00A5);

            // Loop in the selected state.
            while (selected)
            {
                // Wait for serial data or address.
                data = rb2_recv_data_or_address();

                // Handle the serial data.
                if (data == -1)
                {
                    // Ignore errors.
                }
                else if (data == (0x0100 | rb2_address))
                {
                    // We are being reselected.

                    // Send response.
                    rb2_xmit_data(0x00A5);
                }
                else if (data & 0x0100)
                {
                    // Another module is being selected.

                    // We are no longer selected.
                    selected = 0;
                }
                else if (data == 0x00)
                {
                    // Reset all LEDs.
                    leds_reset(0x3f);

                    // Send response.
                    rb2_xmit_data(0x00A5);
                }
                else if (data == 0x01)
                {
                    // Set the indicated LEDs.
                    rb2_set_leds();
                }
                else if (data == 0x02)
                {
                    // Blink the indicated LEDs.
                    rb2_blink_leds();
                }
                else if (data == 0x03)
                {
                    // Reset the indicated LEDs.
                    rb2_reset_leds();
                }
                else if (data == 0x04)
                {
                    // Get the next button.
                    result = (uint16_t) buttons_get();

                    // Send response.
                    rb2_xmit_data(result);
                }
                else if ((data >= 0x05) && (data <= 0x0a))
                {
                    // Get the indicated channel.
                    result = receiver_read(data - 0x05);

                    // Send the channel position.
                    rb2_xmit_data(result);
                }
                else if (data == 0xff)
                {
                    // We are being deselected.

                    // Send response.
                    rb2_xmit_data(0x0000);

                    // We are no longer selected.
                    selected = 0;
                }
                else if (data == 0xfe)
                {
                    // We received ID START command.

                    // Reset the serial id index.
                    rb2_id_index = 0;

                    // Send response which is lenght of ID string.
                    rb2_xmit_data(ID_LENGTH);
                }
                else if (data == 0xfd)
                {
                    // We received ID NEXT command.

                    // Verify the id index and send the data from flash.
                    rb2_xmit_data(rb2_id_index < ID_LENGTH ? (uint16_t) pgm_read_byte(&rb2_id_string[rb2_id_index++]) : 0x0000);
                }
                else if (data == 0xfc)
                {
                    // We received ADDRESS SET command.
                    rb2_address_set();
                }
                else if (data == 0xfb)
                {
                    // We received BOOTLOADER ENTER command.

                    // Send response.
                    rb2_xmit_data(0x00A5);

                    // Start the bootloader immediately.
                    bootloader_start();
                }
                else if (data == 0xfa)
                {
                    // We received BOOTLOADER EXIT command.

                    // We are already out of the bootloader so just send a response.
                    rb2_xmit_data(0x00A5);
                }
            }

            // Set the USART into address only mode.
            usart_address_only(1);
        }
    }
}

