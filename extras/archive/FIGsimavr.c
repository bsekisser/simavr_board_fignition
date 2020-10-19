/*
	FIGsimavr.c

	Copyright 2013 Michael Hughes <squirmyworms@embarqmail.com>

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <SDL/SDL.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timeb.h>

//#define USE_PTHREAD
//#define USE_VCD_FILE
//#define USE_AVR_GDB

#define INLINE 

#ifdef USE_PTHREAD
#include <pthread.h>
#endif

#include "sim_avr.h"
#include "avr_ioport.h"
#include "sim_elf.h"
#include "avr_uart.h"
#include "sim_irq.h"
#include "avr_spi.h"
#include "avr_timer.h"

#include "sim_fast_core_profiler.h"
#include "sim_fast_core.h"

#ifdef USE_AVR_GDB
#include "sim_gdb.h"
#endif

#ifdef USE_VCD_FILE
#include "sim_vcd_file.h"
#endif

#define KHz(hz) ((hz) * 1000ULL)

#define MHz(hz) KHz(KHz(hz))

#define kScreenWidth	320 
#define kScreenHeight	200

#if 0
#define kFIGnition	"FIGnitionPAL16.elf"
#define kFrequency	MHz(16ULL)
#else
#define kFIGnition	"FIGnitionPAL20.elf"
#define kFrequency	MHz(20ULL)
#endif

#if kFrequency == 16000000
#define kVideoBuffWidth		20
#endif

#if kFrequency == 20000000
#define kVideoBuffWidth		25
#endif

#define kVideoBuffHeight	24

#define	kVideoPixelWidth	(kVideoBuffWidth<<3)
#define kVideoScanlines		(kVideoBuffHeight<<3)
#define kVideoBufferSize	((kVideoPixelWidth*kVideoScanlines)<<2)

#define kVideoPixelTop		((kScreenHeight>>1)-(kVideoScanlines>>1))
#define kVideoPixelLeft		((kScreenWidth>>1)-(kVideoPixelWidth>>1))

#define	kRefresh	15
#define	kRefreshCycles	((gFrequency)/kRefresh)
#define kScanlineCycles	(kRefreshCycles/kVideoScanlines)

enum {
	IRQ_KBD_ROW1=0,
	IRQ_KBD_ROW2,
	IRQ_KBD_COUNT
};

typedef struct kbd_fignition_t {
	avr_irq_t*	irq;
	struct avr_t*	avr;
	char		row1_out;
	char		row2_out;
}kbd_fignition_t;

kbd_fignition_t kbd_fignition;

enum {
	IRQ_SPI_BYTE_IN=0,
	IRQ_SPI_BYTE_OUT,
	IRQ_SPI_SRAM_CS,
	IRQ_SPI_FLASH_CS,
	IRQ_SPI_COUNT
};

typedef struct spi_chip_t {
	unsigned char	state;
	unsigned char	status;
	unsigned char	command;
	unsigned short	address;
	unsigned char	data[8192];
}spi_chip_t;

typedef struct spi_fignition_t {
	avr_irq_t*		irq;
	struct avr_t*		avr;

	struct spi_chip_t*	spi_chip;

	struct spi_chip_t	sram;
	struct spi_chip_t	flash;
}spi_fignition_t;	

spi_fignition_t	spi_fignition;

enum {
	IRQ_VIDEO_UART_BYTE_IN=0,
	IRQ_VIDEO_TIMER_PWM0,
	IRQ_VIDEO_TIMER_PWM1,
	IRQ_VIDEO_TIMER_COMPB,
	IRQ_VIDEO_COUNT
};

typedef struct video_fignition_t {
	avr_irq_t*		irq;
	struct avr_t*		avr;
	SDL_Surface*		surface;

	struct video_buffer_t {
		uint8_t			data[kVideoBufferSize];
		uint8_t			x;
		uint8_t			y;
	}buffer;
	int			needRefresh;
	uint64_t		frameCycles;
	uint32_t		frame;
}video_fignition_t;


avr_t*		avr;
int		state;

#ifdef USE_VCD_FILE
avr_vcd_t	vcd_file;
#endif

video_fignition_t video_fignition;

static inline void _PutBWPixel(int x, int y, unsigned long pixel, SDL_Surface* surface) {
	int	bpp=surface->format->BytesPerPixel;
	unsigned char *dst;

	x = (x > (kScreenWidth-1) ? x - kScreenWidth : x);
	y = (y > (kScreenHeight-1) ? y - kScreenHeight : y);

	dst = ((unsigned char *)surface->pixels + (y * surface->pitch)) + x * bpp;
	pixel = (pixel ? 0xffffffff : 0x00000000);

	switch(bpp) {
		case	1:
			*dst = pixel;
			break;
		case	2:
			*(unsigned short *)dst = pixel;
			break;
		case	3:
			dst[0] = pixel;
			dst[1] = pixel;
			dst[2] = pixel;
			break;
		case	4:
			*(unsigned long *)dst = pixel;
			break;
	}
}


static inline void PutBWPixel(int x, int y, unsigned long pixel, SDL_Surface* surface) {
	int px, py;

	px=kVideoPixelLeft + x;
	py=kVideoPixelTop + y;

	_PutBWPixel(px, py, pixel, surface);
}

static void ExpandByte(uint8_t *dst, register int value) {
	*dst++ = (value & 0x80) ? -1 : 0x00; value <<= 1;
	*dst++ = (value & 0x80) ? -1 : 0x00; value <<= 1;
	*dst++ = (value & 0x80) ? -1 : 0x00; value <<= 1;
	*dst++ = (value & 0x80) ? -1 : 0x00; value <<= 1;
	*dst++ = (value & 0x80) ? -1 : 0x00; value <<= 1;
	*dst++ = (value & 0x80) ? -1 : 0x00; value <<= 1;
	*dst++ = (value & 0x80) ? -1 : 0x00; value <<= 1;
	*dst = (value & 0x80) ? -1 : 0x00;
}

static void video_fignition_uart_byte_in_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	video_fignition_t*	p = (video_fignition_t*)param;
	uint8_t			*dst = &p->buffer.data[((p->buffer.y << 5) + p->buffer.x) << 3];

	*dst = value;

	p->buffer.x++;	

	if(kVideoBuffWidth > p->buffer.x)
		return;
	else {
		p->buffer.x=0;
		p->buffer.y++;
		if(kVideoScanlines >= p->buffer.y)
			return;
		else {
			p->buffer.y = 0;
			p->needRefresh=1;
			p->frameCycles = p->avr->cycle / p->frame;
		}
	}
}


static inline void blit1bpp(uint64_t * restrict lwdst, uint64_t * restrict lwsrc) {
#if 1
	for(int px=0; px<=kVideoPixelWidth; px+=(sizeof(*lwdst) << 2)) {
		__builtin_prefetch(lwsrc, 0 , 1);
		__builtin_prefetch(lwdst, 1 , 3);
		*lwdst++ = *lwsrc++;
		*lwdst++ = *lwsrc++;
		*lwdst++ = *lwsrc++;
		*lwdst++ = *lwsrc++;
	}
#else
	int stride = 32;
	for(int px = 0; px <= kVideoPixelWidth; px += stride) {
		__builtin_prefetch(lwsrc, 0 , 1);
		__builtin_prefetch(lwdst, 1 , 3);
		for(int i = 0; i < stride; i += sizeof(uint64_t)) {
			*lwdst++ = *lwsrc++;
		}
	}
#endif
}

static void ExpandScanLine(uint8_t *dst, uint8_t *src) {
	for(int px = 0; px < (kVideoPixelWidth << 3); px += 8) {
		ExpandByte(&dst[px], src[px]);
		__builtin_prefetch(&src[px + 8]);
	}
}

static void VideoScan(struct video_fignition_t* p) {
	uint8_t		*pp, raster[kVideoPixelWidth << 3];
	uint8_t		px,py;
	SDL_Surface*	surface = p->surface;
	int		bpp=surface->format->BytesPerPixel;
	
	SDL_LockSurface(surface);

	for(py=0; py< (kVideoScanlines); py++) {
		PutBWPixel(-(kVideoPixelLeft-2), py, p->frame&0x01, surface);
		pp=&p->buffer.data[py << 8];

		ExpandScanLine(&raster[0], pp); pp = &raster[0];
		
		uint8_t *dst=((unsigned char *)surface->pixels+(py * surface->pitch)) + kVideoPixelLeft * bpp;
		switch(bpp) {
			case	1: {
				blit1bpp((uint64_t *)dst, (uint64_t *)pp);
			}	break;
			case	2: {
				int16_t * restrict wdst = (int16_t * restrict)dst;
				for(px=0; px<=kVideoPixelWidth; px++) {
					*wdst++ = *(int8_t * restrict)pp++;
				}
			}	break;
			case	3: {
				for(px=0; px<=kVideoPixelWidth; px++) {
					*dst++ = *pp;
					*dst++ = *pp;
					*dst++ = *pp++;
				}
			} break;
			case	4: {
				int32_t * restrict lwdst = (int32_t *)dst;
				for(px=0; px<=kVideoPixelWidth; px++) {
					*lwdst++ = *(int8_t * restrict)pp++;
				}
			} break;
			default:
				for(px=0; px<=kVideoPixelWidth;) {
					PutBWPixel(px++, py, *pp++, surface);
				}
		}
	}

	SDL_UnlockSurface(surface);
	p->needRefresh=0;
	p->frame++;
}

void video_fignition_connect(video_fignition_t* p, char uart) {
	uint32_t	f=0;
	avr_ioctl(p->avr, AVR_IOCTL_UART_GET_FLAGS(uart), &f);
	f &= ~(AVR_UART_FLAG_STDIO | AVR_UART_FLAG_POOL_SLEEP);
	avr_ioctl(p->avr, AVR_IOCTL_UART_SET_FLAGS(uart), &f);
	
	avr_connect_irq(avr_io_getirq(p->avr, AVR_IOCTL_UART_GETIRQ(uart), UART_IRQ_OUTPUT), p->irq+IRQ_VIDEO_UART_BYTE_IN);

	avr_connect_irq(avr_io_getirq(p->avr, AVR_IOCTL_TIMER_GETIRQ('2'), TIMER_IRQ_OUT_PWM0), p->irq+IRQ_VIDEO_TIMER_PWM0);
	avr_connect_irq(avr_io_getirq(p->avr, AVR_IOCTL_TIMER_GETIRQ('2'), TIMER_IRQ_OUT_PWM1), p->irq+IRQ_VIDEO_TIMER_PWM1);
}

static const char* uart_irq_names[IRQ_VIDEO_COUNT]={
	[IRQ_VIDEO_UART_BYTE_IN]="8<video_fignition.in",
	[IRQ_VIDEO_TIMER_PWM0]="8<timer.pwm0.sync.in",
	[IRQ_VIDEO_TIMER_PWM1]="8<timer.pwm1.sync.in",
	[IRQ_VIDEO_TIMER_COMPB]="1<timer.compb.sync.in"
};

void video_fignition_init(struct avr_t* avr, video_fignition_t* p, SDL_Surface* surface) {
	p->avr = avr;
	p->irq = avr_alloc_irq(&avr->irq_pool, 0, IRQ_VIDEO_COUNT, uart_irq_names);
	avr_irq_register_notify(p->irq+IRQ_VIDEO_UART_BYTE_IN, video_fignition_uart_byte_in_hook, p);
	
	p->surface = surface;
	p->buffer.x = 0;
	p->buffer.y = 0;
	p->needRefresh = 1;
	p->frame = 1;
}

#define SRAM_WRSR_CMD	0x01

static uint32_t spi_sram_proc(spi_fignition_t* p, uint32_t value) {
	spi_chip_t*	sram=&p->sram;

	printf("[spi_sram_proc] -- state:%02x, command:%02x value:%02x\n", sram->state, sram->command, value);

	switch(sram->command) {
		case	SRAM_WRSR_CMD:
			printf("[spi_sram_proc] -- WRSR:%02x (%02x)\n", value, sram->status);
			sram->status=value;
			sram->command=0;
		default:
			sram->command=value;
	}

	return(0);
}

static uint32_t spi_flash_proc(spi_fignition_t* p, uint32_t value) {
	return(0);
}

void spi_output_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	uint32_t		in=value;
	spi_fignition_t*	p=(spi_fignition_t*)param;

	avr_irq_t*	spi_in_irq=avr_io_getirq(p->avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_INPUT);

	value=0;
	if(&p->sram==p->spi_chip)
		value=spi_sram_proc(p, in);
	else if(&p->flash==p->spi_chip)
		value=spi_flash_proc(p, in);

	printf("[spi_output_hook] -- out:%02x in:%02x\n", in, value);

	avr_raise_irq(spi_in_irq, value);
}

void spi_sram_cs_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	spi_fignition_t*	p=(spi_fignition_t*)param;

	printf("[spi_sram_cs_hook] -- value:%02x\n", value);

	if(value) {
		p->spi_chip=&p->sram;
	} else {
		p->spi_chip=NULL;
	}
}

void spi_flash_cs_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	spi_fignition_t*	p=(spi_fignition_t*)param;

	printf("[spi_flash_cs_hook] -- value:%02x\n", value);

	if(value) {
		p->spi_chip=&p->flash;
	} else {
		p->spi_chip=NULL;
	}
}

void fig_portb_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	spi_fignition_t*	p=(spi_fignition_t*)param;

	printf("[fig_portb_hook] -- value:%02x\n", value);
	p->avr->state=cpu_Stopped;
}

static const char* spi_irq_names[IRQ_SPI_COUNT]={
	[IRQ_SPI_BYTE_IN]="8<spi_fignition.in",
	[IRQ_SPI_BYTE_OUT]="8>spi_fignition.out",
	[IRQ_SPI_SRAM_CS]="=sram.cs",
	[IRQ_SPI_FLASH_CS]="=flash.cs",
};

void spi_fignition_init(struct avr_t* avr, spi_fignition_t* p) {
	p->avr=avr;
	p->irq=avr_alloc_irq(&avr->irq_pool, 0, IRQ_SPI_COUNT, spi_irq_names);
	avr_irq_register_notify(p->irq+IRQ_SPI_BYTE_IN, spi_output_hook, p);
	avr_irq_register_notify(p->irq+IRQ_SPI_SRAM_CS, spi_sram_cs_hook, p);
	avr_irq_register_notify(p->irq+IRQ_SPI_FLASH_CS, spi_flash_cs_hook, p);

	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT), p->irq+IRQ_SPI_BYTE_IN);
	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), IOPORT_IRQ_PIN2), p->irq+IRQ_SPI_SRAM_CS);
	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), IOPORT_IRQ_PIN3), p->irq+IRQ_SPI_FLASH_CS);

	p->spi_chip=NULL;
	p->sram.state=0;
	p->sram.status=0;
	p->sram.command=0;
}

void kbd_row1_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	kbd_fignition_t*	p=(kbd_fignition_t *)param;

	printf("[kbd_row1_hook] -- value:%02x param:%p\n", value, p);

//	avr_raise_irq(avr_io_getirq(p->avr, AVR_IOCTL_IOPORT_GETIRQ('C'), IOPORT_IRQ_PIN_ALL), value);

//	if(0x41==spi_fignition.sram.status)
//		avr->state=cpu_Done;
}

void kbd_row2_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	kbd_fignition_t*	p=(kbd_fignition_t*)param;

	printf("[kbd_row2_hook] -- value:%02x param:%p\n", value, p);

//	avr_raise_irq(avr_io_getirq(p->avr, AVR_IOCTL_IOPORT_GETIRQ('C'), IOPORT_IRQ_PIN_ALL), value);
}

static const char* kbd_irq_names[IRQ_KBD_COUNT]={
	[IRQ_KBD_ROW1]="1>kbd_row1.out",
	[IRQ_KBD_ROW2]="1>kbd_row2.out",
};

void kbd_fignition_init(struct avr_t* avr, kbd_fignition_t* p) {
	p->avr=avr;
	p->irq=avr_alloc_irq(&avr->irq_pool, 0, IRQ_KBD_COUNT, kbd_irq_names);

	avr_irq_register_notify(p->irq+IRQ_KBD_ROW1, kbd_row1_hook, p);
	avr_irq_register_notify(p->irq+IRQ_KBD_ROW2, kbd_row2_hook, p);

	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), IOPORT_IRQ_PIN7), p->irq+IRQ_KBD_ROW1);
	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), IOPORT_IRQ_PIN0), p->irq+IRQ_KBD_ROW2);

	p->row1_out=0;
	p->row2_out=0;
}

void SDLInit(int argc, char* argv[], SDL_Surface** surface) {
	SDL_Init(SDL_INIT_VIDEO);

//	*surface=SDL_SetVideoMode(kScreenWidth, kScreenHeight, 8, SDL_FULLSCREEN|SDL_HWSURFACE);
//	*surface=SDL_SetVideoMode(kScreenWidth, kScreenHeight, 8, SDL_FULLSCREEN|SDL_DOUBLEBUF|SDL_HWSURFACE);
//	*surface=SDL_SetVideoMode(kScreenWidth, kScreenHeight, 8, SDL_FULLSCREEN|SDL_SWSURFACE);
	*surface=SDL_SetVideoMode(kScreenWidth, kScreenHeight, 8, SDL_SWSURFACE);
//	*surface=SDL_SetVideoMode(kScreenWidth, kScreenHeight, 16, SDL_SWSURFACE);
//	*surface=SDL_SetVideoMode(kScreenWidth, kScreenHeight, 24, SDL_SWSURFACE);
//	*surface=SDL_SetVideoMode(kScreenWidth, kScreenHeight, 32, SDL_SWSURFACE);
	if(*surface==NULL)
		exit(0);

	SDL_EnableKeyRepeat(125, 50);
}

typedef struct fignition_thread_t {
	avr_t*			avr;
	pthread_t		thread;
	avr_cycle_count_t	run_cycles;
	avr_cycle_count_t	start_cycle;
	avr_cycle_count_t	last_cycle;
	uint64_t		elapsed_dtime;
	int			aquire_lock;
	int			lock_granted;
}fignition_thread_t;

fignition_thread_t fig_thread;

void * avr_run_thread(void * param) {
	fignition_thread_t*	p=(fignition_thread_t*)param;
	avr_t*			avr;
	uint64_t		prev_dtime, now_dtime;
	avr_cycle_count_t	last_cycle;

	avr=p->avr;

#ifdef USE_PTHREAD
threadLoop:
#endif

	p->start_cycle = avr->cycle;
	last_cycle = avr->cycle + p->run_cycles;

	prev_dtime = avr_fast_core_profiler_get_dtime();
	while(last_cycle > avr->cycle) {
		avr_run(avr);
	}
	now_dtime = avr_fast_core_profiler_get_dtime();

	p->last_cycle = avr->cycle;

	if(now_dtime > prev_dtime)
		p->elapsed_dtime += now_dtime - prev_dtime;
	else
		p->elapsed_dtime += prev_dtime - now_dtime;

#ifdef USE_PTHREAD
	if(p->aquire_lock) {
		p->lock_granted++;
		while(p->aquire_lock) usleep(1);
		p->lock_granted--;
		while(p->aquire_lock) usleep(1);
	}


	if(avr->state == cpu_Running || avr->state == cpu_Sleeping)
		goto threadLoop;
#endif

	return(0);
}

void avr_run_no_thread(void * param, uint64_t * start_cycle, uint64_t * last_cycle) {
	fignition_thread_t*	p=(fignition_thread_t*)param;

#ifdef USE_PTHREAD
	while(0 != p->lock_granted) usleep(1);
	p->aquire_lock++;
	while(0 != p->lock_granted) usleep(1);
#else
	avr_run_thread(param);
#endif

	*start_cycle = p->start_cycle;
	*last_cycle = p->last_cycle;

#ifdef USE_PTHREAD
	p->aquire_lock--;
#endif
}

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

static char kbd_fmap[256];

char kbd_figgicode(char scancode) {
	if(scancode>0x7F)
		return(0xFF);

	return(kbd_fmap[scancode]);
}

static void fig_callback_sleep_override(avr_t * avr, avr_cycle_count_t howLong) {
}

void catch_sig(int sign)
{
	printf("\n\n\n\nsignal caught, simavr terminating\n\n");

	avr->state=cpu_Done;

	if (avr)
		avr_terminate(avr);

	SDL_Quit();
	exit(0);
}

int main(int argc, char *argv[])
{
	elf_firmware_t	f;
	const char*	fname=kFIGnition;

	uint16_t	scancode;
	uint64_t	gFrequency = kFrequency;

	SDL_Surface* surface;
	SDL_Event event;

	signal(SIGINT, catch_sig);
	signal(SIGTERM, catch_sig);

	elf_read_firmware(fname, &f);
	
	strcpy(f.mmcu, "atmega168");
	f.frequency=gFrequency;

	printf("firmware %s f=%d mmcu=%s\n", fname, (int)f.frequency, f.mmcu);

	avr=NULL;	
	avr = avr_make_mcu_by_name(f.mmcu);
	if (!avr) {
		fprintf(stderr, "%s: AVR '%s' not known\n", argv[0], f.mmcu);
		exit(1);
	}
	avr_init(avr);

	avr_fast_core_init(avr);
	
	avr_load_firmware(avr, &f);

	avr->cycle = 0ULL;
	avr->sleep = fig_callback_sleep_override;
	avr->run = avr_fast_core_run_many;
//	avr->log = LOG_TRACE;

#ifdef USE_AVR_GDB
	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	avr->state = cpu_Stopped;
	avr_gdb_init(avr);
#endif

	SDLInit(argc, argv, &surface);

	video_fignition_init(avr, &video_fignition, surface);
	video_fignition_connect(&video_fignition, '0');

	spi_fignition_init(avr, &spi_fignition);
	kbd_fignition_init(avr, &kbd_fignition);

	state=cpu_Running;

	fig_thread.avr = avr;
	fig_thread.run_cycles = kRefreshCycles;
	fig_thread.last_cycle = avr->cycle+1ULL;
	fig_thread.elapsed_dtime = 0ULL;

	uint64_t cyclesPerSecond = avr_fast_core_profiler_dtime_calibrate();
	double cpuCyclePeriod = 1.0 / cyclesPerSecond;
	double eavrCyclePeriod = 1.0 / kFrequency;
	double cycleRatio = cyclesPerSecond / kFrequency;
	
	printf("cpuCyclePeriod: %016.14f  eavrCyclePeriod: %016.14f  cycleRatio: %06.1f\n",
		cpuCyclePeriod, eavrCyclePeriod, cycleRatio);

#ifdef USE_PTHREAD
	pthread_create(&fig_thread.thread, NULL, avr_run_thread, &fig_thread);
	printf("main running");
#endif
	
	int count = 0;
	double run_start_time = avr_fast_core_profiler_get_dtime();
	while((state != cpu_Stopped)&&(state != cpu_Crashed)) {
		uint64_t startCycle;
		uint64_t lastCycle;

		avr_run_no_thread(&fig_thread, &startCycle, &lastCycle);
		
		uint64_t runCycles = lastCycle - startCycle;

		if(video_fignition.needRefresh) {
			VideoScan(&video_fignition);
			SDL_Flip(surface);

			if(20 < video_fignition.frame)
				avr->state = cpu_Stopped;
				
//			if(video_fignition.frameCycles)
//				fig_thread.run_cycles = video_fignition.frameCycles;
//		} count++; if(30 < count) { count = 0;
			double run_dtime = ((double)avr_fast_core_profiler_get_dtime() - run_start_time) / (double)cyclesPerSecond;
			double eacdt = (double)fig_thread.elapsed_dtime / (double)lastCycle;
			double avrmcps = (avr->cycle / run_dtime) / MHz(1);
			printf("[avr_run_thread] - cycle: %016llu ecdt: %016llu eacdt: %08.4f avrmcps: %08.4f\n", 
				avr->cycle, fig_thread.elapsed_dtime, eacdt, avrmcps);
		}

		SDL_PollEvent(&event);
		switch (event.type) {
			case	SDL_QUIT:
				avr->state=cpu_Stopped;
				break;
			case	SDL_KEYDOWN:
				scancode=event.key.keysym.scancode;
				if(/*0x1b*/0x09==scancode)
					state=cpu_Stopped;

				scancode=kbd_unescape(scancode);
				scancode=kbd_figgicode(scancode);
				break;
			default:
				state = avr->state;
		}
	}

#ifdef CONFIG_AVR_FAST_CORE_UINST_PROFILING
	avr_fast_core_profiler_generate_report();
#endif

	avr->state = cpu_Done;

	avr_terminate(avr);
	SDL_Quit();
	return(0);
}

