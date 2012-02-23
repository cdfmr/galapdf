/*******************************************************************************
*                    galapdf action [options] input output                     *
*******************************************************************************/

#include "galapdf.h"
#include "pdfutil.h"

#include <stdio.h>
#include <string.h>
#include <gtypes.h>
#include <parseargs.h>

#if !defined(_WIN32) && !defined(__WIN32__)
#include <signal.h>
#endif

char cfg_file[1024];
int user_cancelled = 0;

enum pdf_action_type
{
	pdf_info,
	pdf_fonts,
	pdf_to_text,
	pdf_extr_imgs,
	pdf_to_imgs,
	pdf_last_act
};

typedef int (*pdf_action_proc) (int, char**);

typedef struct tag_pdf_action
{
	pdf_action_type	action_type;
	const char*		action_name;
	pdf_action_proc	action_proc;
} pdf_action;

int pdf_info_proc(int argc, char** argv);
int pdf_fonts_proc(int argc, char** argv);
int pdf_to_text_proc(int argc, char** argv);
int pdf_extr_imgs_proc(int argc, char** argv);
int pdf_to_imgs_proc(int argc, char** argv);

pdf_action pdf_actions[pdf_last_act] =
{
	{pdf_info,		"info",		pdf_info_proc},
	{pdf_fonts,		"fonts",	pdf_fonts_proc},
	{pdf_to_text,	"totext",	pdf_to_text_proc},
	{pdf_extr_imgs,	"extrimgs",	pdf_extr_imgs_proc},
	{pdf_to_imgs,	"toimgs",	pdf_to_imgs_proc}
};

int print_pdf_info(char* filename,
				   char* owner_password,
				   char* user_password,
				   char* text_enc_name);

int print_pdf_fonts(char* filename,
					char* owner_password,
					char* user_password);

int convert_pdf_to_text(char* filename,
						char* textfile,
						char* owner_password,
						char* user_password,
						char* text_enc_name,
						char* text_eol,
						char* range,
						GBool phys_layout,
						GBool raw_order);

int extract_images_from_pdf(char* filename,
							char* target,
							char* owner_password,
							char* user_password,
							char* range,
							char* format,
							int jpg_quality,
							GBool dump_jpg,
							GBool tiff_jpg);

int convert_pdf_to_images(char* filename,
						  char* target,
						  char* owner_password,
						  char* user_password,
						  char* range,
						  int resolution,
						  GBool mono,
						  GBool gray,
						  char* format,
						  int jpg_quality,
						  GBool multi_page,
						  GBool tiff_jpg);

void signal_trap(int sig)
{
	user_cancelled = 1;
}

int main(int argc, char** argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	
	printf("GalaPDF 1.0.0\n");

	int ret_code = gpret_param_err;

#if defined(_WIN32) || defined(__WIN32__)
	#define DIR_SEP '\\'
#else
	#define DIR_SEP '/'
#endif
	strcpy(cfg_file, argv[0]);
	int i;
	for (i = strlen(cfg_file) - 1; i >= 0; i--)
	{
		if (cfg_file[i] == DIR_SEP)
		{
			cfg_file[i + 1] = 0;
			strcat(cfg_file, "xpdfrc");
			break;
		}
	}
	if (i < 0)
		strcpy(cfg_file, "xpdfrc");

#if !defined(_WIN32) && !defined(__WIN32__)
	sigset(SIGTERM, signal_trap);
#endif

	if (argc >= 3)
	{
		char* action = argv[1];
		for (int i = 0; i < pdf_last_act; i++)
		{
			if (!strcmp(action, pdf_actions[i].action_name))
			{
				ret_code = pdf_actions[i].action_proc(argc, argv);
				break;
			}
		}
	}

	// check for memory leaks
	Object::memCheck(stderr);
	gMemReport(stderr);

	return ret_code;
}

int pdf_info_proc(int argc, char** argv)
{
	char owner_password[33]	= "\001";
	char user_password[33]	= "\001";
	char text_enc_name[128]	= "";
	ArgDesc arg_desc[] =
	{
		{"-opw",	argString,	owner_password,	sizeof(owner_password),	""},
		{"-upw",	argString,	user_password,	sizeof(user_password),	""},
		{"-enc",	argString,	text_enc_name,	sizeof(text_enc_name),	""},
		{0, (ArgKind)0, 0, 0, 0}
	};

	GBool param_ok = parseArgs(arg_desc, &argc, argv);
	if (!param_ok || argc < 3)
		return gpret_param_err;

	return print_pdf_info(argv[2],
						  owner_password, user_password, text_enc_name);
}

int pdf_fonts_proc(int argc, char** argv)
{
	char owner_password[33]	= "\001";
	char user_password[33]	= "\001";
	ArgDesc arg_desc[] =
	{
		{"-opw",	argString,	owner_password,	sizeof(owner_password),	""},
		{"-upw",	argString,	user_password,	sizeof(user_password),	""},
		{0, (ArgKind)0, 0, 0, 0}
	};

	GBool param_ok = parseArgs(arg_desc, &argc, argv);
	if (!param_ok || argc < 3)
		return gpret_param_err;

	return print_pdf_fonts(argv[2], owner_password, user_password);
}

int pdf_to_text_proc(int argc, char** argv)
{
	char owner_password[33]	= "\001";
	char user_password[33]	= "\001";
	char text_enc_name[128]	= "";
	char text_eol[16]		= "";
	char range[1024] = "\0";
	GBool phys_layout	= gFalse;
	GBool raw_order		= gFalse;

	ArgDesc arg_desc[] =
	{
		{"-opw",	argString,	owner_password,	sizeof(owner_password),	""},
		{"-upw",	argString,	user_password,	sizeof(user_password),	""},
		{"-enc",	argString,	text_enc_name,	sizeof(text_enc_name),	""},
		{"-eol",	argString,	&text_eol,		sizeof(text_eol),		""},
		{"-pr",		argString,	range,			sizeof(range),			""},
		{"-raw",	argFlag,	&raw_order,		0,						""},
		{"-layout",	argFlag,	&phys_layout,	0,						""},
		{0, (ArgKind)0, 0, 0, 0}
	};

	GBool param_ok = parseArgs(arg_desc, &argc, argv);
	if (!param_ok || argc < 3)
		return gpret_param_err;

	return convert_pdf_to_text(argv[2], argc > 3 ? argv[3] : NULL,
							   owner_password, user_password, text_enc_name,
							   text_eol, range, phys_layout, raw_order);
}

int pdf_extr_imgs_proc(int argc, char** argv)
{
	char owner_password[33]	= "\001";
	char user_password[33]	= "\001";
	char format[16] = "";
	char range[1024] = "\0";
	int jpg_quality	= 75;
	GBool dump_jpg	= gFalse;
	GBool tiff_jpg	= gFalse;

	ArgDesc arg_desc[] =
	{
		{"-opw",	argString,	owner_password,	sizeof(owner_password),	""},
		{"-upw",	argString,	user_password,	sizeof(user_password),	""},
		{"-fmt",	argString,	format,			sizeof(format),			""},
		{"-pr",		argString,	range,			sizeof(range),			""},
		{"-jq",		argInt,		&jpg_quality,	0,						""},
		{"-j",		argFlag,	&dump_jpg,		0,						""},
		{"-tj",		argFlag,	&tiff_jpg,		0,						""},
		{0, (ArgKind)0, 0, 0, 0}
	};

	GBool param_ok = parseArgs(arg_desc, &argc, argv);
	if (!param_ok || argc < 4)
		return gpret_param_err;

	return extract_images_from_pdf(argv[2], argv[3], owner_password,
								   user_password, range, format, jpg_quality,
								   dump_jpg, tiff_jpg);
}

int pdf_to_imgs_proc(int argc, char** argv)
{
	char owner_password[33]	= "\001";
	char user_password[33]	= "\001";
	char range[1024] = "\0";
	int resolution	= 150;
	int jpg_quality	= 75;
	GBool mono		= gFalse;
	GBool gray		= gFalse;
	GBool multipage	= gFalse;
	GBool tiff_jpg	= gFalse;
	char format[16] = "";

	ArgDesc arg_desc[] =
	{
		{"-opw",	argString,	owner_password,	sizeof(owner_password),	""},
		{"-upw",	argString,	user_password,	sizeof(user_password),	""},
		{"-pr",		argString,	range,			sizeof(range),			""},
		{"-r",		argInt,		&resolution,	0,						""},
		{"-mono",	argFlag,	&mono,			0,						""},
		{"-gray",	argFlag,	&gray,			0,						""},
		{"-fmt",	argString,	format,			sizeof(format),			""},
		{"-jq",		argInt,		&jpg_quality,	0,						""},
		{"-mp",		argFlag,	&multipage,		0,						""},
		{"-tj",		argFlag,	&tiff_jpg,		0,						""},
		{0, (ArgKind)0, 0, 0, 0}
	};

	GBool param_ok = parseArgs(arg_desc, &argc, argv);
	if (!param_ok || argc < 4)
		return gpret_param_err;

	return convert_pdf_to_images(argv[2], argv[3], owner_password,
								 user_password, range, resolution, mono, gray,
								 format, jpg_quality, multipage, tiff_jpg);
}
