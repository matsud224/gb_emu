#pragma once

#include "SDL2/SDL.h"

#define LCDMODE_HBLANK 0
#define LCDMODE_VBLANK 1
#define LCDMODE_SEARCHOAM 2
#define LCDMODE_TRANSFERRING 3

void lcd_init(SDL_Surface *surface);
uint8_t lcd_get_mode(void);
void lcd_change_mode(int mode);
void lcd_clear(Uint32 buf[]);
void lcd_draw_background_oneline(Uint32 buf[]);
void lcd_draw_window_oneline(Uint32 buf[]);
void lcd_draw_sprite_oneline(Uint32 buf[]);
