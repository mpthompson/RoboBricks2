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

#include <avr/io.h>
#include <avr/interrupt.h>
#include "avrx.h"
#include "config.h"
#include "usart.h"

#if (CPUCLK == 8000000)
#define BAUD2UBRR_500K      1
#endif

#if (CPUCLK == 16000000)
#define BAUD2UBRR_500K      3
#endif

#if (CPUCLK == 20000000)
#define BAUD2UBRR_500K      4
#endif

AVRX_MUTEX(rx_ready);                   // AvrX Semaphore for signaling TX routine.
AVRX_MUTEX(tx_ready);                   // AvrX Semaphore for signaling RX routine.

void usart_init(void)
//  Initialize the USART for 9 bit frame communication.
{
    // Set the baud rate.
    UBRR0 = BAUD2UBRR_500K;

    // Set transfer rate doubler.
    UCSR0A = (1<<U2X0);

    // Enable the receive interrupt, receiver, transmitter and 9-bit size.
    UCSR0B = (1<<UDRIE0) | (1<<RXCIE0) | (1<<RXEN0) | (1<<TXEN0) | (1<<UCSZ02);

    // Set frame format: Asynchronous, 9 data, 1 stop bit, no parity.
    UCSR0C = (0<<UMSEL01) | (0<<UMSEL00) |      // Asynchronous UART.
             (0<<UPM01) | (0<<UPM00) |          // Parity disabled.
             (0<<USBS0) |                       // One stop bit.
             (1<<UCSZ01) | (1<<UCSZ00);         // 9-bit size (with UCSZ02).
}


void usart_address_only(uint8_t enabled)
// Enable or disable USART to receive addresses only.
{
    // Warning: There are issues with setting or clearing the MPCM0 bit in
    // that the TXC0 transmit complete flag may be cleared and a pending
    // transmit interrupt is lost.  I believe the transmit byte, receive byte
    // nature of the RoboBricks2 protocol prevents this issue.

    // Is address only mode enabled or disabled?
    if (enabled)
    {
        // Enable address only mode.
        UCSR0A |= (1<<MPCM0);
    }
    else
    {
        // Disable address only mode.
        UCSR0A &= ~(1<<MPCM0);
    }
}


void usart_xmit(uint16_t data)
{
    // Wait for signal.
    AvrXWaitSemaphore(&tx_ready);

    // Set data into the transmit buffer including ninth bit into TXB80.
    if (data & 0x0100)
        UCSR0B |= (1<<TXB80);
    else
        UCSR0B &= ~(1<<TXB80);
    UDR0 = data;

    // Enable interrupt.
    UCSR0B |= (1<<UDRIE0);
}


uint16_t usart_recv(void)
{
    uint8_t hi_byte;
    uint8_t lo_byte;
    uint16_t data;

    // Wait for data to be available.
    AvrXWaitSemaphore(&rx_ready);

    // Get the high and low byte of data.
    hi_byte = (UCSR0B & (1<<RXB80)) ? 0x01 : 0x00;
    lo_byte = UDR0;

    // Set the combined 9 bit value.
    data = (hi_byte << 8) | lo_byte;

    // Enable the receive interrupt.
    UCSR0B |= (1<<RXCIE0);

    return data;
}


AVRX_SIGINT(USART_UDRE_vect)
// USART transmit buffer empty interrupt handler.
{
    // Switch to kernel stack.
    IntProlog();

    // Disable data register empty interrupt.
    UCSR0B &= ~(1<<UDRIE0);
        
    // Enable other interrupt activity.
    sei();

    // Signal back that the transmit buffer is empty.
    AvrXSetSemaphore(&tx_ready);
    
    // Go back to RTOS.
    Epilog();
}


AVRX_SIGINT(USART_RX_vect)
// USART receive interrupt handler.
{
    // Switch to kernel stack.
    IntProlog();

    // Disable the receive interrupt.
    UCSR0B &= ~(1<<RXCIE0);

    // Enable other interrupt activity.
    sei();

    // Signal back that serial data is ready to be read.
    AvrXSetSemaphore(&rx_ready);
    
    // Go  back to RTOS
    Epilog();
}

