#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "sim_avr.h"
#include "avr_ioport.h"

#include "FIGsimavr.h"

#include "spi_sram/microchip_23k640_spi_sram.h"
#include "spi_flash/amic_a25l040_spi_flash.h"

typedef struct fignition_spi_t {
	avr_irq_t*			irq;
	struct avr_t*		avr;

	microchip_23k640_p	sram;
	amic_a25l040_p		flash;
}fignition_spi_t;	

void fignition_spi_connect(fignition_spi_p spi) {
//	fignition_spi_p spi = (fignition_spi_p)p;
	avr_t* avr = spi->avr;

	printf("[%s] connect sram\n", __FUNCTION__);

	avr_irq_t *cs_sram = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 2);
	microchip_23k640_connect(spi->sram, cs_sram);

	printf("[%s] connect flash\n", __FUNCTION__);
	
	avr_irq_t *cs_flash = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 1);
	amic_a25l040_connect(spi->flash, cs_flash);

	printf("\n");
}

int fignition_spi_init(struct avr_t* avr, fignition_spi_pp sspi) {
	*sspi = malloc(sizeof(struct fignition_spi_t));
	
	if (!(*sspi))
		return(-1);
	
	fignition_spi_p spi = *sspi;

	spi->avr = avr;

	microchip_23k640_init(avr, &spi->sram);
	amic_a25l040_init(avr, &spi->flash);

	return(0);
}
