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

#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "boot.h"
#include "config.h"
#include "prog.h"
#include "rb2.h"
#include "usart.h"

void init(void)
{
    // Do some basic house keeping to clean things up.
    DDRB = 0x00;
    PORTB = 0x00;
    DDRC = 0x00;
    PORTC = 0x00;
    DDRD = 0x00;
    PORTD = 0x00;
    TCCR0A = 0x00;
    TCCR0B = 0x00;
    TCCR1A = 0x00;
    TCCR1B = 0x00;
    TCCR2A = 0x00;
    TCCR2B = 0x00;
    UCSR0A = 0x00;
    UCSR0B = 0x00;
}


int
main (void)
{
    uint8_t mcusr;

    // Set up function pointer to RESET vector.
    void (*reset_vector)( void ) = 0x0000;

    // Clear interrupts.
    cli();

    // Read and clear the MCU status register.  We read this
    // to determine the reason for reset.
    mcusr = MCUSR;
    MCUSR = 0;

    // Do some housekeeping cleanup.
    init();

    // Initialize the boot enable module.
    boot_init();

    // Initialize programming module.
    prog_init();

    // Initialize USART module.
    usart_init();

    // Initialize RoboBricks2 protocol module.
    rb2_init();

    // We enter the bootloader for one of two reasons: because
    // the bootloader IO flag is set or we are here for a reason
    // other than a reset.
    if (boot_enabled() || (mcusr == 0))
    {
        // Process the RoboBricks2 protocol.
        rb2_process();
    }

    // Do some housekeeping cleanup.
    init();

    // Call application RESET vector.
    reset_vector();

    return 0;
}


