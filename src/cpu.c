#include <stdio.h>
#include <inttypes.h>

#include "cpu.h"
#include "memory.h"

#include "SDL2/SDL_atomic.h"
#include "SDL2/SDL_mutex.h"

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
static const char *cc_name[] = {"NZ", "Z", "NC", "C"};
static uint8_t const p_table[] = {0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38};

#define R_B 0
#define R_C 1
#define R_D 2
#define R_E 3
#define R_H 4
#define R_L 5
#define R_A 7

#define DD_BC 0
#define DD_DE 1
#define DD_HL 2
#define DD_SP 3

#define SS_BC 0
#define SS_DE 1
#define SS_HL 2
#define SS_SP 3

#define QQ_BC 0
#define QQ_DE 1
#define QQ_HL 2
#define QQ_AF 3

#define CC_NZ 0
#define DD_Z  1
#define DD_NC 2
#define DD_C  3


union reg16 {
	uint16_t hl;
	struct {
		uint8_t h;
		uint8_t l;
	} v;
};

union reg16 reg_bc, reg_de, reg_hl;
uint16_t reg_pc, reg_sp;
uint32_t cr; //演算結果
uint8_t reg_a;
uint32_t FLG_Z, FLG_N, FLG_H, FLG_C, FLG_IME;
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
#define REG_PC reg_pc
#define REG_SP reg_sp

#define OPERAND8 (memory_read8(REG_PC+1))
#define OPERAND16 (((memory_read8(REG_PC+2))<<8) & (memory_read8(REG_PC+1)))
#define FLG_C_01 (!!FLG_C)

#define SETZ (FLG_Z=!(cr&0xff))
#define SETH_CARRY(src) (FLG_H=(((src)&0xf)+(cr&0xf))&0x10)
#define SETH_BORROW(src) (FLG_H=!((((src)&0xf)+(cr&0xf))&0x10))
#define SETC_CARRY (FLG_C=(cr&0x1000))
#define SETC_BORROW (FLG_C=!(cr&0x1000))

#define BINOPA_ADD(v) (cr=REG_A + (v), FLG_N=0, SETZ, SETH_CARRY(REG_A), SETC_CARRY, REG_A=cr)
#define BINOPA_SUB(v) (cr=REG_A - (v), FLG_N=1, SETZ, SETH_BORROW(REG_A), SETC_BORROW, REG_A=cr)
#define BINOPA_CP(v) (cr=REG_A-(v), FLG_N=1, SETZ, SETH_BORROW(REG_A), SETC_BORROW)
#define INC(target) (cr=target + 1, FLG_N=0, SETZ, SETH_CARRY(target), target=cr)
#define INC_HL (tmp2=memory_read8(REG_HL), cr=tmp2 + 1, FLG_N=0, SETZ, SETH_CARRY(tmp2), memory_write8(REG_HL, cr))
#define DEC(target) (cr=target - 1, FLG_N=1, SETZ, SETH_BORROW(target), target=cr)
#define DEC_HL (tmp2=memory_read8(REG_HL), cr=tmp2 - 1, FLG_N=1, SETZ, SETH_BORROW(tmp2), memory_write8(REG_HL, cr))
#define BINOPA_LOGIC(op, v, n, h) (REG_A=REG_A op (v), FLG_N=(n), FLG_H=(h), FLG_Z=!REG_A, FLG_C=0)

#define RLCA (cr=REG_A<<1, FLG_N=0, FLG_H=0, SETC_CARRY, REG_A=cr+FLG_C_01, FLG_Z=0)
#define RLA (cr=(REG_A<<1)+FLG_C_01, FLG_N=0, FLG_H=0, SETC_CARRY, REG_A=cr, FLG_Z=0)
#define RRCA (FLG_C=REG_A&0x1, cr=REG_A>>1,  FLG_N=0, FLG_H=0, REG_A=cr|(FLG_C<<7), FLG_Z=0)
#define RRA (cr=FLG_C_01, FLG_C=REG_A&0x1, cr=(REG_A>>1)|(cr<<7), FLG_N=0, FLG_H=0, REG_A=cr, FLG_Z=0)

#define RLC(r) (cr=(r)<<1, FLG_N=0, FLG_H=0, SETC_CARRY, cr+=FLG_C_01, SETZ,(r)=cr)
#define RL(r) (cr=((r)<<1)+FLG_C_01, FLG_N=0, FLG_H=0, SETC_CARRY, SETZ, (r)=cr)
#define RRC(r) (FLG_C=(r)&0x1, cr=(r)>>1,  FLG_N=0, FLG_H=0, cr=cr|(FLG_C<<7), SETZ, (r)=cr)
#define RR(r) (cr=FLG_C_01, FLG_C=(r)&0x1, cr=((r)>>1)|(cr<<7), FLG_N=0, FLG_H=0, SETZ, (r)=cr)

#define RLC_HL (cr=memory_read8(REG_HL)<<1, FLG_N=0, FLG_H=0, SETC_CARRY, cr+=FLG_C_01, SETZ, memory_write8(REG_HL, cr))
#define RL_HL (cr=(memory_read8(REG_HL)<<1)+FLG_C_01, FLG_N=0, FLG_H=0, SETC_CARRY, SETZ, memory_write8(REG_HL, cr))
#define RRC_HL (tmp=memory_read8(REG_HL), FLG_C=tmp&0x1, cr=tmp>>1,  FLG_N=0, FLG_H=0, cr=cr|(FLG_C<<7), SETZ, memory_write8(REG_HL, cr))
#define RR_HL (tmp=memory_read8(REG_HL), cr=FLG_C_01, FLG_C=tmp&0x1, cr=(tmp>>1)|(cr<<7), FLG_N=0, FLG_H=0, SETZ, memory_write8(REG_HL, cr))

#define SLA(r) (cr = (r) << 1, SETZ, SETC_CARRY, FLG_N = 0, FLG_H = 0, (r)=cr)
#define SWAP(r) (cr = (((r)&0xf)<<4) | (((r)&0xf0)>>4), SETZ, FLG_C=0, FLG_N = 0, FLG_H = 0, (r)=cr)
#define SRA(r) (FLG_C = (r) & 0x1, cr = ((r)&0x80) | ((r) >> 1), SETZ, FLG_N = 0, FLG_H = 0, (r)=cr)
#define SRL(r) (FLG_C = (r) & 0x1, cr = (r) >> 1,  SETZ, FLG_N = 0, FLG_H = 0, (r)=cr)

#define SLA_HL (cr = memory_read8(REG_HL) << 1, SETZ, SETC_CARRY, FLG_N = 0, FLG_H = 0, memory_write8(REG_HL, cr))
#define SWAP_HL (cr=memory_read8(REG_HL), cr = ((cr&0xf)<<4) | ((cr&0xf0)>>4), SETZ, FLG_C=0, FLG_N = 0, FLG_H = 0, memory_write8(REG_HL, cr))
#define SRA_HL (cr=memory_read8(REG_HL), FLG_C = cr & 0x1, cr = (cr&0x80) | (cr >> 1), SETZ, FLG_N = 0, FLG_H = 0, memory_write8(REG_HL, cr))
#define SRL_HL (cr=memory_read8(REG_HL), FLG_C = cr & 0x1, cr = cr >> 1,  SETZ, FLG_N = 0, FLG_H = 0, memory_write8(REG_HL, cr))

#define BIT(b, v) (FLG_Z = (((v) & (0x1<<(b))) == 0), FLG_N=0, FLG_H=1)
#define RES(b, r) ((r) &= ~(0x1<<(b)))
#define RES_HL(b) (memory_write8(REG_HL, memory_read8(REG_HL) & ~(0x1<<(b))))
#define SET(b, r) ((r) |= (0x1<<(b)))
#define SET_HL(b) (memory_write8(REG_HL, memory_read8(REG_HL) | (0x1<<(b))))

#define JR (REG_PC+=(int8_t)(OPERAND8))
#define JP(va) (REG_PC=(va))
#define CALL (memory_write16(REG_SP-2, REG_PC), REG_SP-=2, REG_PC=OPERAND16)
#define CALL_ADDR(addr) (memory_write16(REG_SP-2, REG_PC), REG_SP-=2, REG_PC=(addr))
#define RST(va) (memory_write16(REG_SP-2, REG_PC), REG_PC=(va), REG_SP-=2)

#define RET (REG_PC=memory_read16(REG_SP), REG_SP+=2)

#define PUSH(ss) (memory_write16(REG_SP-2, ss), REG_SP-=2)
#define POP(ss) ((ss)=memory_read16(REG_SP), REG_SP+=2)
#define PUSH_AF (tmp=REG_A<<8, tmp|=(FLG_Z<<7|FLG_N<<6|FLG_H<<5|FLG_C_01<<4), memory_write16(REG_SP-2, tmp), REG_SP-=2)
#define POP_AF (REG_A=memory_read8(REG_SP+1), tmp=memory_read8(REG_SP), FLG_Z=tmp&0x80, FLG_N=((tmp&0x40)==0x40), FLG_H=((tmp&0x20)==0x20), FLG_C=((tmp&0x10)==0x10), REG_SP+=2)

#define ADDHL_16(v) (tmp=REG_HL, cr=REG_HL+(v), FLG_N=0, FLG_C=REG_HL&0x10000, FLG_H=(((v)&0xfff)+(tmp&0xfff))&0x1000, REG_HL=cr)
#define ADDSP_16 (tmp=REG_SP, REG_SP+=(int8_t)(OPERAND8), FLG_Z=0, FLG_N=0, FLG_C=REG_SP&0x10000, FLG_H=((OPERAND8&0xfff)+(tmp&0xfff))&0x1000)


static SDL_sem *intwait_sem;
int is_haltmode = 0;
int is_stopmode = 0;

void cpu_init() {
	intwait_sem = SDL_CreateSemaphore(0);
}

void request_interrupt(uint8_t type) {
	CAS_UPDATE(REG_IF, REG_IF.value|type);
	if(is_haltmode || (is_stopmode&&type==INT_JOYPAD)){
		is_haltmode=0; is_stopmode=0;
		SDL_SemPost(intwait_sem);
	}
}

void cpu_exec() {
	uint32_t cr, tmp, tmp2;

	while(1){
		#ifdef SHOW_DISAS
		disas_one(REG_PC);
		#endif // SHOW_DISAS

		//割り込みチェック
		if(FLG_IME){
			uint8_t masked=REG_IF.value&REG_IE.value;
			uint8_t cause = masked&(~masked + 1); //1になっている一番下の桁
			if(cause){
				FLG_IME=0;
				CAS_UPDATE(REG_IF, REG_IF.value&(~cause));
				switch(cause){
				case 0x1:  CALL_ADDR(0x40); break;
				case 0x2:  CALL_ADDR(0x48); break;
				case 0x4:  CALL_ADDR(0x50); break;
				case 0x8:  CALL_ADDR(0x58); break;
				case 0x10: CALL_ADDR(0x60); break;
				}
			}
		}

		switch(memory_read8(REG_PC)){
		case 0x00: /* NOP - ---- */			REG_PC+=1; continue;
		case 0x01: /* LD BC,nn ---- */  	REG_BC=OPERAND16; REG_PC+=3; continue;
		case 0x02: /* LD (BC),A ---- */  	memory_write8(REG_BC, REG_A); REG_PC+=1; continue;
		case 0x03: /* INC BC ---- */  		REG_BC++; REG_PC+=1; continue;
		case 0x04: /* INC B Z0H- */  		INC(REG_B); REG_PC+=1; continue;
		case 0x05: /* DEC B Z1H- */  		DEC(REG_B); REG_PC+=1; continue;
		case 0x06: /* LD B,n ---- */  		REG_B=OPERAND8; REG_PC+=2; continue;
		case 0x07: /* RLCA - 000C */  		RLCA; REG_PC+=1; continue;
		case 0x08: /* LD (nn),SP ---- */  	memory_write16(OPERAND16, REG_SP); REG_PC+=3; continue;
		case 0x09: /* ADD HL,BC -0HC */  	ADDHL_16(REG_BC); REG_PC+=1; continue;
		case 0x0A: /* LD A,(BC) ---- */  	REG_A=memory_read8(REG_BC); REG_PC+=1; continue;
		case 0x0B: /* DEC BC ---- */  		REG_BC--; REG_PC+=1; continue;
		case 0x0C: /* INC C Z0H- */  		INC(REG_C); REG_PC+=1; continue;
		case 0x0D: /* DEC C Z1H- */  		DEC(REG_C); REG_PC+=1; continue;
		case 0x0E: /* LD C,n ---- */  		REG_C=OPERAND8; REG_PC+=2; continue;
		case 0x0F: /* RRCA - 000C */  		RRCA; REG_PC+=1; continue;
		case 0x10: /* STOP - ---- */
			//TODO: LCDを白くする
			is_stopmode = 1;
			SDL_SemWait(intwait_sem);
			REG_PC+=2;
			continue;
		case 0x11: /* LD DE,nn ---- */  	REG_DE=OPERAND16; REG_PC+=3; continue;
		case 0x12: /* LD (DE),A ---- */  	memory_write8(REG_DE, REG_A); REG_PC+=1; continue;
		case 0x13: /* INC DE ---- */  		REG_DE++; REG_PC+=1; continue;
		case 0x14: /* INC D Z0H- */  		INC(REG_D); REG_PC+=1; continue;
		case 0x15: /* DEC D Z1H- */  		DEC(REG_D); REG_PC+=1; continue;
		case 0x16: /* LD D,n ---- */  		REG_D=OPERAND8; REG_PC+=2; continue;
		case 0x17: /* RLA - 000C */  		RLA; REG_PC+=1; continue;
		case 0x18: /* JR n ---- */  		JR; REG_PC+=2; continue;
		case 0x19: /* ADD HL,DE -0HC */  	ADDHL_16(REG_DE); REG_PC+=1; continue;
		case 0x1A: /* LD A,(DE) ---- */  	REG_A=memory_read8(REG_DE); REG_A = memory_read8(REG_DE); REG_PC+=1; continue;
		case 0x1B: /* DEC DE ---- */  		REG_DE--; REG_PC+=1; continue;
		case 0x1C: /* INC E Z0H- */  		INC(REG_E); REG_PC+=1; continue;
		case 0x1D: /* DEC E Z1H- */  		DEC(REG_E); REG_PC+=1; continue;
		case 0x1E: /* LD E,n ---- */  		REG_E=OPERAND8; REG_PC+=2; continue;
		case 0x1F: /* RRA - 000C */  		RRA; REG_PC+=1; continue;
		case 0x20: /* JR NZ,* ---- */  		if(FLG_Z){JR;} REG_PC+=2; continue;
		case 0x21: /* LD HL,nn ---- */  	REG_HL=OPERAND16; REG_PC+=3; continue;
		case 0x22: /* LD (HL+),A ---- */  	memory_write8(REG_HL, REG_A); REG_HL++; REG_PC+=1; continue;
		case 0x23: /* INC HL ---- */  		REG_HL++; REG_PC+=1; continue;
		case 0x24: /* INC H Z0H- */  		INC(REG_H); REG_PC+=1; continue;
		case 0x25: /* DEC H Z1H- */  		DEC(REG_H); REG_PC+=1; continue;
		case 0x26: /* LD H,n ---- */  		REG_H=OPERAND8; REG_PC+=2; continue;
		case 0x27: /* DAA - Z-HC */
			{
				uint8_t high=REG_A>>4, low=REG_A&0xf;
				uint8_t diff;
				if(FLG_C){
					if(low<0xa)
						if(FLG_H)
							diff=0x66;
						else
							diff=0x60;
					else
						diff=0x66;
				}else{
					if(low<0xa)
						if(high<0xa){
							FLG_C=0;
							if(FLG_H)
								diff=0x06;
							else
								diff=0x00;
						}else{
							FLG_C=1;
							if(FLG_H)
								diff=0x66;
							else
								diff=0x60;
						}
					else
						if(high<0x9){
							FLG_C=0;
							diff=0x06;
						}else{
							FLG_C=1;
							diff=0x66;
						}
				}
				if(FLG_N){
					REG_A-=diff;
					if(FLG_H)
						FLG_H=(low<0x6);
					else
						FLG_H=0;
				}else{
					REG_A+=diff;
					FLG_H=(low>0x9);
				}
				FLG_Z=!REG_A;
				REG_PC+=1; continue;
			}
		case 0x28: /* JR Z,* ---- */  		if(!FLG_Z){JR;} REG_PC+=2; continue;
		case 0x29: /* ADD HL,HL -0HC */  	ADDHL_16(REG_HL); REG_PC+=1; continue;
		case 0x2A: /* LD A,(HL+) ---- */  	REG_A=memory_read8(REG_HL); REG_HL++; REG_PC+=1; continue;
		case 0x2B: /* DEC HL ---- */  		REG_HL--; REG_PC+=1; continue;
		case 0x2C: /* INC L Z0H- */  		INC(REG_L); REG_PC+=1; continue;
		case 0x2D: /* DEC L Z1H- */  		DEC(REG_L); REG_PC+=1; continue;
		case 0x2E: /* LD L,n ---- */  		REG_L=OPERAND8; REG_PC+=2; continue;
		case 0x2F: /* CPL - -11- */  		REG_A=~REG_A; FLG_N=1; FLG_H=1; REG_PC+=1; continue;
		case 0x30: /* JR NC,* ---- */  		if(!FLG_C){JR;} REG_PC+=2; continue;
		case 0x31: /* LD SP,nn ---- */  	REG_SP=OPERAND16; REG_PC+=3; continue;
		case 0x32: /* LD (HL-),A ---- */  	memory_write8(REG_HL, REG_A); REG_HL--; REG_PC+=1; continue;
		case 0x33: /* INC SP ---- */  		REG_SP++; REG_PC+=1; continue;
		case 0x34: /* INC (HL) Z0H- */  	INC_HL; REG_PC+=1; continue;
		case 0x35: /* DEC (HL) Z1H- */  	DEC_HL; REG_PC+=1; continue;
		case 0x36: /* LD (HL),n ---- */  	memory_write8(REG_HL, OPERAND8); REG_PC+=2; continue;
		case 0x37: /* SCF - -001 */  		FLG_C=1; FLG_H=0; FLG_N=0; REG_PC+=1; continue;
		case 0x38: /* JR C,* ---- */  		if(FLG_C){JR;} REG_PC+=1; continue;
		case 0x39: /* ADD HL,SP -0HC */  	ADDHL_16(REG_SP); REG_PC+=1; continue;
		case 0x3A: /* LD A,(HL-) ---- */  	REG_A=memory_read8(REG_HL); REG_HL--; REG_PC+=1; continue;
		case 0x3B: /* DEC SP ---- */  		REG_SP--;  REG_PC+=1; continue;
		case 0x3C: /* INC A Z0H- */  		INC(REG_A); REG_PC+=1; continue;
		case 0x3D: /* DEC A Z1H- */  		DEC(REG_A); REG_PC+=1; continue;
		case 0x3E: /* LD A,# ---- */  		REG_A=OPERAND8; REG_PC+=2; continue;
		case 0x3F: /* CCF - -00C */  		FLG_C=!FLG_C; FLG_H=0; FLG_N=0; REG_PC+=1; continue;
		case 0x40: /* LD B,B ---- */ 		REG_PC+=1; continue;
		case 0x41: /* LD B,C ---- */ 		REG_B = REG_C; REG_PC+=1; continue;
		case 0x42: /* LD B,D ---- */ 		REG_B = REG_D; REG_PC+=1; continue;
		case 0x43: /* LD B,E ---- */ 		REG_B = REG_E; REG_PC+=1; continue;
		case 0x44: /* LD B,H ---- */ 		REG_B = REG_H; REG_PC+=1; continue;
		case 0x45: /* LD B,L ---- */ 		REG_B = REG_L; REG_PC+=1; continue;
		case 0x46: /* LD B,(HL) ---- */ 	REG_B = memory_read8(REG_HL); REG_PC+=1; continue;
		case 0x47: /* LD B,A ---- */ 		REG_B = REG_A; REG_PC+=1; continue;
		case 0x48: /* LD C,B ---- */ 		REG_C = REG_B; REG_PC+=1; continue;
		case 0x49: /* LD C,C ---- */ 		REG_PC+=1; continue;
		case 0x4A: /* LD C,D ---- */ 		REG_C = REG_D; REG_PC+=1; continue;
		case 0x4B: /* LD C,E ---- */ 		REG_C = REG_E; REG_PC+=1; continue;
		case 0x4C: /* LD C,H ---- */ 		REG_C = REG_H; REG_PC+=1; continue;
		case 0x4D: /* LD C,L ---- */ 		REG_C = REG_L; REG_PC+=1; continue;
		case 0x4E: /* LD C,(HL) ---- */ 	REG_C = memory_read8(REG_HL); REG_PC+=1; continue;
		case 0x4F: /* LD C,A ---- */ 		REG_C = REG_A; REG_PC+=1; continue;
		case 0x50: /* LD D,B ---- */ 		REG_D = REG_B; REG_PC+=1; continue;
		case 0x51: /* LD D,C ---- */ 		REG_D = REG_C; REG_PC+=1; continue;
		case 0x52: /* LD D,D ---- */ 		REG_PC+=1; continue;
		case 0x53: /* LD D,E ---- */ 		REG_D = REG_E; REG_PC+=1; continue;
		case 0x54: /* LD D,H ---- */ 		REG_D = REG_H; REG_PC+=1; continue;
		case 0x55: /* LD D,L ---- */ 		REG_D = REG_L; REG_PC+=1; continue;
		case 0x56: /* LD D,(HL) ---- */ 	REG_D = memory_read8(REG_HL); REG_PC+=1; continue;
		case 0x57: /* LD D,A ---- */ 		REG_D = REG_A; REG_PC+=1; continue;
		case 0x58: /* LD E,B ---- */ 		REG_E = REG_B; REG_PC+=1; continue;
		case 0x59: /* LD E,C ---- */ 		REG_E = REG_C; REG_PC+=1; continue;
		case 0x5A: /* LD E,D ---- */ 		REG_E = REG_D; REG_PC+=1; continue;
		case 0x5B: /* LD E,E ---- */ 		REG_PC+=1; continue;
		case 0x5C: /* LD E,H ---- */ 		REG_E = REG_H; REG_PC+=1; continue;
		case 0x5D: /* LD E,L ---- */ 		REG_E = REG_L; REG_PC+=1; continue;
		case 0x5E: /* LD E,(HL) ---- */ 	REG_E = memory_read8(REG_HL); REG_PC+=1; continue;
		case 0x5F: /* LD E,A ---- */ 		REG_E = REG_A; REG_PC+=1; continue;
		case 0x60: /* LD H,B ---- */ 		REG_H = REG_B; REG_PC+=1; continue;
		case 0x61: /* LD H,C ---- */ 		REG_H = REG_C; REG_PC+=1; continue;
		case 0x62: /* LD H,D ---- */ 		REG_H = REG_D; REG_PC+=1; continue;
		case 0x63: /* LD H,E ---- */ 		REG_H = REG_E; REG_PC+=1; continue;
		case 0x64: /* LD H,H ---- */ 		REG_PC+=1; continue;
		case 0x65: /* LD H,L ---- */ 		REG_H = REG_L; REG_PC+=1; continue;
		case 0x66: /* LD H,(HL) ---- */ 	REG_H = memory_read8(REG_HL); REG_PC+=1; continue;
		case 0x67: /* LD H,A ---- */ 		REG_H = REG_A; REG_PC+=1; continue;
		case 0x68: /* LD L,B ---- */ 		REG_L = REG_B; REG_PC+=1; continue;
		case 0x69: /* LD L,C ---- */ 		REG_L = REG_C; REG_PC+=1; continue;
		case 0x6A: /* LD L,D ---- */ 		REG_L = REG_D; REG_PC+=1; continue;
		case 0x6B: /* LD L,E ---- */ 		REG_L = REG_E; REG_PC+=1; continue;
		case 0x6C: /* LD L,H ---- */ 		REG_L = REG_H; REG_PC+=1; continue;
		case 0x6D: /* LD L,L ---- */  		REG_PC+=1; continue;
		case 0x6E: /* LD L,(HL) ---- */ 	REG_L = memory_read8(REG_HL); REG_PC+=1; continue;
		case 0x6F: /* LD L,A ---- */ 		REG_L = REG_A; REG_PC+=1; continue;
		case 0x70: /* LD (HL),B ---- */ 	memory_write8(REG_HL, REG_B); REG_PC+=1; continue;
		case 0x71: /* LD (HL),C ---- */ 	memory_write8(REG_HL, REG_C); continue;
		case 0x72: /* LD (HL),D ---- */ 	memory_write8(REG_HL, REG_D); continue;
		case 0x73: /* LD (HL),E ---- */ 	memory_write8(REG_HL, REG_E); continue;
		case 0x74: /* LD (HL),H ---- */ 	memory_write8(REG_HL, REG_H); continue;
		case 0x75: /* LD (HL),L ---- */ 	memory_write8(REG_HL, REG_L); continue;
		case 0x76: /* HALT - ---- */
			is_haltmode = 1;
			SDL_SemWait(intwait_sem);
			REG_PC+=1;
			continue;
		case 0x77: /* LD (HL),A ---- */ 	memory_write8(REG_HL, REG_A); REG_PC+=1; continue;
		case 0x78: /* LD A,B ---- */ 		REG_A = REG_B; REG_PC+=1; continue;
		case 0x79: /* LD A,C ---- */ 		REG_A = REG_C; REG_PC+=1; continue;
		case 0x7A: /* LD A,D ---- */ 		REG_A = REG_D; REG_PC+=1; continue;
		case 0x7B: /* LD A,E ---- */ 		REG_A = REG_E; REG_PC+=1; continue;
		case 0x7C: /* LD A,H ---- */ 		REG_A = REG_H; REG_PC+=1; continue;
		case 0x7D: /* LD A,L ---- */ 		REG_A = REG_L; REG_PC+=1; continue;
		case 0x7E: /* LD A,(HL) ---- */ 	REG_A = memory_read8(REG_HL); REG_PC+=1; continue;
		case 0x7F: /* LD A,A ---- */  		REG_PC+=1; continue;
		case 0x80: /* ADD A,B Z0HC */  		BINOPA_ADD(REG_B); REG_PC+=1; continue;
		case 0x81: /* ADD A,C Z0HC */  		BINOPA_ADD(REG_C); REG_PC+=1; continue;
		case 0x82: /* ADD A,D Z0HC */  		BINOPA_ADD(REG_D); REG_PC+=1; continue;
		case 0x83: /* ADD A,E Z0HC */  		BINOPA_ADD(REG_E); REG_PC+=1; continue;
		case 0x84: /* ADD A,H Z0HC */  		BINOPA_ADD(REG_H); REG_PC+=1; continue;
		case 0x85: /* ADD A,L Z0HC */  		BINOPA_ADD(REG_L); REG_PC+=1; continue;
		case 0x86: /* ADD A,(HL) Z0HC */  	BINOPA_ADD(memory_read8(REG_HL)); REG_PC+=1; continue;
		case 0x87: /* ADD A,A Z0HC */  		BINOPA_ADD(REG_B); REG_PC+=1; continue;
		case 0x88: /* ADC A,B Z0HC */  		BINOPA_ADD(REG_B+FLG_C_01); REG_PC+=1; continue;
		case 0x89: /* ADC A,C Z0HC */  		BINOPA_ADD(REG_C+FLG_C_01); REG_PC+=1; continue;
		case 0x8A: /* ADC A,D Z0HC */  		BINOPA_ADD(REG_D+FLG_C_01); REG_PC+=1; continue;
		case 0x8B: /* ADC A,E Z0HC */  		BINOPA_ADD(REG_E+FLG_C_01); REG_PC+=1; continue;
		case 0x8C: /* ADC A,H Z0HC */  		BINOPA_ADD(REG_H+FLG_C_01); REG_PC+=1; continue;
		case 0x8D: /* ADC A,L Z0HC */  		BINOPA_ADD(REG_L+FLG_C_01); REG_PC+=1; continue;
		case 0x8E: /* ADC A,(HL) Z0HC */  	BINOPA_ADD(memory_read8(REG_HL)+FLG_C_01); REG_PC+=1; continue;
		case 0x8F: /* ADC A,A Z0HC */  		BINOPA_ADD(REG_A+FLG_C_01); REG_PC+=1; continue;
		case 0x90: /* SUB B Z1HC */  		BINOPA_SUB(REG_B); REG_PC+=1; continue;
		case 0x91: /* SUB C Z1HC */  		BINOPA_SUB(REG_C); REG_PC+=1; continue;
		case 0x92: /* SUB D Z1HC */  		BINOPA_SUB(REG_D); REG_PC+=1; continue;
		case 0x93: /* SUB E Z1HC */  		BINOPA_SUB(REG_E); REG_PC+=1; continue;
		case 0x94: /* SUB H Z1HC */  		BINOPA_SUB(REG_H); REG_PC+=1; continue;
		case 0x95: /* SUB L Z1HC */ 		BINOPA_SUB(REG_L); REG_PC+=1; continue;
		case 0x96: /* SUB (HL) Z1HC */  	BINOPA_SUB(memory_read8(REG_HL)); REG_PC+=1; continue;
		case 0x97: /* SUB A Z1HC */  		BINOPA_SUB(REG_A); REG_PC+=1; continue;
		case 0x98: /* SBC A,B Z1HC */  		BINOPA_SUB(REG_B-FLG_C_01); REG_PC+=1; continue;
		case 0x99: /* SBC A,C Z1HC */  		BINOPA_SUB(REG_C-FLG_C_01); REG_PC+=1; continue;
		case 0x9A: /* SBC A,D Z1HC */  		BINOPA_SUB(REG_D-FLG_C_01); REG_PC+=1; continue;
		case 0x9B: /* SBC A,E Z1HC */  		BINOPA_SUB(REG_E-FLG_C_01); REG_PC+=1; continue;
		case 0x9C: /* SBC A,H Z1HC */  		BINOPA_SUB(REG_H-FLG_C_01); REG_PC+=1; continue;
		case 0x9D: /* SBC A,L Z1HC */  		BINOPA_SUB(REG_L-FLG_C_01); REG_PC+=1; continue;
		case 0x9E: /* SBC A,(HL) Z1HC */  	BINOPA_SUB(memory_read8(REG_HL)-FLG_C_01); REG_PC+=1; continue;
		case 0x9F: /* SBC A,A Z1HC */  		BINOPA_SUB(REG_A-FLG_C_01); REG_PC+=1; continue;
		case 0xA0: /* AND B Z010 */  		BINOPA_LOGIC(&, REG_B, 0, 1); REG_PC+=1; continue;
		case 0xA1: /* AND C Z010 */  		BINOPA_LOGIC(&, REG_C, 0, 1); REG_PC+=1; continue;
		case 0xA2: /* AND D Z010 */  		BINOPA_LOGIC(&, REG_D, 0, 1); REG_PC+=1; continue;
		case 0xA3: /* AND E Z010 */  		BINOPA_LOGIC(&, REG_E, 0, 1); REG_PC+=1; continue;
		case 0xA4: /* AND H Z010 */  		BINOPA_LOGIC(&, REG_H, 0, 1); REG_PC+=1; continue;
		case 0xA5: /* AND L Z010 */  		BINOPA_LOGIC(&, REG_L, 0, 1); REG_PC+=1; continue;
		case 0xA6: /* AND (HL) Z010 */  	BINOPA_LOGIC(&, memory_read8(REG_HL), 0, 1); REG_PC+=1; continue;
		case 0xA7: /* AND A Z010 */  		BINOPA_LOGIC(&, REG_A, 0, 1); REG_PC+=1; continue;
		case 0xA8: /* XOR B Z000 */  		BINOPA_LOGIC(^, REG_B, 0, 0); REG_PC+=1; continue;
		case 0xA9: /* XOR C Z000 */  		BINOPA_LOGIC(^, REG_C, 0, 0); REG_PC+=1; continue;
		case 0xAA: /* XOR D Z000 */  		BINOPA_LOGIC(^, REG_D, 0, 0); REG_PC+=1; continue;
		case 0xAB: /* XOR E Z000 */  		BINOPA_LOGIC(^, REG_E, 0, 0); REG_PC+=1; continue;
		case 0xAC: /* XOR H Z000 */  		BINOPA_LOGIC(^, REG_H, 0, 0); REG_PC+=1; continue;
		case 0xAD: /* XOR L Z000 */  		BINOPA_LOGIC(^, REG_L, 0, 0); REG_PC+=1; continue;
		case 0xAE: /* XOR (HL) Z000 */  	BINOPA_LOGIC(^, memory_read8(REG_HL), 0, 0); REG_PC+=1; continue;
		case 0xAF: /* XOR A Z000 */ 		BINOPA_LOGIC(^, REG_A, 0, 0); REG_PC+=1; continue;
		case 0xB0: /* OR B Z000 */  		BINOPA_LOGIC(|, REG_B, 0, 0); REG_PC+=1; continue;
		case 0xB1: /* OR C Z000 */  		BINOPA_LOGIC(|, REG_C, 0, 0); REG_PC+=1; continue;
		case 0xB2: /* OR D Z000 */  		BINOPA_LOGIC(|, REG_D, 0, 0); REG_PC+=1; continue;
		case 0xB3: /* OR E Z000 */  		BINOPA_LOGIC(|, REG_E, 0, 0); REG_PC+=1; continue;
		case 0xB4: /* OR H Z000 */  		BINOPA_LOGIC(|, REG_H, 0, 0); REG_PC+=1; continue;
		case 0xB5: /* OR L Z000 */  		BINOPA_LOGIC(|, REG_L, 0, 0); REG_PC+=1; continue;
		case 0xB6: /* OR (HL) Z000 */  		BINOPA_LOGIC(|, memory_read8(REG_HL), 0, 0); REG_PC+=1; continue;
		case 0xB7: /* OR A Z000 */  		BINOPA_LOGIC(|, REG_A, 0, 0); REG_PC+=1; continue;
		case 0xB8: /* CP B Z1HC */  		BINOPA_CP(REG_B); REG_PC+=1; continue;
		case 0xB9: /* CP C Z1HC */  		BINOPA_CP(REG_C); REG_PC+=1; continue;
		case 0xBA: /* CP D Z1HC */  		BINOPA_CP(REG_D); REG_PC+=1; continue;
		case 0xBB: /* CP E Z1HC */  		BINOPA_CP(REG_E); REG_PC+=1; continue;
		case 0xBC: /* CP H Z1HC */  		BINOPA_CP(REG_H); REG_PC+=1; continue;
		case 0xBD: /* CP L Z1HC */  		BINOPA_CP(REG_L); REG_PC+=1; continue;
		case 0xBE: /* CP (HL) Z1HC */  		BINOPA_CP(memory_read8(REG_HL)); REG_PC+=1; continue;
		case 0xBF: /* CP A Z1HC */  		BINOPA_CP(REG_A); REG_PC+=1; continue;
		case 0xC0: /* RET NZ ---- */  		if(!FLG_Z){RET;}else{REG_PC+=1;} continue;
		case 0xC1: /* POP BC ---- */  		POP(REG_BC); REG_PC+=1; continue;
		case 0xC2: /* JP NZ,nn ---- */  	if(!FLG_Z){JP(OPERAND16);}else{REG_PC+=3;} continue;
		case 0xC3: /* JP nn ---- */  		JP(OPERAND16); continue;
		case 0xC4: /* CALL NZ,nn ---- */  	if(!FLG_Z){CALL;}else{REG_PC+=3;} continue;
		case 0xC5: /* PUSH BC ---- */  		PUSH(REG_BC); REG_PC+=1; continue;
		case 0xC6: /* ADD A,# Z0HC */  		BINOPA_ADD(OPERAND8); REG_PC+=2; continue;
		case 0xC7: /* RST 00H ---- */ 	 	RST(0x00); continue;
		case 0xC8: /* RET Z ---- */  		if(FLG_Z){RET;}else{REG_PC+=1;} continue;
		case 0xC9: /* RET - ---- */  		RET; continue;
		case 0xCA: /* JP Z,nn ---- */  		if(FLG_Z){JP(OPERAND16);}else{REG_PC+=3;} continue;
		case 0xCB:
			switch(memory_read8(REG_PC+1)){
			case 0x00: /* RLC B Z00C */  	RLC(REG_B); REG_PC+=2; continue;
			case 0x01: /* RLC C Z00C */  	RLC(REG_C); REG_PC+=2; continue;
			case 0x02: /* RLC D Z00C */  	RLC(REG_D); REG_PC+=2; continue;
			case 0x03: /* RLC E Z00C */  	RLC(REG_E); REG_PC+=2; continue;
			case 0x04: /* RLC H Z00C */  	RLC(REG_H); REG_PC+=2; continue;
			case 0x05: /* RLC L Z00C */  	RLC(REG_L); REG_PC+=2; continue;
			case 0x06: /* RLC (HL) Z00C */  RLC_HL; REG_PC+=2; continue;
			case 0x07: /* RLC A Z00C */  	RLC(REG_A); REG_PC+=2; continue;
			case 0x08: /* RRC B Z00C */  	RRC(REG_B); REG_PC+=2; continue;
			case 0x09: /* RRC C Z00C */  	RRC(REG_C); REG_PC+=2; continue;
			case 0x0A: /* RRC D Z00C */  	RRC(REG_D); REG_PC+=2; continue;
			case 0x0B: /* RRC E Z00C */  	RRC(REG_E); REG_PC+=2; continue;
			case 0x0C: /* RRC H Z00C */  	RRC(REG_H); REG_PC+=2; continue;
			case 0x0D: /* RRC L Z00C */  	RRC(REG_L); REG_PC+=2; continue;
			case 0x0E: /* RRC (HL) Z00C */  RRC_HL; REG_PC+=2; continue;
			case 0x0F: /* RRC A Z00C */  	RRC(REG_A); REG_PC+=2; continue;
			case 0x10: /* RL B Z00C */  	RL(REG_B); REG_PC+=2; continue;
			case 0x11: /* RL C Z00C */  	RL(REG_C); REG_PC+=2; continue;
			case 0x12: /* RL D Z00C */  	RL(REG_D); REG_PC+=2; continue;
			case 0x13: /* RL E Z00C */  	RL(REG_E); REG_PC+=2; continue;
			case 0x14: /* RL H Z00C */  	RL(REG_H); REG_PC+=2; continue;
			case 0x15: /* RL L Z00C */  	RL(REG_L); REG_PC+=2; continue;
			case 0x16: /* RL (HL) Z00C */  	RL_HL; REG_PC+=2; continue;
			case 0x17: /* RL A Z00C */  	RL(REG_A); REG_PC+=2; continue;
			case 0x18: /* RR B Z00C */  	RR(REG_B); REG_PC+=2; continue;
			case 0x19: /* RR C Z00C */  	RR(REG_C); REG_PC+=2; continue;
			case 0x1A: /* RR D Z00C */  	RR(REG_D); REG_PC+=2; continue;
			case 0x1B: /* RR E Z00C */  	RR(REG_E); REG_PC+=2; continue;
			case 0x1C: /* RR H Z00C */  	RR(REG_H); REG_PC+=2; continue;
			case 0x1D: /* RR L Z00C */  	RR(REG_L); REG_PC+=2; continue;
			case 0x1E: /* RR (HL) Z00C */  	RR_HL; REG_PC+=2; continue;
			case 0x1F: /* RR A Z00C */  	RR(REG_A); REG_PC+=2; continue;
			case 0x20: /* SLA B Z00C */  	SLA(REG_B); REG_PC+=2; continue;
			case 0x21: /* SLA C Z00C */  	SLA(REG_C); REG_PC+=2; continue;
			case 0x22: /* SLA D Z00C */  	SLA(REG_D); REG_PC+=2; continue;
			case 0x23: /* SLA E Z00C */  	SLA(REG_E); REG_PC+=2; continue;
			case 0x24: /* SLA H Z00C */  	SLA(REG_H); REG_PC+=2; continue;
			case 0x25: /* SLA L Z00C */  	SLA(REG_L); REG_PC+=2; continue;
			case 0x26: /* SLA (HL) Z00C */  SLA_HL; REG_PC+=2; continue;
			case 0x27: /* SLA A Z00C */  	SLA(REG_A); REG_PC+=2; continue;
			case 0x28: /* SRA B Z00C */  	SRA(REG_B); REG_PC+=2; continue;
			case 0x29: /* SRA C Z00C */  	SRA(REG_C); REG_PC+=2; continue;
			case 0x2A: /* SRA D Z00C */  	SRA(REG_D); REG_PC+=2; continue;
			case 0x2B: /* SRA E Z00C */  	SRA(REG_E); REG_PC+=2; continue;
			case 0x2C: /* SRA H Z00C */  	SRA(REG_H); REG_PC+=2; continue;
			case 0x2D: /* SRA L Z00C */  	SRA(REG_L); REG_PC+=2; continue;
			case 0x2E: /* SRA (HL) Z00C */  SRA_HL; REG_PC+=2; continue;
			case 0x2F: /* SRA A Z00C */  	SRA(REG_A); REG_PC+=2; continue;
			case 0x30: /* SWAP B Z000 */  	SWAP(REG_B); REG_PC+=2; continue;
			case 0x31: /* SWAP C Z000 */  	SWAP(REG_C); REG_PC+=2; continue;
			case 0x32: /* SWAP D Z000 */  	SWAP(REG_D); REG_PC+=2; continue;
			case 0x33: /* SWAP E Z000 */  	SWAP(REG_E); REG_PC+=2; continue;
			case 0x34: /* SWAP H Z000 */  	SWAP(REG_H); REG_PC+=2; continue;
			case 0x35: /* SWAP L Z000 */  	SWAP(REG_L); REG_PC+=2; continue;
			case 0x36: /* SWAP (HL) Z000 */ SWAP_HL; REG_PC+=2; continue;
			case 0x37: /* SWAP A Z000 */ 	SWAP(REG_A); REG_PC+=2; continue;
			case 0x38: /* SRL B Z00C */  	SRL(REG_B); REG_PC+=2; continue;
			case 0x39: /* SRL C Z00C */  	SRL(REG_C); REG_PC+=2; continue;
			case 0x3A: /* SRL D Z00C */  	SRL(REG_D); REG_PC+=2; continue;
			case 0x3B: /* SRL E Z00C */  	SRL(REG_E); REG_PC+=2; continue;
			case 0x3C: /* SRL H Z00C */  	SRL(REG_H); REG_PC+=2; continue;
			case 0x3D: /* SRL L Z00C */  	SRL(REG_L); REG_PC+=2; continue;
			case 0x3E: /* SRL (HL) Z00C */  SRL_HL; REG_PC+=2; continue;
			case 0x3F: /* SRL A Z00C */  	SRL(REG_A); REG_PC+=2; continue;
			case 0x40: /* BIT 0,B Z01- */  	BIT(0, REG_B); REG_PC+=2; continue;
			case 0x41: /* BIT 0,C Z01- */  	BIT(0, REG_C); REG_PC+=2; continue;
			case 0x42: /* BIT 0,D Z01- */  	BIT(0, REG_D); REG_PC+=2; continue;
			case 0x43: /* BIT 0,E Z01- */  	BIT(0, REG_E); REG_PC+=2; continue;
			case 0x44: /* BIT 0,H Z01- */  	BIT(0, REG_H); REG_PC+=2; continue;
			case 0x45: /* BIT 0,L Z01- */  	BIT(0, REG_L); REG_PC+=2; continue;
			case 0x46: /* BIT 0,(HL) Z01- */BIT(0, memory_read8(REG_HL)); REG_PC+=2; continue;
			case 0x47: /* BIT 0,A Z01- */  	BIT(0, REG_A); REG_PC+=2; continue;
			case 0x48: /* BIT 1,B Z01- */  	BIT(1, REG_B); REG_PC+=2; continue;
			case 0x49: /* BIT 1,C Z01- */  	BIT(1, REG_C); REG_PC+=2; continue;
			case 0x4A: /* BIT 1,D Z01- */  	BIT(1, REG_D); REG_PC+=2; continue;
			case 0x4B: /* BIT 1,E Z01- */  	BIT(1, REG_E); REG_PC+=2; continue;
			case 0x4C: /* BIT 1,H Z01- */  	BIT(1, REG_H); REG_PC+=2; continue;
			case 0x4D: /* BIT 1,L Z01- */  	BIT(1, REG_L); REG_PC+=2; continue;
			case 0x4E: /* BIT 1,(HL) Z01- */BIT(1, memory_read8(REG_HL)); REG_PC+=2; continue;
			case 0x4F: /* BIT 1,A Z01- */  	BIT(1, REG_A); REG_PC+=2; continue;
			case 0x50: /* BIT 2,B Z01- */  	BIT(2, REG_B); REG_PC+=2; continue;
			case 0x51: /* BIT 2,C Z01- */  	BIT(2, REG_C); REG_PC+=2; continue;
			case 0x52: /* BIT 2,D Z01- */  	BIT(2, REG_D); REG_PC+=2; continue;
			case 0x53: /* BIT 2,E Z01- */  	BIT(2, REG_E); REG_PC+=2; continue;
			case 0x54: /* BIT 2,H Z01- */  	BIT(2, REG_H); REG_PC+=2; continue;
			case 0x55: /* BIT 2,L Z01- */  	BIT(2, REG_L); REG_PC+=2; continue;
			case 0x56: /* BIT 2,(HL) Z01- */BIT(2, memory_read8(REG_HL)); REG_PC+=2; continue;
			case 0x57: /* BIT 2,A Z01- */  	BIT(2, REG_A); REG_PC+=2; continue;
			case 0x58: /* BIT 3,B Z01- */  	BIT(3, REG_B); REG_PC+=2; continue;
			case 0x59: /* BIT 3,C Z01- */  	BIT(3, REG_C); REG_PC+=2; continue;
			case 0x5A: /* BIT 3,D Z01- */  	BIT(3, REG_D); REG_PC+=2; continue;
			case 0x5B: /* BIT 3,E Z01- */  	BIT(3, REG_E); REG_PC+=2; continue;
			case 0x5C: /* BIT 3,H Z01- */  	BIT(3, REG_H); REG_PC+=2; continue;
			case 0x5D: /* BIT 3,L Z01- */  	BIT(3, REG_L); REG_PC+=2; continue;
			case 0x5E: /* BIT 3,(HL) Z01- */BIT(3, memory_read8(REG_HL)); REG_PC+=2; continue;
			case 0x5F: /* BIT 3,A Z01- */  	BIT(3, REG_A); REG_PC+=2; continue;
			case 0x60: /* BIT 4,B Z01- */  	BIT(4, REG_B); REG_PC+=2; continue;
			case 0x61: /* BIT 4,C Z01- */  	BIT(4, REG_C); REG_PC+=2; continue;
			case 0x62: /* BIT 4,D Z01- */  	BIT(4, REG_D); REG_PC+=2; continue;
			case 0x63: /* BIT 4,E Z01- */  	BIT(4, REG_E); REG_PC+=2; continue;
			case 0x64: /* BIT 4,H Z01- */  	BIT(4, REG_H); REG_PC+=2; continue;
			case 0x65: /* BIT 4,L Z01- */  	BIT(4, REG_L); REG_PC+=2; continue;
			case 0x66: /* BIT 4,(HL) Z01- */BIT(4, memory_read8(REG_HL)); REG_PC+=2; continue;
			case 0x67: /* BIT 4,A Z01- */  	BIT(4, REG_A); REG_PC+=2; continue;
			case 0x68: /* BIT 5,B Z01- */  	BIT(5, REG_B); REG_PC+=2; continue;
			case 0x69: /* BIT 5,C Z01- */  	BIT(5, REG_C); REG_PC+=2; continue;
			case 0x6A: /* BIT 5,D Z01- */  	BIT(5, REG_D); REG_PC+=2; continue;
			case 0x6B: /* BIT 5,E Z01- */  	BIT(5, REG_E); REG_PC+=2; continue;
			case 0x6C: /* BIT 5,H Z01- */  	BIT(5, REG_H); REG_PC+=2; continue;
			case 0x6D: /* BIT 5,L Z01- */  	BIT(5, REG_L); REG_PC+=2; continue;
			case 0x6E: /* BIT 5,(HL) Z01- */BIT(5, memory_read8(REG_HL)); REG_PC+=2; continue;
			case 0x6F: /* BIT 5,A Z01- */  	BIT(5, REG_A); REG_PC+=2; continue;
			case 0x70: /* BIT 6,B Z01- */  	BIT(6, REG_B); REG_PC+=2; continue;
			case 0x71: /* BIT 6,C Z01- */  	BIT(6, REG_C); REG_PC+=2; continue;
			case 0x72: /* BIT 6,D Z01- */  	BIT(6, REG_D); REG_PC+=2; continue;
			case 0x73: /* BIT 6,E Z01- */  	BIT(6, REG_E); REG_PC+=2; continue;
			case 0x74: /* BIT 6,H Z01- */  	BIT(6, REG_H); REG_PC+=2; continue;
			case 0x75: /* BIT 6,L Z01- */  	BIT(6, REG_L); REG_PC+=2; continue;
			case 0x76: /* BIT 6,(HL) Z01- */BIT(6, memory_read8(REG_HL)); REG_PC+=2; continue;
			case 0x77: /* BIT 6,A Z01- */  	BIT(6, REG_A); REG_PC+=2; continue;
			case 0x78: /* BIT 7,B Z01- */  	BIT(7, REG_B); REG_PC+=2; continue;
			case 0x79: /* BIT 7,C Z01- */  	BIT(7, REG_C); REG_PC+=2; continue;
			case 0x7A: /* BIT 7,D Z01- */  	BIT(7, REG_D); REG_PC+=2; continue;
			case 0x7B: /* BIT 7,E Z01- */  	BIT(7, REG_E); REG_PC+=2; continue;
			case 0x7C: /* BIT 7,H Z01- */  	BIT(7, REG_H); REG_PC+=2; continue;
			case 0x7D: /* BIT 7,L Z01- */  	BIT(7, REG_L); REG_PC+=2; continue;
			case 0x7E: /* BIT 7,(HL) Z01- */BIT(7, memory_read8(REG_HL)); REG_PC+=2; continue;
			case 0x7F: /* BIT 7,A Z01- */  	BIT(7, REG_A); REG_PC+=2; continue;
			case 0x80: /* RES 0,B ---- */  	RES(0, REG_B); REG_PC+=2; continue;
			case 0x81: /* RES 0,C ---- */  	RES(0, REG_C); REG_PC+=2; continue;
			case 0x82: /* RES 0,D ---- */  	RES(0, REG_D); REG_PC+=2; continue;
			case 0x83: /* RES 0,E ---- */  	RES(0, REG_E); REG_PC+=2; continue;
			case 0x84: /* RES 0,H ---- */  	RES(0, REG_H); REG_PC+=2; continue;
			case 0x85: /* RES 0,L ---- */  	RES(0, REG_L); REG_PC+=2; continue;
			case 0x86: /* RES 0,(HL) ---- */RES_HL(0); REG_PC+=2; continue;
			case 0x87: /* RES 0,A ---- */  	RES(0, REG_A); REG_PC+=2; continue;
			case 0x88: /* RES 1,B ---- */  	RES(1, REG_B); REG_PC+=2; continue;
			case 0x89: /* RES 1,C ---- */  	RES(1, REG_C); REG_PC+=2; continue;
			case 0x8A: /* RES 1,D ---- */  	RES(1, REG_D); REG_PC+=2; continue;
			case 0x8B: /* RES 1,E ---- */  	RES(1, REG_E); REG_PC+=2; continue;
			case 0x8C: /* RES 1,H ---- */  	RES(1, REG_H); REG_PC+=2; continue;
			case 0x8D: /* RES 1,L ---- */  	RES(1, REG_L); REG_PC+=2; continue;
			case 0x8E: /* RES 1,(HL) ---- */RES_HL(1); REG_PC+=2; continue;
			case 0x8F: /* RES 1,A ---- */  	RES(1, REG_A); REG_PC+=2; continue;
			case 0x90: /* RES 2,B ---- */  	RES(2, REG_B); REG_PC+=2; continue;
			case 0x91: /* RES 2,C ---- */  	RES(2, REG_C); REG_PC+=2; continue;
			case 0x92: /* RES 2,D ---- */  	RES(2, REG_D); REG_PC+=2; continue;
			case 0x93: /* RES 2,E ---- */  	RES(2, REG_E); REG_PC+=2; continue;
			case 0x94: /* RES 2,H ---- */  	RES(2, REG_H); REG_PC+=2; continue;
			case 0x95: /* RES 2,L ---- */  	RES(2, REG_L); REG_PC+=2; continue;
			case 0x96: /* RES 2,(HL) ---- */RES_HL(2); REG_PC+=2; continue;
			case 0x97: /* RES 2,A ---- */  	RES(2, REG_A); REG_PC+=2; continue;
			case 0x98: /* RES 3,B ---- */  	RES(3, REG_B); REG_PC+=2; continue;
			case 0x99: /* RES 3,C ---- */  	RES(3, REG_C); REG_PC+=2; continue;
			case 0x9A: /* RES 3,D ---- */  	RES(3, REG_D); REG_PC+=2; continue;
			case 0x9B: /* RES 3,E ---- */  	RES(3, REG_E); REG_PC+=2; continue;
			case 0x9C: /* RES 3,H ---- */  	RES(3, REG_H); REG_PC+=2; continue;
			case 0x9D: /* RES 3,L ---- */  	RES(3, REG_L); REG_PC+=2; continue;
			case 0x9E: /* RES 3,(HL) ---- */RES_HL(3); REG_PC+=2; continue;
			case 0x9F: /* RES 3,A ---- */  	RES(3, REG_A); REG_PC+=2; continue;
			case 0xA0: /* RES 4,B ---- */  	RES(4, REG_B); REG_PC+=2; continue;
			case 0xA1: /* RES 4,C ---- */  	RES(4, REG_C); REG_PC+=2; continue;
			case 0xA2: /* RES 4,D ---- */  	RES(4, REG_D); REG_PC+=2; continue;
			case 0xA3: /* RES 4,E ---- */  	RES(4, REG_E); REG_PC+=2; continue;
			case 0xA4: /* RES 4,H ---- */  	RES(4, REG_H); REG_PC+=2; continue;
			case 0xA5: /* RES 4,L ---- */  	RES(4, REG_L); REG_PC+=2; continue;
			case 0xA6: /* RES 4,(HL) ---- */RES_HL(4); REG_PC+=2; continue;
			case 0xA7: /* RES 4,A ---- */  	RES(4, REG_A); REG_PC+=2; continue;
			case 0xA8: /* RES 5,B ---- */  	RES(5, REG_B); REG_PC+=2; continue;
			case 0xA9: /* RES 5,C ---- */  	RES(5, REG_C); REG_PC+=2; continue;
			case 0xAA: /* RES 5,D ---- */  	RES(5, REG_D); REG_PC+=2; continue;
			case 0xAB: /* RES 5,E ---- */  	RES(5, REG_E); REG_PC+=2; continue;
			case 0xAC: /* RES 5,H ---- */  	RES(5, REG_H); REG_PC+=2; continue;
			case 0xAD: /* RES 5,L ---- */  	RES(5, REG_L); REG_PC+=2; continue;
			case 0xAE: /* RES 5,(HL) ---- */RES_HL(5); REG_PC+=2; continue;
			case 0xAF: /* RES 5,A ---- */  	RES(5, REG_A); REG_PC+=2; continue;
			case 0xB0: /* RES 6,B ---- */  	RES(6, REG_B); REG_PC+=2; continue;
			case 0xB1: /* RES 6,C ---- */ 	RES(6, REG_C); REG_PC+=2; continue;
			case 0xB2: /* RES 6,D ---- */  	RES(6, REG_D); REG_PC+=2; continue;
			case 0xB3: /* RES 6,E ---- */ 	RES(6, REG_E); REG_PC+=2; continue;
			case 0xB4: /* RES 6,H ---- */  	RES(6, REG_H); REG_PC+=2; continue;
			case 0xB5: /* RES 6,L ---- */  	RES(6, REG_L); REG_PC+=2; continue;
			case 0xB6: /* RES 6,(HL) ---- */RES_HL(6); REG_PC+=2; continue;
			case 0xB7: /* RES 6,A ---- */  	RES(6, REG_A); REG_PC+=2; continue;
			case 0xB8: /* RES 7,B ---- */  	RES(7, REG_B); REG_PC+=2; continue;
			case 0xB9: /* RES 7,C ---- */  	RES(7, REG_C); REG_PC+=2; continue;
			case 0xBA: /* RES 7,D ---- */  	RES(7, REG_D); REG_PC+=2; continue;
			case 0xBB: /* RES 7,E ---- */  	RES(7, REG_E); REG_PC+=2; continue;
			case 0xBC: /* RES 7,H ---- */  	RES(7, REG_H); REG_PC+=2; continue;
			case 0xBD: /* RES 7,L ---- */  	RES(7, REG_L); REG_PC+=2; continue;
			case 0xBE: /* RES 7,(HL) ---- */RES_HL(7); REG_PC+=2; continue;
			case 0xBF: /* RES 7,A ---- */  	RES(7, REG_A); REG_PC+=2; continue;
			case 0xC0: /* SET 0,B ---- */  	SET(0, REG_B); REG_PC+=2; continue;
			case 0xC1: /* SET 0,C ---- */  	SET(0, REG_C); REG_PC+=2; continue;
			case 0xC2: /* SET 0,D ---- */  	SET(0, REG_D); REG_PC+=2; continue;
			case 0xC3: /* SET 0,E ---- */  	SET(0, REG_E); REG_PC+=2; continue;
			case 0xC4: /* SET 0,H ---- */  	SET(0, REG_H); REG_PC+=2; continue;
			case 0xC5: /* SET 0,L ---- */  	SET(0, REG_L); REG_PC+=2; continue;
			case 0xC6: /* SET 0,(HL) ---- */SET_HL(0); REG_PC+=2; continue;
			case 0xC7: /* SET 0,A ---- */  	SET(0, REG_A); REG_PC+=2; continue;
			case 0xC8: /* SET 1,B ---- */  	SET(1, REG_B); REG_PC+=2; continue;
			case 0xC9: /* SET 1,C ---- */  	SET(1, REG_C); REG_PC+=2; continue;
			case 0xCA: /* SET 1,D ---- */  	SET(1, REG_D); REG_PC+=2; continue;
			case 0xCB: /* SET 1,E ---- */  	SET(1, REG_E); REG_PC+=2; continue;
			case 0xCC: /* SET 1,H ---- */  	SET(1, REG_H); REG_PC+=2; continue;
			case 0xCD: /* SET 1,L ---- */  	SET(1, REG_L); REG_PC+=2; continue;
			case 0xCE: /* SET 1,(HL) ---- */SET_HL(1); REG_PC+=2; continue;
			case 0xCF: /* SET 1,A ---- */  	SET(1, REG_A); REG_PC+=2; continue;
			case 0xD0: /* SET 2,B ---- */  	SET(2, REG_B); REG_PC+=2; continue;
			case 0xD1: /* SET 2,C ---- */  	SET(2, REG_C); REG_PC+=2; continue;
			case 0xD2: /* SET 2,D ---- */  	SET(2, REG_D); REG_PC+=2; continue;
			case 0xD3: /* SET 2,E ---- */  	SET(2, REG_E); REG_PC+=2; continue;
			case 0xD4: /* SET 2,H ---- */  	SET(2, REG_H); REG_PC+=2; continue;
			case 0xD5: /* SET 2,L ---- */  	SET(2, REG_L); REG_PC+=2; continue;
			case 0xD6: /* SET 2,(HL) ---- */SET_HL(2); REG_PC+=2; continue;
			case 0xD7: /* SET 2,A ---- */  	SET(2, REG_A); REG_PC+=2; continue;
			case 0xD8: /* SET 3,B ---- */  	SET(3, REG_B); REG_PC+=2; continue;
			case 0xD9: /* SET 3,C ---- */  	SET(3, REG_C); REG_PC+=2; continue;
			case 0xDA: /* SET 3,D ---- */  	SET(3, REG_D); REG_PC+=2; continue;
			case 0xDB: /* SET 3,E ---- */  	SET(3, REG_E); REG_PC+=2; continue;
			case 0xDC: /* SET 3,H ---- */  	SET(3, REG_H); REG_PC+=2; continue;
			case 0xDD: /* SET 3,L ---- */  	SET(3, REG_L); REG_PC+=2; continue;
			case 0xDE: /* SET 3,(HL) ---- */SET_HL(3); REG_PC+=2; continue;
			case 0xDF: /* SET 3,A ---- */  	SET(3, REG_A); REG_PC+=2; continue;
			case 0xE0: /* SET 4,B ---- */  	SET(4, REG_B); REG_PC+=2; continue;
			case 0xE1: /* SET 4,C ---- */  	SET(4, REG_C); REG_PC+=2; continue;
			case 0xE2: /* SET 4,D ---- */  	SET(4, REG_D); REG_PC+=2; continue;
			case 0xE3: /* SET 4,E ---- */  	SET(4, REG_E); REG_PC+=2; continue;
			case 0xE4: /* SET 4,H ---- */  	SET(4, REG_H); REG_PC+=2; continue;
			case 0xE5: /* SET 4,L ---- */  	SET(4, REG_L); REG_PC+=2; continue;
			case 0xE6: /* SET 4,(HL) ---- */SET_HL(4); REG_PC+=2; continue;
			case 0xE7: /* SET 4,A ---- */  	SET(4, REG_A); REG_PC+=2; continue;
			case 0xE8: /* SET 5,B ---- */  	SET(5, REG_B); REG_PC+=2; continue;
			case 0xE9: /* SET 5,C ---- */  	SET(5, REG_C); REG_PC+=2; continue;
			case 0xEA: /* SET 5,D ---- */  	SET(5, REG_D); REG_PC+=2; continue;
			case 0xEB: /* SET 5,E ---- */  	SET(5, REG_E); REG_PC+=2; continue;
			case 0xEC: /* SET 5,H ---- */  	SET(5, REG_H); REG_PC+=2; continue;
			case 0xED: /* SET 5,L ---- */  	SET(5, REG_L); REG_PC+=2; continue;
			case 0xEE: /* SET 5,(HL) ---- */SET_HL(5); REG_PC+=2; continue;
			case 0xEF: /* SET 5,A ---- */  	SET(5, REG_A); REG_PC+=2; continue;
			case 0xF0: /* SET 6,B ---- */  	SET(6, REG_B); REG_PC+=2; continue;
			case 0xF1: /* SET 6,C ---- */  	SET(6, REG_C); REG_PC+=2; continue;
			case 0xF2: /* SET 6,D ---- */  	SET(6, REG_D); REG_PC+=2; continue;
			case 0xF3: /* SET 6,E ---- */  	SET(6, REG_E); REG_PC+=2; continue;
			case 0xF4: /* SET 6,H ---- */  	SET(6, REG_H); REG_PC+=2; continue;
			case 0xF5: /* SET 6,L ---- */  	SET(6, REG_L); REG_PC+=2; continue;
			case 0xF6: /* SET 6,(HL) ---- */SET_HL(6); REG_PC+=2; continue;
			case 0xF7: /* SET 6,A ---- */  	SET(6, REG_A); REG_PC+=2; continue;
			case 0xF8: /* SET 7,B ---- */  	SET(7, REG_B); REG_PC+=2; continue;
			case 0xF9: /* SET 7,C ---- */  	SET(7, REG_C); REG_PC+=2; continue;
			case 0xFA: /* SET 7,D ---- */  	SET(7, REG_D); REG_PC+=2; continue;
			case 0xFB: /* SET 7,E ---- */  	SET(7, REG_E); REG_PC+=2; continue;
			case 0xFC: /* SET 7,H ---- */  	SET(7, REG_H); REG_PC+=2; continue;
			case 0xFD: /* SET 7,L ---- */  	SET(7, REG_L); REG_PC+=2; continue;
			case 0xFE: /* SET 7,(HL) ---- */SET_HL(7); REG_PC+=2; continue;
			case 0xFF: /* SET 7,A ---- */  	SET(7, REG_A); REG_PC+=2; continue;
			}
		case 0xCC: /* CALL Z,nn ---- */  	if(FLG_Z){CALL;}else{REG_PC+=3;} continue;
		case 0xCD: /* CALL nn ---- */  		CALL; continue;
		case 0xCE: /* ADC A,# Z0HC */  		BINOPA_ADD(OPERAND8+FLG_C_01); REG_PC+=2; continue;
		case 0xCF: /* RST 08H ---- */  		RST(0x08); continue;
		case 0xD0: /* RET NC ---- */  		if(!FLG_C){RET;}else{REG_PC+=1;} continue;
		case 0xD1: /* POP DE ---- */  		POP(REG_DE); REG_PC+=1; continue;
		case 0xD2: /* JP NC,nn ---- */  	if(!FLG_C){JP(OPERAND16);}else{REG_PC+=3;} continue;
		case 0xD4: /* CALL NC,nn ---- */ 	if(!FLG_C){CALL;}else{REG_PC+=3;} continue;
		case 0xD5: /* PUSH DE ---- */  		PUSH(REG_DE); REG_PC+=1; continue;
		case 0xD6: /* SUB # Z1HC */  		BINOPA_SUB(OPERAND8); REG_PC+=2; continue;
		case 0xD7: /* RST 10H ---- */  		RST(0x10); continue;
		case 0xD8: /* RET C ---- */  		if(FLG_C){RET;}else{REG_PC+=1;} continue;
		case 0xD9: /* RETI - ---- */ 		RET; FLG_IME=1; continue;
		case 0xDA: /* JP C,nn ---- */  		if(FLG_C){JP(OPERAND16);}else{REG_PC+=3;} continue;
		case 0xDC: /* CALL C,nn ---- */  	if(FLG_C){CALL;}else{REG_PC+=3;} continue;
		case 0xDE: /* SBC A,# Z1HC */  		BINOPA_SUB(OPERAND8-FLG_C_01); REG_PC+=2; continue;
		case 0xDF: /* RST 18H ---- */  		RST(0x18); continue;
		case 0xE0: /* LD ($FF00+n),A ---- */memory_write8(0xff00+OPERAND8, REG_A); REG_PC+=2; continue;
		case 0xE1: /* POP HL ---- */  		POP(REG_HL); REG_PC+=1; continue;
		case 0xE2: /* LD ($FF00+C),A ---- */memory_write8(0xff00+REG_C, REG_A); REG_PC+=1; continue;
		case 0xE5: /* PUSH HL ---- */  		PUSH(REG_HL); REG_PC+=1; continue;
		case 0xE6: /* AND # Z010 */  		BINOPA_LOGIC(&, OPERAND8, 0, 1); REG_PC+=2; continue;
		case 0xE7: /* RST 20H ---- */  		RST(0x20); continue;
		case 0xE8: /* ADD SP,n 00HC */  	ADDSP_16; REG_PC+=2; continue;
		case 0xE9: /* JP (HL) ---- */  		JP(memory_read8(REG_HL)); continue;
		case 0xEA: /* LD (nn),A ---- */  	memory_write8(OPERAND16, REG_A); REG_PC+=3; continue;
		case 0xEE: /* XOR * Z000 */  		BINOPA_LOGIC(^, OPERAND8, 0, 0);REG_PC+=2; continue;
		case 0xEF: /* RST 28H ---- */  		RST(0x28); continue;
		case 0xF0: /* LD A,($FF00+n) ---- */REG_A=0xff00+OPERAND8; REG_PC+=2; continue;
		case 0xF1: /* POP AF ---- */  		POP_AF; REG_PC+=1; continue;
		case 0xF2: /* LD A,($FF00+C) ---- */REG_A=0xff00+REG_C; REG_PC+=1; continue;
		case 0xF3: /* DI - ---- */  		FLG_IME=0; REG_PC+=1; continue;
		case 0xF5: /* PUSH AF ---- */  		PUSH_AF; REG_PC+=1; continue;
		case 0xF6: /* OR # Z000 */  		BINOPA_LOGIC(|, OPERAND8, 0, 0);REG_PC+=2; continue;
		case 0xF7: /* RST 30H ---- */  		RST(0x30); continue;
		case 0xF8: /* LDHL SP,n 00HC */  	REG_HL=REG_SP+(int8_t)(OPERAND8); REG_PC+=2; continue;
		case 0xF9: /* LD SP,HL ---- */  	REG_SP=REG_HL; REG_PC+=1; continue;
		case 0xFA: /* LD A,(nn) ---- */  	REG_A=memory_read8(OPERAND16); REG_PC+=3; continue;
		case 0xFB: /* EI - ---- */  		FLG_IME=1; REG_PC+=1; continue;
		case 0xFE: /* CP # Z1HC */  		BINOPA_CP(OPERAND8); REG_PC+=2; continue;
		case 0xFF: /* RST 38H ---- */  		RST(0x38); continue;
		}
	}
}


int disas_one(uint16_t pc) {
	switch(memory_read8(pc)){
	case 0x36:
		//LD (HL),n
		DISAS_PRINT("LD (HL),%hhX", memory_read8(pc+1));
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
		DISAS_PRINT("LD A,(%hhX%hhX)", memory_read8(pc+2), memory_read8(pc+1));
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
		DISAS_PRINT("LD (%hhX%hhX),SP", memory_read8(pc+2), memory_read8(pc+1));
		return 3;
	case 0xea:
		//LD (nn),A
		DISAS_PRINT("LD (%hhX%hhX),A", memory_read8(pc+2), memory_read8(pc+1));
		return 3;
	case 0xf0:
		//LD A,(FF00+n)
		DISAS_PRINT("LD A,(FF00+%hhX)", memory_read8(pc+1));
		return 2;
	case 0xe0:
		//LD (FF00+n),A
		DISAS_PRINT("LD (FF00+%hhX),A", memory_read8(pc+1));
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
		DISAS_PRINT("ADD A,%hhX", memory_read8(pc+1));
		return 2;
	case 0x86:
		//ADD A,(HL)
		DISAS_PRINT("ADD A,(HL)");
		return 1;
	case 0xce:
		//ADC A,n
		DISAS_PRINT("ADC A,%hhX", memory_read8(pc+1));
		return 2;
	case 0x8e:
		//ADC A,(HL)
		DISAS_PRINT("ADC A,(HL)");
		return 1;
	case 0xd6:
		//SUB n
		DISAS_PRINT("SUB %hhX", memory_read8(pc+1));
		return 2;
	case 0x96:
		//SUB (HL)
		DISAS_PRINT("SUB (HL)");
		return 1;
	case 0xde:
		//SBC A,n
		DISAS_PRINT("SBC A,%hhX", memory_read8(pc+1));
		return 2;
	case 0x9e:
		//SBC A,(HL)
		DISAS_PRINT("SBC A,(HL)");
		return 1;
	case 0xe6:
		//AND n
		DISAS_PRINT("AND %hhX", memory_read8(pc+1));
		return 2;
	case 0xa6:
		//AND (HL)
		DISAS_PRINT("AND (HL)");
		return 1;
	case 0xee:
		//XOR n
		DISAS_PRINT("XOR %hhX", memory_read8(pc+1));
		return 2;
	case 0xae:
		//XOR (HL)
		DISAS_PRINT("XOR (HL)");
		return 1;
	case 0xf6:
		//OR n
		DISAS_PRINT("OR %hhX", memory_read8(pc+1));
		return 2;
	case 0xb6:
		//OR (HL)
		DISAS_PRINT("OR (HL)");
		return 1;
	case 0xfe:
		//CP n
		DISAS_PRINT("CP %hhX", memory_read8(pc+1));
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
		DISAS_PRINT("ADD SP,%hhX", memory_read8(pc+1));
		return 2;
	case 0xf8:
		//LD HL,SP+dd
		DISAS_PRINT("LD HL,SP+%hhX", memory_read8(pc+1));
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

		pc++;

		switch(memory_read8(pc)){
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

		switch(BIT7_6(memory_read8(pc))){
		case 0x0:
			switch(BIT5_3(memory_read8(pc))){
			case 0x0:
				//RLC r
				DISAS_PRINT("RLC %s", r_name[BIT2_0(memory_read8(pc))]);
				return 2;
			case 0x1:
				//RRC r
				DISAS_PRINT("RRC %s", r_name[BIT2_0(memory_read8(pc))]);
				return 2;
			case 0x2:
				//RL r
				DISAS_PRINT("RL %s", r_name[BIT2_0(memory_read8(pc))]);
				return 2;
			case 0x3:
				//RR r
				DISAS_PRINT("RR %s", r_name[BIT2_0(memory_read8(pc))]);
				return 2;
			case 0x4:
				//SLA r
				DISAS_PRINT("SLA %s", r_name[BIT2_0(memory_read8(pc))]);
				return 2;
			case 0x5:
				//SRA r
				DISAS_PRINT("SRA %s", r_name[BIT2_0(memory_read8(pc))]);
				return 2;
			case 0x6:
				//SWAP r
				DISAS_PRINT("SWAP %s", r_name[BIT2_0(memory_read8(pc))]);
				return 2;
			case 0x7:
				//SRL r
				DISAS_PRINT("SRL %s", r_name[BIT2_0(memory_read8(pc))]);
				return 2;
			}
			break;
		case 0x1:
			if(BIT2_0(memory_read8(pc))==0x6)
				//BIT b,(HL)
				DISAS_PRINT("BIT %d,(HL)", BIT5_3(memory_read8(pc)));
			else
				//BIT b,r
				DISAS_PRINT("BIT %d,%s", BIT5_3(memory_read8(pc)), r_name[BIT2_0(memory_read8(pc))]);
			return 2;
		case 0x2:
			if(BIT2_0(memory_read8(pc))==0x6)
				//RES b,(HL)
				DISAS_PRINT("RES %d,(HL)", BIT5_3(memory_read8(pc)));
			else
				//RES b,r
				DISAS_PRINT("RES %d,%s", BIT5_3(memory_read8(pc)), r_name[BIT2_0(memory_read8(pc))]);
			return 2;
		case 0x3:
			if(BIT2_0(memory_read8(pc))==0x6)
				//SET b,(HL)
				DISAS_PRINT("SET %d,(HL)", BIT5_3(memory_read8(pc)));
			else
				//SET b,r
				DISAS_PRINT("SET %d,%s", BIT5_3(memory_read8(pc)), r_name[BIT2_0(memory_read8(pc))]);
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
		if(memory_read8(pc+1)==0x0){
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
		DISAS_PRINT("JP %hhX%hhX", memory_read8(pc+2), memory_read8(pc+1));
		return 3;
	case 0xe9:
		//JP HL
		DISAS_PRINT("JP HL");
		return 1;
	case 0x18:
		//JR PC+e
		DISAS_PRINT("JR PC+%hhX", memory_read8(pc+1));
		return 2;
	case 0xcd:
		//CALL nn
		DISAS_PRINT("CALL %hhX%hhX", memory_read8(pc+2), memory_read8(pc+1));
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

	switch(BIT7_6(memory_read8(pc))){
	case 0x0:
		switch(BIT2_0(memory_read8(pc))){
		case 0x0:
			switch(BIT5_3(memory_read8(pc))){
			case 0x7:
				//JR C,e
				DISAS_PRINT("JR C,%hhX", memory_read8(pc+1)+2);
				return 2;
			case 0x6:
				//JR NC,e
				DISAS_PRINT("JR NC,%hhX", memory_read8(pc+1)+2);
				return 1;
			case 0x5:
				//JR Z,e
				DISAS_PRINT("JR Z,%hhX", memory_read8(pc+1)+2);
				return 1;
			case 0x4:
				//JR NZ,e
				DISAS_PRINT("JR NZ,%hhX", memory_read8(pc+1)+2);
				return 1;
			}
			break;
		case 0x1:
			if(BIT3(memory_read8(pc))){
				//ADD HL,ss
				DISAS_PRINT("ADD HL,%s", ss_name[BIT5_4(memory_read8(pc))]);
				return 1;
			}else{
				//LD dd,nn
				DISAS_PRINT("LD %s,%hX", dd_name[BIT5_4(memory_read8(pc))], memory_read8(pc+1));
				return 2;
			}
			break;
		case 0x3:
			if(BIT3(memory_read8(pc)))
				//DEC ss
				DISAS_PRINT("DEC %s", ss_name[BIT5_4(memory_read8(pc))]);
			else
				//INC ss
				DISAS_PRINT("INC %s", ss_name[BIT5_4(memory_read8(pc))]);
			return 1;
			break;
		case 0x4:
			//INC r
			DISAS_PRINT("INC %s", r_name[BIT5_3(memory_read8(pc))]);
			return 1;
		case 0x5:
			//DEC r
			DISAS_PRINT("DEC %s", r_name[BIT5_3(memory_read8(pc))]);
			return 1;
		case 0x6:
			//LD r,n
			DISAS_PRINT("LD %s,%hhX", r_name[BIT5_3(memory_read8(pc))], memory_read8(pc+1));
			return 2;
		}
		break;
	case 0x1:
		if(BIT2_0(memory_read8(pc))==0x6)
			//LD r,(HL)
			DISAS_PRINT("LD %s,(HL)", r_name[BIT5_3(memory_read8(pc))]);
		else if(BIT5_3(memory_read8(pc))==0x6)
			//LD (HL),r
			DISAS_PRINT("LD (HL),%s", r_name[BIT2_0(memory_read8(pc))]);
		else
			//LD r,r'
			DISAS_PRINT("LD %s,%s", r_name[BIT5_3(memory_read8(pc))], r_name[BIT2_0(memory_read8(pc))]);
		return 1;
	case 0x2:
		switch(BIT5_3(memory_read8(pc))){
		case 0x0:
			//ADD A,r
			DISAS_PRINT("ADD A,%s", r_name[BIT2_0(memory_read8(pc))]);
			return 1;
		case 0x1:
			//ADC A,r
			DISAS_PRINT("ADC A,%s", r_name[BIT2_0(memory_read8(pc))]);
			return 1;
		case 0x2:
			//SUB A,r
			DISAS_PRINT("SUB %s", r_name[BIT2_0(memory_read8(pc))]);
			return 1;
		case 0x3:
			//SBC A,r
			DISAS_PRINT("SBC A,%s", r_name[BIT2_0(memory_read8(pc))]);
			return 1;
		case 0x4:
			//AND A,r
			DISAS_PRINT("AND %s", r_name[BIT2_0(memory_read8(pc))]);
			return 1;
		case 0x5:
			//XOR A,r
			DISAS_PRINT("XOR %s", r_name[BIT2_0(memory_read8(pc))]);
			return 1;
		case 0x6:
			//OR A,r
			DISAS_PRINT("OR %s", r_name[BIT2_0(memory_read8(pc))]);
			return 1;
		case 0x7:
			//CP A,r
			DISAS_PRINT("CP %s", r_name[BIT2_0(memory_read8(pc))]);
			return 1;
		}
		break;
	case 0x3:
		switch(BIT2_0(memory_read8(pc))){
		case 0x0:
			//RET cc
			DISAS_PRINT("RET %s", cc_name[BIT5_3(memory_read8(pc))]);
			return 1;
		case 0x1:
			//POP qq
			DISAS_PRINT("POP %s", qq_name[BIT5_4(memory_read8(pc))]);
			return 1;
		case 0x2:
			//JP cc,nn
			DISAS_PRINT("JP %s,%hhX%hhX", cc_name[BIT5_3(memory_read8(pc))], memory_read8(pc+2), memory_read8(pc+1));
			return 3;
		case 0x4:
			//CALL cc,nn
			DISAS_PRINT("CALL %s,%hhX%hhX", cc_name[BIT5_3(memory_read8(pc))], memory_read8(pc+2), memory_read8(pc+1));
			return 3;
		case 0x5:
			//PUSH qq
			DISAS_PRINT("PUSH %s", qq_name[BIT5_4(memory_read8(pc))]);
			return 1;
		case 0x7:
			//RST p
			DISAS_PRINT("RST %hhX", p_table[BIT5_3(memory_read8(pc))]);
			return 2;
		}
		break;
	}

	return 0;
}



