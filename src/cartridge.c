#include "cartridge.h"
#include "memory.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


struct cartridge {
	uint8_t *rom;
	uint8_t *ram;
	struct gb_carthdr header;
	uint8_t ram_enabled;
	uint16_t banknum;
	uint8_t mbc1_mode;
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
	cart->ram_enabled = 1;
	cart->banknum = 0;
	cart->mbc1_mode = 0;
	memcpy(&(cart->header), rom+0x100, sizeof(struct gb_carthdr));

	if(cart->header.cgbflag == CGBFLAG_CGBONLY) {
		free(cart);
		printf("cart_init: cgb only\n");
		return NULL;
	}

	if(memcmp(cart->header.logo, VALID_LOGO, sizeof(VALID_LOGO))!=0) {
		//free(cart);
		printf("cart_init warning: header logo is invalid.\n");
		//return NULL;
	}

	cart->rom0 = cart->rom;

	switch(cart->header.carttype) {
	case CARTTYPE_ROMONLY:
		cart->romn = cart->rom + 0x4000;
		cart->ramn = NULL;
		break;
	case CARTTYPE_MBC1:
		cart->romn = cart->rom + 0x4000;
		cart->ramn = NULL;
		cart->mbc1_mode = 0;
		break;
	default:
		printf("cart_init: catridge type 0x%x is not supported.\n", cart->header.carttype);
		free(cart);
		return NULL;
	}

	return cart;
}

void cart_setram(struct cartridge *cart, uint8_t *ram) {
	cart->ram = ram;
}

struct gb_carthdr *cart_header(struct cartridge *cart) {
	return &(cart->header);
};

static void update_mapping(struct cartridge *cart) {
	switch(cart->header.carttype){
	case CARTTYPE_MBC1:
		if(cart->mbc1_mode==0){
			//ROM Banking Mode
			uint8_t romnum = cart->banknum&0x7f;
			if(romnum==0x0 || romnum==0x20 || romnum==0x40 || romnum==0x60)
				romnum++;
            cart->romn = cart->rom + (0x4000*romnum);
		}else{
			//RAM Banking Mode
			uint8_t romnum = cart->banknum&0x1f;
			if(romnum==0x0)
				romnum++;
			cart->romn = cart->rom + (0x4000*romnum);
		}
		break;
	}
}

void cart_rom0_write8(struct cartridge *cart, uint16_t dst, uint8_t value) {
	switch(cart->header.carttype) {
	case CARTTYPE_MBC1:
	case CARTTYPE_MBC1_RAM:
	case CARTTYPE_MBC1_RAM_BATT:
		if(dst <= 0x1fff){
			cart->ram_enabled = (value==0xa);
		}else{
			//Bank Numberの下位5ビット
			cart->banknum = (cart->banknum&0x60) | (value&0x1f);
			update_mapping(cart);
		}
		break;
	case CARTTYPE_MBC2:
	case CARTTYPE_MBC2_BATT:
		if(dst <= 0x1fff){
			if(!(dst>>8&0x1)) cart->ram_enabled = (value==0xa);
		}else{
			if(dst>>8&0x1){
				cart->banknum = value&0xf;
				update_mapping(cart);
			}
		}
		break;
	case CARTTYPE_MBC3_TIM_BATT:
	case CARTTYPE_MBC3_TIM_RAM_BATT:
	case CARTTYPE_MBC3:
	case CARTTYPE_MBC3_RAM:
	case CARTTYPE_MBC3_RAM_BATT:
		if(dst <= 0x1fff){
			cart->ram_enabled = (value==0xa);
		}else{
			cart->banknum = value&0x7f;
			update_mapping(cart);
		}
		break;
	case CARTTYPE_MBC5:
	case CARTTYPE_MBC5_RAM:
	case CARTTYPE_MBC5_RAM_BATT:
		if(dst <= 0x1fff){
			cart->ram_enabled = (value==0xa);
		}else{
			if(dst<=0x2fff)
				cart->banknum = (cart->banknum&0x100) | value;
			else
				cart->banknum = (cart->banknum&0xff) | (value<<8);
			update_mapping(cart);
		}
		break;
	}


}

void cart_romn_write8(struct cartridge *cart, uint16_t dst, uint8_t value) {

	switch(cart->header.carttype) {
	case CARTTYPE_MBC1:
	case CARTTYPE_MBC1_RAM:
	case CARTTYPE_MBC1_RAM_BATT:
		if(dst >= 0x6000){
			cart->mbc1_mode = value&0x1;
			update_mapping(cart);
		}else if(dst >= 0x4000){
			//Bank Numberの上位2ビット
			cart->banknum = (cart->banknum&0x1f) | (value<<5);
			update_mapping(cart);
		}
		break;
	case CARTTYPE_MBC2:
	case CARTTYPE_MBC2_BATT:
	case CARTTYPE_MBC3_TIM_BATT:
	case CARTTYPE_MBC3_TIM_RAM_BATT:
	case CARTTYPE_MBC3:
	case CARTTYPE_MBC3_RAM:
	case CARTTYPE_MBC3_RAM_BATT:
	case CARTTYPE_MBC5:
	case CARTTYPE_MBC5_RAM:
	case CARTTYPE_MBC5_RAM_BATT:
		break;
	}
}

void cart_ramn_write8(struct cartridge *cart, uint16_t dst, uint8_t value) {
	if(!cart->ram_enabled)
		return;

	switch(cart->header.carttype) {
	}
}

uint8_t cart_rom0_read8(struct cartridge *cart, uint16_t src) {
	switch(cart->header.carttype) {
	case CARTTYPE_ROMONLY:
	case CARTTYPE_MBC1:
		return cart->rom0[src - V_CART_ROM0];
	}

	return 0;
}

uint8_t cart_romn_read8(struct cartridge *cart, uint16_t src) {
	switch(cart->header.carttype) {
	case CARTTYPE_ROMONLY:
	case CARTTYPE_MBC1:
		return cart->romn[src - V_CART_ROMN];
	}

	return 0;
}

uint8_t cart_ramn_read8(struct cartridge *cart, uint16_t src) {
	switch(cart->header.carttype) {
	}

	return 0;
}
