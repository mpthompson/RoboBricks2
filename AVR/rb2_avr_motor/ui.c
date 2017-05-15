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
#include "bootloader.h"
#include "motor.h"
#include "encoder.h"
#include "lcd.h"
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
static uint8_t ui_motor_left_command(uint8_t input);
static uint8_t ui_motor_left_encoder(uint8_t input);
static uint8_t ui_motor_left_pwm(uint8_t input);
static uint8_t ui_motor_left_p_gain(uint8_t input);
static uint8_t ui_motor_left_d_gain(uint8_t input);
static uint8_t ui_motor_left_i_gain(uint8_t input);
static uint8_t ui_motor_right_command(uint8_t input);
static uint8_t ui_motor_right_encoder(uint8_t input);
static uint8_t ui_motor_right_pwm(uint8_t input);
static uint8_t ui_motor_right_p_gain(uint8_t input);
static uint8_t ui_motor_right_d_gain(uint8_t input);
static uint8_t ui_motor_right_i_gain(uint8_t input);
static uint8_t ui_boot_enable(uint8_t input);

const char MT_TOP[] PROGMEM                         = "\x0c" "Motor Tuning";

const char MT_MOTOR_MENU[] PROGMEM                  = "\x0c" "Motor";
const char MT_MOTOR_ENABLE[] PROGMEM                = "\x0c" "Enable";

const char MT_MOTOR_L_COMMAND[] PROGMEM             = "\x0c" "Left Command";
const char MT_MOTOR_L_ENCODER[] PROGMEM             = "\x0c" "Left Encoder";
const char MT_MOTOR_L_PWM[] PROGMEM                 = "\x0c" "Left PWM";
const char MT_MOTOR_L_P_GAIN[] PROGMEM              = "\x0c" "Left P Gain";
const char MT_MOTOR_L_D_GAIN[] PROGMEM              = "\x0c" "Left D Gain";
const char MT_MOTOR_L_I_GAIN[] PROGMEM              = "\x0c" "Left I Gain";

const char MT_MOTOR_R_COMMAND[] PROGMEM             = "\x0c" "Right Command";
const char MT_MOTOR_R_ENCODER[] PROGMEM             = "\x0c" "Right Encoder";
const char MT_MOTOR_R_PWM[] PROGMEM                 = "\x0c" "Right PWM";
const char MT_MOTOR_R_P_GAIN[] PROGMEM              = "\x0c" "Right P Gain";
const char MT_MOTOR_R_D_GAIN[] PROGMEM              = "\x0c" "Right D Gain";
const char MT_MOTOR_R_I_GAIN[] PROGMEM              = "\x0c" "Right I Gain";

const char MT_BOOT_MENU[] PROGMEM                   = "\x0c" "Bootloader";

const menu_state_next ui_menu_state_next[] PROGMEM =
{
//  STATE                       INPUT           NEXT STATE
    { ST_TOP,                   BUTTON_RIGHT,   ST_MOTOR_MENU },

    { ST_MOTOR_MENU,            BUTTON_UP,      ST_BOOT_MENU },
    { ST_MOTOR_MENU,            BUTTON_DOWN,    ST_BOOT_MENU },
    { ST_MOTOR_MENU,            BUTTON_LEFT,    ST_TOP },
    { ST_MOTOR_MENU,            BUTTON_RIGHT,   ST_MOTOR_ENABLE },

    { ST_BOOT_MENU,             BUTTON_UP,      ST_MOTOR_MENU },
    { ST_BOOT_MENU,             BUTTON_DOWN,    ST_MOTOR_MENU },
    { ST_BOOT_MENU,             BUTTON_LEFT,    ST_TOP },
    { ST_BOOT_MENU,             BUTTON_RIGHT,   ST_BOOT_ENABLE },

    { ST_MOTOR_ENABLE,          BUTTON_UP,      ST_MOTOR_R_I_GAIN },
    { ST_MOTOR_ENABLE,          BUTTON_DOWN,    ST_MOTOR_L_COMMAND },
    { ST_MOTOR_ENABLE,          BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_ENABLE,          BUTTON_RIGHT,   ST_MOTOR_ENABLE_SEL },

    { ST_MOTOR_L_COMMAND,       BUTTON_UP,      ST_MOTOR_ENABLE },
    { ST_MOTOR_L_COMMAND,       BUTTON_DOWN,    ST_MOTOR_L_ENCODER },
    { ST_MOTOR_L_COMMAND,       BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_L_COMMAND,       BUTTON_RIGHT,   ST_MOTOR_L_COMMAND_SEL },

    { ST_MOTOR_L_ENCODER,       BUTTON_UP,      ST_MOTOR_L_COMMAND },
    { ST_MOTOR_L_ENCODER,       BUTTON_DOWN,    ST_MOTOR_L_PWM },
    { ST_MOTOR_L_ENCODER,       BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_L_ENCODER,       BUTTON_RIGHT,   ST_MOTOR_L_ENCODER_SEL },

    { ST_MOTOR_L_PWM,           BUTTON_UP,      ST_MOTOR_L_ENCODER },
    { ST_MOTOR_L_PWM,           BUTTON_DOWN,    ST_MOTOR_L_P_GAIN },
    { ST_MOTOR_L_PWM,           BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_L_PWM,           BUTTON_RIGHT,   ST_MOTOR_L_PWM_SEL },

    { ST_MOTOR_L_P_GAIN,        BUTTON_UP,      ST_MOTOR_L_PWM },
    { ST_MOTOR_L_P_GAIN,        BUTTON_DOWN,    ST_MOTOR_L_D_GAIN },
    { ST_MOTOR_L_P_GAIN,        BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_L_P_GAIN,        BUTTON_RIGHT,   ST_MOTOR_L_P_GAIN_SEL },

    { ST_MOTOR_L_D_GAIN,        BUTTON_UP,      ST_MOTOR_L_P_GAIN },
    { ST_MOTOR_L_D_GAIN,        BUTTON_DOWN,    ST_MOTOR_L_I_GAIN },
    { ST_MOTOR_L_D_GAIN,        BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_L_D_GAIN,        BUTTON_RIGHT,   ST_MOTOR_L_D_GAIN_SEL },

    { ST_MOTOR_L_I_GAIN,        BUTTON_UP,      ST_MOTOR_L_D_GAIN },
    { ST_MOTOR_L_I_GAIN,        BUTTON_DOWN,    ST_MOTOR_R_COMMAND },
    { ST_MOTOR_L_I_GAIN,        BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_L_I_GAIN,        BUTTON_RIGHT,   ST_MOTOR_L_I_GAIN_SEL },

    { ST_MOTOR_R_COMMAND,       BUTTON_UP,      ST_MOTOR_L_I_GAIN },
    { ST_MOTOR_R_COMMAND,       BUTTON_DOWN,    ST_MOTOR_R_ENCODER },
    { ST_MOTOR_R_COMMAND,       BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_R_COMMAND,       BUTTON_RIGHT,   ST_MOTOR_R_COMMAND_SEL },

    { ST_MOTOR_R_ENCODER,       BUTTON_UP,      ST_MOTOR_R_COMMAND },
    { ST_MOTOR_R_ENCODER,       BUTTON_DOWN,    ST_MOTOR_R_PWM },
    { ST_MOTOR_R_ENCODER,       BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_R_ENCODER,       BUTTON_RIGHT,   ST_MOTOR_R_ENCODER_SEL },

    { ST_MOTOR_R_PWM,           BUTTON_UP,      ST_MOTOR_R_ENCODER },
    { ST_MOTOR_R_PWM,           BUTTON_DOWN,    ST_MOTOR_R_P_GAIN },
    { ST_MOTOR_R_PWM,           BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_R_PWM,           BUTTON_RIGHT,   ST_MOTOR_R_PWM_SEL },

    { ST_MOTOR_R_P_GAIN,        BUTTON_UP,      ST_MOTOR_R_PWM },
    { ST_MOTOR_R_P_GAIN,        BUTTON_DOWN,    ST_MOTOR_R_D_GAIN },
    { ST_MOTOR_R_P_GAIN,        BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_R_P_GAIN,        BUTTON_RIGHT,   ST_MOTOR_R_P_GAIN_SEL },

    { ST_MOTOR_R_D_GAIN,        BUTTON_UP,      ST_MOTOR_R_P_GAIN },
    { ST_MOTOR_R_D_GAIN,        BUTTON_DOWN,    ST_MOTOR_R_I_GAIN },
    { ST_MOTOR_R_D_GAIN,        BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_R_D_GAIN,        BUTTON_RIGHT,   ST_MOTOR_R_D_GAIN_SEL },

    { ST_MOTOR_R_I_GAIN,        BUTTON_UP,      ST_MOTOR_R_D_GAIN },
    { ST_MOTOR_R_I_GAIN,        BUTTON_DOWN,    ST_MOTOR_ENABLE },
    { ST_MOTOR_R_I_GAIN,        BUTTON_LEFT,    ST_MOTOR_MENU },
    { ST_MOTOR_R_I_GAIN,        BUTTON_RIGHT,   ST_MOTOR_R_I_GAIN_SEL },

    {0,                         0,              0}
};


const menu_state ui_menu_state[] PROGMEM =
{
//    STATE                     STATE TEXT                  STATE_FUNC
    { ST_TOP,                   MT_TOP,                     NULL },

    { ST_MOTOR_MENU,            MT_MOTOR_MENU,              NULL },
    { ST_MOTOR_ENABLE,          MT_MOTOR_ENABLE,            NULL },
    { ST_MOTOR_L_COMMAND,       MT_MOTOR_L_COMMAND,         NULL },
    { ST_MOTOR_L_ENCODER,       MT_MOTOR_L_ENCODER,         NULL },
    { ST_MOTOR_L_PWM,           MT_MOTOR_L_PWM,             NULL },
    { ST_MOTOR_L_P_GAIN,        MT_MOTOR_L_P_GAIN,          NULL },
    { ST_MOTOR_L_D_GAIN,        MT_MOTOR_L_D_GAIN,          NULL },
    { ST_MOTOR_L_I_GAIN,        MT_MOTOR_L_I_GAIN,          NULL },
    { ST_MOTOR_R_COMMAND,       MT_MOTOR_R_COMMAND,         NULL },
    { ST_MOTOR_R_ENCODER,       MT_MOTOR_R_ENCODER,         NULL },
    { ST_MOTOR_R_PWM,           MT_MOTOR_R_PWM,             NULL },
    { ST_MOTOR_R_P_GAIN,        MT_MOTOR_R_P_GAIN,          NULL },
    { ST_MOTOR_R_D_GAIN,        MT_MOTOR_R_D_GAIN,          NULL },
    { ST_MOTOR_R_I_GAIN,        MT_MOTOR_R_I_GAIN,          NULL },

    { ST_MOTOR_ENABLE_SEL,      NULL,                       ui_motor_enable },
    { ST_MOTOR_L_COMMAND_SEL,   NULL,                       ui_motor_left_command },
    { ST_MOTOR_L_ENCODER_SEL,   NULL,                       ui_motor_left_encoder },
    { ST_MOTOR_L_PWM_SEL,       NULL,                       ui_motor_left_pwm },
    { ST_MOTOR_L_P_GAIN_SEL,    NULL,                       ui_motor_left_p_gain },
    { ST_MOTOR_L_D_GAIN_SEL,    NULL,                       ui_motor_left_d_gain },
    { ST_MOTOR_L_I_GAIN_SEL,    NULL,                       ui_motor_left_i_gain },
    { ST_MOTOR_R_COMMAND_SEL,   NULL,                       ui_motor_right_command },
    { ST_MOTOR_R_ENCODER_SEL,   NULL,                       ui_motor_right_encoder },
    { ST_MOTOR_R_PWM_SEL,       NULL,                       ui_motor_right_pwm },
    { ST_MOTOR_R_P_GAIN_SEL,    NULL,                       ui_motor_right_p_gain },
    { ST_MOTOR_R_D_GAIN_SEL,    NULL,                       ui_motor_right_d_gain },
    { ST_MOTOR_R_I_GAIN_SEL,    NULL,                       ui_motor_right_i_gain },

    { ST_BOOT_MENU,             MT_BOOT_MENU,               NULL },
    { ST_BOOT_ENABLE,           NULL,                       ui_boot_enable },

    {0,                         NULL,                       NULL }
};

static uint8_t ui_motor_enable(uint8_t input)
// Handle enabling or disabling motor commands.
{
    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_ENABLE;

    // Handle the input.
    if ((input == BUTTON_UP) || (input == BUTTON_DOWN))
    {
        // Toggle the value.
        motor_enable_set(motor_enable_get() ? 0 : 1);
    }

    // Update the LCD with the state.
    lcd_puts_P(MT_MOTOR_ENABLE);
    lcd_puts_P(motor_enable_get() ? PSTR("\r\nEnabled") : PSTR("\r\nDisabled"));

    // Stay in this state.
    return ST_MOTOR_ENABLE_SEL;
}


static uint8_t ui_motor_left_command(uint8_t input)
{
    int16_t cmd;
    int16_t vel;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_L_COMMAND;

    // Get the current command.
    motor_command_get(&cmd, NULL);
    vel = encoder_get_left_delta();

    // Handle incrementing the command.
    if (input == BUTTON_UP) { cmd += 1; motor_command_set(&cmd, NULL); }
    if (input == BUTTON_DOWN) { cmd -= 1; motor_command_set(&cmd, NULL); }
    if (input == BUTTON_LEFT) { cmd -= 20; motor_command_set(&cmd, NULL); }
    if (input == BUTTON_RIGHT) { cmd += 20; motor_command_set(&cmd, NULL); }

    // Update the LCD with the state.
    lcd_puts_P(MT_MOTOR_L_COMMAND);
    lcd_printf_P(PSTR("\r\n%d %d"), cmd, vel);

    // Stay in this state.
    return ST_MOTOR_L_COMMAND_SEL;
}


static uint8_t ui_motor_left_encoder(uint8_t input)
{
    int16_t vel;
    int32_t pos;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_L_ENCODER;

    // Get the encoder position and velocity.
    vel = encoder_get_left_delta();
    encoder_get_positions(&pos, NULL);

    // Update the LCD with the state.
    lcd_puts_P(MT_MOTOR_L_ENCODER);
    lcd_printf_P(PSTR("\r\n%04x %04x%04x"), vel, (uint16_t) (pos >> 16), (uint16_t) pos);

    // Stay in this state.
    return ST_MOTOR_L_ENCODER_SEL;
}


static uint8_t ui_motor_left_pwm(uint8_t input)
{
    int8_t pwm;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_L_PWM;

    // Get the motor PWM value.
    motor_pwm_get(&pwm, NULL);

    // Update the LCD with the state.
    lcd_puts_P(MT_MOTOR_L_PWM);
    lcd_printf_P(PSTR("\r\n%d"), pwm);

    // Stay in this state.
    return ST_MOTOR_L_PWM_SEL;
}


static uint8_t ui_motor_left_p_gain(uint8_t input)
{
    int16_t gain;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_L_P_GAIN;

    // Get the gain.
    motor_left_gains_get(&gain, NULL, NULL);

    // Handle the input.
    if (input == BUTTON_UP) { gain += 26; motor_left_gains_set(&gain, NULL, NULL); }
    if (input == BUTTON_DOWN) { gain -= 26; motor_left_gains_set(&gain, NULL, NULL); }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_MOTOR_L_P_GAIN);
    lcd_printf_P(PSTR("\r\n%i"), gain);

    // Stay in this state.
    return ST_MOTOR_L_P_GAIN_SEL;
}


static uint8_t ui_motor_left_d_gain(uint8_t input)
{
    int16_t gain;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_L_D_GAIN;

    // Get the gain.
    motor_left_gains_get(NULL, &gain, NULL);

    // Handle the input.
    if (input == BUTTON_UP) { gain += 26; motor_left_gains_set(NULL, &gain, NULL); }
    if (input == BUTTON_DOWN) { gain -= 26; motor_left_gains_set(NULL, &gain, NULL); }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_MOTOR_L_D_GAIN);
    lcd_printf_P(PSTR("\r\n%i"), gain);

    // Stay in this state.
    return ST_MOTOR_L_D_GAIN_SEL;
}


static uint8_t ui_motor_left_i_gain(uint8_t input)
{
    int16_t gain;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_L_I_GAIN;

    // Get the gain.
    motor_left_gains_get(NULL, NULL, &gain);

    // Handle the input.
    if (input == BUTTON_UP) { gain += 13; motor_left_gains_set(NULL, NULL, &gain); }
    if (input == BUTTON_DOWN) { gain -= 13; motor_left_gains_set(NULL, NULL, &gain); }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_MOTOR_L_I_GAIN);
    lcd_printf_P(PSTR("\r\n%i"), gain);

    // Stay in this state.
    return ST_MOTOR_L_I_GAIN_SEL;
}


static uint8_t ui_motor_right_command(uint8_t input)
{
    int16_t cmd;
    int16_t vel;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_R_COMMAND;

    // Get the current command.
    motor_command_get(NULL, &cmd);
    vel = encoder_get_right_delta();

    // Handle incrementing the command.
    if (input == BUTTON_UP) { cmd += 1; motor_command_set(NULL, &cmd); }
    if (input == BUTTON_DOWN) { cmd -= 1; motor_command_set(NULL, &cmd); }
    if (input == BUTTON_LEFT) { cmd -= 20; motor_command_set(NULL, &cmd); }
    if (input == BUTTON_RIGHT) { cmd += 20; motor_command_set(NULL, &cmd); }

    // Update the LCD with the state.
    lcd_puts_P(MT_MOTOR_R_COMMAND);
    lcd_printf_P(PSTR("\r\n%d %d"), cmd, vel);

    // Stay in this state.
    return ST_MOTOR_R_COMMAND_SEL;
}


static uint8_t ui_motor_right_encoder(uint8_t input)
{
    int16_t vel;
    int32_t pos;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_R_ENCODER;

    // Get the encoder position and velocity.
    vel = encoder_get_right_delta();
    encoder_get_positions(NULL, &pos);

    // Update the LCD with the state.
    lcd_puts_P(MT_MOTOR_R_ENCODER);
    lcd_printf_P(PSTR("\r\n%04x %04x%04x"), vel, (uint16_t) (pos >> 16), (uint16_t) pos);

    // Stay in this state.
    return ST_MOTOR_R_ENCODER_SEL;
}


static uint8_t ui_motor_right_pwm(uint8_t input)
{
    int8_t pwm;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_R_PWM;

    // Get the motor PWM value.
    motor_pwm_get(NULL, &pwm);

    // Update the LCD with the state.
    lcd_puts_P(MT_MOTOR_R_PWM);
    lcd_printf_P(PSTR("\r\n%d"), pwm);

    // Stay in this state.
    return ST_MOTOR_R_PWM_SEL;
}


static uint8_t ui_motor_right_p_gain(uint8_t input)
{
    int16_t gain;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_R_P_GAIN;

    // Get the gain.
    motor_right_gains_get(&gain, NULL, NULL);

    // Handle the input.
    if (input == BUTTON_UP) { gain += 13; motor_right_gains_set(&gain, NULL, NULL); }
    if (input == BUTTON_DOWN) { gain -= 13; motor_right_gains_set(&gain, NULL, NULL); }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_MOTOR_R_P_GAIN);
    lcd_printf_P(PSTR("\r\n%i"), gain);

    // Stay in this state.
    return ST_MOTOR_R_P_GAIN_SEL;
}


static uint8_t ui_motor_right_d_gain(uint8_t input)
{
    int16_t gain;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_R_D_GAIN;

    // Get the gain.
    motor_right_gains_get(NULL, &gain, NULL);

    // Handle the input.
    if (input == BUTTON_UP) { gain += 26; motor_right_gains_set(NULL, &gain, NULL); }
    if (input == BUTTON_DOWN) { gain -= 26; motor_right_gains_set(NULL, &gain, NULL); }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_MOTOR_R_D_GAIN);
    lcd_printf_P(PSTR("\r\n%i"), gain);

    // Stay in this state.
    return ST_MOTOR_R_D_GAIN_SEL;
}


static uint8_t ui_motor_right_i_gain(uint8_t input)
{
    int16_t gain;

    // Exit this state with center button.
    if (input == BUTTON_CENTER) return ST_MOTOR_R_I_GAIN;

    // Get the gain.
    motor_right_gains_get(NULL, NULL, &gain);

    // Handle the input.
    if (input == BUTTON_UP) { gain += 26; motor_right_gains_set(NULL, NULL, &gain); }
    if (input == BUTTON_DOWN) { gain -= 26; motor_right_gains_set(NULL, NULL, &gain); }

    // Update the LCD with the robot state.
    lcd_puts_P(MT_MOTOR_R_I_GAIN);
    lcd_printf_P(PSTR("\r\n%i"), gain);

    // Stay in this state.
    return ST_MOTOR_R_I_GAIN_SEL;
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

        // Wait 250 milliseconds for a button.
        button = uio_next_button(250);

        // Do we have a state function?
        if (state_func)
        {
            // Call the state function to update.
            state_next = state_func(button);
        }
        else if (button != BUTTON_NONE)
        {
            // Standard menu state operation.  Use the current 
            // state and the button to determin the next state.
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



