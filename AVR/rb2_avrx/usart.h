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
*/

#ifndef _RB2_USART_H_
#define _RB2_USART_H_ 1

#include <avr/io.h>

void usart_init(void);
void usart_address_only(uint8_t enabled);
void usart_xmit(uint16_t data);
uint16_t usart_recv(void);

static inline uint8_t usart_xmit_ready(void)
{
    // Return true if the transmit buffer is empty.
    return (UCSR0A & (1<<UDRE0)) ? 1 : 0;
}

static inline uint8_t usart_recv_ready(void)
{
    // Return true if the receive buffer is full.
    return (UCSR0A & (1<<RXC0)) ? 1 : 0;
}

#endif // _IMU_USART_H_
