#include <stdio.h>
#include <inttypes.h>

#include "cpu.h"

#define SHOW_DISAS

#ifdef SHOW_DISAS
    #define DISAS_PRINT( fmt, ... ) ((void)0)
#else
    #define DISAS_PRINT( fmt, ... ) \
        fprintf( stderr, \
                  fmt "\n", \
                  ##__VA_ARGS__ \
        )
#endif

const char *r_name[] = {"B", "C", "D", "E", "H", "L", NULL, "A"};
const char *dd_name[] = {"BC", "DE", "HL", "SP"};
const char *qq_name[] = {"BC", "DE", "HL", "AF"};

void disas_one(uint8_t *code, int len) {
	switch(*code){
	case 0x36:
		DISAS_PRINT("ld (HL),%X", *(code+1));
		break;
	case 0x0a:
		DISAS_PRINT("ld A,(BC)");
		break;
	case 0x1a:
		DISAS_PRINT("ld A,(DE)");
		break;
	case 0xfa:
		DISAS_PRINT("ld A,(%X)", *((uint16_t*)(code+1)));
		break;
	case 0x02:
		DISAS_PRINT("ld (BC),A");
		break;
	case 0x12:
		DISAS_PRINT("ld (DE),A");
		break;
	case 0xea:
		DISAS_PRINT("ld (%X),A", *((uint16_t*)(code+1)));
		break;
	case 0xf0:
		DISAS_PRINT("ld A,(FF00+%X)", *(code+1));
		break;
	case 0xe0:
		DISAS_PRINT("ld (FF00+%X),A", *(code+1));
		break;
	case 0xf2:
		DISAS_PRINT("ld A,(FF00+C)");
		break;
	case 0xe2:
		DISAS_PRINT("ld (FF00+C),A");
		break;
	case 0x22:
		DISAS_PRINT("ldi (HL),A");
		break;
	case 0x2a:
		DISAS_PRINT("ldi A,(HL)");
		break;
	case 0x32:
		DISAS_PRINT("ldd (HL),A");
		break;
	case 0x3a:
		DISAS_PRINT("ldd A,(HL)");
		break;
	case 0xf9:
		DISAS_PRINT("ld SP,HL");
		break;
	case 0xc6:
		DISAS_PRINT("add A,%X", *(code+1));
		break;
	case 0x86:
		DISAS_PRINT("add A,(HL)");
		break;
	case 0xce:
		DISAS_PRINT("adc A,%X", *(code+1));
		break;
	case 0x8e:
		DISAS_PRINT("adc A,(HL)");
		break;
	case 0xd6:
		DISAS_PRINT("sub %X", *(code+1));
		break;
	case 0x96:
		DISAS_PRINT("sub (HL)");
		break;
	case 0xde:
		DISAS_PRINT("sbc A,%X", *(code+1));
		break;
	case 0x9e:
		DISAS_PRINT("sbc A,(HL)");
		break;
	case 0xe6:
		DISAS_PRINT("and %X", *(code+1));
		break;
	case 0xa6:
		DISAS_PRINT("and (HL)");
		break;
	case 0xee:
		DISAS_PRINT("xor %X", *(code+1));
		break;
	case 0xae:
		DISAS_PRINT("xor (HL)");
		break;
	case 0xf6:
		DISAS_PRINT("or %X", *(code+1));
		break;
	case 0xb6:
		DISAS_PRINT("or (HL)");
		break;
	case 0xfe:
		DISAS_PRINT("cp %X", *(code+1));
		break;
	case 0xbe:
		DISAS_PRINT("cp (HL)");
		break;
	case 0x34:
		DISAS_PRINT("inc (HL)");
		break;
	case 0x35:
		DISAS_PRINT("dec (HL)");
		break;
	case 0x27:
		DISAS_PRINT("daa");
		break;
	case 0x2f:
		DISAS_PRINT("cpl");
		break;
	case 0xe8:
		DISAS_PRINT("add SP,%x", *((int8_t*)(code+1)));
		break;
	case 0xf8:
		DISAS_PRINT("ld HL,SP+%x", *((int8_t*)(code+1)));
		break;
	case 0x07:
		DISAS_PRINT("rlca");
		break;
	case 0x17:
		DISAS_PRINT("rla");
		break;
	case 0x0f:
		DISAS_PRINT("rrca");
		break;
	case 0x1f:
		DISAS_PRINT("rra");
		break;
	case 0xcb:
		switch(*(code+1)){
		case 0x06:
			DISAS_PRINT("rlc (HL)");
			break;
		case 0x16:
			DISAS_PRINT("rl (HL)");
			break;
		case 0x0e:
			DISAS_PRINT("rrc (HL)");
			break;
		case 0x1e:
			DISAS_PRINT("rr (HL)");
			break;
		case 0x26:
			DISAS_PRINT("sla (HL)");
			break;
		case 0x36:
			DISAS_PRINT("swap (HL)");
			break;
		case 0x2e:
			DISAS_PRINT("sra (HL)");
			break;
		case 0x3e:
			DISAS_PRINT("srl (HL)");
			break;
		}

		switch((*code>>6)&0x3){
		case 0x0:
			switch((*code>>3)&0x7){
			case 0x0:
				DISAS_PRINT("rlc %s", r_name[*code&0x7]);
				break;
			case 0x1:
				DISAS_PRINT("rrc %s", r_name[*code&0x7]);
				break;
			case 0x2:
				DISAS_PRINT("rl %s", r_name[*code&0x7]);
				break;
			case 0x3:
				DISAS_PRINT("rr %s", r_name[*code&0x7]);
				break;
			case 0x4:
				DISAS_PRINT("sla %s", r_name[*code&0x7]);
				break;
			case 0x5:
				DISAS_PRINT("sra %s", r_name[*code&0x7]);
				break;
			case 0x6:
				DISAS_PRINT("swap %s", r_name[*code&0x7]);
				break;
			case 0x7:
				DISAS_PRINT("srl %s", r_name[*code&0x7]);
				break;
			}
			break;
		case 0x1:
			if((*code&0x7)==0x6)
				DISAS_PRINT("bit %d,(HL)", (*code>>3)&0x7);
			else
				DISAS_PRINT("bit %d,%s", (*code>>3)&0x7, r_name[*code&0x7]);
			break;
		case 0x2:
			if((*code&0x7)==0x6)
				DISAS_PRINT("set %d,(HL)", (*code>>3)&0x7);
			else
				DISAS_PRINT("set %d,%s", (*code>>3)&0x7, r_name[*code&0x7]);
			break;
		case 0x3:
			if((*code&0x7)==0x6)
				DISAS_PRINT("res %d,(HL)", (*code>>3)&0x7);
			else
				DISAS_PRINT("res %d,%s", (*code>>3)&0x7, r_name[*code&0x7]);
			break;
		}
		break;
	case 0x3f:
		DISAS_PRINT("ccf");
		break;
	case 0x37:
		DISAS_PRINT("scf");
		break;
	case 0x00:
		DISAS_PRINT("nop");
		break;
	case 0x76:
		DISAS_PRINT("halt");
		break;
	case 0x10:
		//2 byte!!!!
		DISAS_PRINT("stop");
		break;
	case 0xf3:
		DISAS_PRINT("di");
		break;
	case 0xfb:
		DISAS_PRINT("ei");
		break;
	case 0xc3:
		DISAS_PRINT("jp %X", *((uint16_t*)(code+1)));
		break;
	case 0xe9:
		DISAS_PRINT("jp HL");
		break;
	case 0x18:
		DISAS_PRINT("jp PC+%X", *((int8_t*)(code+1)));
		break;
	case 0xcd:
		DISAS_PRINT("call %X", *((uint16_t*)(code+1)));
		break;
	case 0xc9:
		DISAS_PRINT("ret");
		break;
	case 0xd9:
		DISAS_PRINT("reti");
		break;
	}

	switch(*code&0xc0){
	case 0x00:
		//bit7-6: 00
		switch(*code&0x7){
		case 0x0:
			DISAS_PRINT("jr %s,%X", cc_name[(*code>>3)&0x7], *code&0x7);
			break;
		case 0x1:
			if((*code>>3)&0x1)
				DISAS_PRINT("add HL,%X", ss_name[(*code>>4)&0x3]);
			else
				DISAS_PRINT("ld %s,%X", dd_name[(*code>>4)&0x3]);
			break;
		case 0x3:
			if((*code>>3)&0x1)
				DISAS_PRINT("dec %s", ss_name[(*code>>4)&0x3]);
			else
				DISAS_PRINT("inc %s", ss_name[(*code>>4)&0x3]);
			break;
		case 0x4:
			DISAS_PRINT("inc %s", r_name[(*code>>3)&0x3]);
			break;
		case 0x5:
			DISAS_PRINT("dec %s", r_name[(*code>>3)&0x3]);
			break;
		case 0x6:
			DISAS_PRINT("ld %s,%X", r_name[(*code>>3)&0x3], *(code+1));
			break;
		}
		break;
	case 0x40:
		//bit7-6: 01
		if((*code&0x7)==0x6){
			DISAS_PRINT("ld %s,(HL)", r_name[(*code>>3)&0x7]);
		}else if(((*code>>3)&0x7)==0x6){
			DISAS_PRINT("ld (HL),%s", r_name[(*code>>3)&0x7]);
		}else{
			DISAS_PRINT("ld %s,%s", r_name[*code&0x7], r_name[(*code>>3)&0x7]);
		}
		break;
	case 0x80:
		//bit7-6: 10
		switch(*code>>3){
		case 0x0:
			DISAS_PRINT("add A,%s", r_name[*code&0x7]);
			break;
		case 0x1:
			DISAS_PRINT("adc A,%s", r_name[*code&0x7]);
			break;
		case 0x2:
			DISAS_PRINT("sub %s", r_name[*code&0x7]);
			break;
		case 0x3:
			DISAS_PRINT("sbc A,%s", r_name[*code&0x7]);
			break;
		case 0x4:
			DISAS_PRINT("and %s", r_name[*code&0x7]);
			break;
		case 0x5:
			DISAS_PRINT("xor %s", r_name[*code&0x7]);
			break;
		case 0x6:
			DISAS_PRINT("or %s", r_name[*code&0x7]);
			break;
		case 0x7:
			DISAS_PRINT("cp %s", r_name[*code&0x7]);
			break;
		}
		break;
	case 0xc0:
		//bit7-6: 11
		switch(*code&0x3){
		case 0x0:
			DISAS_PRINT("ret %s", cc_name[(*code>>3)&0x7]);
			break;
		case 0x1:
			DISAS_PRINT("pop %s", qq_name[(*code>>4)&0x3]);
			break;
		case 0x2:
			DISAS_PRINT("jp %s,%X", cc_name[(*code>>3)&0x7], *((uint16_t*)(code+1)));
			break;
		case 0x4:
			DISAS_PRINT("call %s,%X", cc_name[(*code>>3)&0x7], *((uint16_t*)(code+1)));
			break;
		case 0x5:
			DISAS_PRINT("push %s", qq_name[(*code>>4)&0x3]);
			break;
		case 0x7:
			DISAS_PRINT("rst %X", t_tbl[(*code>>3)&0x7]);
			break;
		}
		break;
	}
}

void disas(uint8_t *code, int len) {

}

int main() {

	return 0;
}
