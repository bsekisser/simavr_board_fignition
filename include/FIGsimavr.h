#ifndef KHz
	#define KHz(hz) ((hz)*1000ULL)
#endif

#ifndef MHz
	#define MHz(hz) KHz(KHz(hz))
#endif

#if 0
#define kFIGnition	"FIGnitionPAL16.elf"
#define kFrequency	MHz(16ULL)
#else
#define kFIGnition	"FIGnitionPAL20.elf"
#define kFrequency	MHz(20ULL)
#endif

#define NOERR(_err, _doo) \
	do{ \
		if (-1 != _err) { \
			_doo; \
		} \
	} while(0);

typedef struct fignition_t* fignition_p;

#include "fig_init.h"
#include "fig_avr.h"
#include "fig_sdl.h"
#include "fig_video.h"
#include "fig_kbd.h"
#include "fig_spi.h"

typedef struct fignition_t {
		int					state;
		avr_t*				avr;
		fignition_avr_p		avr_thread;

		fignition_sdl_p		sdl;
		fignition_video_p	video;
		
		struct video_raster {
			int8_t		line;
			uint8_t		buffer[256];
		}video_raster;
		
		fignition_kbd_p		kbd;
		uint8_t				kbd_queue;

		fignition_spi_p		spi;
			//	flash
			//	sram
}fignition_t;

#include "fig_init.h"
#include "fig_avr.h"
#include "fig_sdl.h"
#include "fig_video.h"
#include "fig_kbd.h"
#include "fig_spi.h"
