#include "memory.h"

void memory_init() {

}

void memory_write8(uint16_t dst, uint8_t value) {

}

void memory_write16(uint16_t dst, uint16_t value) {
	memory_write8(dst, value&0xff);
	memory_write8(dst+1, value>>8);
}

uint8_t memory_read8(uint16_t src) {

}

uint16_t memory_read16(uint16_t src) {
	return memory_read8(src) | (memory_read8(src+1)<<8);
}
