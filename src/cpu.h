#pragma once

void startup(void);
void cpu_request_interrupt(uint8_t type);
void cpu_exec(int cycles);
int cpu_disas_one(uint16_t pc);

#define INT_VBLANK 0x1
#define INT_LCDSTAT 0x2
#define INT_TIMER 0x4
#define INT_SERIAL 0x8
#define INT_JOYPAD 0x10
