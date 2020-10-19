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
//#include <unistd.h>
#include <SDL/SDL.h>
#include <time.h>
//#include <sys/time.h>
//#include <sys/timeb.h>

//#define USE_PTHREAD
//#define USE_VCD_FILE
//#define USE_AVR_GDB

#define INLINE 

//#ifdef USE_PTHREAD
//#include <pthread.h>
//#endif

#include "sim_avr.h"
#include "avr_ioport.h"
#include "sim_elf.h"
#include "avr_uart.h"
#include "sim_irq.h"
#include "avr_spi.h"
#include "avr_timer.h"

#include "dtime/dtime.h"
#include "FIGsimavr.h"

#ifdef USE_AVR_GDB
#include "sim_gdb.h"
#endif

#ifdef USE_VCD_FILE
#include "sim_vcd_file.h"
#endif

#ifdef USE_VCD_FILE
avr_vcd_t	vcd_file;
#endif

fignition_p fig;

void catch_sig(int sign)
{
	struct avr_t* avr;

	if(fig)
		avr = fig->avr;
	
	printf("\n\n\n\nsignal caught, simavr terminating\n\n");

	if((0 != fig) && (0 != avr)) {
		avr->state=cpu_Done;
		avr_terminate(avr);
	}
	
	SDL_Quit();
	exit(0);
}

int main(int argc, char *argv[])
{
	fignition_t	_fig;
	fig = &_fig;
	
//	uint16_t	scancode;

	int err = fignition_init(argc, argv, fig);
//	printf("%s: err=%04lu\n", __FUNCTION__, err);

	signal(SIGINT, catch_sig);
	signal(SIGTERM, catch_sig);

	fignition_connect(fig);

	uint64_t cyclesPerSecond = dtime_calibrate();
	double cpuCyclePeriod = 1.0 / cyclesPerSecond;
	double eavrCyclePeriod = 1.0 / kFrequency;
	double cycleRatio = cyclesPerSecond / kFrequency;
	
	printf("cpuCyclePeriod: %016.14f  eavrCyclePeriod: %016.14f  cycleRatio: %06.1f\n",
		cpuCyclePeriod, eavrCyclePeriod, cycleRatio);

#ifdef USE_PTHREAD
	pthread_create(&fig_thread.thread, NULL, avr_run_thread, &fig_thread);
	printf("main running");
#endif

#if 0	
	int count = 0;
	double run_start_time = get_dtime();
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
			double run_dtime = ((double)get_dtime() - run_start_time) / (double)cyclesPerSecond;
			double eacdt = (double)fig_thread.elapsed_dtime / (double)lastCycle;
			double avrmcps = (avr->cycle / run_dtime) / MHz(1);
			printf("[avr_run_thread] - cycle: %016llu ecdt: %016llu eacdt: %08.4f avrmcps: %08.4f\n", 
				avr->cycle, fig_thread.elapsed_dtime, eacdt, avrmcps);
		}
	}

#endif

	fig->avr->log = LOG_TRACE;
	fig->avr->run_cycle_limit = 65536;
	
	fignition_avr_run(fig->avr_thread);

	fig->avr->state = cpu_Done;

	avr_terminate(fig->avr);
	SDL_Quit();
	return(0);
}

