#include "cartridge.h"
#include "memory.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void update_mapping(struct cartridge *cart);

struct cartridge {
	uint8_t *rom;
	uint8_t *ram;
	time_t ram_time;
	struct gb_carthdr header;
	uint8_t ram_enabled;
	uint16_t rom_banknum;
	uint8_t ram_banknum;
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
	case CARTTYPE_MBC1_RAM:
	case CARTTYPE_MBC1_RAM_BATT:
		cart->rom_banknum = 0x1;
		cart->mbc1_mode = 0;
		break;
	case CARTTYPE_MBC2:
	case CARTTYPE_MBC2_BATT:
		cart->rom_banknum = 0x1;
		break;
	case CARTTYPE_MBC3_TIM_BATT:
	case CARTTYPE_MBC3_TIM_RAM_BATT:
	case CARTTYPE_MBC3:
	case CARTTYPE_MBC3_RAM:
	case CARTTYPE_MBC3_RAM_BATT:
	case CARTTYPE_MBC5:
	case CARTTYPE_MBC5_RAM:
	case CARTTYPE_MBC5_RAM_BATT:
		cart->rom_banknum = 0x1;
		cart->ram_banknum = 0x0;
		break;
	default:
		printf("cart_init: catridge type 0x%x is not supported.\n", cart->header.carttype);
		free(cart);
		return NULL;
	}

	update_mapping(cart);

	return cart;
}

void cart_setram(struct cartridge *cart, uint8_t *ram, time_t t) {
	cart->ram = ram;
	cart->ram_time = t;
	update_mapping(cart);
}

struct gb_carthdr *cart_header(struct cartridge *cart) {
	return &(cart->header);
};

int get_romsize(int n) {
	if(n <= 0x8)
		return 32768<<n;
	else if(n==0x52)
		return 32768*36;
	else if(n==0x53)
		return 32768*40;
	else if(n==0x54)
		return 32768*46;
	else
		return 0;
}

static void update_mapping(struct cartridge *cart) {
	//ROM
	switch(cart->header.carttype) {
	case CARTTYPE_MBC1:
	case CARTTYPE_MBC1_RAM:
	case CARTTYPE_MBC1_RAM_BATT:
		if(cart->mbc1_mode==0){
			//ROM Banking Mode
			uint8_t romnum = cart->rom_banknum&0x7f;
			if(romnum==0x0 || romnum==0x20 || romnum==0x40 || romnum==0x60)
				romnum++;
			romnum %= get_romsize(cart->header.romsize)/0x4000;
            cart->romn = cart->rom + (0x4000*romnum);
            //printf("MBC1: rom#%d\n", romnum);
		}else{
			//RAM Banking Mode
			uint8_t romnum = cart->rom_banknum&0x1f;
			if(romnum==0x0)
				romnum++;
			romnum %= get_romsize(cart->header.romsize)/0x4000;
			cart->romn = cart->rom + (0x4000*romnum);
			//printf("MBC1: rom#%d\n", romnum);
		}
		break;
	case CARTTYPE_MBC2:
	case CARTTYPE_MBC2_BATT:
	case CARTTYPE_MBC3_TIM_BATT:
	case CARTTYPE_MBC3_TIM_RAM_BATT:
	case CARTTYPE_MBC3:
	case CARTTYPE_MBC3_RAM:
	case CARTTYPE_MBC3_RAM_BATT:
		if(cart->rom_banknum==0x0)
			cart->rom_banknum++;
		cart->romn = cart->rom + (0x4000*cart->rom_banknum);
		//printf("MBC2,3: rom#%d\n", cart->rom_banknum);
		break;
	case CARTTYPE_MBC5:
	case CARTTYPE_MBC5_RAM:
	case CARTTYPE_MBC5_RAM_BATT:
		cart->romn = cart->rom + (0x4000*cart->rom_banknum);
		cart->rom_banknum &= 0x1f; //上位2bitをクリア（bgbではそうしてるみたい）
		//printf("MBC5: rom#%d\n", cart->rom_banknum);
		break;
	}

	if(cart->romn >= cart->rom+get_romsize(cart->header.romsize)){
		//printf("warn: rom#0");
		cart->romn = cart->rom;
	}

	//RAM
	switch(cart->header.carttype) {
	case CARTTYPE_MBC1_RAM:
	case CARTTYPE_MBC1_RAM_BATT:
		if(cart->mbc1_mode==1){
			//RAM Banking Mode
			uint8_t ramnum = cart->rom_banknum>>5;
			cart->ramn = cart->ram + (0x2000*ramnum);
			//printf("MBC1: ram#%d\n", ramnum);
		}else{
			cart->ramn = cart->ram;
			//printf("MBC1: ram#%d\n", 0);
		}
		break;
	case CARTTYPE_MBC3_TIM_RAM_BATT:
	case CARTTYPE_MBC3_RAM:
	case CARTTYPE_MBC3_RAM_BATT:
		if(cart->ram_banknum<=3)
			cart->ramn = cart->ram + (0x2000*cart->ram_banknum);
		else
			cart->ramn = NULL;
		//printf("MBC3: ram#%d\n", cart->ram_banknum);
		break;
	case CARTTYPE_MBC5_RAM:
	case CARTTYPE_MBC5_RAM_BATT:
		cart->ramn = cart->ram + (0x2000*cart->ram_banknum);
		//printf("MBC5: ram#%d\n", cart->ram_banknum);
		break;
	}

	static const int ramsize_table[] = {0,2048,8192,8192*4,8192*16,8192*8};
	if(cart->ramn >= cart->ram+ramsize_table[cart->header.ramsize]){
		cart->ramn = cart->ram;
	}
}

void cart_rom0_write8(struct cartridge *cart, uint16_t dst, uint8_t value) {
	switch(cart->header.carttype) {
	case CARTTYPE_MBC1:
	case CARTTYPE_MBC1_RAM:
	case CARTTYPE_MBC1_RAM_BATT:
		//printf("%X <- %X\n", dst, value);
		if(dst <= 0x1fff){
			cart->ram_enabled = (value==0xa);
		}else{
			//Bank Numberの下位5ビット
			cart->rom_banknum = (cart->rom_banknum&0x60) | (value&0x1f);
			update_mapping(cart);
		}
		break;
	case CARTTYPE_MBC2:
	case CARTTYPE_MBC2_BATT:
		if(dst <= 0x1fff){
			if(!(dst>>8&0x1)) cart->ram_enabled = (value==0xa);
		}else{
			if(dst>>8&0x1){
				cart->rom_banknum = value&0xf;
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
			cart->rom_banknum = value&0x7f;
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
				cart->rom_banknum = (cart->rom_banknum&0x100) | value;
			else
				cart->rom_banknum = (cart->rom_banknum&0xff) | (value<<8);
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
		//printf("%X <- %X\n", dst, value);
		if(dst <= 0x5fff){
			//Bank Numberの上位2ビット
			cart->rom_banknum = (cart->rom_banknum&0x1f) | (value<<5);
			update_mapping(cart);
		}else{
			cart->mbc1_mode = value&0x1;
			cart->rom_banknum &= 0x1f; //上位2bitをクリア（bgbではそうしてるみたい）
			update_mapping(cart);
		}
		break;
	case CARTTYPE_MBC3_TIM_BATT:
	case CARTTYPE_MBC3_TIM_RAM_BATT:
	case CARTTYPE_MBC3:
	case CARTTYPE_MBC3_RAM:
	case CARTTYPE_MBC3_RAM_BATT:
		if(dst <= 0x5fff){
			cart->ram_banknum = value&0xf;
			update_mapping(cart);
		}else{
			//latch clock data
		}
		break;
	case CARTTYPE_MBC5:
	case CARTTYPE_MBC5_RAM:
	case CARTTYPE_MBC5_RAM_BATT:
		if(dst <= 0x5fff){
			cart->ram_banknum = value&0xf;
			update_mapping(cart);
		}
		break;
	}
}

void cart_ramn_write8(struct cartridge *cart, uint16_t dst, uint8_t value) {
	if(!cart->ram_enabled)
		return;

	if(cart->header.ramsize==0)
		return;

	switch(cart->header.carttype){
	case CARTTYPE_MBC2:
	case CARTTYPE_MBC2_BATT:
		cart->ramn[dst - V_CART_RAMN] = value&0xf;
		break;
	case CARTTYPE_MBC3_TIM_BATT:
	case CARTTYPE_MBC3_TIM_RAM_BATT:
		if(!(cart->ram_banknum<=3))
			break;
	default:
		cart->ramn[dst - V_CART_RAMN] = value;
		break;
	}
}

uint8_t cart_rom0_read8(struct cartridge *cart, uint16_t src) {
	return cart->rom0[src - V_CART_ROM0];
}

uint8_t cart_romn_read8(struct cartridge *cart, uint16_t src) {
	//printf("rom read\n");
	return cart->romn[src - V_CART_ROMN];
}

uint8_t cart_ramn_read8(struct cartridge *cart, uint16_t src) {
	switch(cart->header.carttype){
	case CARTTYPE_MBC3_TIM_BATT:
	case CARTTYPE_MBC3_TIM_RAM_BATT:
		{
			time_t t = time(NULL);
			struct tm *local = localtime(&t);
			int daydiff = (cart->ram_time-t)/60*60*24;
			switch(cart->ram_banknum){
			case 0x8:
				return local->tm_sec;
			case 0x9:
				return local->tm_min;
			case 0xa:
				return local->tm_hour;
			case 0xb:
				return daydiff>=0 ? daydiff&0xff : 0;
			case 0xc:
				if(daydiff<0)
					return 0;
				else
					return ((daydiff&0x100)!=0) | (((daydiff&~0x1ff)!=0)<<7);
			}
		}
	default:
		return cart->ramn[src - V_CART_RAMN];
	}
}
