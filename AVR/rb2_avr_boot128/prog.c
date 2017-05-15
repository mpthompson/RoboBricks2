/*
    Portions of this file derived from boot.h v1.20 from avr-libc project.
    See the following link for details:

    http://www.nongnu.org/avr-libc/

    Original copyright notice included below:

    Copyright (c) 2002, 2003, 2004, 2005  Eric B. Weddington
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of the copyright holders nor the names of
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
    IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
    TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    Copyright for original code not derived from boot.h:

    Copyright (c) 2013, Michael P. Thompson <mpthompson@gmail.com>
    All rights reserved.

    $Id$
*/

#include <inttypes.h>
#include <avr/io.h>
#include <avr/boot.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

#include "prog.h"

// Programming state flags.
static uint8_t prog_update_flag;

// Programming buffer addresses.
static uint32_t prog_page_address;
static uint32_t prog_byte_address;

// Programming buffer.
static uint8_t prog_buffer[SPM_PAGESIZE];

// The following define is not used.
#undef BOOTSTRAPPER

static void prog_flash_page_write(uint32_t address)
// Write the Flash data at the indicated address from the program buffer.
{
    uint16_t i;
    uint16_t w;
    uint8_t sreg;
    uint8_t *buf = prog_buffer;

    // Make sure the address falls on a Flash page boundary.
    address &= ~(SPM_PAGESIZE - 1);

    // Disable interrupts.
    sreg = SREG;
    cli();

    // Wait until EEPROM is no longer busy.
    eeprom_busy_wait();

    // Erase the Flash page.
    boot_page_erase(address);

    // Wait until the Flash page is erased.
    boot_spm_busy_wait();

    // Fill the page buffer with the data buffered in memory.
    // Notice that the bufer is written one word at a time.
    for (i = 0; i < SPM_PAGESIZE; i += 2)
    {
        // Set up little-endian word.
        w = *buf++;
        w += (*buf++) << 8;

        // Write the word to the page.
        boot_page_fill (address + i, w);
    }

    // Store the buffer in the Flash page.
    boot_page_write (address);

    // Wait until the Flash page is written.
    boot_spm_busy_wait();

    // Reenable RWW-section again. We need this if we want to jump back
    // to the application after bootloading.
    boot_rww_enable ();

    // Re-enable interrupts (if they were ever enabled).
    SREG = sreg;
}


static void prog_flash_page_read(uint32_t address)
// Write the Flash data at the indicated address from the program buffer.
{
    uint16_t i;
    uint8_t *buf = prog_buffer;

    // Make sure the address falls on a Flash page boundary.
    address &= ~(SPM_PAGESIZE - 1);

    // Fill the memory buffer with the Flash page data.
    for (i = 0; i < SPM_PAGESIZE; ++i)
    {
        // Read the byte from Flash.
        *buf++ = pgm_read_byte_far(address + i);
    }
}


static void prog_eeprom_page_write(uint16_t address)
// Read the EEPROM data at the indicated address into the program buffer.
{
    uint16_t i;
    uint8_t *buf = prog_buffer;

    // Make sure the address falls on a page boundary.
    address &= ~(SPM_PAGESIZE - 1);

    // Fill the memory buffer with the EEPROM data.
    for (i = 0; i < SPM_PAGESIZE; ++i)
    {
        // Write the byte into EEPROM.
        eeprom_write_byte((uint8_t *) address + i, *buf++);
    }
}


static void prog_eeprom_page_read(uint16_t address)
// Read the EEPROM data at the indicated address into the program buffer.
{
    uint16_t i;
    uint8_t *buf = prog_buffer;

    // Make sure the address falls on a page boundary.
    address &= ~(SPM_PAGESIZE - 1);

    // Fill the memory buffer with the EEPROM data.
    for (i = 0; i < SPM_PAGESIZE; ++i)
    {
        // Write the byte into EEPROM.
        *buf++ = eeprom_read_byte((uint8_t *) address + i);
    }
}


void prog_init(void)
// Initialize programming.
{
    // Set the default program buffer address.
    prog_page_address = 0;
    prog_byte_address = 0;

    // Reset programming state flags.
    prog_update_flag = 0;
}


void prog_buffer_set_address(uint32_t address)
// Set the address to be programmed.
{
    uint16_t i;

    // Split the address into a page address and byte address.
    prog_page_address = address & ~(SPM_PAGESIZE - 1);
    prog_byte_address = address & (SPM_PAGESIZE - 1);

    // Initialize the programming buffer.
    for (i = 0; i < SPM_PAGESIZE; ++i) prog_buffer[i] = 0xFF;

    // Which part of Flash/EEPROM are we reading.
    if (prog_page_address < PROG_FLASH_BOOTLOADER)
    {
#if !BOOTSTRAPPER
        // Read the Flash page into the programming buffer.
        prog_flash_page_read(prog_page_address - PROG_FLASH_START);
#else
        // Do nothing.  These pages are protected to prevent
        // overwriting of the bootstrapper application.
#endif
    }
    else if (prog_page_address < PROG_FLASH_END)
    {
#if BOOTSTRAPPER
        // Read the Flash page into the programming buffer.
        prog_flash_page_read(prog_page_address - PROG_FLASH_START);
#else
        // Do nothing.  These pages are protected to prevent
        // overwriting of the bootloader application.
#endif
    }
    else if (prog_page_address < PROG_EEPROM_END)
    {
        // Read the EEPROM page into the programming buffer.
        prog_eeprom_page_read(prog_page_address - PROG_EEPROM_START);
    }

    // Reset the programming buffer update flag.
    prog_update_flag = 0;
}


uint8_t prog_buffer_get_byte(void)
// Get the byte at the current address.
{
    uint8_t databyte;

    // Get the byte within the programming buffer.
    databyte = prog_buffer[prog_byte_address];

    // Increment the byte address within the page.
    ++prog_byte_address;

    // Check the byte address for wrapping.
    if (prog_byte_address > (SPM_PAGESIZE - 1)) prog_byte_address = 0;

    return databyte;
}


void prog_buffer_set_byte(uint8_t databyte)
// Set the byte at the current address.
{
    // Set the byte within the programming buffer.
    prog_buffer[prog_byte_address] = databyte;

    // Increment the byte address within the page.
    ++prog_byte_address;

    // Check the byte address for wrapping.
    if (prog_byte_address > (SPM_PAGESIZE - 1)) prog_byte_address = 0;

    // Set the programming update flag.  This indicates the programming
    // buffer should be written to Flash.
    prog_update_flag = 1;
}


void prog_buffer_update(void)
// If the programming buffer was updated it should now be written to Flash.
{
    // Was the programming buffer updated?
    if (prog_update_flag)
    {
        // Which part of Flash/EEPROM are we writing.
        if (prog_page_address < PROG_FLASH_BOOTLOADER)
        {
#if !BOOTSTRAPPER
            // Write the programming buffer to Flash.
            prog_flash_page_write(prog_page_address - PROG_FLASH_START);
#else
            // Do nothing.  These pages are protected to prevent
            // overwriting of the bootstrapper application.
#endif
        }
        else if (prog_page_address < PROG_FLASH_END)
        {
#if BOOTSTRAPPER
            // Write the programming buffer to Flash.
            prog_flash_page_write(prog_page_address - PROG_FLASH_START);
#else
            // Do nothing.  These pages are protected to prevent
            // overwriting of the bootloader application.
#endif
        }
        else if (prog_page_address < PROG_EEPROM_END)
        {
            // Read the EEPROM page into the programming buffer.
            prog_eeprom_page_write(prog_page_address - PROG_EEPROM_START);
        }

        // Reset the programming buffer update flag.
        prog_update_flag = 0;
    }
}


