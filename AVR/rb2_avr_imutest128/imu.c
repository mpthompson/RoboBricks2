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
#include "avrx.h"
#include "imu.h"
#include "usart.h"

#define IMU_GET_COUNT       1
#define IMU_GET_STATUS      1

// Task control.
AVRX_MUTEX(imu_mutex);

// State variables.
static int16_t imu_pitch_rate;
static int16_t imu_pitch_angle;
static uint16_t imu_gyro_x;
static uint16_t imu_accel_y;
static uint16_t imu_accel_z;

void imu_init(void)
// Initialize the IMU module.
{
    // Prime the IMU semaphore.
    AvrXSetSemaphore(&imu_mutex);
}


uint8_t imu_update(void)
// Update the imu pitch angle and rate.
{
    uint8_t rv = 0;

    // Grab access to the USART.
    usart_grab_access();

    // Select the IMU.
    usart_xmit_discard_echo(0x0140);

    // Get and validate the response.
    if (usart_recv() == 0x00A5)
    {
        // Latch the pitch angle and rate.
        usart_xmit_discard_echo(0x00);

        // Get and validate the response.
        if (usart_recv() == 0x00A5)
        {
            // Get exclusive access to the IMU values.
            AvrXWaitSemaphore(&imu_mutex);

            // IMU Pitch Angle

            // Get the first byte.
            usart_xmit_discard_echo(0x01);

            // Receive the first byte.
            imu_pitch_angle = (uint8_t) usart_recv();

            // Get the second byte.
            usart_xmit_discard_echo(0x02);

            // Receive the second byte.
            imu_pitch_angle = (imu_pitch_angle << 8) | usart_recv();

            // IMU Pitch Rate

            // Get the first byte.
            usart_xmit_discard_echo(0x03);

            // Receive the first byte.
            imu_pitch_rate = (uint8_t) usart_recv();

            // Get the second byte.
            usart_xmit_discard_echo(0x04);

            // Receive the second byte.
            imu_pitch_rate = (imu_pitch_rate << 8) | usart_recv();

            // IMU Gyro X

            // Get the first byte.
            usart_xmit_discard_echo(0x05);

            // Receive the first byte.
            imu_gyro_x = (uint8_t) usart_recv();

            // Get the second byte.
            usart_xmit_discard_echo(0x06);

            // Receive the second byte.
            imu_gyro_x = (imu_gyro_x << 8) | usart_recv();

            // IMU Accel Y

            // Get the first byte.
            usart_xmit_discard_echo(0x07);

            // Receive the first byte.
            imu_accel_y = (uint8_t) usart_recv();

            // Get the second byte.
            usart_xmit_discard_echo(0x08);

            // Receive the second byte.
            imu_accel_y = (imu_accel_y << 8) | usart_recv();

            // IMU Accel Z

            // Get the first byte.
            usart_xmit_discard_echo(0x09);

            // Receive the first byte.
            imu_accel_z = (uint8_t) usart_recv();

            // Get the second byte.
            usart_xmit_discard_echo(0x0a);

            // Receive the second byte.
            imu_accel_z = (imu_accel_z << 8) | usart_recv();

            // Give up exclusive access to the IMU values.
            AvrXSetSemaphore(&imu_mutex);

            // We succeeded.
            rv = 1;
        }
    }

    // Release access to the USART.
    usart_release_access();

    return rv;
}


void imu_pitch_get(int16_t *angle, int16_t *rate)
// Get the pitch angle and rate values.
{
    // Get exclusive access to the IMU values.
    AvrXWaitSemaphore(&imu_mutex);

    // Return the pitch angle and rate.
    *angle = imu_pitch_angle;
    *rate = imu_pitch_rate;

    // Give up exclusive access to the IMU values.
    AvrXSetSemaphore(&imu_mutex);
}


void imu_raw_get(uint16_t *gyro_x, uint16_t *accel_y, uint16_t *accel_z)
// Get the gyro and accelerometer raw values.
{
    // Get exclusive access to the IMU values.
    AvrXWaitSemaphore(&imu_mutex);

    // Return the status and count.
    *gyro_x = imu_gyro_x;
    *accel_y = imu_accel_y;
    *accel_z = imu_accel_z;

    // Give up exclusive access to the IMU values.
    AvrXSetSemaphore(&imu_mutex);
}


