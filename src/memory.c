#include "memory.h"
#include "cartridge.h"
#include <stdlib.h>


static uint8_t*		INTERNAL_VRAM = NULL;
static uint8_t*		INTERNAL_WRAM = NULL;
static uint8_t*		INTERNAL_WRAM_MIRROR;
static uint8_t*		INTERNAL_OAM = NULL;
static uint8_t*		INTERNAL_RESERVED = NULL;
static uint8_t*		INTERNAL_IO = NULL;
static uint8_t*		INTERNAL_STACK = NULL;

SDL_atomic_t REG_IF, REG_IE;

static struct cartridge *cart;

int memory_init(struct cartridge *c) {
	cart = c;
	if((INTERNAL_VRAM = malloc(sizeof(uint8_t) * 0x2000)) == NULL) goto err;
	if((INTERNAL_WRAM = malloc(sizeof(uint8_t) * 0x2000)) == NULL) goto err;
	INTERNAL_WRAM_MIRROR = INTERNAL_WRAM;
	if((INTERNAL_OAM = malloc(sizeof(uint8_t) * 0xa0)) == NULL) goto err;
	if((INTERNAL_RESERVED = malloc(sizeof(uint8_t) * 0x60)) == NULL) goto err;
	if((INTERNAL_IO = malloc(sizeof(uint8_t) * 0x80)) == NULL) goto err;
	if((INTERNAL_STACK = malloc(sizeof(uint8_t) * 0x7f)) == NULL) goto err;
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
							case IO_DIV_R: break;
							case IO_TIMA_R: break;
							case IO_TMA_R: break;
							case IO_TAC_R: break;
							case IO_IF_R: CAS_UPDATE(REG_IF, value); break;
							case IO_LCDC_R: break;
							case IO_STAT_R: break;
							case IO_SCY_R: break;
							case IO_SCX_R: break;
							case IO_LY_R: break;
							case IO_LYC_R: break;
							case IO_DMA_R:
								{
									uint16_t start=(value)<<8, end=(start|0x9f);
									uint16_t oam_dst=0xfe00;
									for(; start<=end; start++)
										memory_write8(oam_dst++, memory_read8(start));
								}
								break;
							case IO_BGP_R: break;
							case IO_OBP0_R: break;
							case IO_OBP1_R: break;
							case IO_WY_R: break;
							case IO_WX_R: break;
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
							case IO_P1_R: break;
							case IO_DIV_R: break;
							case IO_TIMA_R: break;
							case IO_TMA_R: break;
							case IO_TAC_R: break;
							case IO_IF_R: return REG_IF.value;
							case IO_LCDC_R: break;
							case IO_STAT_R: break;
							case IO_SCY_R: break;
							case IO_SCX_R: break;
							case IO_LY_R: break;
							case IO_LYC_R: break;
							case IO_DMA_R: return 0;
							case IO_BGP_R: break;
							case IO_OBP0_R: break;
							case IO_OBP1_R: break;
							case IO_WY_R: break;
							case IO_WX_R: break;
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
