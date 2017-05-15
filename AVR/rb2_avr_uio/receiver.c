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

    Vex receiver task.
*/

#include <stdio.h>
#include <stdint.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "avrx.h"
#include "receiver.h"
#include "usart.h"

// The timer clock prescaler of 8 is selected to yield a 2.5MHz ADC clock
// from an 20 MHz system clock.
#define CSPS        ((0<<CS12) | (1<<CS11) | (0<<CS10))

// Maximum duration of a non-sync pulse is 4000 uS (10000 clock ticks).
#define MAX_DURATION            10000

// Note: Assuming globals are zeroed.
uint8_t pulse_sync;
uint8_t pulse_index;
uint16_t pulse_start;
uint16_t pulse_duration;
int16_t pulse_norm;
int8_t pulses[6];

// Task control.
AVRX_MUTEX(receiver_mutex);
AVRX_TIMER(receiver_timer);

int8_t receiver_read(uint8_t channel)
// Read the value from the specified channel.
{
    // Return the specified channel value.
    return pulses[channel % 6];
}

NAKEDFUNC(receiver_task)
// Task for Vex receiver.
{
    pulses[0] = 1;
    pulses[1] = 2;
    pulses[2] = 3;
    pulses[3] = 4;
    pulses[4] = 5;

    // Enable PB0/ICP1 as an input with a pull-up.
    DDRB &= ~(1<<DDB0);
    PORTB |= (1<<PB0);

    // Initialize the timer/counter1.
    TCNT1 = 0;

    // Set the period to look for a pulse.
    OCR1A = MAX_DURATION;

    // Clear all flags.
    TIFR1 = (1<<ICF1) |
            (1<<OCF1B) |
            (1<<OCF1A) |
            (1<<TOV1);

    // Set timer/counter1 control register A.
    TCCR1A = (0<<COM1A1) | (0<<COM1A0) |                    // Disconnect OC1A.
             (0<<COM1B1) | (0<<COM1B0) |                    // Disconnect OC1B.
             (0<<WGM11) | (0<<WGM10);                       // Mode 0 - normal operation.

    // Set timer/counter1 control register B.
    TCCR1B = (1<<ICNC1) |                                   // Enable input capture noise canceler.
             (0<<ICES1) |                                   // Enable falling edge detect.
             (0<<WGM13) | (0<<WGM12) |                      // Mode 0 - normal operation.
             CSPS;                                          // Clock select prescale.

    // Set timer/counter1 control register C.
    TCCR1C = (0<<FOC1A) | (0<<FOC1B);                       // No force output compare for A or B.

    // Enable both input capture and output compare interrupts.
    TIMSK1 = (1<<ICIE1);                                    // Enable input capture interrupt.

    // Initialize the pulse index.
    pulse_index = 6;

    // Loop forever.
    for (;;)
    {
        // Start the timer.
        AvrXStartTimer(&receiver_timer, 20);

        // Wait for timer or signal to wake up task.
        AvrXWaitTimer(&receiver_timer);

        // Determine if the timer expired or was signaled.
        if (AvrXCancelTimer(&receiver_timer) != 0)
        {
            // Was a sync received?
            if (pulse_sync)
            {
                // Reset the pulse index.
                pulse_index = 0;

                // Reset the sync flag.
                pulse_sync = 0;
            }
            else if (pulse_index < 6)
            {
                // Normalize the pulse fit within 8 bits of data.
                pulse_norm = (int16_t) ((((int32_t) pulse_duration - 2650) * 128) / 1024);
                if (pulse_norm > 127) pulse_norm = 127;
                if (pulse_norm < -127) pulse_norm = -127;

                // Update the indexed pulse duration.
                pulses[pulse_index++] = (int8_t) pulse_norm;
            }
        }
        else
        {
            // Zero out the pulse table.
            for (pulse_index = 0; pulse_index < 6; ++pulse_index) pulses[pulse_index] = 0;
        }

        // Reset the timer semaphore after being canceled.
        receiver_timer.semaphore = SEM_PEND;
    }
}

// Global to quickly store ICR1 value.
static uint16_t icr;

INTERFACE void AvrXIntSetObjectSemaphore(pMutex);

AVRX_SIGINT(TIMER1_CAPT_vect)
// Handles timer/counter1 capture interrupt.  We receive this 
// either on the rising or falling edge of a pulse.
{
    // Switch to kernel stack.
    IntProlog();

    // Capture the ICR.
    icr = ICR1;

    // Handle rising or falling edge of pulse.
    if (TCCR1B & (1<<ICES1))
    {
        // Rising edge of pulse.

        // Determine the pulse duration.
        pulse_duration = icr - pulse_start;

        // Wait for falling edge.
        TCCR1B &= ~(1<<ICES1);

        // Set the sync flag if OCR1A indicates maximum duration exceeded.
        if (TIFR1 & (1<<OCF1A)) pulse_sync = 1;

        // Clear all flags.
        TIFR1 = (1<<ICF1) | (1<<OCF1B) | (1<<OCF1A) | (1<<TOV1);

        // Update OCR1A to avoid false trigger of sync flag.
        OCR1A = icr + MAX_DURATION;

        // Signal that we received a rising pulse edge.
        AvrXIntSetObjectSemaphore((pMutex) &receiver_timer);
    }
    else
    {
        // Falling edge of pulse.

        // Save the start time.
        pulse_start = icr;

        // Clear all flags.
        TIFR1 = (1<<ICF1) | (1<<OCF1B) | (1<<OCF1A) | (1<<TOV1);

        // Set OCF1A flag if we exceed the maximum duration for a pulse.
        OCR1A = icr + MAX_DURATION;

        // Wait for rising edge.
        TCCR1B |= (1<<ICES1);
    }

    // Go  back to RTOS
    Epilog();
}

