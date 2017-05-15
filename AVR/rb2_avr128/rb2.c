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
#include "rb2.h"
#include "usart.h"

// Note: Assume globals are zeroed.

// Serial state variables.
static uint16_t rb2_data;
static uint8_t rb2_address;
static uint8_t rb2_id_index;
static uint8_t rb2_address_pending;
static uint8_t rb2_selected;

// The length of the following serial id string.
#define ID_LENGTH    26

// The serial string is stored in flash memory.
const uint8_t rb2_id_string[] PROGMEM = "\x10\x00\x1d\x01\x03" "\x0b" "AVR-A ROBOT" "\x08" "Thompson";

static void rb2_xmit_data(uint16_t data)
// Send the data word.
{
    // Send the data and discard the echo.
    usart_xmit_discard_echo(data);
}


static uint16_t rb2_recv_data(void)
// Receive the next data word.  Returns error if the data
// word is actually an address.
{
    // Do not receive more data if an address is pending.
    if (!rb2_address_pending)
    {
        // Read the serial data ignoring any timeout.
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
        // Read the serial data ignoring any timeout.
        rb2_data = usart_recv();
    }

    // Reset the address pending flag.
    rb2_address_pending = 0;

    return rb2_data;
}


static uint16_t rb2_recv_default(void)
// Receive the next data word.
{
    // Do not receive more data if an address is pending.
    if (!rb2_address_pending)
    {
        // Read the serial data.
        rb2_data = usart_recv_default();
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


NAKEDFUNC(rb2_task)
// Task to process the RoboBricks2 protocol.
{
    uint16_t data;

    // Read the serial address from EEPROM.
    rb2_address = AvrXReadEEProm((unsigned char *) 0);

    // Loop in the unselected state.
    for (;;)
    {
        // Wait for serial data that other tasks are not reading.
        data = rb2_recv_default();

        // Does this character select us?
        rb2_selected = (data == (0x0100 | rb2_address)) ? 1 : 0;

        // Send the response if selected.
        if (rb2_selected)
        {
            // Obtain exclusive access to the USART.
            usart_grab_access();

            // Send the response.
            rb2_xmit_data(0x00A5);

            // Loop in the selected state.
            while (rb2_selected)
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
                    rb2_selected = 0;
                }
                else if (data == 0xff)
                {
                    // We are being deselected.

                    // Send response.
                    rb2_xmit_data(0x0000);

                    // We are no longer selected.
                    rb2_selected = 0;
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

            // Release exclusive access to the USART.
            usart_release_access();
        }
    }
}

