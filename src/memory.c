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
static uint8_t*		INTERNAL_INTMASK = NULL;

static struct cartridge *cart;

int memory_init(struct cartridge *c) {
	cart = c;
	if((INTERNAL_VRAM = malloc(sizeof(uint8_t) * 0x2000)) == NULL) goto err;
	if((INTERNAL_WRAM = malloc(sizeof(uint8_t) * 0x2000)) == NULL) goto err;
	INTERNAL_WRAM_MIRROR = INTERNAL_WRAM;
	if((INTERNAL_OAM = malloc(sizeof(uint8_t) * 0xa0)) == NULL) goto err;
	if((INTERNAL_RESERVED = malloc(sizeof(uint8_t) * 0x60)) == NULL) goto err;
	if((INTERNAL_IO = malloc(sizeof(uint8_t) * 0x80+0x1)) == NULL) goto err;
	if((INTERNAL_STACK = malloc(sizeof(uint8_t) * 0x7f)) == NULL) goto err;
	INTERNAL_INTMASK = INTERNAL_IO+0x80;
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
	if(INTERNAL_IO!=NULL){ free(INTERNAL_IO); INTERNAL_IO = NULL; INTERNAL_INTMASK = NULL; }
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
								INTERNAL_INTMASK[0] = value;
							}else{
								//INTERNAL_STACK
								INTERNAL_STACK[dst-V_INTERNAL_STACK] = value;
							}
						}else{
							//INTERNAL_IO
							INTERNAL_IO[dst-V_INTERNAL_IO] = value;
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
								return INTERNAL_INTMASK[0];
							}else{
								//INTERNAL_STACK
								return INTERNAL_STACK[src-V_INTERNAL_STACK];
							}
						}else{
							//INTERNAL_IO
							return INTERNAL_IO[src-V_INTERNAL_IO];
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
}

uint16_t memory_read16(uint16_t src) {
	return memory_read8(src) | (memory_read8(src+1)<<8);
}
