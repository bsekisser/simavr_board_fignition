typedef struct fignition_sdl_t** fignition_sdl_pp;
typedef struct fignition_sdl_t* fignition_sdl_p;

void fignition_sdl_event(fignition_sdl_p sdl);
void fignition_sdl_update_raster(fignition_p fig, int band, uint16_t line, uint8_t *raster, int frame_lock);
int fignition_sdl_init(int argc, char* argv[], fignition_p fig, fignition_sdl_pp sdl);
