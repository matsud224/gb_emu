#pragma once

#include <inttypes.h>

#define V_CART_ROM0 		0x0000
#define	V_CART_ROMN			0x4000
#define V_INTERNAL_VRAM			0x8000
#define	V_CART_RAMN			0xa000
#define V_INTERNAL_WRAM			0xc000
#define V_INTERNAL_WRAM_MIRROR	0xe000
#define V_INTERNAL_OAM			0xfe00
#define V_INTERNAL_RESERVED		0xfea0
#define V_INTERNAL_IO			0xff00
#define V_INTERNAL_STACK		0xff80
#define V_INTERNAL_INTMASK		0xffff

#define IO_P1 0xFF00
#define IO_SB 0xFF01
#define IO_SC 0xFF02
#define IO_DIV 0xFF04
#define IO_TIMA 0xFF05
#define IO_TMA 0xFF06
#define IO_TAC 0xFF07
#define IO_IF 0xFF0F
#define IO_NR10 0xFF10
#define IO_NR11 0xFF11
#define IO_NR12 0xFF12
#define IO_NR13 0xFF13
#define IO_NR14 0xFF14
#define IO_NR21 0xFF16
#define IO_NR22 0xFF17
#define IO_NR23 0xFF18
#define IO_NR24 0xFF19
#define IO_NR30 0xFF1A
#define IO_NR31 0xFF1B
#define IO_NR32 0xFF1C
#define IO_NR33 0xFF1D
#define IO_NR34 0xFF1E
#define IO_NR41 0xFF20
#define IO_NR42 0xFF21
#define IO_NR43 0xFF22
#define IO_NR44 0xFF23
#define IO_NR50 0xFF24
#define IO_NR51 0xFF25
#define IO_NR52 0xFF26
#define IO_WAVERAM_BEGIN 0xFF30h
#define IO_LCDC 0xFF40h
#define IO_STAT 0xFF41
#define IO_SCY 0xFF42
#define IO_SCX 0xFF43
#define IO_LY 0xFF44
#define IO_LYC 0xFF45
#define IO_DMA 0xFF46
#define IO_BGP 0xFF47
#define IO_OBP0 0xFF48
#define IO_OBP1 0xFF49
#define IO_WY 0xFF4A
#define IO_WX 0xFF4B
#define IO_BVBK 0xFF50
#define IO_IE 0xFFFF

struct cartridge;

void memory_init(struct cartridge *c);
void memory_write8(uint16_t dst, uint8_t value);
void memory_write16(uint16_t dst, uint16_t value);
uint8_t memory_read8(uint16_t src);
uint16_t memory_read16(uint16_t src);
