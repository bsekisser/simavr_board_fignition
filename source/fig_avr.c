#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
//#include <sys/time.h>
//#include <sys/timeb.h>

#include "sim_avr.h"
#include "sim_elf.h"

#include "dtime/dtime.h"
#include "FIGsimavr.h"

typedef struct fignition_avr_t {
	fignition_p			fig;
	avr_t*				avr;
	pthread_t			thread;
	avr_cycle_count_t	run_cycles;
	avr_cycle_count_t	start_cycle;
	avr_cycle_count_t	last_cycle;
	uint64_t			elapsed_dtime;
	int					aquire_lock;
	int					lock_granted;
}fignition_avr_t;

void * avr_run_thread(void * param) {
	fignition_avr_p	p=(fignition_avr_p)param;
	avr_t*			avr;
	uint64_t		prev_dtime, now_dtime;
	avr_cycle_count_t	last_cycle;

	avr=p->avr;

#ifdef USE_PTHREAD
threadLoop:
#endif

	p->start_cycle = avr->cycle;
	last_cycle = avr->cycle + p->run_cycles;

	prev_dtime = get_dtime();
	while(last_cycle > avr->cycle) {
		avr_run(avr);
	}
	now_dtime = get_dtime();
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
	fignition_avr_p	p=(fignition_avr_p)param;

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

void fignition_avr_run(fignition_avr_p thread)
{
	fignition_p fig = thread->fig;
	avr_t* avr = thread->avr;

//	printf("%s: fig: %08x avr: %08x\n", 
//		__FUNCTION__, fig, avr);

	int state;
	avr->state = cpu_Running;

	int loop_loops = 1;

run_loop: ;
	uint64_t sleep_loops = 0;
	
	uint64_t start_cycle = avr->cycle;
	uint64_t start_dtime = get_dtime();
	uint32_t run_loops = 65536 << 8, loops = run_loops;
	do {
		state = avr_run(avr);
		
		if((state == cpu_Done) || (state == cpu_Crashed))
				break;
		
		if((state == cpu_Sleeping))
			sleep_loops++;
	} while(--loops);
	uint64_t end_dtime = get_dtime();
	uint64_t end_cycle = avr->cycle;
	
	uint64_t loop_dtime = end_dtime - start_dtime;
	uint64_t loop_cycles = end_cycle - start_cycle;
	
	uint64_t cycle_dtime = loop_dtime / loop_cycles;
	
	printf("%s: loop: %08u cycle: %016llu ecdt: %016llu sleep: %016llu\n", 
		__FUNCTION__, run_loops - loops, avr->cycle, cycle_dtime, sleep_loops);

	if((state == cpu_Done) || (state == cpu_Crashed))
		return;

	fignition_sdl_event(fig->sdl);

	if(--loop_loops)
		goto run_loop;
}

void fignition_avr_sleep(avr_t* avr, avr_cycle_count_t cycle)
{
}

void fignition_avr_connect(fignition_avr_p thread)
{
}

int fignition_avr_init(int argc, char *argv[], fignition_p fig, fignition_avr_pp tthread)
{
	*tthread = (fignition_avr_p)malloc(sizeof(fignition_avr_t));
	if(!(*tthread))
		return(-1);
	
	fignition_avr_p thread = *tthread;
	thread->fig = fig;
	
//	char* cwd_path = malloc(PATH_MAX);
//	getcwd(cwd_path, PATH_MAX);
	char* cwd_path = getcwd(NULL, 0);
	
	printf("[%s] cwd_path=%s\n", __FUNCTION__, cwd_path);
	free(cwd_path);
	
	avr_t* avr;
	elf_firmware_t	f;
	const char*	fname="fignition_firmware/" kFIGnition;

	elf_read_firmware(fname, &f);
	
	strcpy(f.mmcu, "atmega168");
	f.frequency=kFrequency;

	printf("firmware %s f=%d mmcu=%s\n", fname, (int)f.frequency, f.mmcu);

	avr = avr_make_mcu_by_name(f.mmcu);
	if (!avr) {
		fprintf(stderr, "%s: AVR '%s' not known\n", argv[0], f.mmcu);
		exit(1);
	}

	avr_init(avr);

	avr_load_firmware(avr, &f);
	
	fig->avr = avr;
	thread->avr = avr;

	avr->sleep = fignition_avr_sleep;
	
#ifdef USE_AVR_GDB
	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	avr->state = cpu_Stopped;
	avr_gdb_init(avr);
#endif

	return(0);
}
