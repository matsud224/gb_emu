#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "cartridge.h"
#include "memory.h"
#include "lcd.h"
#include "joypad.h"
#include "sound.h"
#include "serial.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"
#include "SDL2/SDL_timer.h"
#include "SDL2/SDL_keyboard.h"
#include "SDL2/SDL_gamecontroller.h"

static int SCREEN_WIDTH = 160;
static int SCREEN_HEIGHT = 144;

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

static uint8_t *open_ram(char* filename, unsigned int size, time_t *t) {
	int fd;
	struct stat sbuf;
	fd = open(filename, O_RDWR);
	if(fd == -1){
		//存在しなければ、新たにファイルを作成
		fd = open(filename, O_RDWR|O_CREAT, S_IWUSR|S_IRUSR);
		if(fd == -1)
			return NULL;
		char c = '\0';
		for(unsigned int i=0; i<size; i++)
			write(fd, &c, 1);
		lseek(fd, 0, SEEK_SET);
		puts("new save data created");
		*t = time(NULL);
	}else{
		if(fstat(fd, &sbuf) == -1){
			close(fd);
			return NULL;
		}
		if(sbuf.st_size!=size){
			puts("mismatch between ram size and save data");
			close(fd);
			return NULL;
		}
		*t = sbuf.st_mtim.tv_sec;
	}

	uint8_t *data = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
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

extern int logging_enabled;

extern char	*optarg;
extern int	optind, opterr;
int main(int argc, char *argv[]) {
	int result;
	char *romname;
	char ramname[256] = {'\0'};
	char hostname[256] = {'\0'};
	int port = 35902, zoom = 1;
	int has_ram=0, has_host = 0;
	int tcpmode = 0; //0..使用しない/1..サーバ/2..クライアント
	int force_dmg = 0;
	while((result=getopt(argc, argv, "dlcs:p:h:z:"))!=-1){
		switch(result){
		case 'l':
			//tcp listen(server)
			tcpmode = 1;
			break;
		case 'c':
			//tcp connect(client)
			tcpmode = 2;
			break;
		case 's':
			//save data
			has_ram = 1;
			strncpy(ramname, optarg, 256);
			break;
		case 'p':
			//port no
			port = atoi(optarg);
			break;
		case 'h':
			//host
			has_host = 1;
			strncpy(hostname, optarg, 256);
			break;
		case 'z':
			//zoom
			zoom = atoi(optarg);
			break;
		case 'd':
			//DMG mode(monochrome)
			force_dmg = 1;
			break;
		case ':':
		case '?':
			exit(-1);
		}
	}
	argc -= optind;
	argv += optind;
	//この時点でargv[0]~argv[argc-1]にオプションでない引数が入ってる
	if(argc == 0){
		puts("missing rom file name");
		return -1;
	}
	romname = argv[0];


	SCREEN_HEIGHT *= zoom;
	SCREEN_WIDTH *= zoom;

	uint8_t *rom = open_rom(romname);
	if(rom==NULL){
		printf("open_rom failed\n");
		return -1;
	}

	struct cartridge *cart = cart_init(rom);
	if(cart == NULL){
		printf("cart_init failed\n");
		return -1;
	}

	static const int ramsize_table[] = {0,2048,8192,8192*4,8192*16,8192*8};
	struct gb_carthdr *hdr = cart_header(cart);
    printf("title: %.16s\ncgbflag: 0x%X\ncarttype: 0x%X\nromsize: 0x%X\nramsize: 0x%X(%dKB)\n",
		 hdr->title, hdr->cgbflag, hdr->carttype, hdr->romsize, hdr->ramsize, ramsize_table[hdr->ramsize]);

	if(hdr->ramsize!=0){
		char buf[256];
		char *fname;
		if(!has_ram){
			strncpy(buf, romname, 256);
			strncat(buf, ".save", 256-strlen(buf));
			fname=buf;
		}else{
			fname=ramname;
		}
		time_t t;

		int ramsize = ramsize_table[hdr->ramsize];
		if(hdr->carttype==CARTTYPE_MBC2 || hdr->carttype==CARTTYPE_MBC2_BATT)
			ramsize = 512;

		uint8_t *ram = open_ram(fname, ramsize, &t);
		if(ram == NULL){
			puts("open_ram failed");
			return -1;
		}
		cart_setram(cart, ram, t);
	}

	if(memory_init(cart)){
		free(cart);
		puts("memory_init failed");
		return -1;
	}

	if(force_dmg)
		CGBMODE = 0;

	switch(tcpmode){
	case 1:
		//server
		if(serial_serverinit(port) < 0){
			puts("network error");
		}
		break;
	case 2:
		//client
		if(has_host){
			if(serial_clientinit(hostname, port) < 0){
				puts("network error");
			}
		}else{
			puts("missing hostname");
		}
		break;
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

	sound_init();

	TIMER_START(fps_timer);

	while(!(INTERNAL_IO[IO_LCDC_R]&0x80)){
		cpu_exec(4);
	}

	int quit = 0;
	int over=0;
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
				case SDLK_l:
					logging_enabled = 1;
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

		int avgfps=frame_count/(TIMER_GET(fps_timer)/1000+1);
		if(frame_count%60==0){
			static char wndtitle[64];
			snprintf(wndtitle, 64, "%.16s  FPS = %d %s", hdr->title, avgfps, serial_linked()?"Linked":"");
			SDL_SetWindowTitle(main_window, wndtitle);
		}

		SDL_SetRenderDrawColor(window_renderer, 0xff, 0xff, 0xff, 0xff);
		SDL_RenderClear(window_renderer);

		if(INTERNAL_IO[IO_LCDC_R]&0x80){
			//LCDがON
			RST_LY;
			lcd_clear(bitmap);
			while(INTERNAL_IO[IO_LY_R]<=143){
				lcd_change_mode(2); over=cpu_exec(80-over);
				lcd_change_mode(3); over=cpu_exec(172-over);

				if(INTERNAL_IO[IO_LCDC_R]&0x1)
					lcd_draw_background_oneline(bitmap);

				if(INTERNAL_IO[IO_LCDC_R]&0x20)
					lcd_draw_window_oneline(bitmap);
				if(INTERNAL_IO[IO_LCDC_R]&0x2)
					lcd_draw_sprite_oneline(bitmap);


				lcd_change_mode(0); over=cpu_exec(204-over);

				//H-Blank DMA
				if(CGBMODE && (INTERNAL_IO[IO_HDMA5_R]&0x80) == 0){
					uint16_t src=(INTERNAL_IO[IO_HDMA1_R]<<8) | INTERNAL_IO[IO_HDMA2_R];
					uint16_t dst=(INTERNAL_IO[IO_HDMA3_R]<<8) | INTERNAL_IO[IO_HDMA4_R];
					//transfer 0x10 bytes
					for(int i=0; i<0x10; i++,src++,dst++)
						memory_write8(dst, memory_read8(src));
					int remaining = (INTERNAL_IO[IO_HDMA5_R]&0x7f)/0x10-1;
					remaining -= 0x10;
					if(remaining == 0)
						INTERNAL_IO[IO_HDMA5_R] = 0xff;
					else
						INTERNAL_IO[IO_HDMA5_R] = (remaining+1)*0x10;
					INTERNAL_IO[IO_HDMA1_R] = src>>8;
					INTERNAL_IO[IO_HDMA2_R] = src&0xff;
					INTERNAL_IO[IO_HDMA3_R] = dst>>8;
					INTERNAL_IO[IO_HDMA4_R] = dst&0xff;
				}

				INC_LY;
			}
			SDL_Texture *texture = SDL_CreateTextureFromSurface(window_renderer, bitmap_surface);
			SDL_RenderCopy(window_renderer, texture, NULL, NULL);
			SDL_DestroyTexture(texture);

			lcd_change_mode(LCDMODE_VBLANK);
		}else{
			over=cpu_exec(70224-over);
		}

		SDL_RenderPresent(window_renderer);
		frame_count++;

		if(INTERNAL_IO[IO_LCDC_R]&0x80){
			while(INTERNAL_IO[IO_LY_R]<=153){
				over=cpu_exec(/*456*/468-over); //464 ... for street fighter 2
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

	if(tcpmode>0)
		serial_close();

	return 0;
}
