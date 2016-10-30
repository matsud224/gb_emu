#include "memory.h"
#include "cartridge.h"
#include "cpu.h"
#include <stdlib.h>
#include <time.h>
#include "SDL2/SDL_timer.h"

uint8_t*			INTERNAL_VRAM = NULL;
static uint8_t*		INTERNAL_WRAM = NULL;
static uint8_t*		INTERNAL_WRAM_MIRROR;
uint8_t*			INTERNAL_OAM = NULL;
static uint8_t*		INTERNAL_RESERVED = NULL;
uint8_t*			INTERNAL_IO = NULL;
static uint8_t*		INTERNAL_STACK = NULL;

SDL_atomic_t REG_IF, REG_IE;
uint8_t LCDMODE;
struct timespec div_origin, timer_origin;
SDL_TimerID timer_id = 0;

static struct cartridge *cart;

Uint32 timer_callback(Uint32 interval, void *param) {
	request_interrupt(INT_TIMER);
	return interval;
}

int memory_init(struct cartridge *c) {
	cart = c;
	if((INTERNAL_VRAM = malloc(sizeof(uint8_t) * 0x2000)) == NULL) goto err;
	if((INTERNAL_WRAM = malloc(sizeof(uint8_t) * 0x2000)) == NULL) goto err;
	INTERNAL_WRAM_MIRROR = INTERNAL_WRAM;
	if((INTERNAL_OAM = malloc(sizeof(uint8_t) * 0xa0)) == NULL) goto err;
	if((INTERNAL_RESERVED = malloc(sizeof(uint8_t) * 0x60)) == NULL) goto err;
	if((INTERNAL_IO = malloc(sizeof(uint8_t) * 0x80)) == NULL) goto err;
	if((INTERNAL_STACK = malloc(sizeof(uint8_t) * 0x7f)) == NULL) goto err;
	clock_gettime(CLOCK_MONOTONIC, &div_origin);
	LCDMODE = LCDMODE_VBLANK;
	return 0;
err:
	memory_free();
	return -1;
}

void memory_free() {
	if(INTERNAL_VRAM!=NULL){ free(INTERNAL_VRAM); INTERNAL_VRAM = NULL; }
	if(INTERNAL_WRAM!=NULL){ free(INTERNAL_WRAM); INTERNAL_WRAM = NULL; INTERNAL_WRAM_MIRROR = NULL; }
	if(INTERNAL_OAM!=NULL){ free(INTERNAL_OAM); INTERNAL_OAM = NULL; }
	if(INTERNAL_RESERVED!=NULL){ free(INTERNAL_RESERVED); INTERNAL_RESERVED = NULL; }
	if(INTERNAL_IO!=NULL){ free(INTERNAL_IO); INTERNAL_IO = NULL; }
	if(INTERNAL_STACK!=NULL){ free(INTERNAL_STACK); INTERNAL_STACK = NULL; }
}

uint8_t memory_write8(uint16_t dst, uint8_t value) {
	if(dst & 0x8000){
		if(dst & 0x4000){
			if(dst & 0x2000){
				if(dst & 0x200){
					if(dst & 0x100){
						if(dst & 0x80){
							if(dst == 0xffff){
								//INTERNAL_INTMASK
								CAS_UPDATE(REG_IE, value);
							}else{
								//INTERNAL_STACK
								INTERNAL_STACK[dst-V_INTERNAL_STACK] = value;
							}
						}else{
							//INTERNAL_IO
							switch(dst-V_INTERNAL_IO){
							case IO_P1_R: break;
							case IO_DIV_R: clock_gettime(CLOCK_MONOTONIC, &div_origin); break;
							case IO_TIMA_R: INTERNAL_IO[IO_TIMA_R]=value; break;
							case IO_TMA_R: INTERNAL_IO[IO_TMA_R]=value; break;
							case IO_TAC_R:
								{
									INTERNAL_IO[IO_TAC_R]=value;
									SDL_RemoveTimer(timer_id);
									if(value&0x4){
										//timer start
										clock_gettime(CLOCK_MONOTONIC, &timer_origin);
										int hz_exp;
										switch(value&0x3){
										case 0: hz_exp=12;
										case 1: hz_exp=18;
										case 2: hz_exp=16;
										case 3: hz_exp=14;
										}
										//1ms=10^6sec~~2^20sec
										timer_id=SDL_AddTimer((0xff-INTERNAL_IO[IO_TMA])<<(20-hz_exp), timer_callback, NULL);
									}
								}
								break;
							case IO_IF_R: CAS_UPDATE(REG_IF, value); break;
							case IO_LCDC_R: INTERNAL_IO[IO_LCDC_R]=value; printf("LCDC=0x%X\n", value); break;
							case IO_STAT_R: INTERNAL_IO[IO_STAT_R]=value; break;
							case IO_SCY_R: INTERNAL_IO[IO_SCY_R]=value; break;
							case IO_SCX_R: INTERNAL_IO[IO_SCX_R]=value; break;
							case IO_LY_R: break;
							case IO_LYC_R: INTERNAL_IO[IO_LYC_R]=value; break;
							case IO_DMA_R:
								{
									uint16_t start=(value)<<8, end=(start|0x9f);
									uint16_t oam_dst=0xfe00;
									for(; start<=end; start++)
										memory_write8(oam_dst++, memory_read8(start));
								}
								break;
							case IO_BGP_R: INTERNAL_IO[IO_BGP_R]=value; break;
							case IO_OBP0_R: INTERNAL_IO[IO_OBP0_R]=value; break;
							case IO_OBP1_R: INTERNAL_IO[IO_OBP1_R]=value; break;
							case IO_WY_R: INTERNAL_IO[IO_WY_R]=value; break;
							case IO_WX_R: INTERNAL_IO[IO_WX_R]=value; break;
							}
						}
					}else{
						if(dst & 0x20){
							//INTERNAL_RESERVED
							INTERNAL_RESERVED[dst-V_INTERNAL_RESERVED] = value;
						}else{
							//INTERNAL_OAM
							INTERNAL_OAM[dst-V_INTERNAL_OAM] = value;
						}
					}
				}else{
					//INTERNAL_WRAM_MIRROR
					INTERNAL_WRAM_MIRROR[dst-V_INTERNAL_WRAM_MIRROR] = value;
				}
			}else{
				//INTERNAL_WRAM
				INTERNAL_WRAM[dst-V_INTERNAL_WRAM] = value;
			}
		}else{
			if(dst & 0x2000){
				//CART_RAMN
				cart_ramn_write8(cart, dst, value);
			}else{
				//INTERNAL_VRAM
				INTERNAL_VRAM[dst-V_INTERNAL_VRAM] = value;
				if(dst>=0x9800) printf("VRAM access: VRAM[0x%X]=0x%X\n", dst, value);
			}
		}
	}else{
		if(dst & 0x4000){
			//CART_ROMN
			cart_romn_write8(cart, dst, value);
		}else{
			//CART_ROM0
			cart_rom0_write8(cart, dst, value);
		}
	}

	return value;
}

uint16_t memory_write16(uint16_t dst, uint16_t value) {
	memory_write8(dst, value&0xff);
	memory_write8(dst+1, value>>8);
	return value;
}

uint8_t memory_read8(uint16_t src) {
	if(src & 0x8000){
		if(src & 0x4000){
			if(src & 0x2000){
				if(src & 0x200){
					if(src & 0x100){
						if(src & 0x80){
							if(src == 0xffff){
								//INTERNAL_INTMASK
								return REG_IE.value;
							}else{
								//INTERNAL_STACK
								return INTERNAL_STACK[src-V_INTERNAL_STACK];
							}
						}else{
							//INTERNAL_IO
							switch(src-V_INTERNAL_IO){
							case IO_P1_R: return 0xff;
							case IO_DIV_R:
                                {
                                	//8192Hz
                                	struct timespec now;
                                	clock_gettime(CLOCK_MONOTONIC, &now);
                                	int64_t diff_sec=now.tv_sec-div_origin.tv_sec;
									int64_t diff_usec=(now.tv_nsec-div_origin.tv_nsec)/1000;
									if(diff_usec<0){
										diff_sec--; diff_usec+=1000000;
									}
									return (((diff_sec<<13)&0xff)+((diff_usec>>7)&0xff))&0xff;
                                }
							case IO_TIMA_R:
								{
                                	struct timespec now;
                                	clock_gettime(CLOCK_MONOTONIC, &now);
                                	int64_t diff_sec=now.tv_sec-timer_origin.tv_sec;
									int64_t diff_usec=(now.tv_nsec-timer_origin.tv_nsec)/1000;
									int hz_exp;
									switch(INTERNAL_IO[IO_TAC_R]&0x3){
									case 0: hz_exp=12;
									case 1: hz_exp=18;
									case 2: hz_exp=16;
									case 3: hz_exp=14;
									}
									if(diff_usec<0){
										diff_sec--; diff_usec+=1000000;
									}
									return (INTERNAL_IO[IO_TMA_R]+((diff_sec<<hz_exp)&0xff)+((diff_usec>>(20-hz_exp))&0xff))&0xff;
                                }
							case IO_TMA_R: return INTERNAL_IO[IO_TMA_R];
							case IO_TAC_R: return INTERNAL_IO[IO_TAC_R];
							case IO_IF_R: return REG_IF.value;
							case IO_LCDC_R: return INTERNAL_IO[IO_LCDC_R];
							case IO_STAT_R:
								//下位3bitは別で管理
								return (INTERNAL_IO[IO_STAT_R]&0xf8) | ((INTERNAL_IO[IO_LY_R]==INTERNAL_IO[IO_LYC_R])<<2) | LCDMODE;
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
							}
						}
					}else{
						if(src & 0x20){
							//INTERNAL_RESERVED
							return INTERNAL_RESERVED[src-V_INTERNAL_RESERVED];
						}else{
							//INTERNAL_OAM
							return INTERNAL_OAM[src-V_INTERNAL_OAM];
						}
					}
				}else{
					//INTERNAL_WRAM_MIRROR
					return INTERNAL_WRAM_MIRROR[src-V_INTERNAL_WRAM_MIRROR];
				}
			}else{
				//INTERNAL_WRAM
				return INTERNAL_WRAM[src-V_INTERNAL_WRAM];
			}
		}else{
			if(src & 0x2000){
				//CART_RAMN
				return cart_ramn_read8(cart, src);
			}else{
				//INTERNAL_VRAM
				return INTERNAL_VRAM[src-V_INTERNAL_VRAM];
			}
		}
	}else{
		if(src & 0x4000){
			//CART_ROMN
			return cart_romn_read8(cart, src);
		}else{
			//CART_ROM0
			return cart_rom0_read8(cart, src);
		}
	}

	return 0;
}

uint16_t memory_read16(uint16_t src) {
	return memory_read8(src) | (memory_read8(src+1)<<8);
}
