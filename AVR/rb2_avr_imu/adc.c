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
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "avrx.h"
#include "config.h"
#include "adc.h"

//
// Sense 5DOF IMU - ATmega168
// ==========================
//
// The 10-bit Analog to Digital Converter (ADC) on the ATmega168 is used to 
// provide feedback from the IMU accelerometer and gyros.  The analog inputs 
// are assigned as follows:
//
//  ADC0 (PC0) - Accelerometer X
//  ADC1 (PC1) - Accelerometer Y
//  ADC2 (PC2) - Accelerometer Z
//  ADC3 (PC3) - VRef
//  ADC4 (PC4) - Gyroscope Y
//  ADC5 (PC5) - Gyroscope X
//
// Currently we only concern ourselves with rotation around one axis
// and utilize the Y & Z accelerometer and X gyroscope values.
//

// Defines for IMU channels we are gathering values for.
#define ADC_CHANNEL_ACCEL_Y     1
#define ADC_CHANNEL_ACCEL_Z     2
#define ADC_CHANNEL_GYRO_X      5

// The ADC clock prescaler of 128 is selected to yield a 156.25 KHz ADC clock
// from an 20 MHz system clock.  The ADC clock must be between 50 KHz and
// 200 KHz for maximum resolution.
#define ADPS        ((1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0))

// Note: Assuming globals are zeroed.

// Measured values.
static int16_t meas_accel_y;
static int16_t meas_accel_z;
static int16_t meas_gyro_x;

// Output values.
static int16_t adc_accel_y;
static int16_t adc_accel_z;
static int16_t adc_gyro_x;

// Task control.
AVRX_MUTEX(adc_ready);
AVRX_MUTEX(adc_mutex);
AVRX_TIMER(adc_delay);

//
// Digital Lowpass Filter Implementation
//
// See: A Simple Software Lowpass Filter Suits Embedded-system Applications
// http://www.edn.com/article/CA6335310.html
//
// k    Bandwidth (Normalized to 1Hz)   Rise Time (samples)
// 1    0.1197                          3
// 2    0.0466                          8
// 3    0.0217                          16
// 4    0.0104                          34
// 5    0.0051                          69
// 6    0.0026                          140
// 7    0.0012                          280
// 8    0.0007                          561
//


#define FILTER_SHIFT 1

static int16_t adc_gyro_x_filter(int16_t input)
{
    // Assuming statics are initialized to zero.
    static int32_t filter_reg;

    // Update the filter with the current input.
    filter_reg = filter_reg - (filter_reg >> FILTER_SHIFT) + input;

    // Scale output for unity gain.
    return (int16_t) (filter_reg >> FILTER_SHIFT);
}

static int16_t adc_accel_y_filter(int16_t input)
{
    // Assuming statics are initialized to zero.
    static int32_t filter_reg;

    // Update the filter with the current input.
    filter_reg = filter_reg - (filter_reg >> FILTER_SHIFT) + input;

    // Scale output for unity gain.
    return (int16_t) (filter_reg >> FILTER_SHIFT);
}


static int16_t adc_accel_z_filter(int16_t input)
{
    // Assuming statics are initialized to zero.
    static int32_t filter_reg;

    // Update the filter with the current input.
    filter_reg = filter_reg - (filter_reg >> FILTER_SHIFT) + input;

    // Scale output for unity gain.
    return (int16_t) (filter_reg >> FILTER_SHIFT);
}


void adc_get_values(int16_t *gyro_x, int16_t *accel_y, int16_t *accel_z)
// Get the ADC values.
{
    // Get exclusive access to ADC values for update.
    AvrXWaitSemaphore(&adc_mutex);

    // Return each value with a non-null pointer.
    if (gyro_x) *gyro_x = adc_gyro_x;
    if (accel_y) *accel_y = adc_accel_y;
    if (accel_z) *accel_z = adc_accel_z;

    // Release exclusive access to the ADC values.
    AvrXSetSemaphore(&adc_mutex);
}


NAKEDFUNC(adc_task)
// Task to process the IMU data.
{
    // Prime ADC mutex.
    AvrXSetSemaphore(&adc_mutex);

    // Make sure ports PC1, PC2 and PC5 are configured as inputs.
    PORTC &= ~((1<<PC1) | (1<<PC2) | (1<<PC5));

    // Disable digital input for ADC1, ADC2 and ADC5.
    DIDR0 |= (1<<ADC1D) | (1<<ADC2D) | (1<<ADC5D);

    // Loop continuously gathering ADC samples.
    for (;;)
    {
        // Set the ADC control and status register B.
        ADCSRB = (0<<ADTS2) | (0<<ADTS1) | (0<<ADTS0);  // Free running mode, but auto trigger is not set.

        // Set the ADC multiplexer selection register.
        ADMUX = (0<<REFS1) | (0<<REFS0) |               // Select AREF as voltage reference.
                (0<<ADLAR) |                            // Keep high bits right adjusted.
                ADC_CHANNEL_GYRO_X;                     // Select the next channel.

        // Delay for 2 clock ticks to allow ADC capacitor to charge.
        AvrXDelay(&adc_delay, 2);

        // Initiate the next ADC sample.
        ADCSRA = (1<<ADEN) |                            // Enable ADC.
                 (1<<ADSC) |                            // Enable the first sample.
                 (0<<ADATE) |                           // Disable auto triggering.
                 (1<<ADIF) |                            // Clear any active interrupt.
                 (1<<ADIE) |                            // Enable ADC complete interrupt.
                 ADPS;                                  // Prescale -- see above.

        // Wait for the conversion to be complete.
        AvrXWaitSemaphore(&adc_ready);

        // Store the filtered ADC channel value.
        cli(); meas_gyro_x = ADCW; sei();
        meas_gyro_x = adc_gyro_x_filter(meas_gyro_x);

        // Set the ADC multiplexer selection register.
        ADMUX = (0<<REFS1) | (0<<REFS0) |               // Select AREF as voltage reference.
                (0<<ADLAR) |                            // Keep high bits right adjusted.
                ADC_CHANNEL_ACCEL_Y;                    // Select first channel.

        // Delay for 2 clock ticks to allow ADC capacitor to charge.
        AvrXDelay(&adc_delay, 2);

        // Initiate the next ADC sample.
        ADCSRA = (1<<ADEN) |                            // Enable ADC.
                 (1<<ADSC) |                            // Enable the first sample.
                 (0<<ADATE) |                           // Disable auto triggering.
                 (1<<ADIF) |                            // Clear any active interrupt.
                 (1<<ADIE) |                            // Enable ADC complete interrupt.
                 ADPS;                                  // Prescale -- see above.

        // Wait for the conversion to be complete.
        AvrXWaitSemaphore(&adc_ready);

        // Store the filtered ADC channel value.
        cli(); meas_accel_y = ADCW; sei();
        meas_accel_y = adc_accel_y_filter(meas_accel_y);

        // Set the ADC multiplexer selection register.
        ADMUX = (0<<REFS1) | (0<<REFS0) |               // Select AREF as voltage reference.
                (0<<ADLAR) |                            // Keep high bits right adjusted.
                ADC_CHANNEL_ACCEL_Z;                    // Select the next channel.

        // Delay for 2 clock ticks to allow ADC capacitor to charge.
        AvrXDelay(&adc_delay, 2);

        // Initiate the next ADC sample.
        ADCSRA = (1<<ADEN) |                            // Enable ADC.
                 (1<<ADSC) |                            // Enable the first sample.
                 (0<<ADATE) |                           // Disable auto triggering.
                 (1<<ADIF) |                            // Clear any active interrupt.
                 (1<<ADIE) |                            // Enable ADC complete interrupt.
                 ADPS;                                  // Prescale -- see above.

        // Wait for the conversion to be complete.
        AvrXWaitSemaphore(&adc_ready);

        // Store the filtered ADC channel value.
        cli(); meas_accel_z = ADCW; sei();
        meas_accel_z = adc_accel_z_filter(meas_accel_z);

        // Get exclusive access to ADC values for update.
        AvrXWaitSemaphore(&adc_mutex);

        // Update the adc values from the measured values.
        adc_gyro_x = meas_gyro_x;
        adc_accel_y = meas_accel_y;
        adc_accel_z = meas_accel_z;

        // Release exclusive access to the ADC values.
        AvrXSetSemaphore(&adc_mutex);
    }
}


AVRX_SIGINT(ADC_vect)
// ADC complete interrupt handler.
{
    // Switch to kernel stack.
    IntProlog();

    // Signal that ADC sample is complete.
    AvrXIntSetSemaphore(&adc_ready);

    // Enable other interrupt activity.
    sei();
    
    // Go back to RTOS.
    Epilog();
}




