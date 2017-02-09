#include "memory.h"
#include "cartridge.h"
#include "cpu.h"
#include "lcd.h"
#include "joypad.h"
#include "sound.h"
#include "serial.h"
#include <stdlib.h>
#include <time.h>
#include "SDL2/SDL_keyboard.h"
#include "SDL2/SDL_scancode.h"
#include "SDL2/SDL_gamecontroller.h"

#define IO_DIV_R 0x04
#define IO_TIMA_R 0x05

#define MAX(x,y) ((x)<(y)?(y):(x))

uint8_t*			INTERNAL_VRAM = NULL;
static uint8_t*		INTERNAL_VRAM_VARIABLE = NULL;
static uint8_t*		INTERNAL_WRAM = NULL;
static uint8_t*		INTERNAL_WRAM_VARIABLE = NULL;
uint8_t*			INTERNAL_OAM = NULL;
static uint8_t*		INTERNAL_RESERVED = NULL;
uint8_t*			INTERNAL_IO = NULL;
static uint8_t*		INTERNAL_STACK = NULL;
uint8_t*			COLORPALETTE_BG = NULL;
uint8_t*			COLORPALETTE_SP = NULL;

uint32_t DIV;
uint16_t TIMA;
int CGBMODE;
int SERIALSTATE = 0;
int timer_remaining, timer_interval;
int serial_remaining, serial_interval = 0;


static struct cartridge *cart;

int memory_init(struct cartridge *c) {
	cart = c;

	if(cart_header(c)->cgbflag < CGBFLAG_BOTH)
		CGBMODE = 0;
	else
		CGBMODE = 1;

	if((INTERNAL_OAM = malloc(sizeof(uint8_t) * 0xa0)) == NULL) goto err;
	if((INTERNAL_RESERVED = malloc(sizeof(uint8_t) * 0x60)) == NULL) goto err;
	if((INTERNAL_IO = malloc(sizeof(uint8_t) * 0x100)) == NULL) goto err;
	if((INTERNAL_STACK = malloc(sizeof(uint8_t) * 0x7f)) == NULL) goto err;

	if(CGBMODE){
		if((INTERNAL_VRAM = malloc(sizeof(uint8_t) * 0x2000 * 2)) == NULL) goto err;
		if((INTERNAL_WRAM = malloc(sizeof(uint8_t) * 0x8000)) == NULL) goto err;
		if((COLORPALETTE_BG = malloc(sizeof(uint8_t) * 0x40)) == NULL) goto err;
		if((COLORPALETTE_SP = malloc(sizeof(uint8_t) * 0x40)) == NULL) goto err;
	}else{
		if((INTERNAL_VRAM = malloc(sizeof(uint8_t) * 0x2000)) == NULL) goto err;
		if((INTERNAL_WRAM = malloc(sizeof(uint8_t) * 0x2000)) == NULL) goto err;
	}
	INTERNAL_VRAM_VARIABLE = INTERNAL_VRAM;
	INTERNAL_WRAM_VARIABLE = INTERNAL_WRAM + 0x1000;

	return 0;
err:
	memory_free();
	return -1;
}

void memory_free() {
	if(INTERNAL_VRAM!=NULL){ free(INTERNAL_VRAM); INTERNAL_VRAM = NULL; INTERNAL_VRAM_VARIABLE = NULL; }
	if(INTERNAL_WRAM!=NULL){ free(INTERNAL_WRAM); INTERNAL_WRAM = NULL; INTERNAL_WRAM_VARIABLE = NULL; }
	if(INTERNAL_OAM!=NULL){ free(INTERNAL_OAM); INTERNAL_OAM = NULL; }
	if(INTERNAL_RESERVED!=NULL){ free(INTERNAL_RESERVED); INTERNAL_RESERVED = NULL; }
	if(INTERNAL_IO!=NULL){ free(INTERNAL_IO); INTERNAL_IO = NULL; }
	if(INTERNAL_STACK!=NULL){ free(INTERNAL_STACK); INTERNAL_STACK = NULL; }
	if(COLORPALETTE_BG!=NULL){ free(COLORPALETTE_BG); COLORPALETTE_BG = NULL; }
	if(COLORPALETTE_SP!=NULL){ free(COLORPALETTE_SP); COLORPALETTE_SP = NULL; }
}

uint8_t memory_write8(uint16_t dst, uint8_t value) {
	if(dst < V_CART_ROMN){
		//CART_ROM0
		cart_rom0_write8(cart, dst, value);
	}else if(dst < V_INTERNAL_VRAM){
		//CART_ROMN
		cart_romn_write8(cart, dst, value);
	}else if(dst < V_CART_RAMN){
		//INTERNAL_VRAM(variable area)
		INTERNAL_VRAM_VARIABLE[dst-V_INTERNAL_VRAM] = value;
	}else if(dst < V_INTERNAL_WRAM){
		//CART_RAMN
		cart_ramn_write8(cart, dst, value);
	}else if(dst < V_INTERNAL_WRAM+0x1000){
		//INTERNAL_WRAM(fixed area)
		INTERNAL_WRAM[dst-V_INTERNAL_WRAM] = value;
	}else if(dst < V_INTERNAL_WRAM_MIRROR){
		//INTERNAL_WRAM(variable area)
		INTERNAL_WRAM_VARIABLE[dst-(V_INTERNAL_WRAM+0x1000)] = value;
	}else if(dst < V_INTERNAL_WRAM_MIRROR+0x1000){
		//INTERNAL_WRAM_MIRROR(fixed area)
		INTERNAL_WRAM[dst-V_INTERNAL_WRAM_MIRROR] = value;
	}else if(dst < V_INTERNAL_OAM){
		//INTERNAL_WRAM_MIRROR(variable area)
		INTERNAL_WRAM_VARIABLE[dst-(V_INTERNAL_WRAM_MIRROR+0x1000)] = value;
	}else if(dst < V_INTERNAL_RESERVED){
		//INTERNAL_OAM
		INTERNAL_OAM[dst-V_INTERNAL_OAM] = value;
	}else if(dst < V_INTERNAL_IO){
		//INTERNAL_RESERVED
		INTERNAL_RESERVED[dst-V_INTERNAL_RESERVED] = value;
	}else if(dst < V_INTERNAL_STACK){
		//INTERNAL_IO
		switch(dst-V_INTERNAL_IO){
		case IO_SB_R: INTERNAL_IO[IO_SB_R] = value; break;
		case IO_SC_R:
			if((INTERNAL_IO[IO_SC_R]&0x1) == (value&0x1))
				master_sent = 1;
			INTERNAL_IO[IO_SC_R] = value;
			SERIALSTATE = value>>7;
			if(SERIALSTATE && INTERNAL_IO[IO_SC_R]&0x1){
				master_sent = 0;
			}
			break;
		case IO_P1_R: INTERNAL_IO[IO_P1_R] = value; break;
		case IO_DIV_R: DIV = 0; break;
		case IO_TIMA_R: TIMA=value; break;
		case IO_TMA_R: INTERNAL_IO[IO_TMA_R]=value; break;
		case IO_TAC_R:
			INTERNAL_IO[IO_TAC_R]=value;
			switch(value&0x3){
			case 0: timer_interval = 1024; break;
			case 1: timer_interval = 16; break;
			case 2: timer_interval = 64; break;
			case 3: timer_interval = 256; break;
			}
			timer_remaining = timer_interval;
			break;
		case IO_IF_R: INTERNAL_IO[IO_IF_R]=value; break;
		case IO_LCDC_R: INTERNAL_IO[IO_LCDC_R]=value; break;
		case IO_STAT_R: INTERNAL_IO[IO_STAT_R]=value; break;
		case IO_SCY_R: INTERNAL_IO[IO_SCY_R]=value; break;
		case IO_SCX_R: INTERNAL_IO[IO_SCX_R]=value; break;
		case IO_LY_R: break;
		case IO_LYC_R: INTERNAL_IO[IO_LYC_R]=value; break;
		case IO_DMA_R:
			{
				uint16_t start=(value)<<8, end=(start|0x9f);
				int oam_dst=0;
				for(; start<=end; start++)
					INTERNAL_OAM[oam_dst++] = memory_read8(start);
			}
			break;
		case IO_BGP_R: INTERNAL_IO[IO_BGP_R]=value; break;
		case IO_OBP0_R: INTERNAL_IO[IO_OBP0_R]=value; break;
		case IO_OBP1_R: INTERNAL_IO[IO_OBP1_R]=value; break;
		case IO_WY_R: INTERNAL_IO[IO_WY_R]=value; break;
		case IO_WX_R: INTERNAL_IO[IO_WX_R]=value; break;
		case IO_NR10_R:
		case IO_NR11_R:
		case IO_NR12_R:
		case IO_NR13_R:
		case IO_NR14_R:
			sound_ch1_writereg(dst-V_INTERNAL_IO, value); break;
		case IO_NR21_R:
		case IO_NR22_R:
		case IO_NR23_R:
		case IO_NR24_R:
			sound_ch2_writereg(dst-V_INTERNAL_IO, value); break;
		case IO_NR30_R:
		case IO_NR31_R:
		case IO_NR32_R:
		case IO_NR33_R:
		case IO_NR34_R:
			sound_ch3_writereg(dst-V_INTERNAL_IO, value); break;
		case IO_NR41_R:
		case IO_NR42_R:
		case IO_NR43_R:
		case IO_NR44_R:
			sound_ch4_writereg(dst-V_INTERNAL_IO, value); break;
		case IO_NR50_R:
		case IO_NR51_R:
		case IO_NR52_R:
			sound_master_writereg(dst-V_INTERNAL_IO, value); break;
#define CGBCHECK if(!CGBMODE) break;
		case IO_KEY1_R:
			CGBCHECK;
			/* not implemented */
			puts("CPU double-speed mode is unimplemented!");
			break;
		case IO_VBK_R:
			CGBCHECK;
			INTERNAL_IO[IO_VBK_R] = value;
			INTERNAL_VRAM_VARIABLE = INTERNAL_VRAM + 0x2000*(value & 0x1);
			break;
		case IO_HDMA1_R:
			CGBCHECK; INTERNAL_IO[IO_HDMA1_R] = value; break;
		case IO_HDMA2_R:
			CGBCHECK; INTERNAL_IO[IO_HDMA2_R] = value & 0xf8; break;
		case IO_HDMA3_R:
			CGBCHECK; INTERNAL_IO[IO_HDMA3_R] = (value&0x1f)+0x80; break;
		case IO_HDMA4_R:
			CGBCHECK; INTERNAL_IO[IO_HDMA4_R] = value & 0xf8; break;
		case IO_HDMA5_R:
			CGBCHECK;
			if((value&0x80) == 0){
				//General Purpose DMA
				uint16_t src=(INTERNAL_IO[IO_HDMA1_R]<<8) | INTERNAL_IO[IO_HDMA2_R];
				uint16_t dst=(INTERNAL_IO[IO_HDMA3_R]<<8) | INTERNAL_IO[IO_HDMA4_R];
				int len = (value&0x7f)/0x10-1;
				for(int i=0; i<len; i++,src++,dst++)
					memory_write8(dst, memory_read8(src));
				INTERNAL_IO[IO_HDMA5_R] = 0xff;
			}else{
				//H-Blank DMA
				INTERNAL_IO[IO_HDMA5_R] = value;
			}
			break;
		case IO_BCPS_R:
			CGBCHECK; INTERNAL_IO[IO_BCPS_R] = value; break;
		case IO_BCPD_R:
			CGBCHECK;
			uint8_t bcps = INTERNAL_IO[IO_BCPS_R];
			COLORPALETTE_BG[bcps&0x3f] = value;
			if(bcps&0x80)
				INTERNAL_IO[IO_BCPS_R] = (bcps+1)&0xbf;
			break;
		case IO_OCPS_R:
			CGBCHECK; INTERNAL_IO[IO_OCPS_R] = value; break;
		case IO_OCPD_R:
			CGBCHECK;
			uint8_t ocps = INTERNAL_IO[IO_OCPS_R];
			COLORPALETTE_SP[ocps&0x3f] = value;
			if(ocps&0x80)
				INTERNAL_IO[IO_OCPS_R] = (ocps+1)&0xbf;
			break;
		case IO_SVBK_R:
			CGBCHECK;
			INTERNAL_IO[IO_SVBK_R] = value;
			INTERNAL_WRAM_VARIABLE = INTERNAL_WRAM + 0x1000*MAX(value&0x7, 1);
			break;
		default:
			//wave ramのために
			if(dst>=0xff30 && dst<=0xff3f)
				INTERNAL_IO[dst-V_INTERNAL_IO]=value;
			break;
		}
	}else if(dst < V_INTERNAL_INTMASK){
		//INTERNAL_STACK
		INTERNAL_STACK[dst-V_INTERNAL_STACK] = value;
	}else{
		//INTERNAL_INTMASK
		INTERNAL_IO[IO_IE_R] = value;
	}

	return value;
}

uint16_t memory_write16(uint16_t dst, uint16_t value) {
	memory_write8(dst, value&0xff);
	memory_write8(dst+1, value>>8);
	return value;
}

uint8_t memory_read8(uint16_t src) {
	if(src < V_CART_ROMN){
		//CART_ROM0
		return cart_rom0_read8(cart, src);
	}else if(src < V_INTERNAL_VRAM){
		//CART_ROMN
		return cart_romn_read8(cart, src);
	}else if(src < V_CART_RAMN){
		//INTERNAL_VRAM(variable area)
		return INTERNAL_VRAM_VARIABLE[src-V_INTERNAL_VRAM];
	}else if(src < V_INTERNAL_WRAM){
		//CART_RAMN
		return cart_ramn_read8(cart, src);
	}else if(src < V_INTERNAL_WRAM+0x1000){
		//INTERNAL_WRAM(fixed area)
		return INTERNAL_WRAM[src-V_INTERNAL_WRAM];
	}else if(src < V_INTERNAL_WRAM_MIRROR){
		//INTERNAL_WRAM(variable area)
		return INTERNAL_WRAM_VARIABLE[src-(V_INTERNAL_WRAM+0x1000)];
	}else if(src < V_INTERNAL_WRAM_MIRROR+0x1000){
		//INTERNAL_WRAM_MIRROR(fixed area)
		return INTERNAL_WRAM[src-V_INTERNAL_WRAM_MIRROR];
	}else if(src < V_INTERNAL_OAM){
		//INTERNAL_WRAM_MIRROR(variable area)
		return INTERNAL_WRAM_VARIABLE[src-(V_INTERNAL_WRAM_MIRROR+0x1000)];
	}else if(src < V_INTERNAL_RESERVED){
		//INTERNAL_OAM
		return INTERNAL_OAM[src-V_INTERNAL_OAM];
	}else if(src < V_INTERNAL_IO){
		//INTERNAL_RESERVED
		return INTERNAL_RESERVED[src-V_INTERNAL_RESERVED];
	}else if(src < V_INTERNAL_STACK){
		//INTERNAL_IO
		switch(src-V_INTERNAL_IO){
		case IO_SB_R: return INTERNAL_IO[IO_SB_R];
		case IO_SC_R: return (SERIALSTATE<<7) | (INTERNAL_IO[IO_SC_R] & 0x1) | 0x2;
		case IO_P1_R: return joypad_status();
		case IO_DIV_R: return DIV>>8;
		case IO_TIMA_R: return TIMA;
		case IO_TMA_R: return INTERNAL_IO[IO_TMA_R];
		case IO_TAC_R: return INTERNAL_IO[IO_TAC_R];
		case IO_IF_R: return INTERNAL_IO[IO_IF_R];
		case IO_LCDC_R: return INTERNAL_IO[IO_LCDC_R];
		case IO_STAT_R:
			//下位3bitは別で管理
			return (INTERNAL_IO[IO_STAT_R]&0xf8) | ((INTERNAL_IO[IO_LY_R]==INTERNAL_IO[IO_LYC_R])<<2) | lcd_get_mode();
		case IO_SCY_R: return INTERNAL_IO[IO_SCY_R];
		case IO_SCX_R: return INTERNAL_IO[IO_SCX_R];
		case IO_LY_R: return INTERNAL_IO[IO_LY_R];
		case IO_LYC_R: return INTERNAL_IO[IO_LYC_R];
		case IO_DMA_R: return 0;
		case IO_BGP_R: return INTERNAL_IO[IO_BGP_R];
		case IO_OBP0_R: return INTERNAL_IO[IO_OBP0_R];
		case IO_OBP1_R: return INTERNAL_IO[IO_OBP1_R];
		case IO_WY_R: return INTERNAL_IO[IO_WY_R];
		case IO_WX_R: return INTERNAL_IO[IO_WX_R];
		case IO_NR10_R:
		case IO_NR11_R:
		case IO_NR12_R:
		case IO_NR13_R:
		case IO_NR14_R:
			return sound_ch1_readreg(src-V_INTERNAL_IO);
		case IO_NR21_R:
		case IO_NR22_R:
		case IO_NR23_R:
		case IO_NR24_R:
			return sound_ch2_readreg(src-V_INTERNAL_IO);
		case IO_NR30_R:
		case IO_NR31_R:
		case IO_NR32_R:
		case IO_NR33_R:
		case IO_NR34_R:
			return sound_ch3_readreg(src-V_INTERNAL_IO);
		case IO_NR41_R:
		case IO_NR42_R:
		case IO_NR43_R:
		case IO_NR44_R:
			return sound_ch4_readreg(src-V_INTERNAL_IO);
		case IO_NR50_R:
		case IO_NR51_R:
		case IO_NR52_R:
			return sound_master_readreg(src-V_INTERNAL_IO);
		case IO_KEY1_R:
			CGBCHECK;
			/* not implemented */
			puts("CPU double-speed mode is unimplemented!");
			break;
		case IO_VBK_R: return INTERNAL_IO[IO_VBK_R];
		case IO_HDMA5_R: return INTERNAL_IO[IO_HDMA5_R];
		case IO_BCPS_R: return INTERNAL_IO[IO_BCPS_R];
		case IO_BCPD_R:
			CGBCHECK;
			return COLORPALETTE_BG[INTERNAL_IO[IO_BCPS_R]&0x3f];
		case IO_OCPS_R: return INTERNAL_IO[IO_OCPS_R];
		case IO_OCPD_R:
			CGBCHECK;
			return COLORPALETTE_SP[INTERNAL_IO[IO_OCPS_R]&0x3f];
		case IO_SVBK_R: return INTERNAL_IO[IO_SVBK_R];
		default:
			//wave ramのために
			if(src>=0xff30 && src<=0xff3f)
				return INTERNAL_IO[src-V_INTERNAL_IO];
			break;
		}
	}else if(src < V_INTERNAL_INTMASK){
		//INTERNAL_STACK
		return INTERNAL_STACK[src-V_INTERNAL_STACK];
	}else{
		//INTERNAL_INTMASK
		return INTERNAL_IO[IO_IE_R];
	}

	return 0;
}

uint16_t memory_read16(uint16_t src) {
	return memory_read8(src) | (memory_read8(src+1)<<8);
}
