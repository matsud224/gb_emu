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

#include "SDL2/SDL_mutex.h"
#include "SDL2/SDL_thread.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)


#define SCREEN_WIDTH (160<<2)
#define SCREEN_HEIGHT (144<<2)
#define SCREEN_FPS 60
#define SCREEN_TICK_PER_FRAME (1000 / SCREEN_FPS)



#define TIMER_START(t) ((t)=SDL_GetTicks())
#define TIMER_GET(t) (SDL_GetTicks()-(t))

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
	if( SDL_Init( SDL_INIT_VIDEO ) < 0 ){
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

#define PALETTE(p,n) ((INTERNAL_IO[p]>>(n))&0x3)
#define BGPALETTE(n) ((INTERNAL_IO[IO_BGP_R]>>(n))&0x3)
#define PIXEL_BW(x,y,c) (buf[(y)*160+(x)]=(c)?0x00000000:0x00ffffff)
static void draw_bg_and_win(uint8_t lcdc, Uint32 buf[]) {
	/* not implemented */
	int tiledata = ((lcdc&0x10)?0x8000:0x8800);
	int tilemap = ((lcdc&0x8)?0x9c00:0x9800);
	while(INTERNAL_IO[IO_LY_R]<143)
		INC_LY;
}

static void draw_sprite(uint8_t lcdc, Uint32 buf[]) {
	/* not implemented */
}

int main(int argc, char *argv[]) {
	if(argc != 2){
		fprintf(stderr, "too few argument\n");
		return -1;
	}

	uint8_t *rom = open_rom(argv[1]);
	if(rom==NULL){
		fprintf(stderr, "open_rom failed\n");
		return -1;
	}

	struct cartridge *cart = cart_init(rom);
	if(cart == NULL){
		fprintf(stderr, "cart_init failed\n");
		return -1;
	}
	struct gb_carthdr *hdr = cart_header(cart);
    printf("title: %.16s\ncgbflag: 0x%X\ncarttype: 0x%X\nromsize: 0x%X\nramsize: 0x%X\n",
		 hdr->title, hdr->cgbflag, hdr->carttype, hdr->romsize, hdr->ramsize);

	if(memory_init(cart)){
		free(cart);
		fprintf(stderr, "memory_init failed\n");
		return -1;
	}

	startup();

	SDL_Thread *cpu_thread = SDL_CreateThread(cpu_exec, "cpu_thread", (void*)NULL);
	if(cpu_thread==NULL){
		printf("SDL_CreateThread failed: %s\n", SDL_GetError());
		memory_free();
		return -1;
	}else{
		SDL_DetachThread(cpu_thread);
	}


	static Uint32 bgwin_buf[256*256];
	static Uint32 sprite_buf[160*144];
	SDL_Surface *bgwin_surface=SDL_CreateRGBSurfaceFrom((void *)bgwin_buf, 256, 256, 32, 256*4,
	       0x00ff0000,0x0000ff00,0x000000ff,0xff000000);
	SDL_Surface *sprite_surface=SDL_CreateRGBSurfaceFrom((void *)sprite_buf, 160, 144, 32, 160*4,
	       0x00ff0000,0x0000ff00,0x000000ff,0xff000000);

	int quit=0;
	SDL_Event e;
	Uint32 fps_timer;
	int frame_count=0;

	if(sdl_init() < 0){
		printf("sdl_init failed: %s\n", SDL_GetError());
		return -1;
	}

	TIMER_START(fps_timer);

	while(!quit){
		while(SDL_PollEvent(&e)!=0){
			switch(e.type){
			case SDL_QUIT:
				quit=1;
				break;
			}
		}

		static char wndtitle[32];
		int avgfps=frame_count/(TIMER_GET(fps_timer)/1000+1);
		if(avgfps>2000000)
			avgfps=0;
		if(frame_count%60==0){
			snprintf(wndtitle, 32, "FPS = %d", avgfps);
			SDL_SetWindowTitle(main_window, wndtitle);
		}

		SDL_SetRenderDrawColor(window_renderer, 0xff, 0xff, 0xff, 0xff);
		SDL_RenderClear(window_renderer);

		RST_LY;

		uint8_t lcdc=INTERNAL_IO[IO_LCDC_R];
		if(lcdc&0x80){
			//LCDがON
			if(lcdc&0x21){
				draw_bg_and_win(lcdc, bgwin_buf);
				SDL_Texture *texture = SDL_CreateTextureFromSurface(window_renderer, bgwin_surface);
				SDL_RenderCopy(window_renderer, texture, NULL, NULL);
				SDL_DestroyTexture(texture);
			}
			if(lcdc&0x2){
				draw_sprite(lcdc, sprite_buf);
				SDL_Texture *texture = SDL_CreateTextureFromSurface(window_renderer, sprite_surface);
				SDL_RenderCopy(window_renderer, texture, NULL, NULL);
				SDL_DestroyTexture(texture);
				while(INTERNAL_IO[IO_LY_R]<143)
					INC_LY;
			}
		}

		INC_LY;
		change_lcdmode(LCDMODE_VBLANK);
		wait_cpuclk(4560);
		SDL_RenderPresent(window_renderer);

		frame_count++;
		while(INTERNAL_IO[IO_LY_R]<152)
			INC_LY;
	}

	SDL_DestroyRenderer(window_renderer);
	window_renderer = NULL;
	SDL_DestroyWindow(main_window);
	main_window = NULL;

	SDL_Quit();

	memory_free();

	return 0;
}
