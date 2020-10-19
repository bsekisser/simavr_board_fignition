#ifndef _fig_video_h_once_
#define _fig_video_h_once_

	typedef struct fignition_video_t** fignition_video_pp;
	typedef struct fignition_video_t* fignition_video_p;

	void fignition_video_connect(fignition_video_p video, char uart);
	int fignition_video_init(fignition_p fig, fignition_video_pp vvideo);

#endif /* _fig_video_h_once_ */
