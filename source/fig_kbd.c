#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "sim_avr.h"
#include "avr_ioport.h"

#include "FIGsimavr.h"

enum {
	IRQ_KBD_ROW0=0,
	IRQ_KBD_ROW1,
	IRQ_KBD_COL0,
	IRQ_KBD_COL1,
	IRQ_KBD_COL2,
	IRQ_KBD_COL3,
	IRQ_KBD_LED,
	IRQ_KBD_COUNT
};

typedef struct fignition_kbd_t* fignition_kbd_p;
typedef struct fignition_kbd_t {
	avr_irq_t*	irq;
	struct avr_t*	avr;
	uint8_t		kbd_fmap[256];
	char		row1_out;
	char		row2_out;
}fignition_kbd_t;

static void kbd_row0_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	fignition_kbd_t*	p=(fignition_kbd_t *)param;

	printf("[%s] -- value:%02x param:%p\n", __FUNCTION__, value, p);

//	avr_raise_irq(avr_io_getirq(p->avr, AVR_IOCTL_IOPORT_GETIRQ('C'), IOPORT_IRQ_PIN_ALL), value);

//	if(0x41==spi_fignition.sram.status)
//		avr->state=cpu_Done;
}

static void kbd_row1_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	fignition_kbd_t*	p=(fignition_kbd_t*)param;

	printf("[%s] -- value:%02x param:%p\n", __FUNCTION__, value, p);

//	avr_raise_irq(avr_io_getirq(p->avr, AVR_IOCTL_IOPORT_GETIRQ('C'), IOPORT_IRQ_PIN_ALL), value);
}

#define kbd_hook(_name) \
	static void kbd_##_name##_hook(struct avr_irq_t* irq, uint32_t value, void* param) { \
		printf("[%s] -- value:%02x param:%p\n", __FUNCTION__, value, param); \
	}

kbd_hook(col0)
kbd_hook(col1)
kbd_hook(col2)
kbd_hook(col3)
kbd_hook(led)

char kbd_unescape(char scancode) {
//	printf("%s scancode: 0x%02x\n", __FUNCTION__, scancode);
	switch(scancode) {
		case	11:	scancode=11;	break;	// up
		case	10:	scancode=10;	break;	// down
		case	21:	scancode=9;	break;	// right
//  Are these correct???
		case	8:	scancode=7;	break;	// left (swapped with BS)
		case	127:	scancode=8;	break;
	}
	return(scancode);
}

uint8_t kbd_figgicode(void* p, char scancode) {
	fignition_kbd_p fig = (fignition_kbd_p)p;

	if(scancode>0x7F)
		return(0xFF);

	return(fig->kbd_fmap[scancode]);
}

void fignition_kbd_connect(fignition_kbd_p kbd) {
	avr_t* avr = kbd->avr;

	printf("[%s]\n", __FUNCTION__);

	printf("k-r-0\n");
	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), IOPORT_IRQ_PIN7), kbd->irq + IRQ_KBD_ROW0);
	printf("k-r-1\n");
	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), IOPORT_IRQ_PIN0), kbd->irq + IRQ_KBD_ROW1);
	printf("k-c-0\n");
	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('C'), IOPORT_IRQ_PIN0), kbd->irq + IRQ_KBD_COL0);
	printf("k-c-1\n");
	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('C'), IOPORT_IRQ_PIN1), kbd->irq + IRQ_KBD_COL1);
	printf("k-c-2\n");
	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('C'), IOPORT_IRQ_PIN2), kbd->irq + IRQ_KBD_COL2);
	printf("k-c-3\n");
	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('C'), IOPORT_IRQ_PIN3), kbd->irq + IRQ_KBD_COL3);
	printf("k-led\n");
	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('C'), IOPORT_IRQ_PIN4), kbd->irq + IRQ_KBD_LED);

	printf("\n");
}

static const char* kbd_irq_names[IRQ_KBD_COUNT]={
	[IRQ_KBD_ROW0]="1>kbd_row0.out",
	[IRQ_KBD_ROW1]="1>kbd_row1.out",
	[IRQ_KBD_COL0]="1>kbd_col0.out",
	[IRQ_KBD_COL1]="1>kbd_col1.out",
	[IRQ_KBD_COL2]="1>kbd_col2.out",
	[IRQ_KBD_COL3]="1>kbd_col3.out",
	[IRQ_KBD_LED]="1>kbd_led.out",
};

int fignition_kbd_init(avr_t* avr, fignition_kbd_pp kkbd) {
	fignition_kbd_p kbd = (fignition_kbd_p)malloc(sizeof(struct fignition_kbd_t));
	if (!kbd)
		return(-1);

	*kkbd = (void *)kbd;

	kbd->avr = avr;
	kbd->irq = avr_alloc_irq(&avr->irq_pool, 0, IRQ_KBD_COUNT, kbd_irq_names);

	avr_irq_register_notify(kbd->irq + IRQ_KBD_ROW0, kbd_row0_hook, kbd);
	avr_irq_register_notify(kbd->irq + IRQ_KBD_ROW1, kbd_row1_hook, kbd);
	avr_irq_register_notify(kbd->irq + IRQ_KBD_COL0, kbd_col0_hook, kbd);
	avr_irq_register_notify(kbd->irq + IRQ_KBD_COL1, kbd_col1_hook, kbd);
	avr_irq_register_notify(kbd->irq + IRQ_KBD_COL2, kbd_col2_hook, kbd);
	avr_irq_register_notify(kbd->irq + IRQ_KBD_COL3, kbd_col3_hook, kbd);
	avr_irq_register_notify(kbd->irq + IRQ_KBD_LED, kbd_led_hook, kbd);


	kbd->row1_out=0;
	kbd->row2_out=0;

	return(0);
}
