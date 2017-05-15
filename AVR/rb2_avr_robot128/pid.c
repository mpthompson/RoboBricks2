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

#include <stdint.h>
#include <string.h>
#include "avrx.h"
#include "pid.h"

void pid_init(pid *self)
{
    // Zero out the pid structure.
    memset(self, 0, sizeof(pid));
}

int16_t pid_get_output(pid *self, int16_t error, int16_t rate)
{
    int32_t output;
 
    // Perform the pid calculation to determine the desired velocity.
    output = (int32_t) error * (int32_t) self->p_gain;
    output -= (int32_t) rate * (int32_t) self->d_gain;
    output += (int32_t) self->integral * (int32_t) self->i_gain;

    // Shift by the given shift adjustment and 8 bits to account for the fixed point gain.
    output >>= (self->shift + 8);

    // Range check the PWM output.
    if (output > self->max_output) output = self->max_output;
    if (output < -self->max_output) output = -self->max_output;

    // Decay the integral error.
    if (self->integral > 0) self->integral -= 1;
    if (self->integral < 0) self->integral += 1;

    // Add the error to the integral.
    self->integral += error;

    // Range check the error integral.
    if (self->integral > self->max_integral) self->integral = self->max_integral;
    if (self->integral < -self->max_integral) self->integral = -self->max_integral;

    return (int16_t) output;
}
