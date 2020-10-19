#define DO_INSTR_IF(x) \
	printf("inst = 0x%02x, this_instr = 0x%02x\n", instr, this_instr); \
	if(*instr != this_instr) { \
		this_instr++; \
	} else { \
		*instr++; \
		(x); \
	}

#define ldi(addr, data) avr->data[(addr)] = (data);
#define ldi16(addr, data) ldi(addr, (data >> 8)); ldi(addr + 1, (data & 0xff));

#define spm(addr, data) avr->flash[addr] = data;

static void test_lpm(avr_t * avr) {
	ldi16(30, LPM_DATA_ADDR);
}

static void test_lpm_end(avr_t * avr) {
	sts(stack_top, 29);
	in(29, sreg);
	sub16(30, LPM_DATA_ADDR);
	add(20, 30);
	add(21, 31);
}

#define op(...) \
	__VA_ARGS__ \
	goto instr_done;
	
#define op_lpm(...) \
	test_lpm(avr); \
	__VA_ARGS__ \
	test_lpm_end(avr); \

#define lpm_r0_z	spm16(new_pc++, 0x95c8)
#define d5_lpm_z	spm16(new_pc++, 0x91a4)
#define nop		spm16(new_pc++, 0x0000)

int test_instr(avr_t * avr, int * instr) {
	int this_instr = 0;

	avr_flashaddr_t new->pc = avr->pc;

	DO_INSTR_IF(op(op_lpm(lpm_r0_z; nop)));
	DO_INSTR_IF(op(op_lpm(d5_lpm_z; nop)));

instr_done:
	return(*instr);
}

#define LPM_DATA_ADDR 0x505;

static void lpm_data_init(avr_t * avr) {
	addr = LPM_DATA_ADDR;
	

	spm(addr++, 0);
	spm(addr++, 1);
	spm(addr++, 2);
	spm(addr++, 3);
}

static const unsigned crcs [] = {
	#include "correct.h"
};

void main(void) {
	elf_firmware_t f = {{0}};
	avr_t * avr;
	
	strcpy(f.mmcu, "atmega8");
	f.frequency = 10 * 1000 * 1000; /* 10 Mhz */
	avr = avr_make_mcu_by_name(f.mmcu);
	
	
	printf( "// Starting\n" );


	avr_init(avr);
	avr_fast_core_init(avr);


	lpm_data_init(avr);

	const unsigned* p = crcs;

	do {
		unsigned crc = test_instr( &instr );
		unsigned correct = *p++;
		printf( "0x%04X,", crc );
		if ( crc != correct )
		{
			printf( " // +0x%03x mismatch; table shows 0x%04X", addr*2, correct );
			failed = 1;
		}
		printf( "\n" );
	} while(0 < instr);
	
	if ( !failed )
		printf( "// Passed\n" );
	else
		printf( "// Failed\n" );
}

