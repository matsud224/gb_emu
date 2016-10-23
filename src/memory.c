#include "memory.h"
#include "cartridge.h"
#include <stdlib.h>

static uint8_t*		INTERNAL_VRAM;
static uint8_t*		INTERNAL_WRAM;
static uint8_t*		INTERNAL_WRAM_MIRROR;
static uint8_t*		INTERNAL_OAM;
static uint8_t*		INTERNAL_RESERVED;
static uint8_t*		INTERNAL_IO;
static uint8_t*		INTERNAL_STACK;
static uint8_t*		INTERNAL_INTMASK;

static struct cartridge *cart;

void memory_init(struct cartridge *c) {
	cart = c;
	INTERNAL_VRAM = malloc(sizeof(uint8_t) * 0x2000);
	INTERNAL_WRAM = malloc(sizeof(uint8_t) * 0x2000);
	INTERNAL_WRAM_MIRROR = INTERNAL_WRAM;
	INTERNAL_OAM = malloc(sizeof(uint8_t) * 0xa0);
	INTERNAL_RESERVED = malloc(sizeof(uint8_t) * 0x60);
	INTERNAL_IO = malloc(sizeof(uint8_t) * 0x80+0x1); //INTMASK用に1byte余分に取る
	INTERNAL_STACK = malloc(sizeof(uint8_t) * 0x7f);
	INTERNAL_INTMASK = INTERNAL_IO+0x80;
}

void memory_write8(uint16_t dst, uint8_t value) {
	if(dst < V_CART_ROMN){
		//CART_ROM0
		cart_rom0_write8(cart, dst, value);
	}else if(dst < V_INTERNAL_VRAM){
		//CART_ROMN
		cart_romn_write8(cart, dst, value);
	}else if(dst < V_CART_RAMN){
		//INTERNAL_VRAM
		INTERNAL_VRAM[dst-V_INTERNAL_VRAM] = value;
	}else if(dst < V_INTERNAL_WRAM){
		//CART_RAMN
		cart_ramn_write8(cart, dst, value);
	}else if(dst < V_INTERNAL_WRAM_MIRROR){
		//INTERNAL_WRAM
		INTERNAL_WRAM[dst-V_INTERNAL_WRAM] = value;
	}else if(dst < V_INTERNAL_OAM){
		//INTERNAL_WRAM_MIRROR
		INTERNAL_WRAM_MIRROR[dst-V_INTERNAL_WRAM_MIRROR] = value;
	}else if(dst < V_INTERNAL_RESERVED){
		//INTERNAL_OAM
		INTERNAL_OAM[dst-V_INTERNAL_OAM] = value;
	}else if(dst < V_INTERNAL_IO){
		//INTERNAL_RESERVED
		INTERNAL_RESERVED[dst-V_INTERNAL_RESERVED] = value;
	}else if(dst < V_INTERNAL_STACK){
		//INTERNAL_IO
		INTERNAL_IO[dst-V_INTERNAL_IO] = value;
	}else if(dst < V_INTERNAL_INTMASK){
		//INTERNAL_STACK
		INTERNAL_STACK[dst-V_INTERNAL_STACK] = value;
	}else{
		//INTERNAL_INTMASK
		INTERNAL_INTMASK[0] = value;
	}
}

void memory_write16(uint16_t dst, uint16_t value) {
	memory_write8(dst, value&0xff);
	memory_write8(dst+1, value>>8);
}

uint8_t memory_read8(uint16_t src) {
	if(src < V_CART_ROMN){
		//CART_ROM0
		return cart_rom0_read8(cart, src);
	}else if(src < V_INTERNAL_VRAM){
		//CART_ROMN
		return cart_romn_read8(cart, src);
	}else if(src < V_CART_RAMN){
		//INTERNAL_VRAM
		return INTERNAL_VRAM[src-V_INTERNAL_VRAM];
	}else if(src < V_INTERNAL_WRAM){
		//CART_RAMN
		return cart_ramn_read8(cart, src);
	}else if(src < V_INTERNAL_WRAM_MIRROR){
		//INTERNAL_WRAM
		return INTERNAL_WRAM[src-V_INTERNAL_WRAM];
	}else if(src < V_INTERNAL_OAM){
		//INTERNAL_WRAM_MIRROR
		return INTERNAL_WRAM_MIRROR[src-V_INTERNAL_WRAM_MIRROR];
	}else if(src < V_INTERNAL_RESERVED){
		//INTERNAL_OAM
		return INTERNAL_OAM[src-V_INTERNAL_OAM];
	}else if(src < V_INTERNAL_IO){
		//INTERNAL_RESERVED
		return INTERNAL_RESERVED[src-V_INTERNAL_RESERVED];
	}else if(src < V_INTERNAL_STACK){
		//INTERNAL_IO
		return INTERNAL_IO[src-V_INTERNAL_IO];
	}else if(src < V_INTERNAL_INTMASK){
		//INTERNAL_STACK
		return INTERNAL_STACK[src-V_INTERNAL_STACK];
	}else{
		//INTERNAL_INTMASK
		return INTERNAL_INTMASK[0];
	}
}

uint16_t memory_read16(uint16_t src) {
	return memory_read8(src) | (memory_read8(src+1)<<8);
}
