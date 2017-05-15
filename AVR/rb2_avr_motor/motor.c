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
#include "encoder.h"
#include "motor.h"
#include "usart.h"

// Note: Assuming globals are zeroed.
uint8_t motor_enabled;
int16_t motor_left_cmd;
int16_t motor_left_p_gain;
int16_t motor_left_d_gain;
int16_t motor_left_i_gain;
int16_t motor_left_max_error_i;
int8_t motor_left_pwm;
int16_t motor_right_cmd;
int16_t motor_right_p_gain;
int16_t motor_right_d_gain;
int16_t motor_right_i_gain;
int16_t motor_right_max_error_i;
int8_t motor_right_pwm;

// Task control.
AVRX_TIMER(motor_timer);
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
    if (p_gain) motor_left_p_gain = *p_gain;
    if (d_gain) motor_left_d_gain = *d_gain;
    if (i_gain) motor_left_i_gain = *i_gain;

    // Release exclusive access to the motor variables.
    AvrXSetSemaphore(&motor_mutex);
}


void motor_left_gains_get(int16_t *p_gain, int16_t *d_gain, int16_t *i_gain)
// Get the left motor gain values.  These are 8:8 fixed point values.
{
    // Get exclusive access to the motor variables.
    AvrXWaitSemaphore(&motor_mutex);

    // Get the left gain values.
    if (p_gain) *p_gain = motor_left_p_gain;
    if (d_gain) *d_gain = motor_left_d_gain;
    if (i_gain) *i_gain = motor_left_i_gain;

    // Release exclusive access to the motor variables.
    AvrXSetSemaphore(&motor_mutex);
}


void motor_right_gains_set(int16_t *p_gain, int16_t *d_gain, int16_t *i_gain)
// Set the right motor gain values.  These are 8:8 fixed point values.
{
    // Get exclusive access to the motor variables.
    AvrXWaitSemaphore(&motor_mutex);

    // Set the right gain values.
    if (p_gain) motor_right_p_gain = *p_gain;
    if (d_gain) motor_right_d_gain = *d_gain;
    if (i_gain) motor_right_i_gain = *i_gain;

    // Release exclusive access to the motor variables.
    AvrXSetSemaphore(&motor_mutex);
}


void motor_right_gains_get(int16_t *p_gain, int16_t *d_gain, int16_t *i_gain)
// Get the right motor gain values.  These are 8:8 fixed point values.
{
    // Get exclusive access to the motor variables.
    AvrXWaitSemaphore(&motor_mutex);

    // Get the right gain values.
    if (p_gain) *p_gain = motor_right_p_gain;
    if (d_gain) *d_gain = motor_right_d_gain;
    if (i_gain) *i_gain = motor_right_i_gain;

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
    usart_xmit(0x0150);

    // Discard the echo.
    usart_recv(100);

    // Get and validate the response.
    if (usart_recv(50) == 0x00A5)
    {
        // Update the duty cycle.
        usart_xmit(0x000c);

        // Discard the echo.
        usart_recv(50);

        // Select motor 1 speed.
        usart_xmit(0x0001);

        // Discard the echo.
        usart_recv(50);

        // Set motor 1 speed.
        usart_xmit((uint8_t) -right_pwm);

        // Discard the echo.
        usart_recv(50);

        // Select motor 2 speed.
        usart_xmit(0x0003);

        // Discard the echo.
        usart_recv(50);

        // Set motor 3 speed.
        usart_xmit((uint8_t) -left_pwm);

        // Discard the echo.
        usart_recv(50);
    }

    // Release access to the USART.
    usart_release_access();
}


static int8_t motor_left_calc_pwm(int16_t motor_vel)
{
    int16_t error;
    int16_t error_d;
    int32_t pwm_output;
    static int16_t error_i;
    static int16_t prev_error;

    // Determine velocity error.
    error = motor_left_cmd - motor_vel;

    // Determine velocity error derivative.
    error_d = prev_error - error;

    // Save the current velocity error.
    prev_error = error;

    // Multiply error and error derivative by 8:8 fixed gains to determine PWM output.
    pwm_output = (int32_t) error * (int32_t) motor_left_p_gain;
    pwm_output -= (int32_t) error_d * (int32_t) motor_left_d_gain;
    pwm_output += (int32_t) error_i * (int32_t) motor_left_i_gain;

    // Shift to account for fixed point gain.
    pwm_output >>= 8;

    // Range check the PWM output.
    if (pwm_output > 127) pwm_output = 127;
    if (pwm_output < -127) pwm_output = -127;

    // Decay the integral error.
    if (error_i > 0) error_i -= 1;
    if (error_i < 0) error_i += 1;

    // Add the error to the error integral.
    error_i += error;

    // Range check the error integral.
    if (error_i > 255) error_i = 255;
    if (error_i < -255) error_i = -255;

    return (int8_t) pwm_output;
}


static int8_t motor_right_calc_pwm(int16_t motor_vel)
{
    int16_t error;
    int16_t error_d;
    int32_t pwm_output;
    static int16_t error_i;
    static int16_t prev_error;

    // Determine velocity error.
    error = motor_right_cmd - motor_vel;

    // Determine velocity error derivative.
    error_d = prev_error - error;

    // Save the current velocity error.
    prev_error = error;

    // Multiply error and error derivative by 8:8 fixed gains to determine PWM output.
    pwm_output = (int32_t) error * (int32_t) motor_right_p_gain;
    pwm_output -= (int32_t) error_d * (int32_t) motor_right_d_gain;
    pwm_output += (int32_t) error_i * (int32_t) motor_right_i_gain;

    // Shift to account for fixed point gain.
    pwm_output >>= 8;

    // Range check the PWM output.
    if (pwm_output > 127) pwm_output = 127;
    if (pwm_output < -127) pwm_output = -127;

    // Decay the integral error.
    if (error_i > 0) error_i -= 1;
    if (error_i < 0) error_i += 1;

    // Add the error to the error integral.
    error_i += error;

    // Range check the error integral.
    if (error_i > 255) error_i = 255;
    if (error_i < -255) error_i = -255;

    return (int8_t) pwm_output;
}


NAKEDFUNC(motor_task)
// Motor control task.
{
    // Initialize the gains.
    motor_left_p_gain = 0x0780;
    motor_left_d_gain = 0x0066;
    motor_left_i_gain = 0x0099;
    motor_right_p_gain = 0x0780;
    motor_right_d_gain = 0x0066;
    motor_right_i_gain = 0x0099;

    // Prime the motor mutex.
    AvrXSetSemaphore(&motor_mutex);

    // Initialize the encoder module.
    encoder_init();

    // Main control loop.
    for (;;)
    {
        register int8_t left_pwm = 0;
        register int8_t right_pwm = 0;

        // Start the 10 millisecond timer.
        AvrXStartTimer(&motor_timer, 10);

        // Update the motor encoder values.
        encoder_update();

        // Is PWM enabled to the motors?
        if (motor_enabled)
        {
            // Get the left and right pwm values using the delta between encoder
            // updates as the velocity over the last 10 milliseconds.
            left_pwm = motor_left_calc_pwm(encoder_get_left_delta());
            right_pwm = motor_right_calc_pwm(encoder_get_right_delta());
        }

    	// Update motors with new PWM values.
        motor_pwm_set(left_pwm, right_pwm);

    	// Wait for the remainder of the 10 milliseconds to elapse.
        AvrXWaitTimer(&motor_timer);
    }
}


