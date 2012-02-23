#ifndef _galapdf_h_
#define _galapdf_h_

extern char cfg_file[1024];
extern int user_cancelled;

enum galapdf_ret
{
	gpret_param_err			= -1,
	gpret_success			= 0,
	gpret_user_cancelled	= 1,
	gpret_cant_get_textenc	= 2,
	gpret_cant_open_pdf		= 3,
	gpret_pdf_encrypted		= 4,
	gpret_bad_eol			= 5,
	gpret_dont_allow_copy	= 6,
	gpret_cant_open_target	= 7,
	gpret_cant_init_textout	= 8,
	gpret_save_image_error	= 9
};

#endif // _galapdf_h_
