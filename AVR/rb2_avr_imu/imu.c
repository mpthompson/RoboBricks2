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
#include <math.h>
#include "avrx.h"
#include "config.h"
#include "adc.h"
#include "imu.h"
#include "tilt.h"

// Note: Assuming globals are zeroed.

// ADC measurement samples.
static int16_t meas_accel_y;
static int16_t meas_accel_z;
static int16_t meas_gyro_x;

// Kalman state variables.
static float gyro_x;
static float accel_y;
static float accel_z;
static float pitch_rate;
static float pitch_measured;
static float pitch_angle;
static tilt pitch_tilt_state;

// Output variables.
static int16_t imu_accel_y;
static int16_t imu_accel_z;
static int16_t imu_gyro_x;
static int16_t imu_pitch_angle;
static int16_t imu_pitch_rate;

// Latched variables.
static int16_t latched_accel_y;
static int16_t latched_accel_z;
static int16_t latched_gyro_x;
static int16_t latched_pitch_angle;
static int16_t latched_pitch_rate;

// IMU timer control block.
AVRX_TIMER(imu_timer);

// Semaphore for exclusive access to angle position and rate.
AVRX_MUTEX(imu_mutex);

void imu_latch(void)
// Latch the current IMU angle position and rate.
{
    // Get exclusive access to IMU values for update.
    AvrXWaitSemaphore(&imu_mutex);

    // Latch the measured raw values.
    latched_accel_y = imu_accel_y;
    latched_accel_z = imu_accel_z;
    latched_gyro_x = imu_gyro_x;

    // Latch the angle and rate.
    latched_pitch_rate = imu_pitch_rate;
    latched_pitch_angle = imu_pitch_angle;

    // Release exclusive access to the IMU values.
    AvrXSetSemaphore(&imu_mutex);
}


int16_t imu_get_pitch_angle(void)
// Get latched IMU angle position as 8:8 fixed point value.
{
    return latched_pitch_angle;
}


int16_t imu_get_pitch_rate(void)
// Get latched IMU angle rate as 8:8 fixed point value.
{
    return latched_pitch_rate;
}


int16_t imu_get_gyro_x(void)
// Get latched gyro x value.
{
    return latched_gyro_x;
}


int16_t imu_get_accel_y(void)
// Get latched acceleration y value.
{
    return latched_accel_y;
}


int16_t imu_get_accel_z(void)
// Get latched acceleration z value.
{
    return latched_accel_z;
}


NAKEDFUNC(imu_task)
// Task to process the IMU data.
{
    // Prime the mutex semaphore.
    AvrXSetSemaphore(&imu_mutex);

    // Initialize the tilt module.
    tilt_init(&pitch_tilt_state, 0.02, 0.3, 0.003, 0.001);

    // Loop processing IMU data.
    for (;;)
    {
        // Start the 20 millisecond timer.
        AvrXStartTimer(&imu_timer, 20);

        // Grab the latest ADC samples.
        adc_get_values(&meas_gyro_x, &meas_accel_y, &meas_accel_z);

        // Convert the raw measured IMU values to floating point values.
        gyro_x = (float) meas_gyro_x;
        accel_y = (float) meas_accel_y;
        accel_z = (float) meas_accel_z;

        // Zero adjust the gyro values.  A better way of dynamically determining
        // these values must be found rather than using hard coded constants.
        gyro_x -= 514.0;

        // Zero adjust the accelerometer values.  A better way of dynamically determining
        // these values must be found rather than using hard coded constants.
        accel_y -= 512.0;
        accel_z -= 512.0;

        // Determine the pitch in radians using the Y and Z acclerometer data.  Note the 
        // accelerometer vectors that are perpendicular to the rotation of the axis are used.  
        // We can compute the angle for the full 360 degree rotation with no linearization 
        // errors by using the arctangent of the two accelerometer readings.  The accelerometer 
        // values do not need to be scaled into actual units, but must be zeroed and have the 
        // same scale.  Note that we manipulate the sign of the acceleration so the sign of 
        // the accelerometer derived angles match the gyro rates.
        pitch_measured = atan2(accel_y, accel_z);

        // Determine gyro angular rate from raw analog values.
        // Each ADC unit: 3000 / 1024 = 2.9297 mV
        // Gyro measures rate: 114.591559 mV/radians/second
        // Each ADC unit equals: 2.9297 / 114.591559 = 0.025566346 radians/sec
        // Gyro rate: adc * 0.025566346 radians/sec
        pitch_rate = gyro_x * 0.025566346;
        
        // Pass the measured pitch and pitch rate through the Extended Kalman filter to
        // determine the estimated pitch values in radians.
        tilt_state_update(&pitch_tilt_state, pitch_rate);
        tilt_kalman_update(&pitch_tilt_state, pitch_measured);

        // Get the estimated pitch rate and pitch angle in degrees.
        pitch_rate = tilt_get_rate(&pitch_tilt_state) * 57.29578;
        pitch_angle = tilt_get_angle(&pitch_tilt_state) * 57.29578;

        // Get exclusive access to IMU values for update.
        AvrXWaitSemaphore(&imu_mutex);

        // Save the measured raw values.
        imu_gyro_x = meas_gyro_x;
        imu_accel_y = meas_accel_y;
        imu_accel_z = meas_accel_z;

        // Save the computed angle and rate as 8:8 fixed point values.
        imu_pitch_rate = (int16_t) (pitch_rate * 256.0);
        imu_pitch_angle = (int16_t) (pitch_angle * 256.0);

        // Release exclusive access to the IMU values.
        AvrXSetSemaphore(&imu_mutex);

        // Wait for the remainder of the 20 milliseconds to elapse.
        AvrXWaitTimer(&imu_timer);
    }
}

