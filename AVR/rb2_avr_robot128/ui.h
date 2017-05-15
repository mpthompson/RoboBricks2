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

#ifndef _RB2_UI_H_
#define _RB2_UI_H_ 1

// User interface menu states.

#define ST_TOP                  1

#define ST_MOTOR_MENU           10
#define ST_MOTOR_ENABLE         11
#define ST_MOTOR_VELOCITY       12
#define ST_MOTOR_PWM            13
#define ST_MOTOR_P_GAIN         14
#define ST_MOTOR_D_GAIN         15
#define ST_MOTOR_I_GAIN         16

#define ST_MOTOR_ENABLE_SEL     21
#define ST_MOTOR_VELOCITY_SEL   22
#define ST_MOTOR_PWM_SEL        23
#define ST_MOTOR_P_GAIN_SEL     24
#define ST_MOTOR_D_GAIN_SEL     25
#define ST_MOTOR_I_GAIN_SEL     26

#define ST_BALANCE_MENU         30
#define ST_BALANCE_P_GAIN       31
#define ST_BALANCE_D_GAIN       32
#define ST_BALANCE_I_GAIN       33
#define ST_BALANCE_T_COMP       34

#define ST_BALANCE_P_GAIN_SEL   41
#define ST_BALANCE_D_GAIN_SEL   42
#define ST_BALANCE_I_GAIN_SEL   43
#define ST_BALANCE_T_COMP_SEL   44

#define ST_SPEED_MENU           50
#define ST_SPEED_P_GAIN         51
#define ST_SPEED_D_GAIN         52
#define ST_SPEED_I_GAIN         53

#define ST_SPEED_P_GAIN_SEL     61
#define ST_SPEED_D_GAIN_SEL     62
#define ST_SPEED_I_GAIN_SEL     63

#define ST_CONTROL_MENU         70
#define ST_CONTROL_RC           71

#define ST_CONTROL_RC_SEL       81

#define ST_IMU_MENU             90
#define ST_IMU_PITCH            91
#define ST_IMU_RAW              92

#define ST_IMU_PITCH_SEL        101
#define ST_IMU_RAW_SEL          102

#define ST_BOOT_MENU            110
#define ST_BOOT_ENABLE          111

#endif // _RB2_UI_H_
