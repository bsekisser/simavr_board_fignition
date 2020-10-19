#include <SDL/SDL.h>

#include "sim_avr.h"

#include "FIGsimavr.h"

typedef struct fignition_sdl_t* fignition_sdl_p;
typedef struct fignition_sdl_t {
		fignition_p		fig;
		SDL_Surface*	surface;
		SDL_Event		event;
		uint16_t		width;
		uint16_t		height;
}fignition_sdl_t;

void fignition_sdl_event(fignition_sdl_p sdl) {
	fignition_p fig = sdl->fig;
	avr_t* avr = fig->avr;

	uint8_t scancode;

	SDL_PollEvent(&sdl->event);
	switch (sdl->event.type) {
		case	SDL_QUIT:
			avr->state = cpu_Stopped;
			break;
		case	SDL_KEYDOWN:
			scancode = sdl->event.key.keysym.scancode;
			if(/*0x1b*/0x09==scancode)
				avr->state = cpu_Stopped;

//			scancode = kbd_unescape(scancode);
//			scancode = kbd_figgicode(scancode);
			break;
		default:
			fig->state = avr->state;
	}
}

static void fignition_sdl_put_bw_pixel(
	SDL_Surface* surface,
	uint16_t x, uint16_t y,
	uint32_t pixel)
{
	uint16_t bpp = surface->format->BytesPerPixel;
	uint8_t* dst = (uint8_t *)surface->pixels + y * surface->pitch + x * bpp;
	pixel = (pixel ? 0xffffffff : 0x00000000);

	switch(bpp) {
		case	1:
			*dst = pixel;
		break;
		case	2:
			*(uint16_t*)dst = pixel;
		break;
		case	3:
			dst[0] = pixel;
			dst[1] = pixel;
			dst[2] = pixel;
		break;
		case	4:
			*(uint32_t*)dst = pixel;
		break;
	}

}

void fignition_sdl_update_raster(fignition_p fig, int band, uint16_t line, uint8_t *raster, int frame_lock)
{
	fignition_sdl_p sdl = fig->sdl;
	SDL_Surface* surface = sdl->surface;
	
	if(!frame_lock)
		SDL_LockSurface(surface);

	uint16_t lline = line << 1;
	
	fignition_sdl_put_bw_pixel(surface, 1, 0 + lline, band);
	fignition_sdl_put_bw_pixel(surface, sdl->width - 1, 0 + lline, band);
	fignition_sdl_put_bw_pixel(surface, 3, 0 + lline, band);
	fignition_sdl_put_bw_pixel(surface, 2, 1 + lline, band);
//	fignition_sdl_put_bw_pixel(surface, 1, 2 + lline, band);
//	fignition_sdl_put_bw_pixel(surface, 2, 2 + lline, band);

	uint16_t x = 6;
	for(uint8_t i = 0; i < 0x3f; i++) {
		uint_fast8_t c = *raster++;

		for(uint8_t cp = 0; cp <= 7; cp++) {

			uint16_t xx = x << 1;

			xx = (( xx < (sdl->width - 2)) ? xx : sdl->width - 2);

			uint_fast8_t cc = c & 1;

			fignition_sdl_put_bw_pixel(surface, 0 + xx, 0 + lline, cc);
			fignition_sdl_put_bw_pixel(surface, 1 + xx, 0 + lline, cc);
			fignition_sdl_put_bw_pixel(surface, 0 + xx, 1 + lline, cc);
			fignition_sdl_put_bw_pixel(surface, 1 + xx, 1 + lline, cc);
			
			x++;
			c >>= 1;
		}
	}

	if(!frame_lock) {
		SDL_UnlockSurface(surface);
		SDL_Flip(surface);
	}
}

static void SDLInit(SDL_Surface** surface, uint16_t width, uint16_t height) {
	SDL_Init(SDL_INIT_VIDEO);

//	*surface=SDL_SetVideoMode(kScreenWidth, kScreenHeight, 8, SDL_FULLSCREEN|SDL_HWSURFACE);
//	*surface=SDL_SetVideoMode(kScreenWidth, kScreenHeight, 8, SDL_FULLSCREEN|SDL_DOUBLEBUF|SDL_HWSURFACE);
//	*surface=SDL_SetVideoMode(kScreenWidth, kScreenHeight, 8, SDL_FULLSCREEN|SDL_SWSURFACE);
//	*surface=SDL_SetVideoMode(kScreenWidth, kScreenHeight, 8, SDL_SWSURFACE);
	*surface=SDL_SetVideoMode(width, height, 8, SDL_SWSURFACE);
//	*surface=SDL_SetVideoMode(640, 480, 8, SDL_SWSURFACE);
//	*surface=SDL_SetVideoMode(kScreenWidth, kScreenHeight, 16, SDL_SWSURFACE);
//	*surface=SDL_SetVideoMode(kScreenWidth, kScreenHeight, 24, SDL_SWSURFACE);
//	*surface=SDL_SetVideoMode(kScreenWidth, kScreenHeight, 32, SDL_SWSURFACE);
	if(*surface==NULL)
		exit(0);

	SDL_EnableKeyRepeat(125, 50);
}

int fignition_sdl_init(int argc, char* argv[], fignition_p fig, fignition_sdl_pp ssdl) {
	*ssdl = (fignition_sdl_p)malloc(sizeof(fignition_sdl_t));
	if(!(*ssdl))
		return(-1);
		
	fignition_sdl_p sdl = *ssdl;
	
	sdl->fig = fig;
	sdl->width = 512;
	sdl->height = 400;
	
	SDLInit(&sdl->surface, sdl->width, sdl->height);

	return(0);
}
