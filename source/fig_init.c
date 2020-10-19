#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "sim_avr.h"
#include "sim_elf.h"

#include "FIGsimavr.h"

void fignition_connect(fignition_p fig)
{
	printf("[%s]\n", __FUNCTION__);
	fignition_video_connect(fig->video, '0');
	fignition_kbd_connect(fig->kbd);
	fignition_spi_connect(fig->spi);
	printf("\n");
}

int fignition_init(int argc, char *argv[], fignition_p fig)
{
	int err = fignition_sdl_init(argc, argv, fig, &fig->sdl);

	NOERR(err, err = fignition_avr_init(argc, argv, fig, &fig->avr_thread));

	avr_t* avr = fig->avr;
	NOERR(err, err = fignition_video_init(fig, &fig->video));

	NOERR(err, err = fignition_kbd_init(avr, &fig->kbd));
	NOERR(err, err = fignition_spi_init(avr, &fig->spi));

	return err;
}
