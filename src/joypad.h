#pragma once

#include "SDL2/SDL_joystick.h"

//使用するジョイスティックに応じて以下を変更
//ジョイスティックの感度
#define JOYSTICK_DEAD_ZONE 8000
//ボタン番号の対応
#define JOYSTICK_BUTTON_A 0
#define JOYSTICK_BUTTON_B 1
#define JOYSTICK_BUTTON_SELECT 6
#define JOYSTICK_BUTTON_START 7


extern int JOYPAD_INPUTDEVICE;
#define INPUTDEVICE_KEYBOARD 0
#define INPUTDEVICE_JOYSTICK 1

void joypad_init(SDL_Joystick *js);
uint8_t joypad_status(void);
void joypad_close(void);
