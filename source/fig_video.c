#include <stdlib.h>
#include <stdio.h>

#include "sim_avr.h"
#include "avr_ioport.h"
#include "avr_uart.h"
#include "avr_timer.h"

#include "FIGsimavr.h"

enum {
	IRQ_VIDEO_UART_BYTE_IN=0,
	IRQ_VIDEO_D3_OC2B,
	IRQ_VIDEO_COUNT
};

typedef struct video_frame_t* video_frame_p;
typedef	struct video_frame_t {
	uint8_t			data[65536 * 2];
	uint16_t		x;
	uint16_t		y;
	uint32_t		byte_count;
	int				band;
}video_frame_t;

typedef struct fignition_video_t {
	avr_irq_t*			irq;
	struct avr_t*		avr;
	fignition_p			fig;
	video_frame_t 		frame;
	uint32_t			frame_count;
}fignition_video_t;

static void fignition_video_data_byte_in_hook(struct avr_irq_t* irq, uint32_t value, void* param) {
	fignition_video_p video = (fignition_video_p)param;
	video_frame_p frame = &video->frame;
	
	uint8_t *raster = &frame->data[(frame->y & 0x7f) << 8];
	uint8_t *dst = &raster[frame->x & 0xff];

	value = (value >> 4) | (value << 4);
	/* 0011=3 1100=c */
	value = ((value >> 2) & 0x33) | ((value << 2) & 0xcc);
	/* 0101=5 1010=a */
	value = ((value >> 1) & 0x55) | ((value << 1) & 0xaa);

	*dst = value;

	frame->x++;
	frame->byte_count++;
	
	if(24 >= frame->x)
		return;
	else {
		frame->x=0;
//		printf("%s: video(%p), line cycle (0x%04x, %06u)\n", __FUNCTION__, video, frame->y, frame->y);
		fignition_sdl_update_raster(video->fig, frame->band, frame->y, raster, 0);
		frame->y++;
		if(191 >= frame->y)
			return;
		else {
			frame->y = 0;
			video->frame_count++;
			printf("%s: video(%p), frame cycle\n", __FUNCTION__, video);
			frame->band ^= 1;
		}
	}
}

static void fignition_video_sync_in_hook(struct avr_irq_t* irq, uint32_t value, void* param)
{
	fignition_video_p	video = (fignition_video_p)param;
	video_frame_p frame = &video->frame;

#if 1
	printf("[%s]: video(%p), value(0x%04x, %06u)\n", __FUNCTION__, video, value, value);
	printf("[%s]: x(0x%04x, %06u), y(0x%04x, %06u), byte_count(%04x), band(%02x)\n",
		__FUNCTION__, 
		frame->x, frame->x, 
		frame->y, frame->y, 
		frame->byte_count, frame->band);
#endif
	
	frame->byte_count = 0;
}

void fignition_video_connect(fignition_video_p video, char uart) {
	avr_t* avr = video->avr;
	
	uint32_t	f=0;
	
//	printf("[%s] set stdio flag\n", __FUNCTION__);

	avr_ioctl(avr, AVR_IOCTL_UART_GET_FLAGS(uart), &f);
	f &= ~(AVR_UART_FLAG_STDIO | AVR_UART_FLAG_POLL_SLEEP);
	avr_ioctl(avr, AVR_IOCTL_UART_SET_FLAGS(uart), &f);
	
//	printf("[%s] connect uart->in hook\n", __FUNCTION__);

//	avr_irq_t*	src = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 1);
	avr_irq_t*	src = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ(uart), UART_IRQ_OUTPUT);
	avr_irq_t*	dst = video->irq+IRQ_VIDEO_UART_BYTE_IN;

//	printf("[%s] set stdio flag -- src: %08x, dst: %08x\n", __FUNCTION__, src, dst);

	avr_connect_irq(src, dst);

//	printf("[%s] connect D3OC2B -> hook\n", __FUNCTION__);

//	src = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 3);
	src = avr_io_getirq(avr, AVR_IOCTL_TIMER_GETIRQ('2'), TIMER_IRQ_OUT_PWM1);
//	src = avr_io_getirq(avr, AVR_IOCTL_TIMER_GETIRQ('2'), TIMER_IRQ_OUT_COMP + 1);
	dst = video->irq + IRQ_VIDEO_D3_OC2B;
	avr_connect_irq(src, dst);

//	printf("\n");
}

static const char* uart_irq_names[IRQ_VIDEO_COUNT]={
	[IRQ_VIDEO_UART_BYTE_IN]="8<fignition_video_data.in",
	[IRQ_VIDEO_D3_OC2B]="1<timer.d3_oc2b.sync.in"
};

int fignition_video_init(fignition_p fig, fignition_video_pp vvideo) {
	avr_t* avr = fig->avr;
	
	*vvideo = (fignition_video_p)malloc(sizeof(fignition_video_t));
	if(!(*vvideo))
		return(-1);
		
	fignition_video_p video = *vvideo;

	video->avr = avr;
	video->fig = fig;
	video->irq = avr_alloc_irq(&avr->irq_pool, 0, IRQ_VIDEO_COUNT, uart_irq_names);
	avr_irq_register_notify(video->irq + IRQ_VIDEO_UART_BYTE_IN, fignition_video_data_byte_in_hook, video);
	avr_irq_register_notify(video->irq + IRQ_VIDEO_D3_OC2B, fignition_video_sync_in_hook, video);

	video->frame.x = 0;
	video->frame.y = 0;
	video->frame.band = 1;
	
	return(0);
}
