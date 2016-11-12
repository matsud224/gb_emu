#include "lcd.h"
#include "memory.h"
#include "cpu.h"
#include "SDL2/SDL.h"

struct RGB{
	Uint8 r,g,b;
};

static struct RGB ACTUALCOLOR[4] = {{222,249,208},
									 {139,192,112},
									 {68,100,59},
									 {36,54,31}};
static Uint32 ABSCOLOR[4];

#define PALETTE(p,n) ((INTERNAL_IO[p]>>((n)<<1))&0x3)
#define BGPALETTE(n) ((INTERNAL_IO[IO_BGP_R]>>((n)<<1))&0x3)

#define SPRITECOUNT 40

static int LCDMODE = LCDMODE_VBLANK;

uint8_t lcd_get_mode() {
	return LCDMODE;
}

void lcd_change_mode(int mode) {
	LCDMODE = mode;
	switch(mode){
	case LCDMODE_HBLANK:
		if(INTERNAL_IO[IO_STAT_R]&0x8)
			cpu_request_interrupt(INT_LCDSTAT);
		break;
	case LCDMODE_VBLANK:
		cpu_request_interrupt(INT_VBLANK);
		if(INTERNAL_IO[IO_STAT_R]&0x10)
			cpu_request_interrupt(INT_LCDSTAT);
		break;
	case LCDMODE_SEARCHOAM:
		if(INTERNAL_IO[IO_STAT_R]&0x20)
			cpu_request_interrupt(INT_LCDSTAT);
		break;
	}
}



void lcd_init(SDL_Surface *surface) {
	for(int i=0; i<4; i++)
		ABSCOLOR[i] = SDL_MapRGBA(surface->format, ACTUALCOLOR[i].r, ACTUALCOLOR[i].g, ACTUALCOLOR[i].b, 255);
}

void lcd_clear(Uint32 buf[]) {
	for(int i=0; i<160*144; i++)
		buf[i] = ABSCOLOR[0];
}

void lcd_draw_background_oneline(Uint32 buf[]) {
	uint8_t lcdc = INTERNAL_IO[IO_LCDC_R];
	uint8_t y = INTERNAL_IO[IO_LY_R];
	uint8_t *tilemap = INTERNAL_VRAM+(((lcdc&0x8)?0x9c00:0x9800)-V_INTERNAL_VRAM);
	uint8_t *tiledata = INTERNAL_VRAM+(((lcdc&0x10)?0x8000:0x9000)-V_INTERNAL_VRAM);
	uint8_t scx=INTERNAL_IO[IO_SCX_R], scy=INTERNAL_IO[IO_SCY_R];

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


void lcd_draw_window_oneline(Uint32 buf[]) {
	uint8_t lcdc = INTERNAL_IO[IO_LCDC_R];
	uint8_t y = INTERNAL_IO[IO_LY_R];
	uint8_t *tilemap = INTERNAL_VRAM+(((lcdc&0x40)?0x9c00:0x9800)-V_INTERNAL_VRAM);
	uint8_t *tiledata = INTERNAL_VRAM+(((lcdc&0x10)?0x8000:0x9000)-V_INTERNAL_VRAM);
	int wx=INTERNAL_IO[IO_WX_R]-7, wy=INTERNAL_IO[IO_WY_R];

	int map_y=y-wy, in_y=map_y%8;
	if(y<wy) return;
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

void lcd_draw_sprite_oneline(Uint32 buf[]) {
	uint8_t lcdc = INTERNAL_IO[IO_LCDC_R];
	uint8_t scr_y = INTERNAL_IO[IO_LY_R];
	uint8_t *tiledata = INTERNAL_VRAM+(0x8000-V_INTERNAL_VRAM);
	uint8_t oam_sorted[SPRITECOUNT*4];
	uint8_t *oam_last = oam_sorted+((SPRITECOUNT-1)*4);

	memcpy(oam_sorted, INTERNAL_OAM, SPRITECOUNT*4);
	for(int i=SPRITECOUNT-1; i>0; i--)
		for(int j=0; j<i; j++)
			if(oam_sorted[j*4+1]>oam_sorted[(j+1)*4+1]){
				uint8_t temp[4];
				for(int n=0; n<4; n++)
					temp[n] = oam_sorted[j*4+n];
				for(int n=0; n<4; n++)
					oam_sorted[j*4+n] = oam_sorted[(j+1)*4+n];
				for(int n=0; n<4; n++)
					oam_sorted[(j+1)*4+n] = temp[n];
			}

	for(uint8_t *attr=oam_last; attr>=oam_sorted; attr-=4){
        int sp_y=attr[0]-16, sp_x=attr[1]-8;
        uint8_t flags=attr[3];
        uint16_t palette_addr = (flags&0x10)?IO_OBP1_R:IO_OBP0_R;
		if(!(sp_y<=scr_y && scr_y<sp_y+((lcdc&0x4)?16:8)) || sp_x==-8 || sp_x>=160)
			continue;

		if(lcdc&0x4){
			//8x16 mode
			uint8_t *upperdata = tiledata + (attr[2]&0xfe)*16;
			uint8_t *lowerdata = tiledata + (attr[2]|0x01)*16;
			int in_y=(flags&0x40)?(15-(scr_y-sp_y)):(scr_y-sp_y);
			uint8_t lower=(in_y&0x8)?lowerdata[(in_y-8)*2]:upperdata[in_y*2],
					upper=(in_y&0x8)?lowerdata[(in_y-8)*2+1]:upperdata[in_y*2+1];
			for(int x=0; x<8; x++){
				int scr_x=(flags&0x20)?(sp_x+(7-x)):(sp_x+x);
				if(scr_x<0 || scr_x>=160) continue;
				Uint32 cnum = ((upper>>(7-x))&0x1)<<1 | ((lower>>(7-x))&0x1);
				if(cnum!=0 && (!(flags&0x80) || buf[scr_y*160 + scr_x]==ABSCOLOR[BGPALETTE(0)]))
					buf[scr_y*160 + scr_x] = ABSCOLOR[PALETTE(palette_addr, cnum)]; //0なら透過
			}
		}else{
			//8x8 mode
			uint8_t *thisdata = tiledata + attr[2]*16;
			int in_y=(flags&0x40)?(7-(scr_y-sp_y)):(scr_y-sp_y);
			uint8_t lower=thisdata[in_y*2], upper=thisdata[in_y*2+1];
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
