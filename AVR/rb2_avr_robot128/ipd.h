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

#ifndef _RB2_IPD_H_
#define _RB2_IPD_H_ 1

typedef struct
{
    int16_t p_gain;
    int16_t d_gain;
    int16_t i_gain;
    int16_t max_output;
    int32_t integral;
} ipd;

inline static int16_t ipd_get_p_gain(ipd *self) { return self->p_gain; }
inline static int16_t ipd_get_d_gain(ipd *self) { return self->d_gain; }
inline static int16_t ipd_get_i_gain(ipd *self) { return self->i_gain; }
inline static int16_t ipd_get_max_output(ipd *self) { return self->max_output; }

inline static void ipd_set_p_gain(ipd *self, int16_t p_gain) { self->p_gain = p_gain; }
inline static void ipd_set_d_gain(ipd *self, int16_t d_gain) { self->d_gain = d_gain; }
inline static void ipd_set_i_gain(ipd *self, int16_t i_gain) { self->i_gain = i_gain; }
inline static void ipd_set_max_output(ipd *self, int16_t max_output) { self->max_output = max_output; }

void ipd_init(ipd *self);
int16_t ipd_get_output(ipd *self, int16_t position, int16_t error, int16_t velocity);

#endif // _RB2_IPD_H_
