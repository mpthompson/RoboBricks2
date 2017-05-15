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

#include <stdint.h>
#include <stdarg.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "avrx.h"
#include "config.h"
#include "serial.h"

#if (CPUCLK == 16000000)
#define BAUD2UBRR_9600      207
#endif

// Format flags.
#define PAD_RIGHT 1
#define PAD_ZERO 2

// The following should be enough for 16 bit int.
#define PRINT_BUF_LEN 8

// Used for temporary character storage.
static char serial_buffer[PRINT_BUF_LEN];

AVRX_MUTEX(tx_ready);                   // AvrX semaphore for signalling TX routine.
AVRX_MUTEX(rx_ready);                   // AvrX semaphore for signalling RX routine.
AVRX_MUTEX(rx_timeout);                 // AvrX semaphore for signalling RX timeout.
AVRX_MUTEX(serial_mutex);                // AvrX semaphore USART access.

// User I/O module timer control block.
TimerControlBlock rx_timer;

// Indicates the current own of the USART.
volatile pProcessID serial_owner = NOPID;

void serial_init(void)
//  Initialize the USART for 9 bit frame communication.
{
    // Set the baud rate.
    UBRR0H = BAUD2UBRR_9600 >> 8;
    UBRR0L = BAUD2UBRR_9600 & 0xff;

    // Set transfer rate doubler.
    UCSR0A = (1<<U2X0);

    // Enable the receive interrupt, receiver, transmitter and 8-bit size.
    UCSR0B = (1<<UDRIE0) | (1<<RXCIE0) | (1<<RXEN0) | (1<<TXEN0) | (0<<UCSZ02);

    // Set frame format: Asynchronous, 8 data, 1 stop bit, no parity.
    UCSR0C = (0<<UMSEL0) |                      // Asynchronous UART.
             (0<<UPM01) | (0<<UPM00) |          // Parity disabled.
             (0<<USBS0) |                       // One stop bit.
             (1<<UCSZ01) | (1<<UCSZ00);         // 8-bit size (with UCSZ02).

    // Prime the USART mutex.
    AvrXSetSemaphore(&serial_mutex);
}


void serial_grab_access(void)
// Grab exclusive access to the USART.
{
    uint8_t dummy;

    // Wait for exclusive access.
    AvrXWaitSemaphore(&serial_mutex);

    // Set the USART owner.
    serial_owner = AvrXSelf();

    // Flush the receive buffer on the USART.
    while (UCSR0A & (1<<RXC0)) dummy = UDR0;
}


void serial_release_access(void)
// Release exclusive access to the USART.
{
    // Reset the USART owner.
    serial_owner = NOPID;

    // Release exclusive access.
    AvrXSetSemaphore(&serial_mutex);
}


void serial_xmit(uint16_t data)
// Transmit 8 bit data over the USART.
{
    // Wait for signal.
    AvrXWaitSemaphore(&tx_ready);

    // Set data into the transmit buffer.
    UDR0 = (uint16_t) data;

    // Enable interrupt.
    UCSR0B |= (1<<UDRIE0);
}


uint16_t serial_recv(uint16_t timeout)
// Return next 9 bit word from the USART or -1 if timeout.
{
    uint8_t data;

    // Use the default timeout if zero.
    if (!timeout) timeout = 1000;

    // Is the serial port ready to receive data?
    if (~UCSR0A & (1<<RXC0))
    {
        // Start the timer.
        AvrXStartTimer(&rx_timer, timeout);

        // Wait for timer or signal to wake up task.
        AvrXWaitTimer(&rx_timer);

        // Cancel the timer in case in case of signal.
        AvrXCancelTimer(&rx_timer);

        // Reset the timer semaphore after being canceled.
        rx_timer.semaphore = SEM_PEND;
    }

    // Is the serial port ready to receive data?
    if (UCSR0A & (1<<RXC0))
    {
        // Get the data.
        data = (uint8_t) UDR0;
    }
    else
    {
        // Return error character.
        data = -1;
    }

    // Enable the receive interrupt.
    UCSR0B |= (1<<RXCIE0);

    return data;
}


AVRX_SIGINT(SIG_USART0_DATA)
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

INTERFACE void AvrXIntSetObjectSemaphore(pMutex);

AVRX_SIGINT(SIG_USART0_RECV)
// USART receive interrupt handler.
{
    // Switch to kernel stack.
    IntProlog();

    // Disable the receive interrupt.
    UCSR0B &= ~(1<<RXCIE0);

    // Enable other interrupt activity.
    sei();

    // Signal the receiver task.
    AvrXIntSetObjectSemaphore((pMutex) &rx_timer);

    // Go  back to RTOS
    Epilog();
}


static void serial_putc(char ch)
{
    // Send the character.
    serial_xmit((uint16_t) ch);
}


static void serial_prints(const char *string, int8_t width, int8_t pad)
{
	const char *ptr;
    char padchar = (pad & PAD_ZERO) ? '0' : ' ';
	int8_t len = 0;

	if (width > 0)
    {
		for (ptr = string; *ptr; ++ptr) ++len;
        width -= (len >= width) ? width : len;
	}

	if (!(pad & PAD_RIGHT))
    {
		for (; width > 0; --width)
        {
            serial_putc(padchar);
		}
	}

	for (; *string; ++string)
    {
        serial_putc(*string);
	}

	for (; width > 0; --width)
    {
        serial_putc(padchar);
	}
}


static void serial_printi(int i, int8_t b, int8_t sg, int8_t width, int8_t pad, char letbase)
{
	char *s;
	int8_t t;
    int8_t neg;

    neg = 0;

    // We'll work our way backwards through the buffer.
	s = serial_buffer + PRINT_BUF_LEN - 1;
	*s = '\0';

    // Optimize for zero.
    if (i == 0)
    {
        *--s = '0';
    }
    else
    {
        // Flag negative numbers.
    	if (sg && (b == 10) && (i < 0))
        {
    		neg = 1;
    		i = -i;
    	}

        // Determine the backwards sequence of digits in the number.
        // Notice special handling to keep values positive.
    	while (i)
        {
    		t = (unsigned int) i % b;
    		i = (unsigned int) i / b;
            *--s = (t < 10) ? '0' + t : letbase + t - 10;
    	}

        // Handle the negative sign if needed.
    	if (neg)
        {
    		if (width && (pad & PAD_ZERO))
            {
                serial_putc('-');
    			--width;
    		}
    		else
            {
    			*--s = '-';
    		}
    	}
    }

    // Pad and align the resulting string.
    serial_prints(s, width, pad);
}


void serial_printf_P(const char *format, ...)
{
    char ch;
    char *s;
    int8_t pad;
	int8_t width;
    va_list args;

    // Grab access to the USART.
    serial_grab_access();

    // Start varargs processing.
    va_start(args, format);

    // Format the output.
    for (ch = pgm_read_byte(format); ch; ch = pgm_read_byte(++format))
    {
        // Is this a formatting character?
		if (ch == '%')
        {
            // Get the next character.
            ch = pgm_read_byte(++format);

			width = pad = 0;
			if (ch == '\0') break;
			if (ch == '%') goto out;
			if (ch == '-')
            {
                // Set the pad right flag.
				pad |= PAD_RIGHT;

                // Get the next character.
                ch = pgm_read_byte(++format);
			}
			while (ch == '0')
            {
                // Set the pad zero flag.
				pad |= PAD_ZERO;

                // Get the next character.
                ch = pgm_read_byte(++format);
			}
			for (; ch >= '0' && ch <= '9'; ch = pgm_read_byte(++format))
            {
                // Add to the padding width.
				width *= 10;
				width += ch - '0';
			}
			if (ch == 's')
            {
				s = (char *) va_arg(args, int);
				serial_prints(s ? s : "(null)", width, pad);
				continue;
			}
			if (ch == 'd')
            {
				serial_printi(va_arg(args, int), 10, 1, width, pad, 'a');
				continue;
			}
			if (ch == 'x')
            {
				serial_printi(va_arg(args, int), 16, 0, width, pad, 'a');
				continue;
			}
			if (ch == 'X')
            {
				serial_printi(va_arg(args, int), 16, 0, width, pad, 'A');
				continue;
			}
			if (ch == 'u')
            {
				serial_printi(va_arg(args, int), 10, 0, width, pad, 'a');
				continue;
			}
			if (ch == 'c')
            {
				// Char are converted to int then pushed on the stack.
				serial_buffer[0] = (char) va_arg(args, int);
				serial_buffer[1] = '\0';
				serial_prints(serial_buffer, width, pad);
				continue;
			}
		}
		else
        {
		out:
            serial_putc(ch);
		}
	}

    // End varargs processing.
	va_end(args);

    // Release access to the USART.
    serial_release_access();
}


void serial_puts_P(const char *text)
{
    char ch;

    // Grab access to the USART.
    serial_grab_access();

    // Write each character to the LCD.
    for (ch = pgm_read_byte(text); ch; ch = pgm_read_byte(++text))
    {
        // Output the character.
        serial_putc(ch);
    }

    // Release access to the USART.
    serial_release_access();
}



