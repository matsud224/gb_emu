#pragma once

#include <inttypes.h>

struct gb_cartridge {
	uint8_t entrypoint[3];
	uint8_t logo[0x30];
	char title[0x10];
	uint8_t mcode[0x4];
	uint8_t cgbflag;
	uint8_t nlcode[2];
	uint8_t sgbflag;
	uint8_t carttype;
	uint8_t romsize;
	uint8_t ramsize;
	uint8_t dstcode;
	uint8_t olcode;
	uint8_t romver;
	uint8_t hdrchksum;
	uint16_t glbchksum;
};

#define CGBFLAG_BOTH		0x80
#define CGBFLAG_CGBONLY		0xc0

#define SGBFLAG_NOT_SUPPORT	0x00
#define SGBFLAG_SUPPORT		0x03

#define CARTTYPE_ROMONLY			0x00
#define CARTTYPE_MBC1				0x01
#define CARTTYPE_MBC1_RAM			0x02
#define CARTTYPE_MBC1_RAM_BATT		0x03
#define CARTTYPE_MBC2				0x05
#define CARTTYPE_MBC2_BATT			0x06
#define CARTTYPE_ROM_RAM			0x08
#define CARTTYPE_ROM_RAM_BATT		0x09
#define CARTTYPE_MMM01				0x0b
#define CARTTYPE_MMM01_RAM			0x0c
#define CARTTYPE_MMM01_RAM_BATT		0x0d
#define CARTTYPE_MBC3_TIM_BATT		0x0f
#define CARTTYPE_MBC3_TIM_RAM_BATT	0x10
#define CARTTYPE_MBC3				0x11
#define CARTTYPE_MBC3_RAM			0x12
#define CARTTYPE_MBC3_RAM_BATT		0x13
#define CARTTYPE_MBC4				0x15
#define CARTTYPE_MBC4_RAM			0x16
#define CARTTYPE_MBC4_RAM_BATT		0x17
#define CARTTYPE_MBC5				0x19
#define CARTTYPE_MBC5_RAM			0x1a
#define CARTTYPE_MBC5_RAM_BATT		0x1b
#define CARTTYPE_MBC5_RUMBLE		0x1c
#define CARTTYPE_MBC5_RUMBLE_RAM	0x1d
#define CARTTYPE_MBC5_RUMBLE_RAM_BATT	0x1e
#define CARTTYPE_MBC6				0x20
#define CARTTYPE_MBC7_SENSOR_RUMBLE_RAM_BATT	0x22
#define CARTTYPE_POCKETCAMERA		0xfc
#define CARTTYPE_BANDAITAMA			0xfd
#define CARTTYPE_HUC3				0xfe
#define CARTTYPE_HUC1_RAM_BATT		0xff

#define ROMSIZE_32K		0x00
#define ROMSIZE_64K		0x01
#define ROMSIZE_128K	0x02
#define ROMSIZE_256K	0x03
#define ROMSIZE_512K	0x04
#define ROMSIZE_1M		0x05
#define ROMSIZE_2M		0x06
#define ROMSIZE_4M		0x07
#define ROMSIZE_8M		0x08
#define ROMSIZE_1_1M	0x52
#define ROMSIZE_1_2M	0x53
#define ROMSIZE_1_5M	0x54

#define RAMSIZE_NONE	0x00
#define RAMSIZE_2K		0x01
#define RAMSIZE_8K		0x02
#define RAMSIZE_32K		0x03
#define RAMSIZE_128K	0x04
#define RAMSIZE_64K		0x05

#define DSTCODE_JAPANESE		0x00
#define DSTCODE_NON_JAPANESE	0x01
