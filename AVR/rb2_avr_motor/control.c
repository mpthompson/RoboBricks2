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
#include <avr/io.h>
#include "avrx.h"
#include "control.h"
#include "motor.h"
#include "imu.h"
#include "uio.h"

#define DEFAULT_OFFSET      0
#define DEFAULT_P_GAIN      (0x3000)
#define DEFAULT_D_GAIN      (0x0200)

#define MAX_PWM             127
#define MIN_PWM             (-MAX_PWM)

// Note: Assuming globals are zeroed.
static uint8_t control_enabled;
static int16_t control_offset;
static int16_t control_p_gain;
static int16_t control_d_gain;

// Task control.
AVRX_MUTEX(control_mutex);
AVRX_TIMER(control_timer);

void control_set_enable(uint8_t enable)
// Get the control enable flag.
{
    control_enabled = enable;
}


uint8_t control_get_enable(void)
// Set the control enable flag.
{
    return control_enabled;
}


void control_set_offset(int16_t offset)
// Set the control offset.
{
    // Get access to control values.
    AvrXWaitSemaphore(&control_mutex);

    // Set the offset.
    control_offset = offset;    

    // Release access to control values.
    AvrXSetSemaphore(&control_mutex);
}


int16_t control_get_offset(void)
// Get the control offset.
{
    int16_t rv;

    // Get access to control values.
    AvrXWaitSemaphore(&control_mutex);

    // Return the offset.
    rv = control_offset;    

    // Release access to control values.
    AvrXSetSemaphore(&control_mutex);

    return rv;
}


void control_set_p_gain(int16_t p_gain)
// Set the control proportional gain.
{
    // Get access to control values.
    AvrXWaitSemaphore(&control_mutex);

    // Set the p gain.
    control_p_gain = p_gain;    

    // Release access to control values.
    AvrXSetSemaphore(&control_mutex);
}


int16_t control_get_p_gain(void)
// Get the control proportional gain.
{
    int16_t rv;

    // Get access to control values.
    AvrXWaitSemaphore(&control_mutex);

    // Return the pgain value.
    rv = control_p_gain;    

    // Release access to control values.
    AvrXSetSemaphore(&control_mutex);

    return rv;
}


void control_set_d_gain(int16_t d_gain)
// Set the control derivative gain.
{
    // Get access to control values.
    AvrXWaitSemaphore(&control_mutex);

    // Set the d gain.
    control_d_gain = d_gain;    

    // Release access to control values.
    AvrXSetSemaphore(&control_mutex);
}


int16_t control_get_d_gain(void)
// Get the control derivative gain.
{
    int16_t rv;

    // Get access to control values.
    AvrXWaitSemaphore(&control_mutex);

    // Return the pgain value.
    rv = control_d_gain;    

    // Release access to control values.
    AvrXSetSemaphore(&control_mutex);

    return rv;
}


static int32_t control_gain_multiply(int16_t value, uint16_t gain)
// Multiplies the 8:8 fixed point by the 8:8 fixed point gain value.
{
    int32_t result;

    // Multiply the value times the gain.
    result = (int32_t) value * (int32_t) gain;

    // Shift by 8 to account for fixed point gain.
    result >>= 8;

    return result;
}


NAKEDFUNC(control_task)
// Task for robot motor control.
{
    static uint8_t count;
    static uint8_t sub_count;
    static int8_t left_pwm;
    static int8_t right_pwm;
    static int16_t pitch_angle;
    static int16_t pitch_error;
    static int16_t pitch_rate;
    static int32_t pwm_output;

    // Initialize the gains.
    control_p_gain = DEFAULT_P_GAIN;
    control_d_gain = DEFAULT_D_GAIN;
    control_offset = DEFAULT_OFFSET;

    // Prime the control mutix.
    AvrXSetSemaphore(&control_mutex);

    // Initialize the IMU module.
    imu_init();

    // Main control loop.
    for (;;)
    {
        // Start the 20 millisecond timer.
        AvrXStartTimer(&control_timer, 20);

        // By default, turn power off to the motors.
        left_pwm = 0;
        right_pwm = 0;

        // Update the count and leds.
        if (++count == 25)
        {
            // Set the leds.
            uio_leds_set((1 << sub_count));
            uio_leds_reset(~(1 << sub_count));

            // Increment the sub-count.
            sub_count = sub_count < 5 ? sub_count + 1 : 0;

            count = 0;
        }

        // Update the IMU pitch values.
        if (imu_update())
        {
            // Get the pitch values.
            imu_get_pitch(&pitch_angle, &pitch_rate);

            // Is control enabled for the robot.
            if (control_enabled)
            {
                // Make sure the limits are not exceeded.
                if ((pitch_angle < 2560) && (pitch_angle > -2560))
                {
                    // Get access to control values.
                    AvrXWaitSemaphore(&control_mutex);

                    // Determine the proportional error.
                    // XXX pitch_error = (pitch_command + control_offset) - pitch_angle;
                    pitch_error = control_offset - pitch_angle;

                    // Combine the proportional and derivative values into a combined value.
                    pwm_output = control_gain_multiply(pitch_error, control_p_gain) - 
                                 control_gain_multiply(pitch_rate, control_d_gain);

                    // Release access to control values.
                    AvrXSetSemaphore(&control_mutex);

                    // Remove the lowest 8 significant bits from the pwm output.
                    pwm_output >>= 8;

                    // Range check the pwm output.
                    if (pwm_output > MAX_PWM) pwm_output = MAX_PWM;
                    if (pwm_output < MIN_PWM) pwm_output = MIN_PWM;

            		// Set the PWM values for the left and right motor.
                    left_pwm = (int8_t) pwm_output;
                    right_pwm = (int8_t) pwm_output;
                }
            }
        }

    	// Update motors with new PWM values.
        motor_set_pwm(left_pwm, right_pwm);

    	// Wait for the remainder of the 20 milliseconds to elapse.
        AvrXWaitTimer(&control_timer);
    }
}



