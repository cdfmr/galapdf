#include "galapdf.h"
#include "pdfutil.h"
#include <TextOutputDev.h>

int convert_pdf_to_text(char* filename,
						char* textfile,
						char* owner_password,
						char* user_password,
						char* text_enc_name,
						char* text_eol,
						char* range,
						GBool phys_layout,
						GBool raw_order)
{
	if (user_cancelled)
		return gpret_user_cancelled;

	// load config
	xpdf_rc xrc(text_enc_name);
	const UnicodeMap* umap = xrc.get_umap();
	if (!umap)
		return gpret_cant_get_textenc;

	if (text_eol[0] && !globalParams->setTextEOL(text_eol))
		return gpret_bad_eol;
	globalParams->setTextPageBreaks(gFalse);

	// open file
	xpdf_doc xdoc(filename, owner_password, user_password);
	PDFDoc* doc = xdoc.get_doc();
	if (!doc->isOk())
		return doc->getErrorCode() == errEncrypted ?
			   gpret_pdf_encrypted :
			   gpret_cant_open_pdf;

	// check for copy permission
	// if (!doc->okToCopy())
	//	 return gpret_dont_allow_copy;

	if (user_cancelled)
		return gpret_user_cancelled;

	// construct text file name
	if (!textfile)
	{
		char _textfile[256];
		memset(_textfile, 0, sizeof(_textfile));

		char* p = filename + strlen(filename) - 4;
#if defined(_WIN32) || defined(__WIN32__)
		if (!_stricmp(p, ".pdf"))
#else
		if (!strcasecmp(p, ".pdf"))
#endif
			strncpy(_textfile, filename, strlen(filename) - 4);
		else
			strcpy(_textfile, filename);
		strcat(_textfile, ".txt");
		textfile = _textfile;
	}

	// get page range
	page_range range_list(range);
	if (*range == '\0')
	{
		range_list.add_item(range_item(1, doc->getNumPages()));
	}

	// try open text file
	FILE* f;
	if (strcmp(textfile, "-"))
	{
		f = fopen(textfile, "wb");
		if (!f)
			return gpret_cant_open_target;
		fclose(f);
	}

	if (user_cancelled)
		return gpret_user_cancelled;

	// write text file
	TextOutputDev text_out(textfile, phys_layout, raw_order, gFalse);
	if (!text_out.isOk())
		return gpret_cant_init_textout;
	int progress = 0;
	for (int i = 0; i < range_list.item_count(); i++)
	{
		range_item& item = range_list.get_item(i);
		for (int pg = item.first;
			 pg <= min(item.last, doc->getNumPages());
			 pg++)
		{
			if (user_cancelled)
				return gpret_user_cancelled;
			doc->displayPage(&text_out, pg, 72, 72, 0, gFalse, gTrue, gFalse);
			printf("progress: %d\n",
				   ++progress * 100 / range_list.page_count());
		}
	}

	return gpret_success;
}
