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

#define PAD_RIGHT 1
#define PAD_ZERO 2

// The following should be enough for 16 bit int.
#define PRINT_BUF_LEN 7

static int prints(char **out, const char *string, int8_t width, int8_t pad)
{
	int pc = 0;
	int8_t len = 0;
	const char *ptr;
    char padchar = (pad & PAD_ZERO) ? '0' : ' ';

	if (width > 0)
    {
		for (ptr = string; *ptr; ++ptr) ++len;
        width -= (len >= width) ? width : len;
	}

	if (!(pad & PAD_RIGHT))
    {
		for ( ; width > 0; --width)
        {
			**out = padchar;
            ++(*out);
			++pc;
		}
	}

	for ( ; *string ; ++string)
    {
		**out = *string;
        ++(*out);
		++pc;
	}

	for ( ; width > 0; --width)
    {
		**out = padchar;
        ++(*out);
		++pc;
	}

	return pc;
}


static int printi(char **out, int8_t i, int8_t b, int8_t sg, int8_t width, int8_t pad, int8_t letbase)
{
	int8_t t;
    int8_t neg = 0;
	int8_t u = i;
    int pc = 0;
	char *s;
	char print_buf[PRINT_BUF_LEN];

	if (i == 0)
    {
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return prints (out, print_buf, width, pad);
	}

	if (sg && (b == 10) && (i < 0))
    {
		neg = 1;
		u = -i;
	}

	s = print_buf + PRINT_BUF_LEN-1;
	*s = '\0';

	while (u)
    {
		t = u % b;
		if( t >= 10 ) t += letbase - '0' - 10;
		*--s = t + '0';
		u /= b;
	}

	if (neg)
    {
		if (width && (pad & PAD_ZERO))
        {
			**out = '-';
            ++(*out);
			++pc;
			--width;
		}
		else
        {
			*--s = '-';
		}
	}

	return pc + prints (out, s, width, pad);
}


static int print(char **out, const char *format, va_list args )
{
	int8_t width;
    int8_t pad;
	int8_t pc = 0;
    char *s;
	char scr[2];

	for (; *format != 0; ++format)
    {
		if (*format == '%')
        {
			++format;
			width = pad = 0;
			if (*format == '\0') break;
			if (*format == '%') goto out;
			if (*format == '-')
            {
				++format;
				pad = PAD_RIGHT;
			}
			while (*format == '0')
            {
				++format;
				pad |= PAD_ZERO;
			}
			for ( ; *format >= '0' && *format <= '9'; ++format)
            {
				width *= 10;
				width += *format - '0';
			}
			if (*format == 's' )
            {
				s = (char *) va_arg( args, int );
				pc += prints (out, s ? s : "(null)", width, pad);
				continue;
			}
			if (*format == 'd')
            {
				pc += printi(out, va_arg( args, int ), 10, 1, width, pad, 'a');
				continue;
			}
			if (*format == 'x')
            {
				pc += printi (out, va_arg( args, int ), 16, 0, width, pad, 'a');
				continue;
			}
			if (*format == 'X')
            {
				pc += printi (out, va_arg( args, int ), 16, 0, width, pad, 'A');
				continue;
			}
			if (*format == 'u')
            {
				pc += printi (out, va_arg( args, int ), 10, 0, width, pad, 'a');
				continue;
			}
			if ( *format == 'c')
            {
				// Char are converted to int then pushed on the stack.
				scr[0] = (char) va_arg(args, int);
				scr[1] = '\0';
				pc += prints (out, scr, width, pad);
				continue;
			}
		}
		else
        {
		out:
			**out = *format;
            ++(*out);
			++pc;
		}
	}
	**out = '\0';
	va_end(args);

	return pc;
}

int sxprintf(char *out, const char *format, ...)
{
    va_list args;

    va_start( args, format );
    return print( &out, format, args );
}

