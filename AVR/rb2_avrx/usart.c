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

AVRX_MUTEX(rx_ready);                   // AvrX Semaphore for signalling TX routine.
AVRX_MUTEX(tx_ready);                   // AvrX Semaphore for signalling RX routine.
AVRX_MUTEX(tx_mutex);                   // To allow sharing of port for sending.
uint16_t rx_data;                       // Receive buffer.

void usart_init(void)
//  Initialize the USART for 9 bit frame communication.
{
    // Initialize transmit mutex.
    AvrXSetSemaphore(&tx_mutex);
    
    // Set the baud rate.
    UBRR0 = BAUD2UBRR_500K;

    // Set transfer rate doubler.
    UCSR0A = (1<<U2X0);

    // Enable the receive interrupt, receiver, transmitter and 9-bit size.
    UCSR0B = (1<<RXCIE0) | (1<<RXEN0) | (1<<TXEN0) | (1<<UCSZ02);

    // Set frame format: Asynchronous, 9 data, 1 stop bit, no parity.
    UCSR0C = (0<<UMSEL01) | (0<<UMSEL00) |      // Asynchronous UART.
             (0<<UPM01) | (0<<UPM00) |          // Parity disabled.
             (0<<USBS0) |                       // One stop bit.
             (1<<UCSZ01) | (1<<UCSZ00);         // 9-bit size (with UCSZ02).
}


void usart_address_only(uint8_t enabled)
// Enable or disable USART to receive addresses only.
{
    // Warning: There are issues with setting or clearning the MPCM0 bit in
    // that the TXC0 transmit complete flag may be cleared and a pending
    // transmit interrupt is lost.  I believe the transmit byte, receive byte
    // nature of the RoboBricks2 protocol prevents this issue.

    // Is address only mode neabled or disabled?
    if (enabled)
    {
        // Enable address only mode.
        sbi(UCSR0A, MPCM0);
    }
    else
    {
        // Disable address only mode.
        cbi(UCSR0A, MPCM0);
    }
}

void usart_xmit(uint16_t data)
{
    // Get exclusive access to the transmitter.
    AvrXWaitSemaphore(&tx_mutex);

    // Check if we need to wait until the buffer is empty.
    if (bit_is_clear(UCSR0A, UDRE0))
    {
        // Enable interrupt.
        sbi(UCSR0B, UDRIE0);

        // Wait for signal.
        AvrXWaitSemaphore(&tx_ready);
    }

    // Copy ninth bit to TXB80.
    UCSR0B &= ~(1<<TXB80);
    if (data & 0x0100) UCSR0B |= (1<<TXB80);

    // Put data into the transmit buffer.
    UDR0 = data;

    // Give up access to the transmitter.   
    AvrXSetSemaphore(&tx_mutex);
}


uint16_t usart_recv(void)
{
    // Wait for receive interrupt to signal us
    AvrXWaitSemaphore(&rx_ready);
    
    return rx_data;
}


// 
// UART0 Transmit Buffer Empty ISR
//
AVRX_SIGINT(SIG_USART_DATA)
{
    // Disable interrupts.
    cbi(UCSR0B, UDRIE0);
        
    // Switch to kernel stack.
    IntProlog();

    // Signal we are ready for more.
    AvrXIntSetSemaphore(&tx_ready);
    
    // Go back to RTOS.
    Epilog();
}


// 
// UART0 Receive ISR
//
AVRX_SIGINT(SIG_USART_RECV)
{
    uint8_t resh;
    uint8_t resl;
    uint8_t status;

    // Disable receive interrupt.
    cbi(UCSR0B, RXCIE0);

    // Switch to kernel stack.
    IntProlog();

    // Read the status bytes.
    status = UCSR0A;

    // Read ninth data bit first and then the 8 data bits.
    resh = UCSR0B;
    resl = UDR0;

    // Did we get an error?
    if (status & ((1<<FE0) | (1<<DOR0) | (1<<UPE0)))
    {
        // Indicate bad data.
        rx_data = -1;
    }
    else
    {
        // Isolate the ninth bit.
        resh = (resh >> 1) & 0x01;

        // Return the combined 9 bit value.
        rx_data = ((resh << 8) | resl);
    }

    // Signal back to any waiting process.
    AvrXIntSetSemaphore(&rx_ready);
    
    // Re-enable interrupts
    sbi(UCSR0B, RXCIE0);
      
    // Go  back to RTOS
    Epilog();
}

