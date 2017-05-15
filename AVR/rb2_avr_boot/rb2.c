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

    RoboBricks2 Protocol Module for a bootloader.
*/

#include <math.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <string.h>

#include "config.h"
#include "prog.h"
#include "rb2.h"
#include "usart.h"

// Serial state variables.
static uint16_t rb2_data;
static uint8_t rb2_address;
static uint8_t rb2_id_index;
static uint8_t rb2_address_pending;

// The length of the following serial id string.
#define ID_LENGTH    25

// The serial string is stored in flash memory.
const uint8_t rb2_id_string[] PROGMEM = "\x10\x00\x1d\x01\x03" "\x0a" "AVR-A BOOT" "\x08" "Thompson";

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
                eeprom_write_byte((void *) 0, address_update);

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


static void rb2_read_buffer(void)
//  Handle the slave read buffer command.
{
    uint16_t i;
    uint8_t hi_address;
    uint8_t lo_address;
    uint8_t data_count;
    uint16_t data;
    uint16_t address;

    // Send response.
    rb2_xmit_data(0x00A5);

    // Wait to receive the next data.
    data = rb2_recv_data();

    // Check for error.
    if (data != -1)
    {
        // Get the address hi byte.
        hi_address = (uint8_t) data;

        // Send response.
        rb2_xmit_data(hi_address);

        // Wait to receive the next data.
        data = rb2_recv_data();

        // Check for error.
        if (data != -1)
        {
            // Get the address lo byte.
            lo_address = (uint8_t) data;

            // Send response.
            rb2_xmit_data(lo_address);

            // Wait to receive the next data.
            data = rb2_recv_data();

            // Check for error.
            if (data != -1)
            {
                // Get the data count.
                data_count = (uint8_t) data;

                // Send response.
                rb2_xmit_data(data_count);

                // Combine the high address and low address into an address.
                address = (hi_address << 8) | lo_address;

                // Adjust the address if needed.
                while (address > PROG_EEPROM_END) address -= PROG_EEPROM_END;

                // Set the address to be read.
                prog_buffer_set_address(address);

                // Loop over each data to be recieved.
                for (i = 0; i < data_count; ++i)
                {
                    // Wait to receive the next data.
                    data = rb2_recv_data();

                    // Break if we received an error.
                    if (data == -1) break;

                    // Read the next byte from the buffer and send as the response.
                    rb2_xmit_data(prog_buffer_get_byte());
                }
            }
        }
    }
}


static void rb2_write_buffer(void)
//  Handle the slave write buffer command.
{
    uint16_t i;
    uint8_t hi_address;
    uint8_t lo_address;
    uint8_t data_count;
    uint16_t data;
    uint16_t address;

    // Send response.
    rb2_xmit_data(0x00A5);

    // Wait to receive the next data.
    data = rb2_recv_data();

    // Check for error.
    if (data != -1)
    {
        // Get the address hi byte.
        hi_address = (uint8_t) data;

        // Send response.
        rb2_xmit_data(hi_address);

        // Wait to receive the next data.
        data = rb2_recv_data();

        // Check for error.
        if (data != -1)
        {
            // Get the address lo byte.
            lo_address = (uint8_t) data;

            // Send response.
            rb2_xmit_data(lo_address);

            // Wait to receive the next data.
            data = rb2_recv_data();

            // Check for error.
            if (data != -1)
            {
                // Get the data count.
                data_count = (uint8_t) data;

                // Send response.
                rb2_xmit_data(data_count);

                // Combine the high address and low address into an address.
                address = (hi_address << 8) | lo_address;

                // Adjust the address if needed.
                while (address > PROG_EEPROM_END) address -= PROG_EEPROM_END;

                // Set the address to be read.
                prog_buffer_set_address(address);

                // Loop over each data to be recieved.
                for (i = 0; i < data_count; ++i)
                {
                    // Wait to receive the next data.
                    data = rb2_recv_data();

                    // Break if we received an error.
                    if (data == -1) break;

                    // Send response which is the character we received.
                    rb2_xmit_data(data);

                    // Write the data to the buffer.
                    prog_buffer_set_byte((uint8_t) data);
                }

                // Write the buffer to Flash or EEPROM memory.
                if (data != -1) prog_buffer_update();
            }
        }
    }
}


void rb2_init(void) 
// Initialize the RoboBricks2 protocol module.
{
    // Initialize the serial state.
    rb2_data = -1;
    rb2_id_index = 0;
    rb2_address_pending = 0;

    // Read the serial address from EEPROM.
    rb2_address = eeprom_read_byte((void *) 0);
}


void rb2_process(void)
// Process the RoboBricks2 protocol.
{
    uint16_t data;
    uint8_t selected;
    uint8_t processing;

    // Perform processing until told otherwise.
    processing = 1;

    // Initial state is unselected.
    selected = 0;

    // Loop in the unselected state.
    while (processing)
    {
        // Wait for serial data or address.
        data = rb2_recv_data_or_address();

        // Does this character select us?
        selected = (data == (0x0100 | rb2_address)) ? 1 : 0;

        // Send the response if selected.
        if (selected) rb2_xmit_data(0x00A5);

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
                
                // We are already in the bootloader so we just send a response.
                rb2_xmit_data(0x00A5);
            }
            else if (data == 0xfa)
            {
                // We received BOOTLOADER EXIT command.
                
                // Send response.
                rb2_xmit_data(0x00A5);

                // Terminate processing.
                selected = 0;
                processing = 0;
            }
            else if (data == 0x01)
            {
                // We received READ BUFFER command.
                rb2_read_buffer();
            }
            else if (data == 0x02)
            {
                // We received WRITE BUFFER command.
                rb2_write_buffer();
            }
        }
    }
}


