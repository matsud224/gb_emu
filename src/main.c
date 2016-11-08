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

#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"
#include "SDL2/SDL_timer.h"
#include "SDL2/SDL_keyboard.h"
#include "SDL2/SDL_gamecontroller.h"

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define SCREEN_WIDTH (160<<2)
#define SCREEN_HEIGHT (144<<2)
#define SCREEN_FPS 60
#define SCREEN_TICK_PER_FRAME (1000 / SCREEN_FPS)

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

	joystick = NULL;
	if(SDL_NumJoysticks() > 0){
		joystick = SDL_JoystickOpen(0);
	}

	if(joystick == NULL){
		printf("Keyboard mode\n");
		INPUTTYPE = INPUT_KEYBOARD;
	}else{
		printf("Gamepad mode\n");
		INPUTTYPE = INPUT_JOYSTICK;
	}

	return 0;
}

#define INC_LY ((++INTERNAL_IO[IO_LY_R]==INTERNAL_IO[IO_LYC_R] && INTERNAL_IO[IO_STAT_R]&0x40)?request_interrupt(INT_LCDSTAT):0)
#define RST_LY (((INTERNAL_IO[IO_LY_R]=0)==INTERNAL_IO[IO_LYC_R] && INTERNAL_IO[IO_STAT_R]&0x40)?request_interrupt(INT_LCDSTAT):0)
static void change_lcdmode(int mode) {
	LCDMODE = mode;
	switch(mode){
	case LCDMODE_HBLANK:
		if(INTERNAL_IO[IO_STAT_R]&0x8)
			request_interrupt(INT_LCDSTAT);
		break;
	case LCDMODE_VBLANK:
		request_interrupt(INT_VBLANK);
		if(INTERNAL_IO[IO_STAT_R]&0x10)
			request_interrupt(INT_LCDSTAT);
		break;
	case LCDMODE_SEARCHOAM:
		if(INTERNAL_IO[IO_STAT_R]&0x20)
			request_interrupt(INT_LCDSTAT);
		break;
	}
}


//画面に表示する色
struct RGB{
	Uint8 r,g,b;
};

struct RGB ACTUALCOLOR[4] = {{222,249,208},
							 {139,192,112},
							 {68,100,59},
							 {36,54,31}};
Uint32 ABSCOLOR[4];

#define PALETTE(p,n) ((INTERNAL_IO[p]>>((n)<<1))&0x3)
#define BGPALETTE(n) ((INTERNAL_IO[IO_BGP_R]>>((n)<<1))&0x3)

static void draw_background(uint8_t lcdc, Uint32 buf[]) {
	uint8_t *tilemap = INTERNAL_VRAM+(((lcdc&0x8)?0x9c00:0x9800)-V_INTERNAL_VRAM);
	uint8_t *tiledata = INTERNAL_VRAM+(((lcdc&0x10)?0x8000:0x9000)-V_INTERNAL_VRAM);
	uint8_t scx=INTERNAL_IO[IO_SCX_R], scy=INTERNAL_IO[IO_SCY_R];
/*
	puts("---------------------");
	printf("LCDC=0x%X\n", lcdc);
	for(int y=0; y<32; y++){
		for(int x=0; x<32; x++){
			printf("%02X ", tilemap[y*32+x]);
		}
		printf("\n");
	}
	puts("---------------------");
*/
	for(int y=0; y<144; y++){
		int map_y=(y+scy)%256, tile_y=map_y>>3;
		int in_y=map_y%8;
		for(int x=0; x<160; x++){
			int map_x=(x+scx)%256, tile_x=map_x>>3;
            uint8_t *thisdata; //タイルパターンデータの開始アドレス
            if(lcdc&0x10)
				thisdata = tiledata + ((uint8_t)tilemap[tile_y*32+tile_x]*16);
			else
				thisdata = tiledata + ((int8_t)tilemap[tile_y*32+tile_x]*16);

			int in_x=map_x%8;
			uint8_t lower=thisdata[in_y*2], upper=thisdata[in_y*2+1];
			buf[y*160 + x] = ABSCOLOR[BGPALETTE(((upper>>(7-in_x))&0x1)<<1 | ((lower>>(7-in_x))&0x1))];
		}
	}
}


static void draw_window(uint8_t lcdc, Uint32 buf[]) {
	uint8_t *tilemap = INTERNAL_VRAM+(((lcdc&0x40)?0x9c00:0x9800)-V_INTERNAL_VRAM);
	uint8_t *tiledata = INTERNAL_VRAM+(((lcdc&0x10)?0x8000:0x9000)-V_INTERNAL_VRAM);
	int wx=INTERNAL_IO[IO_WX_R]-7, wy=INTERNAL_IO[IO_WY_R];

	for(int y=0; y<144; y++){
		int map_y=y-wy, in_y=map_y%8;
		if(y<wy) continue;
		for(int x=0; x<160; x++){
			if(x<wx) continue;
			int map_x=x-wx;
            int tile_y=map_y>>3, tile_x=map_x>>3;
            uint8_t *thisdata; //タイルパターンデータの開始アドレス
            if(lcdc&0x10)
				thisdata = tiledata + ((uint8_t)tilemap[tile_y*32+tile_x]*16);
			else
				thisdata = tiledata + ((int8_t)tilemap[tile_y*32+tile_x]*16);

			int in_x=map_x%8;
			uint8_t lower=thisdata[in_y*2], upper=thisdata[in_y*2+1];
			buf[y*160 + x] = ABSCOLOR[BGPALETTE(((upper>>(7-in_x))&0x1)<<1 | ((lower>>(7-in_x))&0x1))];
		}
	}
}

static void draw_sprite(uint8_t lcdc, Uint32 buf[]) {
	uint8_t *tiledata = INTERNAL_VRAM+(0x8000-V_INTERNAL_VRAM);
	uint8_t *oam_termial = INTERNAL_OAM+(4*40);

	for(uint8_t *attr=INTERNAL_OAM; attr<oam_termial; attr+=4){
        int sp_y=attr[0]-16, sp_x=attr[1]-8;
        uint8_t flags=attr[3];
        uint16_t palette_addr = (flags&0x10)?IO_OBP1_R:IO_OBP0_R;
		if(sp_y==-16 || sp_y>=144 || sp_x==-8 || sp_x>=160)
			continue;

		if(lcdc&0x4){
			//8x16 mode
			uint8_t *upperdata = tiledata + (attr[2]&0xfe)*16;
			uint8_t *lowerdata = tiledata + (attr[2]|0x01)*16;
			for(int y=0; y<16; y++){
				int scr_y=(flags&0x40)?(sp_y+(15-y)):(sp_y+y);
				if(scr_y<0 || scr_y>=144) continue;
				uint8_t lower=(y&0x8)?lowerdata[(y-8)*2]:upperdata[y*2],
						upper=(y&0x8)?lowerdata[(y-8)*2+1]:upperdata[y*2+1];
				for(int x=0; x<8; x++){
					int scr_x=(flags&0x20)?(sp_x+(7-x)):(sp_x+x);
					if(scr_x<0 || scr_x>=160) continue;
					Uint32 cnum = ((upper>>(7-x))&0x1)<<1 | ((lower>>(7-x))&0x1);
					if(cnum!=0 && (!(flags&0x80) || buf[scr_y*160 + scr_x]==ABSCOLOR[BGPALETTE(0)]))
						buf[scr_y*160 + scr_x] = ABSCOLOR[PALETTE(palette_addr, cnum)]; //0なら透過
				}
			}
		}else{
			//8x8 mode
			uint8_t *thisdata = tiledata + attr[2]*16;
			for(int y=0; y<8; y++){
				int scr_y=(flags&0x40)?(sp_y+(7-y)):(sp_y+y);
				if(scr_y<0 || scr_y>=144) continue;
				uint8_t lower=thisdata[y*2], upper=thisdata[y*2+1];
				for(int x=0; x<8; x++){
					int scr_x=(flags&0x20)?(sp_x+(7-x)):(sp_x+x);
					if(scr_x<0 || scr_x>=160) continue;
					Uint32 cnum = ((upper>>(7-x))&0x1)<<1 | ((lower>>(7-x))&0x1);
					if(cnum!=0 && (!(flags&0x80) || buf[scr_y*160 + scr_x]==ABSCOLOR[BGPALETTE(0)]))
						buf[scr_y*160 + scr_x] = ABSCOLOR[PALETTE(palette_addr, cnum)]; //0なら透過
				}
			}
		}
	}
}

int main(int argc, char *argv[]) {
	if(argc != 2){
		printf("too few argument\n");
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
/*
	puts("---MEMORY TEST---");
	uint32_t i=0x8000;
	uint8_t v=7;
	do{
		memory_write8(i, v);
		if(memory_read8(i)!=v)
			printf("ERROR: \t0x%X\n", i);
		i++; v++;
	}while(i!=0x10000);

	static uint8_t copied[0x8000];
	for(uint16_t i=0; i<0x8000; i++)
		copied[i] = memory_read8(i);
	if(memcmp(rom, copied, 0x8000) != 0)
		puts("ROM error!!!");
	puts("-----------------");
*/
	startup();

	static Uint32 bitmap[160*144];
	SDL_Surface *bitmap_surface=SDL_CreateRGBSurfaceFrom((void *)bitmap, 160, 144, 32, 160*4,
	       0x00ff0000,0x0000ff00,0x000000ff,0xff000000);

	for(int i=0; i<4; i++)
		ABSCOLOR[i] = SDL_MapRGBA(bitmap_surface->format, ACTUALCOLOR[i].r, ACTUALCOLOR[i].g, ACTUALCOLOR[i].b, 255);

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
						request_interrupt(INT_JOYPAD);
					break;
				case SDLK_z:
				case SDLK_x:
				case SDLK_LEFTBRACKET:
				case SDLK_RIGHTBRACKET:
					if((INTERNAL_IO[IO_P1_R]&0x20)==0)
						request_interrupt(INT_JOYPAD);
					break;
				case SDLK_s:
					SDL_SaveBMP(bitmap_surface, "screenshot.bmp");
					break;
				default:
					break;
				}
				break;
			case SDL_JOYAXISMOTION:
				if(INPUTTYPE!=INPUT_JOYSTICK) break;
				if(abs(e.jaxis.value) > JOYSTICK_DEAD_ZONE)
					request_interrupt(INT_JOYPAD);
				break;
			case SDL_JOYBUTTONDOWN:
				switch(e.jbutton.button){
				case JOYSTICK_BUTTON_A:
				case JOYSTICK_BUTTON_B:
				case JOYSTICK_BUTTON_SELECT:
				case JOYSTICK_BUTTON_START:
					request_interrupt(INT_JOYPAD);
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

		SDL_SetRenderDrawColor(window_renderer, ACTUALCOLOR[0].r, ACTUALCOLOR[0].g, ACTUALCOLOR[0].b, 0xff);
		SDL_RenderClear(window_renderer);

		uint8_t lcdc=INTERNAL_IO[IO_LCDC_R];
		if(lcdc&0x80){
			//LCDがON
			RST_LY;
			change_lcdmode(2); cpu_exec(83);
			change_lcdmode(3); cpu_exec(175);

			if(lcdc&0x1)
				draw_background(lcdc, bitmap);
			else
				for(int i=0; i<1024; i++)
					bitmap[i] = ABSCOLOR[0];

			if(lcdc&0x20)
				draw_window(lcdc, bitmap);
			if(lcdc&0x2)
				draw_sprite(lcdc, bitmap);

			SDL_Texture *texture = SDL_CreateTextureFromSurface(window_renderer, bitmap_surface);
			SDL_RenderCopy(window_renderer, texture, NULL, NULL);
			SDL_DestroyTexture(texture);

			INC_LY;
			change_lcdmode(0); cpu_exec(207);

			while(INTERNAL_IO[IO_LY_R]<143){
				change_lcdmode(2); cpu_exec(83);
				change_lcdmode(3); cpu_exec(175);
				INC_LY;
				change_lcdmode(0); cpu_exec(207);
			}

			INC_LY;
			change_lcdmode(LCDMODE_VBLANK);
		}else{
			cpu_exec(70224);
		}

		SDL_RenderPresent(window_renderer);
		frame_count++;

		if(lcdc&0x80){
			while(INTERNAL_IO[IO_LY_R]<152){
				cpu_exec(510);
				INC_LY;
			}
		}
	}

	if(joystick!=NULL)
		SDL_JoystickClose(joystick);

	SDL_DestroyRenderer(window_renderer);
	window_renderer = NULL;
	SDL_DestroyWindow(main_window);
	main_window = NULL;

	SDL_Quit();

	memory_free();

	return 0;
}
