#include "cartridge.h"
#include "memory.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct cartridge {
	uint8_t *rom;
	struct gb_carthdr header;
	enum {
		MBC1_16_8, MBC1_4_32
	} mbc1_mode;
	int ram_protected;
	uint8_t *rom0;
	uint8_t *romn;
	uint8_t *ramn;
};

const uint8_t VALID_LOGO[] = {
	0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
	0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
	0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
};

struct cartridge *cart_init(uint8_t *rom) {
	struct cartridge *cart = malloc(sizeof(struct cartridge));
	cart->rom = rom;
	cart->ram_protected = 0;
	memcpy(&(cart->header), rom+0x100, sizeof(struct gb_carthdr));

	if(cart->header.cgbflag == CGBFLAG_CGBONLY) {
		free(cart);
		fprintf(stderr, "cart_init: cgb only");
		return NULL;
	}

	if(memcmp(cart->header.logo, VALID_LOGO, sizeof(VALID_LOGO))!=0) {
		free(cart);
		fprintf(stderr, "cart_init: header logo is invalid.");
		return NULL;
	}

	cart->rom0 = cart->rom;

	switch(cart->header.carttype) {
	case CARTTYPE_ROMONLY:
		cart->romn = cart->rom + 0x4000;
		cart->ramn = NULL;
		break;
	default:
		free(cart);
		fprintf(stderr, "cart_init: catridge type 0x%x is not supported.", cart->header.carttype);
		return NULL;
	}

	return cart;
};

struct gb_carthdr *cart_header(struct cartridge *cart) {
	return &(cart->header);
};


void cart_rom0_write8(struct cartridge *cart, uint16_t dst, uint8_t value) {
	if(dst <= 0xbfff){
		//RAM書き込み保護の切り替え
		cart->ram_protected = (value==0xa);
	}
}

void cart_romn_write8(struct cartridge *cart, uint16_t dst, uint8_t value) {
	switch(cart->header.carttype) {
	case CARTTYPE_ROMONLY:
		break;
	}
}

void cart_ramn_write8(struct cartridge *cart, uint16_t dst, uint8_t value) {
	if(cart->ram_protected)
		return;

	switch(cart->header.carttype) {
	case CARTTYPE_ROMONLY:
		break;
	}
}

uint8_t cart_rom0_read8(struct cartridge *cart, uint16_t src) {
	switch(cart->header.carttype) {
	case CARTTYPE_ROMONLY:
		return cart->rom0[src - V_CART_ROM0];
	}

	return 0;
}

uint8_t cart_romn_read8(struct cartridge *cart, uint16_t src) {
	switch(cart->header.carttype) {
	case CARTTYPE_ROMONLY:
		return cart->romn[src - V_CART_ROMN];
	}

	return 0;
}

uint8_t cart_ramn_read8(struct cartridge *cart, uint16_t src) {
	switch(cart->header.carttype) {

	}

	return 0;
}
