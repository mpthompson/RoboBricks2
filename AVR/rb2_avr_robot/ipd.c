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
#include <string.h>
#include "avrx.h"
#include "ipd.h"

void ipd_init(ipd *self)
{
    // Zero out the ipd structure.
    memset(self, 0, sizeof(ipd));
}

int16_t ipd_get_output(ipd *self, int16_t position, int16_t error, int16_t velocity)
// The feedback approach implemented here was first published in Richard Phelan's
// Automatic Control Systems, Cornell University Press, 1977 (ISBN 0-8014-1033-9)
//
// The theory of operation of this function will be filled in later, but the
// diagram below should gives a general picture of how it is intended to work.
//
//
//                           +<------- bounds checking -------+
//                           |                                |
//             |¯¯¯¯¯|   |¯¯¯¯¯¯¯¯|   |¯¯¯¯¯|   |¯¯¯¯¯¯¯¯¯|   |
//  command -->|  -  |-->|integral|-->|  -  |-->|  motor  |-->+-> actuator
//             |_____|   |________|   |_____|   |_________|   |
//                |                      |                    |
//                |                      +<-- Kv * velocity --+
//                |                      |                    |
//                |                      +<-- Kp * position --+
//                |                                           |
//                +<-------------Ki * position ---------------+
//
{
    int32_t output;
    int32_t position_output;
    int32_t velocity_output;
 
    // Multiply the command error by the fixed integral gain value and add to the 
    // accumulator adjusting for multiplication.  Note: The command error is maintained 
    // with 32 bit precision in the accumulator although only the upper 24 bits are 
    // used for output calculations.
    self->integral += (int32_t) error * (int32_t) self->i_gain;

    // Get the upper 24 bits of the integral output.
    output = (self->integral >> 8);

    // Get the position and velocity outputs.
    position_output = ((int32_t) position * (int32_t) self->p_gain) >> 8;
    velocity_output = ((int32_t) velocity * (int32_t) self->d_gain) >> 8;

    // The difference between the integral output minus the position output determines 
    // the direction and magnitude of the output while velocity output functions as a 
    // frictional component within the system.
    output -= position_output - velocity_output;

    // Is the output saturated?  If so we need to clip the integral accumulator just at 
    // the saturation level and limit the output.
    if (output > self->max_output)
    {
        // Calculate a new integral accumulator that is just on the verge of saturation.
        self->integral = (position_output + velocity_output + self->max_output) << 8;

        // Limit the output.
        output = self->max_output;
    }
    else if (output < -self->max_output)
    {
        // Calculate a new integral accumulator that is just on the verge of saturation.
        self->integral = (position_output + velocity_output - self->max_output) << 8;

        // Limit the output.
        output = -self->max_output;
    }
 
    return (int16_t) output;
}
