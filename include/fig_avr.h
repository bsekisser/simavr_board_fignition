#ifndef _fig_avr_h_once_
#define _fig_avr_h_once_

	typedef struct fignition_avr_t** fignition_avr_pp;
	typedef struct fignition_avr_t* fignition_avr_p;

	void fignition_avr_run(fignition_avr_p thread);
	void fignition_avr_connect(fignition_avr_p thread);
	int fignition_avr_init(int argc, char *argv[], fignition_p fig, fignition_avr_pp tthread);

#endif /* _fig_avr_h_once_ */
