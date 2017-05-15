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
#include <avr/io.h>
#include "avrx.h"
#include "encoder.h"
#include "motor.h"
#include "pid.h"
#include "usart.h"

// Default values.
#define DEFAULT_P_GAIN          0x0780
#define DEFAULT_D_GAIN          0x0066
#define DEFAULT_I_GAIN          0x0099
#define DEFAULT_MAX_OUTPUT      0x7f
#define DEFAULT_MAX_INTEGRAL    0xff

// Note: Assuming globals are zeroed.
static uint8_t motor_enabled;
static pid motor_left_pid;
static pid motor_right_pid;
static int8_t motor_left_pwm;
static int8_t motor_right_pwm;
static int16_t motor_left_cmd;
static int16_t motor_right_cmd;

// Task control.
AVRX_MUTEX(motor_mutex);

void motor_enable_set(uint8_t enable)
// Set the enabled flag.
{
    // Set the enabled flag.
    motor_enabled = enable;
}


uint8_t motor_enable_get(void)
// Set the enabled flag.
{
    // Get the enabled flag.
    return motor_enabled;
}


void motor_command_set(int16_t *left_cmd, int16_t *right_cmd)
// Set the left/right motor command velocity values.  The velocity 
// is specified in number of encoder units to move over 10 milliseconds.
{
    // Get exclusive access to the motor variables.
    AvrXWaitSemaphore(&motor_mutex);

    // Set the left/right motor command value.
    if (left_cmd) motor_left_cmd = *left_cmd;
    if (right_cmd) motor_right_cmd = *right_cmd;

    // Release exclusive access to the motor variables.
    AvrXSetSemaphore(&motor_mutex);
}


void motor_command_get(int16_t *left_cmd, int16_t *right_cmd)
// Get the left/right motor command velocity values.    The velocity 
// is specified in number of encoder units to move over 10 milliseconds.
{
    // Get exclusive access to the motor variables.
    AvrXWaitSemaphore(&motor_mutex);

    // Set the left/right motor command value.
    if (left_cmd) *left_cmd = motor_left_cmd;
    if (right_cmd) *right_cmd = motor_right_cmd;

    // Release exclusive access to the motor variables.
    AvrXSetSemaphore(&motor_mutex);
}


void motor_left_gains_set(int16_t *p_gain, int16_t *d_gain, int16_t *i_gain)
// Set the left motor gain values.  These are 8:8 fixed point values.
{
    // Get exclusive access to the motor variables.
    AvrXWaitSemaphore(&motor_mutex);

    // Set the left gain values.
    if (p_gain) pid_set_p_gain(&motor_left_pid, *p_gain);
    if (d_gain) pid_set_d_gain(&motor_left_pid, *d_gain);
    if (i_gain) pid_set_i_gain(&motor_left_pid, *i_gain);

    // Release exclusive access to the motor variables.
    AvrXSetSemaphore(&motor_mutex);
}


void motor_left_gains_get(int16_t *p_gain, int16_t *d_gain, int16_t *i_gain)
// Get the left motor gain values.  These are 8:8 fixed point values.
{
    // Get exclusive access to the motor variables.
    AvrXWaitSemaphore(&motor_mutex);

    // Get the left gain values.
    if (p_gain) *p_gain = pid_get_p_gain(&motor_left_pid);
    if (d_gain) *d_gain = pid_get_d_gain(&motor_left_pid);
    if (i_gain) *i_gain = pid_get_i_gain(&motor_left_pid);

    // Release exclusive access to the motor variables.
    AvrXSetSemaphore(&motor_mutex);
}


void motor_right_gains_set(int16_t *p_gain, int16_t *d_gain, int16_t *i_gain)
// Set the right motor gain values.  These are 8:8 fixed point values.
{
    // Get exclusive access to the motor variables.
    AvrXWaitSemaphore(&motor_mutex);

    // Set the right gain values.
    if (p_gain) pid_set_p_gain(&motor_right_pid, *p_gain);
    if (d_gain) pid_set_d_gain(&motor_right_pid, *d_gain);
    if (i_gain) pid_set_i_gain(&motor_right_pid, *i_gain);

    // Release exclusive access to the motor variables.
    AvrXSetSemaphore(&motor_mutex);
}


void motor_right_gains_get(int16_t *p_gain, int16_t *d_gain, int16_t *i_gain)
// Get the right motor gain values.  These are 8:8 fixed point values.
{
    // Get exclusive access to the motor variables.
    AvrXWaitSemaphore(&motor_mutex);

    // Get the right gain values.
    if (p_gain) *p_gain = pid_get_p_gain(&motor_right_pid);
    if (d_gain) *d_gain = pid_get_d_gain(&motor_right_pid);
    if (i_gain) *i_gain = pid_get_i_gain(&motor_right_pid);

    // Release exclusive access to the motor variables.
    AvrXSetSemaphore(&motor_mutex);
}


void motor_pwm_get(int8_t *left_pwm, int8_t *right_pwm)
// Get the left/right motor pwm values.
{
    // Get exclusive access to the motor variables.
    AvrXWaitSemaphore(&motor_mutex);

    // Set the left/right motor command value.
    if (left_pwm) *left_pwm = motor_left_pwm;
    if (right_pwm) *right_pwm = motor_right_pwm;

    // Release exclusive access to the motor variables.
    AvrXSetSemaphore(&motor_mutex);
}


static void motor_pwm_set(int8_t left_pwm, int8_t right_pwm)
// Set the pwm values in the motor control module.
{
    // Save the PWM values.
    motor_left_pwm = left_pwm;
    motor_right_pwm = right_pwm;

    // Grab access to the USART.
    usart_grab_access();

    // Select the MidiMotor2 module.
    usart_xmit_discard_echo(0x0150);

    // Get and validate the response.
    if (usart_recv() == 0x00A5)
    {
        // Update the duty cycle.
        usart_xmit_discard_echo(0x000c);

        // Select motor 1 speed.
        usart_xmit_discard_echo(0x0001);

        // Set motor 1 speed.
        usart_xmit_discard_echo((uint8_t) -right_pwm);

        // Select motor 2 speed.
        usart_xmit_discard_echo(0x0003);

        // Set motor 3 speed.
        usart_xmit_discard_echo((uint8_t) -left_pwm);
    }

    // Release access to the USART.
    usart_release_access();
}


void motor_init(void)
// Initialize motor control information.
{
    // Initialize the left pid gains.
    pid_set_p_gain(&motor_left_pid, DEFAULT_P_GAIN);
    pid_set_d_gain(&motor_left_pid, DEFAULT_D_GAIN);
    pid_set_i_gain(&motor_left_pid, DEFAULT_I_GAIN);
    pid_set_max_output(&motor_left_pid, DEFAULT_MAX_OUTPUT);
    pid_set_max_integral(&motor_left_pid, DEFAULT_MAX_INTEGRAL);

    // Initialize the right pid gains.
    pid_set_p_gain(&motor_right_pid, DEFAULT_P_GAIN);
    pid_set_d_gain(&motor_right_pid, DEFAULT_D_GAIN);
    pid_set_i_gain(&motor_right_pid, DEFAULT_I_GAIN);
    pid_set_max_output(&motor_right_pid, DEFAULT_MAX_OUTPUT);
    pid_set_max_integral(&motor_right_pid, DEFAULT_MAX_INTEGRAL);

    // Prime the motor mutex.
    AvrXSetSemaphore(&motor_mutex);
}


void motor_update(void)
// Main motor control function.  This should be called after the
// motor encoder values have been updated with new information.
{
    int8_t left_pwm;
    int8_t right_pwm;
    int16_t left_error;
    int16_t right_error;
    int16_t left_error_d;
    int16_t right_error_d;
    int16_t encoder_left_delta;
    int16_t encoder_right_delta;
    static int16_t prev_left_error;
    static int16_t prev_right_error;

    // By default the pwm values are zeroed.
    left_pwm = 0;
    right_pwm = 0;

    // Is PWM enabled to the motors?
    if (motor_enabled)
    {
        // Get the encoder deltas.
        encoder_get_deltas(&encoder_left_delta, &encoder_right_delta);

        // Get exclusive access to the motor variables.
        AvrXWaitSemaphore(&motor_mutex);

        // Determine left/right velocity error.
        left_error = motor_left_cmd - encoder_left_delta;
        right_error = motor_right_cmd - encoder_right_delta;

        // Determine left/right velocity error derivative.
        left_error_d = prev_left_error - left_error;
        right_error_d = prev_right_error - right_error;

        // Save the current velocity error.
        prev_left_error = left_error;
        prev_right_error = right_error;

        // Process the error and error through the pid algorithm.
        left_pwm = (int8_t) pid_get_output(&motor_left_pid, left_error, left_error_d);
        right_pwm = (int8_t) pid_get_output(&motor_right_pid, right_error, right_error_d);

        // Release exclusive access to the motor variables.
        AvrXSetSemaphore(&motor_mutex);
    }

  	// Update motors with new PWM values.
    motor_pwm_set(left_pwm, right_pwm);
}


