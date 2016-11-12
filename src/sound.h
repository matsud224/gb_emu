#pragma once

#include <inttypes.h>

void sound_init(void);
void sound_ch1_writereg(uint16_t ioreg, uint8_t value);
void sound_ch2_writereg(uint16_t ioreg, uint8_t value);
void sound_ch3_writereg(uint16_t ioreg, uint8_t value);
void sound_ch4_writereg(uint16_t ioreg, uint8_t value);
void sound_master_writereg(uint16_t ioreg, uint8_t value);
uint8_t sound_ch1_readreg(uint16_t ioreg);
uint8_t sound_ch2_readreg(uint16_t ioreg);
uint8_t sound_ch3_readreg(uint16_t ioreg);
uint8_t sound_ch4_readreg(uint16_t ioreg);
uint8_t sound_master_readreg(uint16_t ioreg);
