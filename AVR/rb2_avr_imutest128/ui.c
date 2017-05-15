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
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "avrx.h"
#include "balance.h"
#include "bootloader.h"
#include "control.h"
#include "motor.h"
#include "imu.h"
#include "lcd.h"
#include "speed.h"
#include "ui.h"
#include "uio.h"

// Note: Assume globals are zeroed.
uint8_t ui_button;
uint16_t ui_response;

typedef struct PROGMEM
{
    uint8_t state;
    const char *text;
    uint8_t (*func)(uint8_t input);
} menu_state;

typedef struct PROGMEM
{
    uint8_t state;
    uint8_t input;
    uint8_t state_next;
} menu_state_next;

// Predeclare functions.
static uint8_t ui_motor_enable(uint8_t input);
static uint8_t ui_motor_velocity(uint8_t input);
static uint8_t ui_motor_pwm(uint8_t input);
static uint8_t ui_motor_p_gain(uint8_t input);
static uint8_t ui_motor_d_gain(uint8_t input);
static uint8_t ui_motor_i_gain(uint8_t input);
static uint8_t ui_balance_p_gain(uint8_t input);
static uint8_t ui_balance_d_gain(uint8_t input);
static uint8_t ui_balance_i_gain(uint8_t input);
static uint8_t ui_balance_t_comp(uint8_t input);
static uint8_t ui_speed_p_gain(uint8_t input);
static uint8_t ui_speed_d_gain(uint8_t input);
static uint8_t ui_speed_i_gain(uint8_t input);
static uint8_t ui_control_rc(uint8_t input);
static uint8_t ui_imu_pitch(uint8_t input);
static uint8_t ui_imu_raw(uint8_t input);
static uint8_t ui_boot_enable(uint8_t input);

const char MT_TOP[] PROGMEM                         = "\x0c" "Balance 'Bot";

const char MT_MOTOR_MENU[] PROGMEM                  = "\x0c" "Motor";
const char MT_MOTOR_ENABLE[] PROGMEM                = "\x0c" "Enable";
const char MT_MOTOR_VELOCITY[] PROGMEM              = "\x0c" "Velocity";
const char MT_MOTOR_PWM[] PROGMEM                   = "\x0c" "PWM";
const char MT_MOTOR_P_GAIN[] PROGMEM                = "\x0c" "P Gain";
const char MT_MOTOR_D_GAIN[] PROGMEM                = "\x0c" "D Gain";
const char MT_MOTOR_I_GAIN[] PROGMEM                = "\x0c" "I Gain";

const char MT_BALANCE_MENU[] PROGMEM                = "\x0c" "Balance";
const char MT_BALANCE_P_GAIN[] PROGMEM              = "\x0c" "P Gain";
const char MT_BALANCE_D_GAIN[] PROGMEM              = "\x0c" "D Gain";
const char MT_BALANCE_I_GAIN[] PROGMEM              = "\x0c" "I Gain";
const char MT_BALANCE_T_COMP[] PROGMEM              = "\x0c" "T Comp";

const char MT_SPEED_MENU[] PROGMEM                  = "\x0c" "Speed";
const char MT_SPEED_P_GAIN[] PROGMEM                = "\x0c" "P Gain";
const char MT_SPEED_D_GAIN[] PROGMEM                = "\x0c" "D Gain";
const char MT_SPEED_I_GAIN[] PROGMEM                = "\x0c" "I Gain";

const char MT_CONTROL_MENU[] PROGMEM                = "\x0c" "Control";
const char MT_CONTROL_RC[] PROGMEM                  = "\x0c" "RC Values";

const char MT_IMU_MENU[] PROGMEM                    = "\x0c" "IMU";
const char MT_IMU_PITCH[] PROGMEM                   = "\x0c" "Pitch & Rate";
const char MT_IMU_RAW[] PROGMEM                     = "\x0c" "Raw Values";


const char MT_BOOT_MENU[] PROGMEM                 = "\x0c" "Bootloader";

const menu_state_next ui_menu_state_next[] PROGMEM =
{
//  STATE                       INPUT           NEXT STATE
    { ST_TOP,                   BUTTON_RIGHT,   ST_MOTOR_MENU },

    { ST_MOTOR_MENU,            BUTTON_UP,      ST_BOOT_MENU },
    { ST_MOTOR_MENU,            BUTTON_DOWN,    ST_BALANCE_MENU },
    { ST_MOTOR_MENU,            BUTTON_LEFT,    ST_TOP },
    { ST_MOTOR_MENU,            BUTTON_RIGHT,   ST_MOTOR_ENABLE },

    { ST_BALANCE_MENU,          BUTTON_UP,      ST_MOTOR_MENU },
    { ST_BALANCE_MENU,          BUTTON_DOWN,    ST_SPEED_MENU },
    { ST_BALANCE_MENU,          BUTTON_LEFT,    ST_TOP },
    { ST_BALANCE_MENU,          BUTTON_RIGHT,   ST_BALANCE_P_GAIN },

    { ST_SPEED_MENU,            BUTTON_UP,      ST_BALANCE_MENU },
    { ST_SPEED_MENU,            BUTTON_DOWN,    ST_CONTROL_MENU },
    { ST_SPEED_MENU,            BUTTON_LEFT,    ST_TOP },
    { ST_SPEED_MENU,            BUTTON_RIGHT,   ST_SPEED_P_GAIN },

    { ST_CONTROL_MENU,          BUTTON_UP,      ST_SPEED_MENU },
    { ST_CONTROL_MENU,          BUTTON_DOWN,    ST_IMU_MENU },
    { ST_CONTROL_MENU,          BUTTON_LEFT,    ST_TOP },
    { ST_CONTROL_MENU,          BUTTON_RIGHT,   ST_CONTROL_RC },

    { ST_IMU_MENU,              BUTTON_UP,      ST_CONTROL_MENU },
    { ST_IMU_MENU,              BUTTON_DOWN,    ST_BOOT_MENU },
    { ST_IMU_MENU,              BUTTON_LEFT,    ST_TOP },
    { ST_IMU_MENU,              BUTTON_RIGHT,   ST_IMU_PITCH },

    { ST_BOOT_MENU,             BUTTON_UP,      ST_IMU_MENU },
    { ST_BOOT_MENU,             BUTTON_DOWN,    ST_MOTOR_MENU },
    { ST_BOOT_MENU,             BUTTON_LEFT,    ST_TOP },
    { ST_BOOT_MENU,             BUTTON_RIGHT,   ST_BOOT_ENABLE },

    { ST_MOTOR_ENABLE,          BUTTON_UP,      ST_MOTOR_I_GAIN },
    { ST_MOTOR_ENABLE,          BUTTON_DOWN,    ST_MOTOR_VELOCITY },
    { ST_MOTOR_ENABLE,          BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_ENABLE,          BUTTON_RIGHT,   ST_MOTOR_ENABLE_SEL },

    { ST_MOTOR_VELOCITY,        BUTTON_UP,      ST_MOTOR_ENABLE },
    { ST_MOTOR_VELOCITY,        BUTTON_DOWN,    ST_MOTOR_PWM },
    { ST_MOTOR_VELOCITY,        BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_VELOCITY,        BUTTON_RIGHT,   ST_MOTOR_VELOCITY_SEL },

    { ST_MOTOR_PWM,             BUTTON_UP,      ST_MOTOR_VELOCITY },
    { ST_MOTOR_PWM,             BUTTON_DOWN,    ST_MOTOR_P_GAIN },
    { ST_MOTOR_PWM,             BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_PWM,             BUTTON_RIGHT,   ST_MOTOR_PWM_SEL },

    { ST_MOTOR_P_GAIN,          BUTTON_UP,      ST_MOTOR_PWM },
    { ST_MOTOR_P_GAIN,          BUTTON_DOWN,    ST_MOTOR_D_GAIN },
    { ST_MOTOR_P_GAIN,          BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_P_GAIN,          BUTTON_RIGHT,   ST_MOTOR_P_GAIN_SEL },

    { ST_MOTOR_D_GAIN,          BUTTON_UP,      ST_MOTOR_P_GAIN },
    { ST_MOTOR_D_GAIN,          BUTTON_DOWN,    ST_MOTOR_I_GAIN },
    { ST_MOTOR_D_GAIN,          BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_D_GAIN,          BUTTON_RIGHT,   ST_MOTOR_D_GAIN_SEL },

    { ST_MOTOR_I_GAIN,          BUTTON_UP,      ST_MOTOR_D_GAIN },
    { ST_MOTOR_I_GAIN,          BUTTON_DOWN,    ST_MOTOR_ENABLE },
    { ST_MOTOR_I_GAIN,          BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_I_GAIN,          BUTTON_RIGHT,   ST_MOTOR_I_GAIN_SEL },

    { ST_BALANCE_P_GAIN,        BUTTON_UP,      ST_BALANCE_T_COMP },
    { ST_BALANCE_P_GAIN,        BUTTON_DOWN,    ST_BALANCE_D_GAIN },
    { ST_BALANCE_P_GAIN,        BUTTON_LEFT,    ST_BALANCE_MENU },
    { ST_BALANCE_P_GAIN,        BUTTON_RIGHT,   ST_BALANCE_P_GAIN_SEL },

    { ST_BALANCE_D_GAIN,        BUTTON_UP,      ST_BALANCE_P_GAIN },
    { ST_BALANCE_D_GAIN,        BUTTON_DOWN,    ST_BALANCE_I_GAIN },
    { ST_BALANCE_D_GAIN,        BUTTON_LEFT,    ST_BALANCE_MENU },
    { ST_BALANCE_D_GAIN,        BUTTON_RIGHT,   ST_BALANCE_D_GAIN_SEL },

    { ST_BALANCE_I_GAIN,        BUTTON_UP,      ST_BALANCE_D_GAIN },
    { ST_BALANCE_I_GAIN,        BUTTON_DOWN,    ST_BALANCE_T_COMP },
    { ST_BALANCE_I_GAIN,        BUTTON_LEFT,    ST_BALANCE_MENU },
    { ST_BALANCE_I_GAIN,        BUTTON_RIGHT,   ST_BALANCE_I_GAIN_SEL },

    { ST_BALANCE_T_COMP,        BUTTON_UP,      ST_BALANCE_I_GAIN },
    { ST_BALANCE_T_COMP,        BUTTON_DOWN,    ST_BALANCE_P_GAIN },
    { ST_BALANCE_T_COMP,        BUTTON_LEFT,    ST_BALANCE_MENU },
    { ST_BALANCE_T_COMP,        BUTTON_RIGHT,   ST_BALANCE_T_COMP_SEL },

    { ST_SPEED_P_GAIN,          BUTTON_UP,      ST_SPEED_I_GAIN },
    { ST_SPEED_P_GAIN,          BUTTON_DOWN,    ST_SPEED_D_GAIN },
    { ST_SPEED_P_GAIN,          BUTTON_LEFT,    ST_SPEED_MENU },
    { ST_SPEED_P_GAIN,          BUTTON_RIGHT,   ST_SPEED_P_GAIN_SEL },

    { ST_SPEED_D_GAIN,          BUTTON_UP,      ST_SPEED_P_GAIN },
    { ST_SPEED_D_GAIN,          BUTTON_DOWN,    ST_SPEED_I_GAIN },
    { ST_SPEED_D_GAIN,          BUTTON_LEFT,    ST_SPEED_MENU },
    { ST_SPEED_D_GAIN,          BUTTON_RIGHT,   ST_SPEED_D_GAIN_SEL },

    { ST_SPEED_I_GAIN,          BUTTON_UP,      ST_SPEED_D_GAIN },
    { ST_SPEED_I_GAIN,          BUTTON_DOWN,    ST_SPEED_P_GAIN },
    { ST_SPEED_I_GAIN,          BUTTON_LEFT,    ST_SPEED_MENU },
    { ST_SPEED_I_GAIN,          BUTTON_RIGHT,   ST_SPEED_I_GAIN_SEL },

    { ST_CONTROL_RC,            BUTTON_UP,      ST_CONTROL_RC },
    { ST_CONTROL_RC,            BUTTON_DOWN,    ST_CONTROL_RC },
    { ST_CONTROL_RC,            BUTTON_LEFT,    ST_CONTROL_MENU },
    { ST_CONTROL_RC,            BUTTON_RIGHT,   ST_CONTROL_RC_SEL },

    { ST_IMU_PITCH,             BUTTON_UP,      ST_IMU_RAW },
    { ST_IMU_PITCH,             BUTTON_DOWN,    ST_IMU_RAW },
    { ST_IMU_PITCH,             BUTTON_LEFT,    ST_IMU_MENU },
    { ST_IMU_PITCH,             BUTTON_RIGHT,   ST_IMU_PITCH_SEL },

    { ST_IMU_RAW,               BUTTON_UP,      ST_IMU_PITCH },
    { ST_IMU_RAW,               BUTTON_DOWN,    ST_IMU_PITCH },
    { ST_IMU_RAW,               BUTTON_LEFT,    ST_IMU_MENU },
    { ST_IMU_RAW,               BUTTON_RIGHT,   ST_IMU_RAW_SEL },

    {0,                         0,              0}
};


const menu_state ui_menu_state[] PROGMEM =
{
//    STATE                     STATE TEXT                  STATE_FUNC          STATE_TIMEOUT
    { ST_TOP,                   MT_TOP,                     NULL },

    { ST_MOTOR_MENU,            MT_MOTOR_MENU,              NULL },
    { ST_MOTOR_ENABLE,          MT_MOTOR_ENABLE,            NULL },
    { ST_MOTOR_VELOCITY,        MT_MOTOR_VELOCITY,          NULL },
    { ST_MOTOR_PWM,             MT_MOTOR_PWM,               NULL },
    { ST_MOTOR_P_GAIN,          MT_MOTOR_P_GAIN,            NULL },
    { ST_MOTOR_D_GAIN,          MT_MOTOR_D_GAIN,            NULL },
    { ST_MOTOR_I_GAIN,          MT_MOTOR_I_GAIN,            NULL },

    { ST_MOTOR_ENABLE_SEL,      NULL,                       ui_motor_enable },
    { ST_MOTOR_VELOCITY_SEL,    NULL,                       ui_motor_velocity },
    { ST_MOTOR_PWM_SEL,         NULL,                       ui_motor_pwm },
    { ST_MOTOR_P_GAIN_SEL,      NULL,                       ui_motor_p_gain },
    { ST_MOTOR_D_GAIN_SEL,      NULL,                       ui_motor_d_gain },
    { ST_MOTOR_I_GAIN_SEL,      NULL,                       ui_motor_i_gain },

    { ST_BALANCE_MENU,          MT_BALANCE_MENU,            NULL },
    { ST_BALANCE_P_GAIN,        MT_BALANCE_P_GAIN,          NULL },
    { ST_BALANCE_D_GAIN,        MT_BALANCE_D_GAIN,          NULL },
    { ST_BALANCE_I_GAIN,        MT_BALANCE_I_GAIN,          NULL },
    { ST_BALANCE_T_COMP,        MT_BALANCE_T_COMP,          NULL },

    { ST_BALANCE_P_GAIN_SEL,    NULL,                       ui_balance_p_gain },
    { ST_BALANCE_D_GAIN_SEL,    NULL,                       ui_balance_d_gain },
    { ST_BALANCE_I_GAIN_SEL,    NULL,                       ui_balance_i_gain },
    { ST_BALANCE_T_COMP_SEL,    NULL,                       ui_balance_t_comp },

    { ST_SPEED_MENU,            MT_SPEED_MENU,              NULL },
    { ST_SPEED_P_GAIN,          MT_SPEED_P_GAIN,            NULL },
    { ST_SPEED_D_GAIN,          MT_SPEED_D_GAIN,            NULL },
    { ST_SPEED_I_GAIN,          MT_SPEED_I_GAIN,            NULL },

    { ST_SPEED_P_GAIN_SEL,      NULL,                       ui_speed_p_gain },
    { ST_SPEED_D_GAIN_SEL,      NULL,                       ui_speed_d_gain },
    { ST_SPEED_I_GAIN_SEL,      NULL,                       ui_speed_i_gain },

    { ST_CONTROL_MENU,          MT_CONTROL_MENU,            NULL },
    { ST_CONTROL_RC,            MT_CONTROL_RC,              NULL },

    { ST_CONTROL_RC_SEL,        NULL,                       ui_control_rc },

    { ST_IMU_MENU,              MT_IMU_MENU,                NULL },
    { ST_IMU_PITCH,             MT_IMU_PITCH,               NULL },
    { ST_IMU_RAW,               MT_IMU_RAW,                 NULL },

    { ST_IMU_PITCH_SEL,         NULL,                       ui_imu_pitch },
    { ST_IMU_RAW_SEL,           NULL,                       ui_imu_raw },

    { ST_BOOT_MENU,             MT_BOOT_MENU,               NULL },
    { ST_BOOT_ENABLE,           NULL,                       ui_boot_enable },

    {0,                         NULL,                       NULL }
};

static uint8_t ui_motor_enable(uint8_t input)
// Handle enable/disable of motor.
{
    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_ENABLE;

    // Handle the input.
    if ((input == BUTTON_UP) || (input == BUTTON_DOWN))
    {
        // Toggle the value.
        motor_enable_set(motor_enable_get() ? 0 : 1);
    }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_MOTOR_ENABLE);
    lcd_puts_P(motor_enable_get() ? PSTR("\r\nOn") : PSTR("\r\nOff"));

    // Stay in this state.
    return ST_MOTOR_ENABLE_SEL;
}


static uint8_t ui_motor_velocity(uint8_t input)
{
    int16_t left_vel;
    int16_t right_vel;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_VELOCITY;

    // Get the left and right motor velocity.
    motor_command_get(&left_vel, &right_vel);

    // Update the LCD with the robot state.
    lcd_puts_P(MT_MOTOR_VELOCITY);
    lcd_printf_P(PSTR("\r\n%d %d"), left_vel, right_vel);

    // Stay in this state.
    return ST_MOTOR_VELOCITY_SEL;
}


static uint8_t ui_motor_pwm(uint8_t input)
{
    int8_t left_pwm;
    int8_t right_pwm;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_PWM;

    // Get the left and right motor PWM.
    motor_pwm_get(&left_pwm, &right_pwm);

    // Update the LCD with the robot state.
    lcd_puts_P(MT_MOTOR_PWM);
    lcd_printf_P(PSTR("\r\n%d %d"), (int16_t) left_pwm, (int16_t) right_pwm);

    // Stay in this state.
    return ST_MOTOR_PWM_SEL;
}


static uint8_t ui_motor_p_gain(uint8_t input)
{
    int16_t gain;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_P_GAIN;

    // Get the gain.
    motor_left_gains_get(&gain, NULL, NULL);

    // Handle the input.
    if (input == BUTTON_UP) { gain += 26; motor_left_gains_set(&gain, NULL, NULL); motor_right_gains_set(&gain, NULL, NULL); }
    if (input == BUTTON_DOWN) { gain -= 26; motor_left_gains_set(&gain, NULL, NULL); motor_right_gains_set(&gain, NULL, NULL); }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_MOTOR_P_GAIN);
    lcd_printf_P(PSTR("\r\n%i"), gain);

    // Stay in this state.
    return ST_MOTOR_P_GAIN_SEL;
}


static uint8_t ui_motor_d_gain(uint8_t input)
{
    int16_t gain;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_D_GAIN;

    // Get the gain.
    motor_left_gains_get(NULL, &gain, NULL);

    // Handle the input.
    if (input == BUTTON_UP) { gain += 26; motor_left_gains_set(NULL, &gain, NULL); motor_right_gains_set(NULL, &gain, NULL); }
    if (input == BUTTON_DOWN) { gain -= 26; motor_left_gains_set(NULL, &gain, NULL); motor_right_gains_set(NULL, &gain, NULL); }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_MOTOR_D_GAIN);
    lcd_printf_P(PSTR("\r\n%i"), gain);

    // Stay in this state.
    return ST_MOTOR_D_GAIN_SEL;
}


static uint8_t ui_motor_i_gain(uint8_t input)
{
    int16_t gain;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_I_GAIN;

    // Get the gain.
    motor_left_gains_get(NULL, NULL, &gain);

    // Handle the input.
    if (input == BUTTON_UP) { gain += 13; motor_left_gains_set(NULL, NULL, &gain); motor_right_gains_set(NULL, NULL, &gain); }
    if (input == BUTTON_DOWN) { gain -= 13; motor_left_gains_set(NULL, NULL, &gain); motor_right_gains_set(NULL, NULL, &gain); }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_MOTOR_I_GAIN);
    lcd_printf_P(PSTR("\r\n%i"), gain);

    // Stay in this state.
    return ST_MOTOR_I_GAIN_SEL;
}


static uint8_t ui_balance_p_gain(uint8_t input)
// Handle setting the p gain value.
{
    int16_t gain;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_BALANCE_P_GAIN;

    // Get the gain.
    balance_gains_get(&gain, NULL, NULL, NULL);

    // Handle the input.
    if (input == BUTTON_UP) { gain += 12; balance_gains_set(&gain, NULL, NULL, NULL); }
    if (input == BUTTON_DOWN) { gain -= 12; balance_gains_set(&gain, NULL, NULL, NULL); }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_BALANCE_P_GAIN);
    lcd_printf_P(PSTR("\r\n%i"), gain);

    // Stay in this state.
    return ST_BALANCE_P_GAIN_SEL;
}


static uint8_t ui_balance_d_gain(uint8_t input)
// Handle setting the d gain value.
{
    int16_t gain;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_BALANCE_D_GAIN;

    // Get the gain.
    balance_gains_get(NULL, &gain, NULL, NULL);

    // Handle the input.
    if (input == BUTTON_UP) { gain += 12; balance_gains_set(NULL, &gain, NULL, NULL); }
    if (input == BUTTON_DOWN) { gain -= 12; balance_gains_set(NULL, &gain, NULL, NULL); }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_BALANCE_D_GAIN);
    lcd_printf_P(PSTR("\r\n%i"), gain);

    // Stay in this state.
    return ST_BALANCE_D_GAIN_SEL;
}


static uint8_t ui_balance_i_gain(uint8_t input)
// Handle setting the d gain value.
{
    int16_t gain;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_BALANCE_I_GAIN;

    // Get the gain.
    balance_gains_get(NULL, NULL, &gain, NULL);

    // Handle the input.
    if (input == BUTTON_UP) { gain += 12; balance_gains_set(NULL, NULL, &gain, NULL); }
    if (input == BUTTON_DOWN) { gain -= 12; balance_gains_set(NULL, NULL, &gain, NULL); }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_BALANCE_I_GAIN);
    lcd_printf_P(PSTR("\r\n%i"), gain);

    // Stay in this state.
    return ST_BALANCE_I_GAIN_SEL;
}


static uint8_t ui_balance_t_comp(uint8_t input)
// Handle setting the control tilt compensation value.
{
    int16_t t_comp;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_BALANCE_T_COMP;

    // Get the tilt compensation.
    balance_gains_get(NULL, NULL, NULL, &t_comp);

    // Handle the input.
    if (input == BUTTON_UP) { t_comp += 26; balance_gains_set(NULL, NULL, NULL, &t_comp); }
    if (input == BUTTON_DOWN) { t_comp -= 26; balance_gains_set(NULL, NULL, NULL, &t_comp); }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_BALANCE_T_COMP);
    lcd_printf_P(PSTR("\r\n%i"), t_comp);

    // Stay in this state.
    return ST_BALANCE_T_COMP_SEL;
}


static uint8_t ui_speed_p_gain(uint8_t input)
// Handle setting the p gain value.
{
    int16_t gain;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_SPEED_P_GAIN;

    // Get the gain.
    speed_gains_get(&gain, NULL, NULL);

    // Handle the input.
    if (input == BUTTON_UP) { gain += 26; speed_gains_set(&gain, NULL, NULL); }
    if (input == BUTTON_DOWN) { gain -= 26; speed_gains_set(&gain, NULL, NULL); }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_SPEED_P_GAIN);
    lcd_printf_P(PSTR("\r\n%i"), gain);

    // Stay in this state.
    return ST_SPEED_P_GAIN_SEL;
}


static uint8_t ui_speed_d_gain(uint8_t input)
// Handle setting the d gain value.
{
    int16_t gain;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_SPEED_D_GAIN;

    // Get the gain.
    speed_gains_get(NULL, &gain, NULL);

    // Handle the input.
    if (input == BUTTON_UP) { gain += 26; speed_gains_set(NULL, &gain, NULL); }
    if (input == BUTTON_DOWN) { gain -= 26; speed_gains_set(NULL, &gain, NULL); }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_SPEED_D_GAIN);
    lcd_printf_P(PSTR("\r\n%i"), gain);

    // Stay in this state.
    return ST_SPEED_D_GAIN_SEL;
}


static uint8_t ui_speed_i_gain(uint8_t input)
// Handle setting the d gain value.
{
    int16_t gain;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_SPEED_I_GAIN;

    // Get the gain.
    speed_gains_get(NULL, NULL, &gain);

    // Handle the input.
    if (input == BUTTON_UP) { gain += 26; speed_gains_set(NULL, NULL, &gain); }
    if (input == BUTTON_DOWN) { gain -= 26; speed_gains_set(NULL, NULL, &gain); }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_SPEED_I_GAIN);
    lcd_printf_P(PSTR("\r\n%i"), gain);

    // Stay in this state.
    return ST_SPEED_I_GAIN_SEL;
}


static uint8_t ui_control_rc(uint8_t input)
// Display RC values.
{
    int8_t channel_1;
    int8_t channel_2;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_CONTROL_RC;

    // Get the IMU raw accelerometer and gyro values.
    uio_get_rc(&channel_1, &channel_2);

    // Update the LCD with the RC state.
    lcd_puts_P(MT_CONTROL_RC);
    lcd_printf_P(PSTR("\r\n%d %d"), (int16_t) channel_1, (int16_t) channel_2);

    // Stay in this state.
    return ST_CONTROL_RC_SEL;
}


static uint8_t ui_imu_pitch(uint8_t input)
// Display IMU pitch values.
{
    int16_t pitch_angle;
    int16_t pitch_rate;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_IMU_PITCH;

    // Get the IMU pitch values.
    imu_pitch_get(&pitch_angle, &pitch_rate);

    // Update the LCD with the pitch state.
    lcd_puts_P(MT_IMU_PITCH);
    lcd_printf_P(PSTR("\r\n%i %i"), pitch_angle, pitch_rate);

    // Stay in this state.
    return ST_IMU_PITCH_SEL;
}


static uint8_t ui_imu_raw(uint8_t input)
// Display IMU raw values.
{
    uint16_t imu_gyro_x;
    uint16_t imu_accel_y;
    uint16_t imu_accel_z;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_IMU_RAW;

    // Get the IMU raw accelerometer and gyro values.
    imu_raw_get(&imu_gyro_x, &imu_accel_y, &imu_accel_z);

    // Update the LCD with the IMU state.
    lcd_puts_P(MT_IMU_RAW);
    lcd_printf_P(PSTR("\r\n%04x %04x %04x"), imu_gyro_x, imu_accel_y, imu_accel_z);

    // Stay in this state.
    return ST_IMU_RAW_SEL;
}


static uint8_t ui_boot_enable(uint8_t input)
// Manually enter the bootloader.
{
    // Cancel this state with left button.
    if (input == BUTTON_LEFT) return ST_BOOT_MENU;

    // Start the bootloader if center button is pressed.
    if (input == BUTTON_CENTER)
    {
        // Give feedback.
        lcd_puts_P(PSTR("\x0c" "Starting\r\nbootloader..."));

        // Start the bootloader.
        bootloader_start();
    }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_BOOT_MENU);
    lcd_puts_P(PSTR("\r\nStart"));

    // Stay in this state.
    return ST_BOOT_ENABLE;
}


NAKEDFUNC(ui_task)
// Task for robot user interface.
{
    static uint8_t i;
    static uint8_t button;
    static uint8_t state = ST_TOP;
    static uint8_t state_next = ST_TOP;
    static const char *state_text = MT_TOP;
    static uint8_t (*state_func)(uint8_t) = NULL;

    // Main control loop.
    for (;;)
    {
        // Output the state text if we have any.
        if (state_text) lcd_puts_P(state_text);

        // Wait for a button to be pressed.
        button = uio_next_button(state_func ? 250 : 0);

        // Do we have a state function?
        if (state_func)
        {
            // Call the state function to update.
            state_next = state_func(button);
        }
        else if (button != BUTTON_NONE)
        {
            // Standard menu state operation.  Use the current 
            // state and the button to determine the next state.
            for (i = 0; pgm_read_byte(&ui_menu_state_next[i].state); ++i)
            {
                // Match the state and button.
                if ((pgm_read_byte(&ui_menu_state_next[i].state) == state) &&
                    (pgm_read_byte(&ui_menu_state_next[i].input) == button))
                {
                    // We made a match.
                    state_next = pgm_read_byte(&ui_menu_state_next[i].state_next);

                    break;
                }
            }
        }

        // Did we find a state?
        if (state_next != state)
        {
            // Move to the next state.
            state = state_next;

            // Loop over each state to find any state text and any state function.
            for (i = 0; pgm_read_byte(&ui_menu_state[i].state); ++i)
            {
                // Did we find the state?
                if (pgm_read_byte(&ui_menu_state[i].state) == state)
                {
                    // Get the state text.
                    state_text = (const char PROGMEM *) pgm_read_word(&ui_menu_state[i].text);

                    // Get the state function.
                    state_func = (uint8_t (*)(uint8_t)) pgm_read_word(&ui_menu_state[i].func);

                    break;
                }
            }

            // If we have a state function give it a chance to update the display.
            if (state_func) state_func(BUTTON_NONE);
        }
    }
}


