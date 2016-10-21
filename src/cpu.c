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

#define BIT7_6(v) (((v)>>6)&0x3)
#define BIT5_3(v) (((v)>>3)&0x7)
#define BIT2_0(v) ((v)&0x7)
#define BIT5_4(v) (((v)>>4)&0x3)
#define BIT3(v) (((v)>>3)&0x1)

const char *r_name[] = {"B", "C", "D", "E", "H", "L", NULL, "A"};
const char *dd_name[] = {"BC", "DE", "HL", "SP"};
const char *qq_name[] = {"BC", "DE", "HL", "AF"};
const uint8_t p_table[] = {0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38};

int disas_one(uint8_t *code, int len) {
	switch(*code){
	case 0x36:
		//LD (HL),n
		DISAS_PRINT("LD (HL),%hhX", *(code+1));
		return 2;
	case 0x0a:
		//LD A,(BC)
		DISAS_PRINT("LD A,(BC)");
		break;
	case 0x1a:
		//LD A,(DE)
		DISAS_PRINT("LD A,(DE)");
		break;
	case 0xfa:
		//LD A,(nn)
		DISAS_PRINT("LD A,(%hX)", *(code+1));
		break;
	case 0x02:
		//LD (BC),A
		DISAS_PRINT("LD (BC),A");
		break;
	case 0x12:
		//LD (DE),A
		DISAS_PRINT("LD (DE),A");
		break;
	case 0x08:
		//LD (nn),SP
		DISAS_PRINT("LD (%hX),SP", *(code+1));
		break;
	case 0xea:
		//LD (nn),A
		DISAS_PRINT("LD (%hX),A", *(code+1));
		break;
	case 0xf0:
		//LD A,(FF00+n)
		DISAS_PRINT("LD A,(FF00+%hhX)", *(code+1));
		break;
	case 0xe0:
		//LD (FF00+n),A
		DISAS_PRINT("LD (FF00+%hhX),A", *(code+1));
		break;
	case 0xf2:
		//LD A,(FF00+C)
		DISAS_PRINT("LD A,(FF00+C)");
		break;
	case 0xe2:
		//LD (FF00+C),A
		DISAS_PRINT("LD (FF00+C),A");
		break;
	case 0x22:
		//LDI (HL),A
		DISAS_PRINT("LDI (HL),A");
		break;
	case 0x2a:
		//LDI A,(HL)
		DISAS_PRINT("LDI A,(HL)");
		break;
	case 0x32:
		//LDD (HL),A
		DISAS_PRINT("LDD (HL),A");
		break;
	case 0x3a:
		//LDD A,(HL)
		DISAS_PRINT("LDD A,(HL)");
		break;
	case 0xf9:
		//LD SP,HL
		DISAS_PRINT("LD SP,HL");
		break;
	case 0xc6:
		//ADD A,n
		DISAS_PRINT("ADD A,%hhX", *(code+1));
		break;
	case 0x86:
		//ADD A,(HL)
		DISAS_PRINT("ADD A,(HL)");
		break;
	case 0xce:
		//ADC A,n
		DISAS_PRINT("ADC A,%hhX", *(code+1));
		break;
	case 0x8e:
		//ADC A,(HL)
		DISAS_PRINT("ADC A,(HL)");
		break;
	case 0xd6:
		//SUB n
		DISAS_PRINT("SUB %hhX", *(code+1));
		break;
	case 0x96:
		//SUB (HL)
		DISAS_PRINT("SUB (HL)");
		break;
	case 0xde:
		//SBC A,n
		DISAS_PRINT("SBC A,%hhX", *(code+1));
		break;
	case 0x9e:
		//SBC A,(HL)
		DISAS_PRINT("SBC A,(HL)");
		break;
	case 0xe6:
		//AND n
		DISAS_PRINT("AND %hhX", *(code+1));
		break;
	case 0xa6:
		//AND (HL)
		DISAS_PRINT("AND (HL)");
		break;
	case 0xee:
		//XOR n
		DISAS_PRINT("XOR %hhX", *(code+1));
		break;
	case 0xae:
		//XOR (HL)
		DISAS_PRINT("XOR (HL)");
		break;
	case 0xf6:
		//OR n
		DISAS_PRINT("OR %hhX", *(code+1));
		break;
	case 0xb6:
		//OR (HL)
		DISAS_PRINT("OR (HL)");
		break;
	case 0xfe:
		//CP n
		DISAS_PRINT("CP %hhX", *(code+1));
		break;
	case 0xbe:
		//CP (HL)
		DISAS_PRINT("CP (HL)");
		break;
	case 0x34:
		//INC (HL)
		DISAS_PRINT("INC (HL)");
		break;
	case 0x35:
		//DEC (HL)
		DISAS_PRINT("DEC (HL)");
		break;
	case 0x27:
		//DAA
		DISAS_PRINT("daa");
		break;
	case 0x2f:
		//CPL
		DISAS_PRINT("CPL");
		break;
	case 0xe8:
		//ADD SP,dd
		DISAS_PRINT("ADD SP,%hhX", *(code+1));
		break;
	case 0xf8:
		//LD HL,SP+dd
		DISAS_PRINT("LD HL,SP+%hhX", *(code+1));
		break;
	case 0x07:
        //RLCA
		DISAS_PRINT("RLCA");
		break;
	case 0x17:
		//RLA
		DISAS_PRINT("RLA");
		break;
	case 0x0f:
		//RRCA
		DISAS_PRINT("RRCA");
		break;
	case 0x1f:
		//RRA
		DISAS_PRINT("RRA");
		break;
	case 0xcb:

		code++;

		switch(*code){
		case 0x06:
			//RLC (HL)
			DISAS_PRINT("RLC (HL)");
			break;
		case 0x16:
			//RL (HL)
			DISAS_PRINT("RL (HL)");
			break;
		case 0x0e:
			//RRC (HL)
			DISAS_PRINT("RRC (HL)");
			break;
		case 0x1e:
			//RR (HL)
			DISAS_PRINT("RR (HL)");
			break;
		case 0x26:
			//SLA (HL)
			DISAS_PRINT("SLA (HL)");
			break;
		case 0x36:
			//SWAP (HL)
			DISAS_PRINT("SWAP (HL)");
			break;
		case 0x2e:
			//SRA (HL)
			DISAS_PRINT("SRA (HL)");
			break;
		case 0x3e:
			//SRL (HL)
			DISAS_PRINT("SRL (HL)");
			break;
		}

		switch(BIT7_6(*code)){
		case 0x0:
			switch(BIT5_3(*code)){
			case 0x0:
				//RLC r
				DISAS_PRINT("RLC %s", r_name[BIT2_0(*code)]);
				break;
			case 0x1:
				//RRC r
				DISAS_PRINT("RRC %s", r_name[BIT2_0(*code)]);
				break;
			case 0x2:
				//RL r
				DISAS_PRINT("RL %s", r_name[BIT2_0(*code)]);
				break;
			case 0x3:
				//RR r
				DISAS_PRINT("RR %s", r_name[BIT2_0(*code)]);
				break;
			case 0x4:
				//SLA r
				DISAS_PRINT("SLA %s", r_name[BIT2_0(*code)]);
				break;
			case 0x5:
				//SRA r
				DISAS_PRINT("SRA %s", r_name[BIT2_0(*code)]);
				break;
			case 0x6:
				//SWAP r
				DISAS_PRINT("SWAP %s", r_name[BIT2_0(*code)]);
				break;
			case 0x7:
				//SRL r
				DISAS_PRINT("SRL %s", r_name[BIT2_0(*code)]);
				break;
			}
			break;
		case 0x1:
			if(BIT2_0(*code)==0x6)
				//BIT b,(HL)
				DISAS_PRINT("BIT %d,(HL)", BIT5_3(*code));
			else
				//BIT b,r
				DISAS_PRINT("BIT %d,%s", BIT5_3(*code), r_name[BIT2_0(*code)]);
			break;
		case 0x2:
			if(BIT2_0(*code)==0x6)
				//RES b,(HL)
				DISAS_PRINT("RES %d,(HL)", BIT5_3(*code));
			else
				//RES b,r
				DISAS_PRINT("RES %d,%s", BIT5_3(*code), r_name[BIT2_0(*code)]);
			break;
		case 0x3:
			if(BIT2_0(*code)==0x6)
				//SET b,(HL)
				DISAS_PRINT("SET %d,(HL)", BIT5_3(*code));
			else
				//SET b,r
				DISAS_PRINT("SET %d,%s", BIT5_3(*code), r_name[BIT2_0(*code)]);
			break;
		}
		break;
	case 0x3f:
		//CCF
		DISAS_PRINT("CCF");
		break;
	case 0x37:
		//SCF
		DISAS_PRINT("SCF");
		break;
	case 0x00:
		//NOP
		DISAS_PRINT("NOP");
		break;
	case 0x76:
		//HALT
		DISAS_PRINT("HALT");
		break;
	case 0x10:
		//STOP
		if(*(code+1)==0x0)
			DISAS_PRINT("STOP");
		break;
	case 0xf3:
		//DI
		DISAS_PRINT("DI");
		break;
	case 0xfb:
		//EI
		DISAS_PRINT("EI");
		break;
	case 0xc3:
		//JP nn
		DISAS_PRINT("JP %hX", *(code+1));
		break;
	case 0xe9:
		//JP HL
		DISAS_PRINT("JP HL");
		break;
	case 0x18:
		//JR PC+e
		DISAS_PRINT("JR PC+%hhX", *(code+1));
		break;
	case 0xcd:
		//CALL nn
		DISAS_PRINT("CALL %hX", *(code+1));
		break;
	case 0xc9:
		//RET
		DISAS_PRINT("RET");
		break;
	case 0xd9:
		//RETI
		DISAS_PRINT("RETI");
		break;
	}

	switch(BIT7_6(*code)){
	case 0x0:
		switch(BIT2_0(*code)){
		case 0x0:
			switch(BIT5_3(*code)){
			case 0x7:
				//JR C,e
				DISAS_PRINT("JR C,%hhX", *(code+1)+2);
				break;
			case 0x6:
				//JR NC,e
				DISAS_PRINT("JR NC,%hhX", *(code+1)+2);
				break;
			case 0x5:
				//JR Z,e
				DISAS_PRINT("JR Z,%hhX", *(code+1)+2);
				break;
			case 0x4:
				//JR NZ,e
				DISAS_PRINT("JR NZ,%hhX", *(code+1)+2);
				break;
			}
			break;
		case 0x1:
			if(BIT3(*code))
				//ADD HL,ss
				DISAS_PRINT("ADD HL,%s", ss_name[BIT5_4(*code)]);
			else
				//LD dd,nn
				DISAS_PRINT("LD %s,%hX", dd_name[BIT5_4(*code)], *(code+1));
			break;
		case 0x3:
			if(BIT3(*code))
				//DEC ss
				DISAS_PRINT("DEC %s", ss_name[BIT5_4(*code)]);
			else
				//INC ss
				DISAS_PRINT("INC %s", ss_name[BIT5_4(*code)]);
			break;
		case 0x4:
			//INC r
			DISAS_PRINT("INC %s", r_name[BIT5_3(*code)]);
			break;
		case 0x5:
			//DEC r
			DISAS_PRINT("DEC %s", r_name[BIT5_3(*code)]);
			break;
		case 0x6:
			//LD r,n
			DISAS_PRINT("LD %s,%hhX", r_name[BIT5_3(*code)], *(code+1));
			break;
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
		break;
	case 0x2:
		switch(BIT5_3(*code)){
		case 0x0:
			//ADD A,r
			DISAS_PRINT("ADD A,%s", r_name[BIT2_0(*code)]);
			break;
		case 0x1:
			//ADC A,r
			DISAS_PRINT("ADC A,%s", r_name[BIT2_0(*code)]);
			break;
		case 0x2:
			//SUB A,r
			DISAS_PRINT("SUB %s", r_name[BIT2_0(*code)]);
			break;
		case 0x3:
			//SBC A,r
			DISAS_PRINT("SBC A,%s", r_name[BIT2_0(*code)]);
			break;
		case 0x4:
			//AND A,r
			DISAS_PRINT("AND %s", r_name[BIT2_0(*code)]);
			break;
		case 0x5:
			//XOR A,r
			DISAS_PRINT("XOR %s", r_name[BIT2_0(*code)]);
			break;
		case 0x6:
			//OR A,r
			DISAS_PRINT("OR %s", r_name[BIT2_0(*code)]);
			break;
		case 0x7:
			//CP A,r
			DISAS_PRINT("CP %s", r_name[BIT2_0(*code)]);
			break;
		}
		break;
	case 0x3:
		switch(BIT2_0(*code)){
		case 0x0:
			//RET cc
			DISAS_PRINT("RET %s", cc_name[BIT5_3(*code)]);
			break;
		case 0x1:
			//POP qq
			DISAS_PRINT("POP %s", qq_name[BIT5_4(*code)]);
			break;
		case 0x2:
			//JP cc,nn
			DISAS_PRINT("JP %s,%hX", cc_name[BIT5_3(*code)], *(code+1));
			break;
		case 0x4:
			//CALL cc,nn
			DISAS_PRINT("CALL %s,%hX", cc_name[BIT5_3(*code)], *(code+1));
			break;
		case 0x5:
			//PUSH qq
			DISAS_PRINT("PUSH %s", qq_name[BIT5_4(*code)]);
			break;
		case 0x7:
			//RST p
			DISAS_PRINT("RST %hhX", p_table[BIT5_3(*code)]);
			break;
		}
		break;
	}
}

void disas(uint8_t *code, int len) {

}


