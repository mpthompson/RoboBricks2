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

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "usart.h"
#include "config.h"

#if (FOSC == 8000000)
#define BAUD2UBRR_500K      1
#endif

#if (FOSC == 16000000)
#define BAUD2UBRR_500K      3
#endif

#if (FOSC == 20000000)
#define BAUD2UBRR_500K      4
#endif

void usart_init(void)
{
    // Set the baud rate.
    UBRR0 = BAUD2UBRR_500K;

    // Set transfer rate doubler.
    UCSR0A = (1<<U2X0);

    // Enable the receiver and transmitter.
    UCSR0B = (1<<RXEN0) | (1<<TXEN0) | (1<<UCSZ02);

    // Set frame format: Asynchronous, 9 data, 1 stop bit, no parity.
    UCSR0C = (0<<UMSEL01) | (0<<UMSEL00) |      // Asynchronous UART.
             (0<<UPM01) | (0<<UPM00) |          // Parity disabled.
             (0<<USBS0) |                       // One stop bit.
             (1<<UCSZ01) | (1<<UCSZ00);         // 9-bit size (with UCSZ02).
}


void usart_xmit(uint16_t data)
{
    // Clear the receive buffer.
    while (UCSR0A & (1<<RXC0)) UDR0;

    // Wait for the transmit buffer to be empty.
    while (!(UCSR0A & (1<<UDRE0)));

    // Copy ninth bit to TXB8.
    UCSR0B &= ~(1<<TXB80);
    if (data & 0x0100) UCSR0B |= (1<<TXB80);

    // Put data into the transmit buffer.
    UDR0 = data;
}


uint16_t usart_recv(void)
{
    uint8_t resh;
    uint8_t resl;
    uint8_t status;

    // Wait for data to be received.
    while (!(UCSR0A & (1<<RXC0)));

    // Read the status bytes.
    status = UCSR0A;

    // Read ninth data bit first and then the 8 data bits.
    resh = UCSR0B;
    resl = UDR0;

    // If there was an error clear the receive buffer of data.
    if (status & ((1<<FE0) | (1<<DOR0) | (1<<UPE0)))
    {
        // Clear the receive buffer.
        while (UCSR0A & (1<<RXC0)) UDR0;

        return -1;
    }

    // Isolate the ninth bit.
    resh = (resh >> 1) & 0x01;

    // Return the combined 9 bit value.
    return ((resh << 8) | resl);
}


