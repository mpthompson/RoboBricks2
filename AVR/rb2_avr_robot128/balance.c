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
#include "avrx.h"
#include "balance.h"
#include "encoder.h"
#include "imu.h"
#include "motor.h"
#include "ipd.h"

#define DEFAULT_P_GAIN      ((int16_t) (03.14 * 256))
#define DEFAULT_D_GAIN      ((int16_t) (00.00 * 256))
#define DEFAULT_I_GAIN      ((int16_t) (02.15 * 256))
#define DEFAULT_T_COMP      ((int16_t) (-0.50 * 256))
#define DEFAULT_MAX_VEL     ((int16_t) (160.00 * 128))

// Note: Assuming globals are zeroed.
static int16_t balance_tilt;
static int16_t balance_t_comp;
static ipd balance_ipd;

// Task control.
AVRX_MUTEX(balance_mutex);

void balance_init(void)
{
    // Initialize the pid gains and tilt compensation.
    ipd_set_p_gain(&balance_ipd, DEFAULT_P_GAIN);
    ipd_set_d_gain(&balance_ipd, DEFAULT_D_GAIN);
    ipd_set_i_gain(&balance_ipd, DEFAULT_I_GAIN);
    ipd_set_max_output(&balance_ipd, DEFAULT_MAX_VEL);
    balance_t_comp = DEFAULT_T_COMP;

    // Prime the balance mutex.
    AvrXSetSemaphore(&balance_mutex);
}


void balance_update(void)
// Main balance control loop.
{
    int16_t left_vel;
    int16_t right_vel;
    int16_t pitch_error;
    int16_t vel_output;
    int16_t pitch_angle;
    int16_t pitch_rate;

    // By default set the motor velocity to zero.
    left_vel = 0;
    right_vel = 0;

    // Update the IMU pitch values.
    if (imu_update())
    {
        // Get the IMU pitch values.
        imu_pitch_get(&pitch_angle, &pitch_rate);

        // Make sure the limits are not exceeded.
        if ((pitch_angle < 5120) && (pitch_angle > -5120))
        {
            // Get access to control values.
            AvrXWaitSemaphore(&balance_mutex);

            // Determine the proportional error from the balance tilt.  The tilt
            // compensation is added in to adjust for unbalanced loads on the robot.
            pitch_error = balance_tilt + balance_t_comp - pitch_angle;

            // Perform the ipd calculation.
            vel_output = ipd_get_output(&balance_ipd, pitch_angle, pitch_error, pitch_rate);

            // Pull seven bits from the output.
            vel_output >>= 7;

            // Release access to control values.
            AvrXSetSemaphore(&balance_mutex);

    		// Set the PWM values for the left and right motor.
            left_vel = -vel_output;
            right_vel = -vel_output;
        }
    }

    // Set the velocity of the left and right motor.
    motor_command_set(&left_vel, &right_vel);
}


void balance_tilt_set(int16_t tilt)
// Set the balance tilt.  The tilt controls the velocity of the robot
// in the forward and backwards direction.
{
    // Get access to control values.
    AvrXWaitSemaphore(&balance_mutex);

    // Update the tilt value.
    balance_tilt = tilt;

    // Release access to control values.
    AvrXSetSemaphore(&balance_mutex);
}


void balance_gains_set(int16_t *p_gain, int16_t *d_gain, int16_t *i_gain, int16_t *t_comp)
// Set the balance gains and tilt compensation.
{
    // Get access to control values.
    AvrXWaitSemaphore(&balance_mutex);

    // Set the balance gains and tilt compensation.
    if (p_gain) ipd_set_p_gain(&balance_ipd, *p_gain);
    if (d_gain) ipd_set_d_gain(&balance_ipd, *d_gain);
    if (i_gain) ipd_set_i_gain(&balance_ipd, *i_gain);
    if (t_comp) balance_t_comp = *t_comp;

    // Release access to control values.
    AvrXSetSemaphore(&balance_mutex);
}


void balance_gains_get(int16_t *p_gain, int16_t *d_gain, int16_t *i_gain, int16_t *t_comp)
// Get the balance gains and tilt compensation.
{
    // Get access to control values.
    AvrXWaitSemaphore(&balance_mutex);

    // Set the gains.
    if (p_gain) *p_gain = ipd_get_p_gain(&balance_ipd);
    if (d_gain) *d_gain = ipd_get_d_gain(&balance_ipd);
    if (i_gain) *i_gain = ipd_get_i_gain(&balance_ipd);
    if (t_comp) *t_comp = balance_t_comp;

    // Release access to control values.
    AvrXSetSemaphore(&balance_mutex);
}




