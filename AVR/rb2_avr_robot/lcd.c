/*
	Copyright 2001, 2002 Georges Menie (www.menie.org)
	stdarg version contributed by Christian Ettinger

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// http://www.menie.org/georges/embedded/

#include <stdint.h>
#include <stdarg.h>
#include <avr/pgmspace.h>
#include "avrx.h"
#include "lcd.h"
#include "usart.h"

// The size of the LCD circular buffer.
#define LCD_BUFFER_LEN      40

// The size of the LCD temporary printf buffer.
#define LCD_TEMP_LEN        8

// LCD printf padding flags.
#define PAD_RIGHT           1
#define PAD_ZERO            2

// Note: Assuming globals are zeroed.

// Protects LCD state information.
AVRX_MUTEX(lcd_mutex);

// LCD state information.
static uint8_t lcd_head;
static uint8_t lcd_tail;
static char lcd_temp[LCD_TEMP_LEN];
static char lcd_buffer[LCD_BUFFER_LEN];

void lcd_init(void)
// Initialize the LCD state.
{
    // Prime the LCD semaphore.
    AvrXSetSemaphore(&lcd_mutex);
}


void lcd_update(void)
// Update the LCD state.
{
    // Get access to the LCD information.
    AvrXWaitSemaphore(&lcd_mutex);

    // Should we update the LCD?
    if (lcd_tail != lcd_head)
    {
        // Grab access to the USART.
        usart_grab_access();

        // Select the LCD.
        usart_xmit_discard_echo(0x0120);

        // Get and validate the response.
        if (usart_recv() == 0x00A5)
        {
            // Output LCD characters until the buffer is empty.
            while (lcd_tail != lcd_head)
            {
                // Send the character.
                usart_xmit_discard_echo(lcd_buffer[lcd_tail]);

                // Increment and wrap the lcd tail.
                lcd_tail = ((lcd_tail + 1) % LCD_BUFFER_LEN);
            }
        }

        // Release access to the USART.
        usart_release_access();
    }

    // Release access to the LCD information.
    AvrXSetSemaphore(&lcd_mutex);
}


void lcd_putc(char ch)
// Puts a single character into the LCD buffer.
{
    // Get access to the LCD information.
    AvrXWaitSemaphore(&lcd_mutex);

    // Place the character at the head of the buffer.
    lcd_buffer[lcd_head] = ch;

    // Increment and wrap the lcd head.
    lcd_head = ((lcd_head + 1) % LCD_BUFFER_LEN);

    // Increment and wrap the tail if buffer collision.
    if (lcd_head == lcd_tail) lcd_tail = ((lcd_tail + 1) % LCD_BUFFER_LEN);

    // Release access to the LCD information.
    AvrXSetSemaphore(&lcd_mutex);
}


void lcd_puts_P(const char *text)
// LCD puts function where string is in program flash.
{
    char ch;

    // Write each character to the LCD.
    for (ch = pgm_read_byte_near(text); ch; ch = pgm_read_byte_near(++text))
    {
        // Output the character.
        lcd_putc(ch);
    }
}


static void lcd_prints(const char *string, int8_t width, int8_t pad)
// LCD printf support function.
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
            lcd_putc(padchar);
		}
	}

	for (; *string; ++string)
    {
        lcd_putc(*string);
	}

	for (; width > 0; --width)
    {
        lcd_putc(padchar);
	}
}


static void lcd_printi(int i, int8_t b, int8_t sg, int8_t width, int8_t pad, char letbase)
// LCD printf support function.
{
	char *s;
	int8_t t;
    int8_t neg;

    neg = 0;

    // We'll work our way backwards through the buffer.
	s = lcd_temp + LCD_TEMP_LEN - 1;
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
                lcd_putc('-');
    			--width;
    		}
    		else
            {
    			*--s = '-';
    		}
    	}
    }

    // Pad and align the resulting string.
    lcd_prints(s, width, pad);
}


static void lcd_print_fix(int i)
// LCD printf support function.
{
	char *s;
    uint8_t v;
    uint8_t t;
    uint8_t f;
    uint8_t d;

    // Point to the buffer.
    s = lcd_temp;

    // Flag negative numbers.
	if (i < 0)
    {
        lcd_putc('-');
		i = -i;
	}

    // Handle the integer portion first.
    v = ((unsigned int) i >> 8);

    // Determine each integer digit.
    for (f = 0, d = 100; d > 0; d /= 10)
    {
        // Determine the decimal digit.
        t = (unsigned int) v / d;

        // Output the digit.
        if ((t > 0) || (d < 10) || (f)) { f = 1; lcd_putc('0' + t); }

        // Update the integer portion.
        v %= d;
    }

    lcd_putc('.');

    // Handle the fractional portion second.
    v = (((unsigned int) i & 0xff) * 100) / 256;

    // Determine each fractional digit.
    for (d = 10; d > 0; d /= 10)
    {
        // Determine the decimal digit.
        t = (unsigned int) v / d;

        // Place the digit into the buffer.
        lcd_putc('0' + t);

        // Update the integer portion.
        v %= d;
    }
}


void lcd_printf_P(const char *format, ...)
// LCD printf function where format string is in program flash.
{
    char ch;
    char *s;
    int8_t pad;
	int8_t width;
    va_list args;

    // Start varargs processing.
    va_start(args, format);

    // Format the output.
    for (ch = pgm_read_byte_near(format); ch; ch = pgm_read_byte_near(++format))
    {
        // Is this a formatting character?
		if (ch == '%')
        {
            // Get the next character.
            ch = pgm_read_byte_near(++format);

			width = pad = 0;
			if (ch == '\0') break;
			if (ch == '%') goto out;
			if (ch == '-')
            {
                // Set the pad right flag.
				pad |= PAD_RIGHT;

                // Get the next character.
                ch = pgm_read_byte_near(++format);
			}
			while (ch == '0')
            {
                // Set the pad zero flag.
				pad |= PAD_ZERO;

                // Get the next character.
                ch = pgm_read_byte_near(++format);
			}
			for (; ch >= '0' && ch <= '9'; ch = pgm_read_byte_near(++format))
            {
                // Add to the padding width.
				width *= 10;
				width += ch - '0';
			}
			if (ch == 's')
            {
				s = (char *) va_arg(args, int);
				lcd_prints(s ? s : "(null)", width, pad);
				continue;
			}
			if (ch == 'd')
            {
				lcd_printi(va_arg(args, int), 10, 1, width, pad, 'a');
				continue;
			}
			if (ch == 'x')
            {
				lcd_printi(va_arg(args, int), 16, 0, width, pad, 'a');
				continue;
			}
			if (ch == 'X')
            {
				lcd_printi(va_arg(args, int), 16, 0, width, pad, 'A');
				continue;
			}
			if (ch == 'u')
            {
				lcd_printi(va_arg(args, int), 10, 0, width, pad, 'a');
				continue;
			}
			if (ch == 'c')
            {
				// Char are converted to int then pushed on the stack.
				lcd_temp[0] = (char) va_arg(args, int);
				lcd_temp[1] = '\0';
				lcd_prints(lcd_temp, width, pad);
				continue;
			}
			if (ch == 'i')
            {
				// Fixed point conversion.
				lcd_print_fix(va_arg(args, int));
				continue;
			}
		}
		else
        {
		out:
            lcd_putc(ch);
		}
	}

    // End varargs processing.
	va_end(args);
}



