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

    $Id: prog.h,v 1.1 2006/03/06 07:54:16 mpthompson Exp $
*/

#ifndef _RB2_PROG_H_
#define _RB2_PROG_H_ 1

#ifdef __AVR_ATmega128__

// Flash/EEPROM Page Information
#define PROG_PAGE_SIZE          (256)

// Flash/EEPROM Address Information
#define PROG_FLASH_START        (0x00000)
#define PROG_FLASH_BOOTLOADER   (0x1F800)
#define PROG_FLASH_END          (0x20000)
#define PROG_EEPROM_START       (0x20000)
#define PROG_EEPROM_END         (0x21000)

#endif

extern void prog_init(void);
extern void prog_buffer_set_address(uint32_t address);
extern uint8_t prog_buffer_get_byte(void);
extern void prog_buffer_set_byte(uint8_t databyte);
extern void prog_buffer_update(void);

#endif // _RB2_PROG_H_
