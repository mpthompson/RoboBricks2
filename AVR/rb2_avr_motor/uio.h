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

#ifndef _RB2_UIO_H_
#define _RB2_UIO_H_ 1

// Button constants.
#define BUTTON_NONE     0
#define BUTTON_UP       1
#define BUTTON_DOWN     2
#define BUTTON_RIGHT    3
#define BUTTON_LEFT     4
#define BUTTON_CENTER   5

void uio_leds_set(uint8_t leds);
void uio_leds_blink(uint8_t leds);
void uio_leds_reset(uint8_t leds);
uint8_t uio_next_button(uint16_t timeout);

#endif // _RB2_UIO_H_
