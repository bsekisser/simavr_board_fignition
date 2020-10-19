#ifndef _fig_kbd_h_once_
#define _fig_kbd_h_once_

	typedef struct fignition_kbd_t** fignition_kbd_pp;
	typedef struct fignition_kbd_t* fignition_kbd_p;

	void fignition_kbd_connect(fignition_kbd_p kbd);
	int fignition_kbd_init(struct avr_t* avr, fignition_kbd_pp kbd);

#endif /* _fig_kbd_h_once_ */
