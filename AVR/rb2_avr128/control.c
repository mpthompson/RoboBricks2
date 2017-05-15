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
#include <avr/io.h>
#include "avrx.h"
#include "balance.h"
#include "control.h"
#include "encoder.h"
#include "heading.h"
#include "lcd.h"
#include "motor.h"
#include "imu.h"
#include "pid.h"
#include "speed.h"
#include "uio.h"

// Note: Assuming globals are zeroed.

// Task control.
AVRX_TIMER(control_timer);

static void led_update(void)
{
    static uint8_t count;
    static uint8_t sub_count;

    // Update the count and leds.
    if (++count == 50)
    {
        // Set the leds.
        uio_leds_set((1 << sub_count));
        uio_leds_reset(~(1 << sub_count));

        // Increment the sub-count.
        sub_count = sub_count < 5 ? sub_count + 1 : 0;

        count = 0;
    }
}


NAKEDFUNC(control_task)
// Main task for robot control.
{
    static uint8_t control_count;

    // Initialize the LCD module.
    lcd_init();

    // Initialize the user I/O module.
    uio_init();

    // Initialize the IMU module.
    imu_init();

    // Initialize the encoder module.
    encoder_init();

    // Initialize the motor module.
    motor_init();

    // Initialize the speed control.
    speed_init();

    // Initialize the balance control.
    balance_init();

    // Initialize the heading control.
    heading_init();

    // Main control loop.
    for (;;)
    {
        // Start the 10 millisecond timer.
        AvrXStartTimer(&control_timer, 10);

        // Update the LEDs.
        led_update();

        // Collect and update the motor encoder values to determine the 
        // position and velocity of each motor.  The encoder values are 
        // used for closed loop velocity control of the motors as well as 
        // determining robot position, speed and heading.
        encoder_update();

        // Only perform balancing PID loop every 20 milliseconds.
        if ((control_count & 0x01) == 0x01)
        {
            // Update the speed control.  This produces a tilt value
            // which is then passed to the balance control.
            speed_update();

            // Collect the IMU information and update the motor 
            // velocity values for the left and right motors.
            balance_update();

            // Adjust the motor velocity values for heading.
            heading_update();
        }

        // Send the latest PWM power settings to the left and right motors.
        motor_update();

        // Poll the I/O module every 160 milliseconds.
        if ((control_count & 0x0f) == 0x00)
        {
            //  Poll for user input from the user I/O module.
            uio_update();
        }

        // Update the LCD every 80 milliseconds.
        if ((control_count & 0x07) == 0x04)
        {
            // Update the LCD.
            lcd_update();
        }

    	// Wait for the remainder of the 10 milliseconds to elapse.
        AvrXWaitTimer(&control_timer);

        // Increment the control counter.
        ++control_count;
    }
}
