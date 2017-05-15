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
#include "uio.h"
#include "motor.h"
#include "pid.h"
#include "speed.h"

#define DEFAULT_P_GAIN      ((int16_t) (00.00 * 256))
#define DEFAULT_D_GAIN      ((int16_t) (00.00 * 256))
#define DEFAULT_I_GAIN      ((int16_t) (00.00 * 256))
#define DEFAULT_MAX_TILT    ((int16_t) (03.00 * 256))

// Note: Assuming globals are zeroed.
static pid speed_pid;

// Task control.
AVRX_MUTEX(speed_mutex);

void speed_init(void)
{
    // Initialize the pid gains and tilt compensation.
    pid_set_p_gain(&speed_pid, DEFAULT_P_GAIN);
    pid_set_d_gain(&speed_pid, DEFAULT_D_GAIN);
    pid_set_i_gain(&speed_pid, DEFAULT_I_GAIN);
    pid_set_max_output(&speed_pid, DEFAULT_MAX_TILT);
    pid_set_max_integral(&speed_pid, 128);

    // Prime the balance mutex.
    AvrXSetSemaphore(&speed_mutex);
}


void speed_update(void)
// Main speed control loop.
{
    int8_t speed_rc;
    int16_t tilt;
    static int32_t tilt_filter;

    // Get the RC control values.
    uio_get_rc(NULL, &speed_rc);

    // Convert the RC value to a tilt value.
    tilt = ((int16_t) speed_rc * 6);

    // Pass the speed through a low pass filter to prevent wild swings.
    tilt_filter = tilt_filter - (tilt_filter >> 3) + tilt;
    tilt = (int16_t) (tilt_filter >> 3);

    // Set the tilt which is the output of the this control loop.
    balance_tilt_set(tilt);
}


#if 0
void speed_update(void)
// Main speed control loop.
{
    int8_t speed_rc;
    int16_t tilt;
    int16_t speed_error;
    int16_t speed_error_d;
    int16_t speed_actual;
    int16_t speed_desired;
    int16_t encoder_left_delta;
    int16_t encoder_right_delta;
    static int16_t prev_speed_error;
    static int32_t speed_filter;

    // Get the RC control values.
    uio_get_rc(NULL, &speed_rc);

    // Pass the speed through a low pass filter to prevent wild swings.
    speed_filter = speed_filter - (speed_filter >> 4) + speed_rc;
    speed_desired = (speed_filter >> 4);

    // Get the encoder deltas.
    encoder_get_deltas(&encoder_left_delta, &encoder_right_delta);

    // Determine the robot speed from the encoder deltas.  This is the
    // speed of the robot in encoder ticks per 10ms.
    speed_actual = (encoder_left_delta + encoder_right_delta) >> 1;

    // Get access to control values.
    AvrXWaitSemaphore(&speed_mutex);

    // Determine the speed error.
    speed_error = speed_desired - speed_actual;

    // Determine the speed error derivative.
    speed_error_d = prev_speed_error - speed_error;

    // Save the current speed error.
    prev_speed_error = speed_error;

    // Perform the pid calculation.
    tilt = pid_get_output(&speed_pid, speed_error, speed_error_d);

    // Release access to control values.
    AvrXSetSemaphore(&speed_mutex);

    // Set the tilt which is the output of the this control loop.
    balance_tilt_set(tilt);
}
#endif

void speed_gains_set(int16_t *p_gain, int16_t *d_gain, int16_t *i_gain)
// Set the speed gains.
{
    // Get access to control values.
    AvrXWaitSemaphore(&speed_mutex);

    // Set the balance gains and tilt compensation.
    if (p_gain) pid_set_p_gain(&speed_pid, *p_gain);
    if (d_gain) pid_set_d_gain(&speed_pid, *d_gain);
    if (i_gain) pid_set_i_gain(&speed_pid, *i_gain);

    // Release access to control values.
    AvrXSetSemaphore(&speed_mutex);
}


void speed_gains_get(int16_t *p_gain, int16_t *d_gain, int16_t *i_gain)
// Get the speed gains.
{
    // Get access to control values.
    AvrXWaitSemaphore(&speed_mutex);

    // Set the gains.
    if (p_gain) *p_gain = pid_get_p_gain(&speed_pid);
    if (d_gain) *d_gain = pid_get_d_gain(&speed_pid);
    if (i_gain) *i_gain = pid_get_i_gain(&speed_pid);

    // Release access to control values.
    AvrXSetSemaphore(&speed_mutex);
}


