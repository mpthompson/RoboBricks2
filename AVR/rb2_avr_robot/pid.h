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

#ifndef _RB2_PID_H_
#define _RB2_PID_H_ 1

typedef struct
{
    int16_t p_gain;
    int16_t d_gain;
    int16_t i_gain;
    int16_t integral;
    int16_t max_output;
    int16_t max_integral;
    uint8_t shift;
} pid;

inline static int16_t pid_get_p_gain(pid *self) { return self->p_gain; }
inline static int16_t pid_get_d_gain(pid *self) { return self->d_gain; }
inline static int16_t pid_get_i_gain(pid *self) { return self->i_gain; }
inline static int16_t pid_get_integral(pid *self) { return self->integral; }
inline static int16_t pid_get_max_output(pid *self) { return self->max_output; }
inline static int16_t pid_get_max_integral(pid *self) { return self->max_integral; }
inline static uint8_t pid_get_shift(pid *self) { return self->shift; }

inline static void pid_set_p_gain(pid *self, int16_t p_gain) { self->p_gain = p_gain; }
inline static void pid_set_d_gain(pid *self, int16_t d_gain) { self->d_gain = d_gain; }
inline static void pid_set_i_gain(pid *self, int16_t i_gain) { self->i_gain = i_gain; }
inline static void pid_set_integral(pid *self, int16_t integral) { self->integral = integral; }
inline static void pid_set_max_output(pid *self, int16_t max_output) { self->max_output = max_output; }
inline static void pid_set_max_integral(pid *self, int16_t max_integral) { self->max_integral = max_integral; }
inline static void pid_set_shift(pid *self, uint8_t shift) { self->shift = shift; }

void pid_init(pid *self);
int16_t pid_get_output(pid *self, int16_t error, int16_t rate);

#endif // _RB2_PID_H_
