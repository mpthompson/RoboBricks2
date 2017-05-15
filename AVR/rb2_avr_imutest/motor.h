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

#ifndef _RB2_MOTOR_H_
#define _RB2_MOTOR_H_ 1

void motor_enable_set(uint8_t enable);
uint8_t motor_enable_get(void);
void motor_command_set(int16_t *left_cmd, int16_t *right_cmd);
void motor_command_get(int16_t *left_cmd, int16_t *right_cmd);
void motor_left_gains_set(int16_t *p_gain, int16_t *d_gain, int16_t *i_gain);
void motor_left_gains_get(int16_t *p_gain, int16_t *d_gain, int16_t *i_gain);
void motor_right_gains_set(int16_t *p_gain, int16_t *d_gain, int16_t *i_gain);
void motor_right_gains_get(int16_t *p_gain, int16_t *d_gain, int16_t *i_gain);
void motor_pwm_get(int8_t *left_pwm, int8_t *right_pwm);

void motor_init(void);
void motor_update(void);

#endif // _RB2_MOTOR_H_
