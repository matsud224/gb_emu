#include <stdio.h>
#include <inttypes.h>

#include "cpu.h"
#include "memory.h"

#define SHOW_DISAS

#ifndef SHOW_DISAS
    #define DISAS_PRINT( fmt, ... ) ((void)0)
#else
    #define DISAS_PRINT( fmt, ... ) \
        fprintf( stderr, \
                  fmt "\n", \
                  ##__VA_ARGS__ \
        )
#endif

#define BIT7_6(v) (((v)>>6)&0x3)
#define BIT5_3(v) (((v)>>3)&0x7)
#define BIT2_0(v) ((v)&0x7)
#define BIT5_4(v) (((v)>>4)&0x3)
#define BIT3(v) (((v)>>3)&0x1)

static const char *r_name[] = {"B", "C", "D", "E", "H", "L", NULL, "A"};
static const char *dd_name[] = {"BC", "DE", "HL", "SP"};
static const char *qq_name[] = {"BC", "DE", "HL", "AF"};
static const char *ss_name[] = {"BC", "DE", "HL", "SP"};
static const char *cc_name[] = {"NZ", "Z", "NC", "C", "PO", "PE", "P", "M"};

union reg16 {
	uint16_t hl;
	struct {
		uint8_t h;
		uint8_t l;
	} v;
};

union reg16 reg_bc, reg_de, reg_hl, reg_sp, reg_pc;
uint32_t temp_val=0;
uint8_t reg_a;
int FLG_Z, FLG_N, FLG_H, FLG_C;
#define REG_B reg_bc.v.h
#define REG_C reg_bc.v.l
#define REG_D reg_de.v.h
#define REG_E reg_de.v.l
#define REG_H reg_hl.v.h
#define REG_L reg_hl.v.l
#define REG_A reg_a
#define REG_BC reg_bc.hl
#define REG_DE reg_de.hl
#define REG_HL reg_hl.hl
#define REG_PC reg_pc.hl
#define REG_SP reg_sp.hl

#define SETFLG_H(src) (FLG_H = (((src)^temp_val)&0x10)>>4)
#define SETFLG_Z (FLG_Z = !temp_val)
#define SETFLG_C8(n) (FLG_C = (n)?(!((temp_val & 0x100)>>8)):((temp_val & 0x100)>>8))
#define SETFLG_C16(n) (FLG_C = (n)?(!((temp_val & 0x10000)>>16)):((temp_val & 0x10000)>>16))

static uint8_t* const r_table[] = {&REG_B, &REG_C, &REG_D, &REG_E, &REG_H, &REG_L, NULL, &REG_A};
static uint16_t* const dd_table[] = {&REG_BC, &REG_DE, &REG_HL, &REG_SP};
static uint16_t* const qq_table[] = {&REG_BC, &REG_DE, &REG_HL};
static uint16_t* const ss_table[] = {&REG_BC, &REG_DE, &REG_HL, &REG_SP};
static uint8_t const p_table[] = {0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38};

int cpu_exec(uint8_t *code) {
	uint8_t cond;

	while(1){
		#ifdef SHOW_DISAS
		disas_one(code);
		#endif // SHOW_DISAS

		switch(*code){
		case 0x36:
			//LD (HL),n
			memory_write8(REG_HL, *(code+1));
			return 2;
		case 0x0a:
			//LD A,(BC)
			REG_A = memory_read8(REG_BC);
			return 1;
		case 0x1a:
			//LD A,(DE)
			REG_A = memory_read8(REG_DE);
			return 1;
		case 0xfa:
			//LD A,(nn)
			REG_A = memory_read8((*(code+2)<<8) & *(code+1));
			return 3;
		case 0x02:
			//LD (BC),A
			memory_write8(REG_BC, REG_A);
			return 1;
		case 0x12:
			//LD (DE),A
			memory_write8(REG_DE, REG_A);
			return 1;
		case 0x08:
			//LD (nn),SP
			memory_write16((*(code+2)<<8) & *(code+1), REG_SP);
			return 3;
		case 0xea:
			//LD (nn),A
			memory_write8((*(code+2)<<8) & *(code+1), REG_A);
			return 3;
		case 0xf0:
			//LD A,(FF00+n)
			REG_A = memory_read8(0xff00 + *(code+1));
			return 2;
		case 0xe0:
			//LD (FF00+n),A
			memory_write8(0xff00 + *(code+1), REG_A);
			return 2;
		case 0xf2:
			//LD A,(FF00+C)
			REG_A = memory_read8(0xff00 + REG_C);
			return 1;
		case 0xe2:
			//LD (FF00+C),A
			memory_write8(0xff00 + REG_C, REG_A);
			return 1;
		case 0x22:
			//LDI (HL),A
			memory_write8(REG_HL, REG_A);
			REG_HL++;
			return 1;
		case 0x2a:
			//LDI A,(HL)
			REG_A = memory_read8(REG_HL);
			REG_HL++;
			return 1;
		case 0x32:
			//LDD (HL),A
			memory_write8(REG_HL, REG_A);
			REG_HL--;
			return 1;
		case 0x3a:
			//LDD A,(HL)
			REG_A = memory_read8(REG_HL);
			REG_HL--;
			return 1;
		case 0xf9:
			//LD SP,HL
			REG_SP = REG_HL;
			return 1;
		case 0xc6:
			//ADD A,n
			temp_val = REG_A + *(code+1);
			SETFLG_C8(0); SETFLG_Z; FLG_N = 0; SETFLG_H(REG_A);
			REG_A = temp_val;
			return 2;
		case 0x86:
			//ADD A,(HL)
			temp_val = REG_A + memory_read8(REG_HL);
			SETFLG_C8(0); SETFLG_Z; FLG_N = 0; SETFLG_H(REG_A);
			REG_A = temp_val;
			return 1;
		case 0xce:
			//ADC A,n
			temp_val = REG_A + *(code+1) + FLG_C;
			SETFLG_C8(0); SETFLG_Z; FLG_N = 0; SETFLG_H(REG_A);
			REG_A = temp_val;
			return 2;
		case 0x8e:
			//ADC A,(HL)
			temp_val = REG_A + memory_read8(REG_HL) + FLG_C;
			SETFLG_C8(0); SETFLG_Z; FLG_N = 0; SETFLG_H(REG_A);
			REG_A = temp_val;
			return 1;
		case 0xd6:
			//SUB n
			temp_val = REG_A - *(code+1);
			SETFLG_C8(1); SETFLG_Z; FLG_N = 1; SETFLG_H(REG_A);
			REG_A = temp_val;
			return 2;
		case 0x96:
			//SUB (HL)
			temp_val = REG_A - memory_read8(REG_HL);
			SETFLG_C8(1); SETFLG_Z; FLG_N = 1; SETFLG_H(REG_A);
			REG_A = temp_val;
			return 1;
		case 0xde:
			//SBC A,n
			temp_val = REG_A - *(code+1) - FLG_C;
			SETFLG_C8(1); SETFLG_Z; FLG_N = 1; SETFLG_H(REG_A);
			REG_A = temp_val;
			return 2;
		case 0x9e:
			//SBC A,(HL)
			temp_val = REG_A - memory_read8(REG_HL) - FLG_C;
			SETFLG_C8(1); SETFLG_Z; FLG_N = 1; SETFLG_H(REG_A);
			REG_A = temp_val;
			return 1;
		case 0xe6:
			//AND n
			temp_val = REG_A & *(code+1);
			FLG_C = 0; SETFLG_Z; FLG_N = 0; FLG_H = 1;
			REG_A = temp_val;
			return 2;
		case 0xa6:
			//AND (HL)
			temp_val = REG_A & memory_read8(REG_HL);
			FLG_C = 0; SETFLG_Z; FLG_N = 0; FLG_H = 1;
			REG_A = temp_val;
			return 1;
		case 0xee:
			//XOR n
			temp_val = REG_A ^ *(code+1);
			FLG_C = 0; SETFLG_Z; FLG_N = 0; FLG_H = 0;
			REG_A = temp_val;
			return 2;
		case 0xae:
			//XOR (HL)
			temp_val = REG_A ^ memory_read8(REG_HL);
			FLG_C = 0; SETFLG_Z; FLG_N = 0; FLG_H = 0;
			REG_A = temp_val;
			return 1;
		case 0xf6:
			//OR n
			temp_val = REG_A | *(code+1);
			FLG_C = 0; SETFLG_Z; FLG_N = 0; FLG_H = 0;
			REG_A = temp_val;
			return 2;
		case 0xb6:
			//OR (HL)
			temp_val = REG_A | memory_read8(REG_HL);
			FLG_C = 0; SETFLG_Z; FLG_N = 0; FLG_H = 0;
			REG_A = temp_val;
			return 1;
		case 0xfe:
			//CP n
			temp_val = REG_A - *(code+1);
			SETFLG_C8(1); SETFLG_Z; FLG_N = 1; SETFLG_H(REG_A);
			return 2;
		case 0xbe:
			//CP (HL)
			temp_val = REG_A - memory_read8(REG_HL);
			SETFLG_C8(1); SETFLG_Z; FLG_N = 1; SETFLG_H(REG_A);
			return 1;
		case 0x34:
			//INC (HL)
			temp_val = memory_read8(REG_HL)+1;
			SETFLG_Z; FLG_N = 0; SETFLG_H(REG_A);
			memory_write8(REG_HL, temp_val);
			return 1;
		case 0x35:
			//DEC (HL)
			temp_val = memory_read8(REG_HL)-1;
			SETFLG_Z; FLG_N = 1; SETFLG_H(REG_A);
			memory_write8(REG_HL, temp_val);
			return 1;
		case 0x27:
			//DAA
			temp_val = REG_A;
			if(FLG_N){
				//減算
				if(FLG_H && (REG_A&0xf)>=0x6){
					if(FLG_C && (REG_A&0xf0)>=0x60){
						temp_val += 0x9a;
					}else{
						temp_val += 0xfa;
						FLG_C = 0;
					}
				}else if(FLG_C && (REG_A&0xf0)>=0x60){
					temp_val += 0xa0;
				}

			}else{
				//加算
				if(FLG_H || (REG_A&0xf)>=0xa)
					temp_val += 0x6;
				if((temp_val&0xf0)>=0xa0 || FLG_C || (temp_val && 0x100))
					temp_val += 0x60;
				SETFLG_C8(0);
			}
			SETFLG_Z;
			REG_A = temp_val;
			return 1;
		case 0x2f:
			//CPL
			temp_val = ~REG_A;
			FLG_N = 1; FLG_H = 1;
			REG_A = temp_val;
			return 1;
		case 0xe8:
			//ADD SP,dd
			temp_val = REG_SP + (int8_t)(*(code+1));
			SETFLG_C16(0); FLG_Z = 0; FLG_N = 0; SETFLG_H(REG_SP);
			REG_SP = temp_val;
			return 2;
		case 0xf8:
			//LD HL,SP+dd
			temp_val = REG_SP + (int8_t)(*(code+1));
			SETFLG_C16(0); FLG_Z = 0; FLG_N = 0; SETFLG_H(REG_SP);
			REG_HL = temp_val;
			return 2;
		case 0x07:
			//RLCA
			temp_val = REG_A << 1;
			SETFLG_C8(0); FLG_N = 0; FLG_H = 0; FLG_Z = 0;
			REG_A = temp_val + FLG_C;
			return 1;
		case 0x17:
			//RLA
			temp_val = (REG_A << 1) + FLG_C;
			SETFLG_C8(0); FLG_N = 0; FLG_H = 0; FLG_Z = 0;
			REG_A = temp_val;
			return 1;
		case 0x0f:
			//RRCA
			FLG_C = REG_A & 0x1;
			temp_val = REG_A >> 1;
			FLG_N = 0; FLG_H = 0; FLG_Z = 0;
			REG_A = temp_val | (FLG_C<<7);
			return 1;
		case 0x1f:
			//RRA
			temp_val = FLG_C; //一時的に退避
			FLG_C = REG_A & 0x1;
			temp_val = (REG_A >> 1) | (temp_val<<7);
			FLG_N = 0; FLG_H = 0; FLG_Z = 0;
			REG_A = temp_val;
			return 1;
		case 0xcb:

			code++;

			switch(*code){
			case 0x06:
				//RLC (HL)
				temp_val = memory_read8(REG_HL) << 1;
				SETFLG_Z; SETFLG_C8(0); FLG_N = 0; FLG_H = 0;
				memory_write8(REG_HL, temp_val + FLG_C);
				return 2;
			case 0x16:
				//RL (HL)
				temp_val = (memory_read8(REG_HL) << 1) + FLG_C;
				SETFLG_Z; SETFLG_C8(0); FLG_N = 0; FLG_H = 0;
				memory_write8(REG_HL, temp_val);
				return 2;
			case 0x0e:
				//RRC (HL)
				FLG_C = memory_read8(REG_HL) & 0x1;
				temp_val = memory_read8(REG_HL) >> 1;
				SETFLG_Z; FLG_N = 0; FLG_H = 0;
				memory_write8(REG_HL, temp_val | (FLG_C<<7));
				return 2;
			case 0x1e:
				//RR (HL)
				temp_val = FLG_C; //一時的に退避
				FLG_C = memory_read8(REG_HL) & 0x1;
				temp_val = (memory_read8(REG_HL) >> 1) | (temp_val<<7);
				SETFLG_Z; FLG_N = 0; FLG_H = 0;
				memory_write8(REG_HL, temp_val);
				return 2;
			case 0x26:
				//SLA (HL)
				temp_val = memory_read8(REG_HL) << 1;
				SETFLG_Z; SETFLG_C8(0); FLG_N = 0; FLG_H = 0;
				memory_write8(REG_HL, temp_val);
				return 2;
			case 0x36:
				//SWAP (HL)
				temp_val = ((memory_read8(REG_HL)&0xf)<<4) | ((memory_read8(REG_HL)&0xf0)>>4);
				SETFLG_Z; FLG_C = 0; FLG_N = 0; FLG_H = 0;
				memory_write8(REG_HL, temp_val);
				return 2;
			case 0x2e:
				//SRA (HL)
				FLG_C = memory_read8(REG_HL) & 0x1;
				temp_val = (memory_read8(REG_HL)&0x80) | (memory_read8(REG_HL) >> 1);
				SETFLG_Z; FLG_N = 0; FLG_H = 0;
				memory_write8(REG_HL, temp_val);
				return 2;
			case 0x3e:
				//SRL (HL)
				FLG_C = memory_read8(REG_HL) & 0x1;
				temp_val = memory_read8(REG_HL) >> 1;
				SETFLG_Z; FLG_N = 0; FLG_H = 0;
				memory_write8(REG_HL, temp_val);
				return 2;
			}


			switch(BIT7_6(*code)){
			case 0x0:
				switch(BIT5_3(*code)){
				case 0x0:
					//RLC r
					temp_val = *r_table[BIT2_0(*code)] << 1;
					SETFLG_Z; SETFLG_C8(0); FLG_N = 0; FLG_H = 0;
					*r_table[BIT2_0(*code)] = temp_val + FLG_C;
					return 2;
				case 0x1:
					//RRC r
					FLG_C = *r_table[BIT2_0(*code)] & 0x1;
					temp_val = *r_table[BIT2_0(*code)] >> 1;
					SETFLG_Z; FLG_N = 0; FLG_H = 0;
					*r_table[BIT2_0(*code)] = temp_val | (FLG_C<<7);
					return 2;
				case 0x2:
					//RL r
					temp_val = (*r_table[BIT2_0(*code)] << 1) + FLG_C;
					SETFLG_Z; SETFLG_C8(0); FLG_N = 0; FLG_H = 0;
					*r_table[BIT2_0(*code)] = temp_val;
					return 2;
				case 0x3:
					//RR r
					temp_val = FLG_C; //一時的に退避
					FLG_C = *r_table[BIT2_0(*code)] & 0x1;
					temp_val = (*r_table[BIT2_0(*code)] >> 1) | (temp_val<<7);
					SETFLG_Z; FLG_N = 0; FLG_H = 0;
					*r_table[BIT2_0(*code)] = temp_val;
					return 2;
				case 0x4:
					//SLA r
					temp_val = *r_table[BIT2_0(*code)] << 1;
					SETFLG_Z; SETFLG_C8(0); FLG_N = 0; FLG_H = 0;
					*r_table[BIT2_0(*code)] = temp_val;
					return 2;
				case 0x5:
					//SRA r
					FLG_C = *r_table[BIT2_0(*code)] & 0x1;
					temp_val = (*r_table[BIT2_0(*code)]&0x80) | (*r_table[BIT2_0(*code)] >> 1);
					SETFLG_Z; FLG_N = 0; FLG_H = 0;
					*r_table[BIT2_0(*code)] = temp_val;
					return 2;
				case 0x6:
					//SWAP r
					temp_val = ((*r_table[BIT2_0(*code)]&0xf)<<4) | ((*r_table[BIT2_0(*code)]&0xf0)>>4);
					SETFLG_Z; FLG_C = 0; FLG_N = 0; FLG_H = 0;
					*r_table[BIT2_0(*code)] = temp_val;
					return 2;
				case 0x7:
					//SRL r
					FLG_C = *r_table[BIT2_0(*code)] & 0x1;
					temp_val = *r_table[BIT2_0(*code)] >> 1;
					SETFLG_Z; FLG_N = 0; FLG_H = 0;
					*r_table[BIT2_0(*code)] = temp_val;
					return 2;
				}
				break;
			case 0x1:
				if(BIT2_0(*code)==0x6){
					//BIT b,(HL)
					FLG_Z = (memory_read8(REG_HL) & (0x1<<BIT2_0(*code))) == 0;
					FLG_N = 0; FLG_H = 1;
				}else{
					//BIT b,r
					FLG_Z = (*r_table[BIT2_0(*code)] & (0x1<<BIT2_0(*code))) == 0;
					FLG_N = 0; FLG_H = 1;
				}
				return 2;
			case 0x2:
				if(BIT2_0(*code)==0x6){
					//RES b,(HL)
					memory_write8(REG_HL, memory_read8(REG_HL) | (0x1<<BIT2_0(*code)));
				}else{
					//RES b,r
					*r_table[BIT2_0(*code)] = *r_table[BIT2_0(*code)] | (0x1<<BIT2_0(*code));
				}
				return 2;
			case 0x3:
				if(BIT2_0(*code)==0x6){
					//SET b,(HL)
					memory_write8(REG_HL, memory_read8(REG_HL) & ~(0x1<<BIT2_0(*code)));
				}else{
					//SET b,r
					*r_table[BIT2_0(*code)] = *r_table[BIT2_0(*code)] & ~(0x1<<BIT2_0(*code));
				}
				return 2;
			}
			break;
		case 0x3f:
			//CCF
			FLG_C = !FLG_C; FLG_N = 0; FLG_H = 0;
			return 1;
		case 0x37:
			//SCF
			FLG_C = 1; FLG_N = 0; FLG_H = 0;
			return 1;
		case 0x00:
			//NOP
			return 1;
		case 0x76:
			//HALT
			DISAS_PRINT("HALT");
			return 1;
		case 0x10:
			//STOP
			if(*(code+1)==0x0){
				DISAS_PRINT("STOP");
				return 2;
			}
			break;
		case 0xf3:
			//DI
			DISAS_PRINT("DI"); //ime=0
			return 1;
		case 0xfb:
			//EI
			DISAS_PRINT("EI"); //ime=1
			return 1;
		case 0xc3:
			//JP nn
			REG_PC = (*(code+2)<<8) & *(code+1);
			return 3;
		case 0xe9:
			//JP HL
			REG_PC = REG_HL;
			return 1;
		case 0x18:
			//JR PC+e
			REG_PC += (int8_t)(*(code+1));
			return 2;
		case 0xcd:
			//CALL nn
			REG_SP -= 2;
			memory_write16(REG_SP, REG_PC);
			REG_PC = (*(code+2)<<8) & *(code+1);
			return 3;
		case 0xc9:
			//RET
			REG_PC = memory_read16(REG_SP); REG_SP+=2;
			return 1;
		case 0xd9:
			//RETI
			DISAS_PRINT("RETI");
			return 1;
		}

		switch(BIT7_6(*code)){
		case 0x0:
			switch(BIT2_0(*code)){
			case 0x0:
				switch(BIT5_3(*code)){
				case 0x7:
					//JR C,e
					if(FLG_C)
						REG_PC += (int8_t)(*(code+1));
					return 2;
				case 0x6:
					//JR NC,e
					if(!FLG_C)
						REG_PC += (int8_t)(*(code+1));
					return 1;
				case 0x5:
					//JR Z,e
					if(FLG_Z)
						REG_PC += (int8_t)(*(code+1));
					return 1;
				case 0x4:
					//JR NZ,e
					if(!FLG_Z)
						REG_PC += (int8_t)(*(code+1));
					return 1;
				}
				break;
			case 0x1:
				if(BIT3(*code)){
					//ADD HL,ss
					temp_val = REG_HL + *ss_table[BIT5_4(*code)];
					SETFLG_H(REG_HL); SETFLG_C16(0); FLG_N = 0;
					REG_HL = temp_val;
					return 1;
				}else{
					//LD dd,nn
					*dd_table[BIT5_4(*code)] = (*(code+2)<<8) & *(code+1);
					return 2;
				}
				break;
			case 0x3:
				if(BIT3(*code)){
					//DEC ss
					(*ss_table[BIT5_4(*code)])--;
				}else{
					//INC ss
					(*ss_table[BIT5_4(*code)])++;
				}
				return 1;
				break;
			case 0x4:
				//INC r
				temp_val = *r_table[BIT5_3(*code)]+1;
				FLG_N = 0; SETFLG_Z; SETFLG_H(*r_table[BIT5_3(*code)]);
				*r_table[BIT5_3(*code)] = temp_val;
				return 1;
			case 0x5:
				//DEC r
				temp_val = *r_table[BIT5_3(*code)]-1;
				FLG_N = 1; SETFLG_Z; SETFLG_H(*r_table[BIT5_3(*code)]);
				*r_table[BIT5_3(*code)] = temp_val;
				return 1;
			case 0x6:
				//LD r,n
				*r_table[BIT5_3(*code)] = *(code+1);
				return 2;
			}
			break;
		case 0x1:
			if(BIT2_0(*code)==0x6)
				//LD r,(HL)
				*r_table[BIT5_3(*code)] = memory_read8(REG_HL);
			else if(BIT5_3(*code)==0x6)
				//LD (HL),r
				memory_write8(REG_HL, *r_table[BIT2_0(*code)]);
			else
				//LD r,r'
				*r_table[BIT5_3(*code)] = *r_table[BIT2_0(*code)];
			return 1;
		case 0x2:
			switch(BIT5_3(*code)){
			case 0x0:
				//ADD A,r
				temp_val = REG_A + *r_table[BIT2_0(*code)];
				SETFLG_C8(0); SETFLG_Z; FLG_N = 0; SETFLG_H(REG_A);
				REG_A = temp_val;
				return 1;
			case 0x1:
				//ADC A,r
				temp_val = REG_A + *r_table[BIT2_0(*code)] + FLG_C;
				SETFLG_C8(0); SETFLG_Z; FLG_N = 0; SETFLG_H(REG_A);
				REG_A = temp_val;
				return 1;
			case 0x2:
				//SUB A,r
				temp_val = REG_A - *r_table[BIT2_0(*code)];
				SETFLG_C8(1); SETFLG_Z; FLG_N = 1; SETFLG_H(REG_A);
				REG_A = temp_val;
				return 1;
			case 0x3:
				//SBC A,r
				temp_val = REG_A - *r_table[BIT2_0(*code)] - FLG_C;
				SETFLG_C8(1); SETFLG_Z; FLG_N = 1; SETFLG_H(REG_A);
				REG_A = temp_val;
				return 1;
			case 0x4:
				//AND A,r
				temp_val = REG_A & *r_table[BIT2_0(*code)];
				FLG_C = 0; SETFLG_Z; FLG_N = 0; FLG_H = 1;
				REG_A = temp_val;
				return 1;
			case 0x5:
				//XOR A,r
				temp_val = REG_A ^ *r_table[BIT2_0(*code)];
				FLG_C = 0; SETFLG_Z; FLG_N = 0; FLG_H = 0;
				REG_A = temp_val;
				return 1;
			case 0x6:
				//OR A,r
				temp_val = REG_A | *r_table[BIT2_0(*code)];
				FLG_C = 0; SETFLG_Z; FLG_N = 0; FLG_H = 0;
				REG_A = temp_val;
				return 1;
			case 0x7:
				//CP A,r
				temp_val = REG_A - *r_table[BIT2_0(*code)];
				SETFLG_C8(1); SETFLG_Z; FLG_N = 1; SETFLG_H(REG_A);
				return 1;
			}
			break;
		case 0x3:
			switch(BIT2_0(*code)){
			case 0x0:
				//RET cc
				switch(BIT5_3(*code)){
				case 0: cond = !FLG_Z; break;
				case 1: cond = FLG_Z; break;
				case 2: cond = !FLG_C; break;
				case 3: cond = FLG_C; break;
				}
				if(cond){
					REG_PC = memory_read16(REG_SP); REG_SP+=2;
				}
				return 1;
			case 0x1:
				//POP qq
				if(BIT5_4(*code) == 3){
					REG_A = memory_read8(REG_SP+1);
					uint8_t reg_f = memory_read8(REG_SP);
					FLG_C = (reg_f & 0x10) != 0;
					FLG_H = (reg_f & 0x20) != 0;
					FLG_N = (reg_f & 0x40) != 0;
					FLG_Z = (reg_f & 0x80) != 0;
				}else{
					*qq_table[BIT5_4(*code)] = memory_read16(REG_SP);
				}
				REG_SP += 2;
				return 1;
			case 0x2:
				//JP cc,nn
				switch(BIT5_3(*code)){
				case 0: cond = !FLG_Z; break;
				case 1: cond = FLG_Z; break;
				case 2: cond = !FLG_C; break;
				case 3: cond = FLG_C; break;
				}
				if(cond)
					REG_PC = (*(code+2)<<8) & *(code+1);
				return 3;
			case 0x4:
				//CALL cc,nn
				switch(BIT5_3(*code)){
				case 0: cond = !FLG_Z; break;
				case 1: cond = FLG_Z; break;
				case 2: cond = !FLG_C; break;
				case 3: cond = FLG_C; break;
				}
				if(cond){
					REG_SP -=2;
					memory_write16(REG_SP, REG_PC);
					REG_PC = (*(code+2)<<8) & *(code+1);
				}
				return 3;
			case 0x5:
				//PUSH qq
				REG_SP -= 2;
				memory_write16(REG_SP, *qq_table[BIT5_4(*code)]);
				return 1;
			case 0x7:
				//RST p
				REG_SP -= 2;
				memory_write16(REG_SP, REG_PC);
				REG_PC = p_table[BIT5_3(*code)];
				return 2;
			}
			break;
		}
	}

	return 0;
}



int disas_one(uint8_t *code) {
	switch(*code){
	case 0x36:
		//LD (HL),n
		DISAS_PRINT("LD (HL),%hhX", *(code+1));
		return 2;
	case 0x0a:
		//LD A,(BC)
		DISAS_PRINT("LD A,(BC)");
		return 1;
	case 0x1a:
		//LD A,(DE)
		DISAS_PRINT("LD A,(DE)");
		return 1;
	case 0xfa:
		//LD A,(nn)
		DISAS_PRINT("LD A,(%hhX%hhX)", *(code+2), *(code+1));
		return 3;
	case 0x02:
		//LD (BC),A
		DISAS_PRINT("LD (BC),A");
		return 1;
	case 0x12:
		//LD (DE),A
		DISAS_PRINT("LD (DE),A");
		return 1;
	case 0x08:
		//LD (nn),SP
		DISAS_PRINT("LD (%hhX%hhX),SP", *(code+2), *(code+1));
		return 3;
	case 0xea:
		//LD (nn),A
		DISAS_PRINT("LD (%hhX%hhX),A", *(code+2), *(code+1));
		return 3;
	case 0xf0:
		//LD A,(FF00+n)
		DISAS_PRINT("LD A,(FF00+%hhX)", *(code+1));
		return 2;
	case 0xe0:
		//LD (FF00+n),A
		DISAS_PRINT("LD (FF00+%hhX),A", *(code+1));
		return 2;
	case 0xf2:
		//LD A,(FF00+C)
		DISAS_PRINT("LD A,(FF00+C)");
		return 1;
	case 0xe2:
		//LD (FF00+C),A
		DISAS_PRINT("LD (FF00+C),A");
		return 1;
	case 0x22:
		//LDI (HL),A
		DISAS_PRINT("LDI (HL),A");
		return 1;
	case 0x2a:
		//LDI A,(HL)
		DISAS_PRINT("LDI A,(HL)");
		return 1;
	case 0x32:
		//LDD (HL),A
		DISAS_PRINT("LDD (HL),A");
		return 1;
	case 0x3a:
		//LDD A,(HL)
		DISAS_PRINT("LDD A,(HL)");
		return 1;
	case 0xf9:
		//LD SP,HL
		DISAS_PRINT("LD SP,HL");
		return 1;
	case 0xc6:
		//ADD A,n
		DISAS_PRINT("ADD A,%hhX", *(code+1));
		return 2;
	case 0x86:
		//ADD A,(HL)
		DISAS_PRINT("ADD A,(HL)");
		return 1;
	case 0xce:
		//ADC A,n
		DISAS_PRINT("ADC A,%hhX", *(code+1));
		return 2;
	case 0x8e:
		//ADC A,(HL)
		DISAS_PRINT("ADC A,(HL)");
		return 1;
	case 0xd6:
		//SUB n
		DISAS_PRINT("SUB %hhX", *(code+1));
		return 2;
	case 0x96:
		//SUB (HL)
		DISAS_PRINT("SUB (HL)");
		return 1;
	case 0xde:
		//SBC A,n
		DISAS_PRINT("SBC A,%hhX", *(code+1));
		return 2;
	case 0x9e:
		//SBC A,(HL)
		DISAS_PRINT("SBC A,(HL)");
		return 1;
	case 0xe6:
		//AND n
		DISAS_PRINT("AND %hhX", *(code+1));
		return 2;
	case 0xa6:
		//AND (HL)
		DISAS_PRINT("AND (HL)");
		return 1;
	case 0xee:
		//XOR n
		DISAS_PRINT("XOR %hhX", *(code+1));
		return 2;
	case 0xae:
		//XOR (HL)
		DISAS_PRINT("XOR (HL)");
		return 1;
	case 0xf6:
		//OR n
		DISAS_PRINT("OR %hhX", *(code+1));
		return 2;
	case 0xb6:
		//OR (HL)
		DISAS_PRINT("OR (HL)");
		return 1;
	case 0xfe:
		//CP n
		DISAS_PRINT("CP %hhX", *(code+1));
		return 2;
	case 0xbe:
		//CP (HL)
		DISAS_PRINT("CP (HL)");
		return 1;
	case 0x34:
		//INC (HL)
		DISAS_PRINT("INC (HL)");
		return 1;
	case 0x35:
		//DEC (HL)
		DISAS_PRINT("DEC (HL)");
		return 1;
	case 0x27:
		//DAA
		DISAS_PRINT("daa");
		return 1;
	case 0x2f:
		//CPL
		DISAS_PRINT("CPL");
		return 1;
	case 0xe8:
		//ADD SP,dd
		DISAS_PRINT("ADD SP,%hhX", *(code+1));
		return 2;
	case 0xf8:
		//LD HL,SP+dd
		DISAS_PRINT("LD HL,SP+%hhX", *(code+1));
		return 2;
	case 0x07:
        //RLCA
		DISAS_PRINT("RLCA");
		return 1;
	case 0x17:
		//RLA
		DISAS_PRINT("RLA");
		return 1;
	case 0x0f:
		//RRCA
		DISAS_PRINT("RRCA");
		return 1;
	case 0x1f:
		//RRA
		DISAS_PRINT("RRA");
		return 1;
	case 0xcb:

		code++;

		switch(*code){
		case 0x06:
			//RLC (HL)
			DISAS_PRINT("RLC (HL)");
			return 2;
		case 0x16:
			//RL (HL)
			DISAS_PRINT("RL (HL)");
			return 2;
		case 0x0e:
			//RRC (HL)
			DISAS_PRINT("RRC (HL)");
			return 2;
		case 0x1e:
			//RR (HL)
			DISAS_PRINT("RR (HL)");
			return 2;
		case 0x26:
			//SLA (HL)
			DISAS_PRINT("SLA (HL)");
			return 2;
		case 0x36:
			//SWAP (HL)
			DISAS_PRINT("SWAP (HL)");
			return 2;
		case 0x2e:
			//SRA (HL)
			DISAS_PRINT("SRA (HL)");
			return 2;
		case 0x3e:
			//SRL (HL)
			DISAS_PRINT("SRL (HL)");
			return 2;
		}

		switch(BIT7_6(*code)){
		case 0x0:
			switch(BIT5_3(*code)){
			case 0x0:
				//RLC r
				DISAS_PRINT("RLC %s", r_name[BIT2_0(*code)]);
				return 2;
			case 0x1:
				//RRC r
				DISAS_PRINT("RRC %s", r_name[BIT2_0(*code)]);
				return 2;
			case 0x2:
				//RL r
				DISAS_PRINT("RL %s", r_name[BIT2_0(*code)]);
				return 2;
			case 0x3:
				//RR r
				DISAS_PRINT("RR %s", r_name[BIT2_0(*code)]);
				return 2;
			case 0x4:
				//SLA r
				DISAS_PRINT("SLA %s", r_name[BIT2_0(*code)]);
				return 2;
			case 0x5:
				//SRA r
				DISAS_PRINT("SRA %s", r_name[BIT2_0(*code)]);
				return 2;
			case 0x6:
				//SWAP r
				DISAS_PRINT("SWAP %s", r_name[BIT2_0(*code)]);
				return 2;
			case 0x7:
				//SRL r
				DISAS_PRINT("SRL %s", r_name[BIT2_0(*code)]);
				return 2;
			}
			break;
		case 0x1:
			if(BIT2_0(*code)==0x6)
				//BIT b,(HL)
				DISAS_PRINT("BIT %d,(HL)", BIT5_3(*code));
			else
				//BIT b,r
				DISAS_PRINT("BIT %d,%s", BIT5_3(*code), r_name[BIT2_0(*code)]);
			return 2;
		case 0x2:
			if(BIT2_0(*code)==0x6)
				//RES b,(HL)
				DISAS_PRINT("RES %d,(HL)", BIT5_3(*code));
			else
				//RES b,r
				DISAS_PRINT("RES %d,%s", BIT5_3(*code), r_name[BIT2_0(*code)]);
			return 2;
		case 0x3:
			if(BIT2_0(*code)==0x6)
				//SET b,(HL)
				DISAS_PRINT("SET %d,(HL)", BIT5_3(*code));
			else
				//SET b,r
				DISAS_PRINT("SET %d,%s", BIT5_3(*code), r_name[BIT2_0(*code)]);
			return 2;
		}
		break;
	case 0x3f:
		//CCF
		DISAS_PRINT("CCF");
		return 1;
	case 0x37:
		//SCF
		DISAS_PRINT("SCF");
		return 1;
	case 0x00:
		//NOP
		DISAS_PRINT("NOP");
		return 1;
	case 0x76:
		//HALT
		DISAS_PRINT("HALT");
		return 1;
	case 0x10:
		//STOP
		if(*(code+1)==0x0){
			DISAS_PRINT("STOP");
			return 2;
		}
		break;
	case 0xf3:
		//DI
		DISAS_PRINT("DI");
		return 1;
	case 0xfb:
		//EI
		DISAS_PRINT("EI");
		return 1;
	case 0xc3:
		//JP nn
		DISAS_PRINT("JP %hhX%hhX", *(code+2), *(code+1));
		return 3;
	case 0xe9:
		//JP HL
		DISAS_PRINT("JP HL");
		return 1;
	case 0x18:
		//JR PC+e
		DISAS_PRINT("JR PC+%hhX", *(code+1));
		return 2;
	case 0xcd:
		//CALL nn
		DISAS_PRINT("CALL %hhX%hhX", *(code+2), *(code+1));
		return 3;
	case 0xc9:
		//RET
		DISAS_PRINT("RET");
		return 1;
	case 0xd9:
		//RETI
		DISAS_PRINT("RETI");
		return 1;
	}

	switch(BIT7_6(*code)){
	case 0x0:
		switch(BIT2_0(*code)){
		case 0x0:
			switch(BIT5_3(*code)){
			case 0x7:
				//JR C,e
				DISAS_PRINT("JR C,%hhX", *(code+1)+2);
				return 2;
			case 0x6:
				//JR NC,e
				DISAS_PRINT("JR NC,%hhX", *(code+1)+2);
				return 1;
			case 0x5:
				//JR Z,e
				DISAS_PRINT("JR Z,%hhX", *(code+1)+2);
				return 1;
			case 0x4:
				//JR NZ,e
				DISAS_PRINT("JR NZ,%hhX", *(code+1)+2);
				return 1;
			}
			break;
		case 0x1:
			if(BIT3(*code)){
				//ADD HL,ss
				DISAS_PRINT("ADD HL,%s", ss_name[BIT5_4(*code)]);
				return 1;
			}else{
				//LD dd,nn
				DISAS_PRINT("LD %s,%hX", dd_name[BIT5_4(*code)], *(code+1));
				return 2;
			}
			break;
		case 0x3:
			if(BIT3(*code))
				//DEC ss
				DISAS_PRINT("DEC %s", ss_name[BIT5_4(*code)]);
			else
				//INC ss
				DISAS_PRINT("INC %s", ss_name[BIT5_4(*code)]);
			return 1;
			break;
		case 0x4:
			//INC r
			DISAS_PRINT("INC %s", r_name[BIT5_3(*code)]);
			return 1;
		case 0x5:
			//DEC r
			DISAS_PRINT("DEC %s", r_name[BIT5_3(*code)]);
			return 1;
		case 0x6:
			//LD r,n
			DISAS_PRINT("LD %s,%hhX", r_name[BIT5_3(*code)], *(code+1));
			return 2;
		}
		break;
	case 0x1:
		if(BIT2_0(*code)==0x6)
			//LD r,(HL)
			DISAS_PRINT("LD %s,(HL)", r_name[BIT5_3(*code)]);
		else if(BIT5_3(*code)==0x6)
			//LD (HL),r
			DISAS_PRINT("LD (HL),%s", r_name[BIT2_0(*code)]);
		else
			//LD r,r'
			DISAS_PRINT("LD %s,%s", r_name[BIT5_3(*code)], r_name[BIT2_0(*code)]);
		return 1;
	case 0x2:
		switch(BIT5_3(*code)){
		case 0x0:
			//ADD A,r
			DISAS_PRINT("ADD A,%s", r_name[BIT2_0(*code)]);
			return 1;
		case 0x1:
			//ADC A,r
			DISAS_PRINT("ADC A,%s", r_name[BIT2_0(*code)]);
			return 1;
		case 0x2:
			//SUB A,r
			DISAS_PRINT("SUB %s", r_name[BIT2_0(*code)]);
			return 1;
		case 0x3:
			//SBC A,r
			DISAS_PRINT("SBC A,%s", r_name[BIT2_0(*code)]);
			return 1;
		case 0x4:
			//AND A,r
			DISAS_PRINT("AND %s", r_name[BIT2_0(*code)]);
			return 1;
		case 0x5:
			//XOR A,r
			DISAS_PRINT("XOR %s", r_name[BIT2_0(*code)]);
			return 1;
		case 0x6:
			//OR A,r
			DISAS_PRINT("OR %s", r_name[BIT2_0(*code)]);
			return 1;
		case 0x7:
			//CP A,r
			DISAS_PRINT("CP %s", r_name[BIT2_0(*code)]);
			return 1;
		}
		break;
	case 0x3:
		switch(BIT2_0(*code)){
		case 0x0:
			//RET cc
			DISAS_PRINT("RET %s", cc_name[BIT5_3(*code)]);
			return 1;
		case 0x1:
			//POP qq
			DISAS_PRINT("POP %s", qq_name[BIT5_4(*code)]);
			return 1;
		case 0x2:
			//JP cc,nn
			DISAS_PRINT("JP %s,%hhX%hhX", cc_name[BIT5_3(*code)], *(code+2), *(code+1));
			return 3;
		case 0x4:
			//CALL cc,nn
			DISAS_PRINT("CALL %s,%hhX%hhX", cc_name[BIT5_3(*code)], *(code+2), *(code+1));
			return 3;
		case 0x5:
			//PUSH qq
			DISAS_PRINT("PUSH %s", qq_name[BIT5_4(*code)]);
			return 1;
		case 0x7:
			//RST p
			DISAS_PRINT("RST %hhX", p_table[BIT5_3(*code)]);
			return 2;
		}
		break;
	}

	return 0;
}



