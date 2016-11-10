#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include "cpu.h"
#include "cartridge.h"
#include "memory.h"
#include "lcd.h"
#include "joypad.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"
#include "SDL2/SDL_timer.h"
#include "SDL2/SDL_keyboard.h"
#include "SDL2/SDL_gamecontroller.h"

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define SCREEN_WIDTH (160<<2)
#define SCREEN_HEIGHT (144<<2)

#define TIMER_START(t) ((t)=SDL_GetTicks())
#define TIMER_GET(t) (SDL_GetTicks()-(t))

#define MIN(x,y) ((x)<(y)?(x):(y))

static SDL_Window *main_window;
static SDL_Renderer *window_renderer;

static uint8_t *open_rom(char* filename) {
	int fd;
	struct stat sbuf;
	fd = open(filename, O_RDONLY);
	if(fd == -1)
		return NULL;
	if(fstat(fd, &sbuf) == -1){
		close(fd);
		return NULL;
	}

	uint8_t *data = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if(data == MAP_FAILED){
		close(fd);
		return NULL;
	}

	return data;
}

static int sdl_init() {
	if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER ) < 0 ){
		printf( "SDL Init failed : %s\n", SDL_GetError() );
		return -1;
	}else{
		//Create window
		main_window = SDL_CreateWindow( "gb_emu", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN );
		if( main_window == NULL ){
			printf( "SDL_CreateWindow failed : %s\n", SDL_GetError() );
			return -1;
		}

		window_renderer = SDL_CreateRenderer( main_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC );
		if(window_renderer == NULL){
			printf( "SDL_CreateRenderer failed : %s\n", SDL_GetError() );
			return -1;
		}
	}

	SDL_Joystick *joystick = NULL;
	if(SDL_NumJoysticks() > 0){
		joystick = SDL_JoystickOpen(0);
	}

	if(joystick == NULL){
		printf("Keyboard mode\n");
		joypad_init(NULL);
	}else{
		printf("Gamepad mode\n");
		joypad_init(joystick);
	}

	return 0;
}


#define INC_LY ((++INTERNAL_IO[IO_LY_R]==INTERNAL_IO[IO_LYC_R] && INTERNAL_IO[IO_STAT_R]&0x40)?cpu_request_interrupt(INT_LCDSTAT):0)
#define RST_LY (((INTERNAL_IO[IO_LY_R]=0)==INTERNAL_IO[IO_LYC_R] && INTERNAL_IO[IO_STAT_R]&0x40)?cpu_request_interrupt(INT_LCDSTAT):0)

int main(int argc, char *argv[]) {
	if(argc != 2){
		printf("too few arguments\n");
		return -1;
	}

	uint8_t *rom = open_rom(argv[1]);
	if(rom==NULL){
		printf("open_rom failed\n");
		return -1;
	}

	struct cartridge *cart = cart_init(rom);
	if(cart == NULL){
		printf("cart_init failed\n");
		return -1;
	}
	struct gb_carthdr *hdr = cart_header(cart);
    printf("title: %.16s\ncgbflag: 0x%X\ncarttype: 0x%X\nromsize: 0x%X\nramsize: 0x%X\n",
		 hdr->title, hdr->cgbflag, hdr->carttype, hdr->romsize, hdr->ramsize);

	if(memory_init(cart)){
		free(cart);
		printf("memory_init failed\n");
		return -1;
	}

	startup();

	static Uint32 bitmap[160*144];
	SDL_Surface *bitmap_surface=SDL_CreateRGBSurfaceFrom((void *)bitmap, 160, 144, 32, 160*4,
	       0x00ff0000,0x0000ff00,0x000000ff,0xff000000);
	lcd_init(bitmap_surface);

	SDL_Event e;
	Uint32 fps_timer;
	int frame_count=0;

	if(sdl_init() < 0){
		printf("sdl_init failed: %s\n", SDL_GetError());
		return -1;
	}

	TIMER_START(fps_timer);

	int quit = 0;
	while(!quit){
		while(SDL_PollEvent(&e)!=0){
			switch(e.type){
			case SDL_QUIT:
				quit=1;
				break;
			case SDL_KEYDOWN:
				switch(e.key.keysym.sym){
				case SDLK_RIGHT:
				case SDLK_LEFT:
				case SDLK_UP:
				case SDLK_DOWN:
					if((INTERNAL_IO[IO_P1_R]&0x10)==0)
						cpu_request_interrupt(INT_JOYPAD);
					break;
				case SDLK_z:
				case SDLK_x:
				case SDLK_LEFTBRACKET:
				case SDLK_RIGHTBRACKET:
					if((INTERNAL_IO[IO_P1_R]&0x20)==0)
						cpu_request_interrupt(INT_JOYPAD);
					break;
				case SDLK_s:
					SDL_SaveBMP(bitmap_surface, "screenshot.bmp");
					break;
				default:
					break;
				}
				break;
			case SDL_JOYAXISMOTION:
				if(abs(e.jaxis.value) > JOYSTICK_DEAD_ZONE)
					cpu_request_interrupt(INT_JOYPAD);
				break;
			case SDL_JOYBUTTONDOWN:
				switch(e.jbutton.button){
				case JOYSTICK_BUTTON_A:
				case JOYSTICK_BUTTON_B:
				case JOYSTICK_BUTTON_SELECT:
				case JOYSTICK_BUTTON_START:
					cpu_request_interrupt(INT_JOYPAD);
				}
				break;
			}
		}

		if(quit) break;

		static char wndtitle[32];
		int avgfps=frame_count/(TIMER_GET(fps_timer)/1000+1);
		if(frame_count%60==0){
			snprintf(wndtitle, 32, "%.16s  FPS = %d", hdr->title, avgfps);
			SDL_SetWindowTitle(main_window, wndtitle);
		}

		SDL_SetRenderDrawColor(window_renderer, 0xff, 0xff, 0xff, 0xff);
		SDL_RenderClear(window_renderer);

		if(INTERNAL_IO[IO_LCDC_R]&0x80){
			//LCDがON
			RST_LY;

			while(INTERNAL_IO[IO_LY_R]<=143){
				lcd_change_mode(2); cpu_exec(83);
				lcd_change_mode(3); cpu_exec(175);

				if(INTERNAL_IO[IO_LCDC_R]&0x1)
					lcd_draw_background_oneline(bitmap);
				else
					lcd_clear(bitmap);

				if(INTERNAL_IO[IO_LCDC_R]&0x20)
					lcd_draw_window_oneline(bitmap);
				if(INTERNAL_IO[IO_LCDC_R]&0x2)
					lcd_draw_sprite_oneline(bitmap);

				INC_LY;
				lcd_change_mode(0); cpu_exec(207);
			}

			SDL_Texture *texture = SDL_CreateTextureFromSurface(window_renderer, bitmap_surface);
			SDL_RenderCopy(window_renderer, texture, NULL, NULL);
			SDL_DestroyTexture(texture);

			INC_LY;
			lcd_change_mode(LCDMODE_VBLANK);
		}else{
			cpu_exec(70224);
		}

		SDL_RenderPresent(window_renderer);
		frame_count++;

		if(INTERNAL_IO[IO_LCDC_R]&0x80){
			while(INTERNAL_IO[IO_LY_R]<152){
				cpu_exec(510);
				INC_LY;
			}
		}
	}

	joypad_close();

	SDL_DestroyRenderer(window_renderer);
	window_renderer = NULL;
	SDL_DestroyWindow(main_window);
	main_window = NULL;

	SDL_Quit();

	memory_free();

	return 0;
}
