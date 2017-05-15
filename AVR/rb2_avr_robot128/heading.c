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
#include <stdio.h>
#include "avrx.h"
#include "balance.h"
#include "encoder.h"
#include "heading.h"
#include "motor.h"
#include "pid.h"
#include "uio.h"

// Note: Assuming globals are zeroed.

void heading_init(void)
{
    // Do nothing.
}


void heading_update(void)
// Main heading control loop.
{
    int8_t heading_rc;
    int16_t heading;
    int16_t left_motor_velocity;
    int16_t right_motor_velocity;
    static int32_t heading_filter;
    
    // Get the motor velocities.
    motor_command_get(&left_motor_velocity, &right_motor_velocity);

    // Get the RC control values.
    uio_get_rc(&heading_rc, NULL);

    // Pass into a 16 bit variable for manipulation.
    heading = heading_rc;

    // Create a generous deadband for the heading.
    if (heading > 10) heading -= 10;
    else if (heading < -10) heading += 10;
    else heading = 0;

    // Pass the heading through a low pass filter to prevent wild swings.
    heading_filter = heading_filter - (heading_filter >> 3) + heading;
    heading = (int16_t) (heading_filter >> 3);

    // Convert the heading to a speed differential to be applied to the motor velocities.
    heading = heading >> 2;

    // Update the motor velocities.
    left_motor_velocity += heading;
    right_motor_velocity -= heading;

    // Set the heading adjusted motor velocities.
    motor_command_set(&left_motor_velocity, &right_motor_velocity);
}
