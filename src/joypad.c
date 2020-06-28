#include "joypad.h"
#include "memory.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_gamecontroller.h"

int JOYPAD_INPUTDEVICE;

static SDL_Joystick *joystick;

void joypad_init(SDL_Joystick *js) {
	joystick = js;
	if(joystick==NULL)
		JOYPAD_INPUTDEVICE = INPUTDEVICE_KEYBOARD;
	else
		JOYPAD_INPUTDEVICE = INPUTDEVICE_JOYSTICK;
}

uint8_t joypad_status() {
	uint8_t p1=INTERNAL_IO[IO_P1_R];
	int p10=1, p11=1, p12=1, p13=1, p14 = p1&0x10, p15 = p1&0x20;
	SDL_PumpEvents();
	if(JOYPAD_INPUTDEVICE == INPUTDEVICE_KEYBOARD){
		const Uint8 *state=SDL_GetKeyboardState(NULL);
		if(!p14){
			if(state[SDL_GetScancodeFromKey(RIGHT_KEY)]) p10=0;
			if(state[SDL_GetScancodeFromKey(LEFT_KEY)]) p11=0;
			if(state[SDL_GetScancodeFromKey(UP_KEY)]) p12=0;
			if(state[SDL_GetScancodeFromKey(DOWN_KEY)]) p13=0;
		}
		if(!p15){
			if(state[SDL_GetScancodeFromKey(A_KEY)]) p10=0;
			if(state[SDL_GetScancodeFromKey(B_KEY)]) p11=0;
			if(state[SDL_GetScancodeFromKey(SELECT_KEY)]) p12=0;
			if(state[SDL_GetScancodeFromKey(START_KEY)]) p13=0;
		}
	}else if(JOYPAD_INPUTDEVICE == INPUTDEVICE_JOYSTICK){
		if(!p14){
			if(SDL_JoystickGetAxis(joystick, 0) > JOYSTICK_DEAD_ZONE) p10=0;
			if(SDL_JoystickGetAxis(joystick, 0) < -JOYSTICK_DEAD_ZONE) p11=0;
			if(SDL_JoystickGetAxis(joystick, 1) < -JOYSTICK_DEAD_ZONE) p12=0;
			if(SDL_JoystickGetAxis(joystick, 1) > JOYSTICK_DEAD_ZONE) p13=0;
		}
		if(!p15){
			if(SDL_JoystickGetButton(joystick, JOYSTICK_BUTTON_A)) p10=0;
			if(SDL_JoystickGetButton(joystick, JOYSTICK_BUTTON_B)) p11=0;
			if(SDL_JoystickGetButton(joystick, JOYSTICK_BUTTON_SELECT)) p12=0;
			if(SDL_JoystickGetButton(joystick, JOYSTICK_BUTTON_START)) p13=0;
		}
	}

	return 0x3<<6 | p15<<5 | p14<<4 | p13<<3 | p12<<2 | p11<<1 | p10;
}

void joypad_close() {
	if(joystick!=NULL)
		SDL_JoystickClose(joystick);
}
