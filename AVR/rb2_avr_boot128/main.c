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

#include "config.h"
#include "prog.h"
#include "rb2.h"
#include "usart.h"

int
main (void)
{
    uint8_t mcucsr;

    // Set up function pointer to RESET vector.
    void (*reset_vector)( void ) = 0x0000;

    // Clear interrupts.
    cli();

    // Read and clear the MCU status register.  The MCUSR is used
    // to determine the reason for activation of the bootloader.
    mcucsr = MCUCSR;
    MCUCSR = 0;

    // Reset the MCU if we got to this code for a reason other than a reset.
    // We do this to make sure the MCU is a sane state.
    if ((mcucsr & ((1<<JTRF) | (1<<WDRF) | (1<<BORF) | (1<<EXTRF) | (1<<PORF))) == 0)
    {
        // Enable the watchdog with minimum pre-scaling.
        WDTCR = (0<<WDCE) | (1<<WDE) | (0<<WDP2) | (0<<WDP1) | (0<<WDP0);

        // Wait for reset to occur.
        for (;;);
    }

    // Set port PB1/SCK to input with pull-up.  Having this pin 
    // shorted to ground is a flag to keep the bootloader active 
    // and to not automatically start the application code.
    DDRB &= ~(1<<DDB2);
    PORTB |= (1<<PB1);

    // We activate the bootloader if the PB1 pin is held low or
    // if we are here for a reset other than a power on reset.
    // if ((mcucsr & (1<<PORF)) != (1<<PORF))
    if (((PINB & (1<<PINB1)) == 0) || ((mcucsr & (1<<PORF)) != (1<<PORF)))
    {
        // Initialize programming module.
        prog_init();

        // Initialize USART module.
        usart_init();

        // Initialize RoboBricks2 protocol module.
        rb2_init();

        // Process the RoboBricks2 protocol.
        rb2_process();
    }

    // Call application RESET vector.
    reset_vector();

    return 0;
}


