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

AVRX_MUTEX(tx_ready);                   // AvrX semaphore for signaling TX routine.
AVRX_MUTEX(rx_ready);                   // AvrX semaphore for signaling RX routine.
AVRX_MUTEX(rx_default_ready);           // AvrX semaphore for signaling RX default routine.
AVRX_MUTEX(rx_timeout);                 // AvrX semaphore for signaling RX timeout.
AVRX_MUTEX(usart_mutex);                // AvrX semaphore USART access.

// User I/O module timer control block.
// TimerControlBlock rx_timer;

// Indicates the current owner of the USART.
volatile pProcessID usart_owner = NOPID;

void usart_init(void)
//  Initialize the USART for 9 bit frame communication.
{
    // Set the baud rate.
    UBRR1H = BAUD2UBRR_500K >> 8;
    UBRR1L = BAUD2UBRR_500K & 0xff;

    // Set transfer rate doubler.
    UCSR1A = (1<<U2X1);

    // Enable the receive interrupt, receiver, transmitter and 9-bit size.
    UCSR1B = (1<<UDRIE1) | (1<<RXCIE1) | (1<<RXEN1) | (1<<TXEN1) | (1<<UCSZ12);

    // Set frame format: Asynchronous, 9 data, 1 stop bit, no parity.
    UCSR1C = (0<<UMSEL1) |                      // Asynchronous UART.
             (0<<UPM11) | (0<<UPM10) |          // Parity disabled.
             (0<<USBS1) |                       // One stop bit.
             (1<<UCSZ11) | (1<<UCSZ10);         // 9-bit size (with UCSZ12).

    // Prime the USART mutex.
    AvrXSetSemaphore(&usart_mutex);
}


void usart_grab_access(void)
// Grab exclusive access to the USART.
{
    uint8_t dummy;

    // Wait for exclusive access.
    AvrXWaitSemaphore(&usart_mutex);

    // Set the USART owner.
    usart_owner = AvrXSelf();

    // Flush the receive buffer on the USART.
    while (UCSR1A & (1<<RXC1)) dummy = UDR1;
}


void usart_release_access(void)
// Release exclusive access to the USART.
{
    // Reset the USART owner.
    usart_owner = NOPID;

    // Release exclusive access.
    AvrXSetSemaphore(&usart_mutex);
}


void usart_xmit(uint16_t data)
// Transmit 9 bit data over the USART.
{
    // Wait for signal.
    AvrXWaitSemaphore(&tx_ready);

    // Set data into the transmit buffer including ninth bit into TXB81.
    if (data & 0x0100)
        UCSR1B |= (1<<TXB81);
    else
        UCSR1B &= ~(1<<TXB81);
    UDR1 = (uint8_t) data;

    // Enable interrupt.
    UCSR1B |= (1<<UDRIE1);
}


void usart_xmit_discard_echo(uint16_t data)
// Transmit 9 bit data over the USART and eats the echo.
{
    // Transmit the data.
    usart_xmit(data);

    // Discard the echo.
    usart_recv();
}


uint16_t usart_recv(void)
// Receive the character from the USART.
{
    uint8_t hi_byte;
    uint8_t lo_byte;
    uint16_t data;

    // Wait for data to be available.
    AvrXWaitSemaphore(&rx_ready);

    // Get the high and low byte of data.
    hi_byte = (UCSR1B & (1<<RXB81)) ? 0x01 : 0x00;
    lo_byte = UDR1;

    // Set the combined 9 bit value.
    data = (hi_byte << 8) | lo_byte;

    // Enable the receive interrupt.
    UCSR1B |= (1<<RXCIE1);

    return data;
}


uint16_t usart_recv_default(void)
// Receive the character from the USART.
{
    uint8_t hi_byte;
    uint8_t lo_byte;
    uint16_t data;

    // Wait for data to be available.
    AvrXWaitSemaphore(&rx_default_ready);

    // Get the high and low byte of data.
    hi_byte = (UCSR1B & (1<<RXB81)) ? 0x01 : 0x00;
    lo_byte = UDR1;

    // Set the combined 9 bit value.
    data = (hi_byte << 8) | lo_byte;

    // Enable the receive interrupt.
    UCSR1B |= (1<<RXCIE1);

    return data;
}


AVRX_SIGINT(USART1_UDRE_vect)
// USART transmit buffer empty interrupt handler.
{
    // Switch to kernel stack.
    IntProlog();

    // Disable data register empty interrupt.
    UCSR1B &= ~(1<<UDRIE1);
        
    // Enable other interrupt activity.
    sei();

    // Signal back that the transmit buffer is empty.
    AvrXSetSemaphore(&tx_ready);
    
    // Go back to RTOS.
    Epilog();
}


AVRX_SIGINT(USART1_RX_vect)
// USART receive interrupt handler.
{
    // Switch to kernel stack.
    IntProlog();

    // Disable the receive interrupt.
    UCSR1B &= ~(1<<RXCIE1);

    // Enable other interrupt activity.
    sei();

    // Signal back that serial data is ready to be read.
    if (usart_owner == NOPID)
    {
        // Signal the default receiver task.
        AvrXIntSetSemaphore(&rx_default_ready);
    }
    else
    {
        // Signal the owned receiver task.
        AvrXIntSetSemaphore(&rx_ready);
    }

    // Go  back to RTOS
    Epilog();
}


