#include "galapdf.h"
#include "pdfutil.h"
#include <time.h>
#include <math.h>
#include <PDFDocEncoding.h>

static void print_str(Dict *infoDict, char *key, char *text, UnicodeMap *uMap);
static void print_date(Dict *infoDict, char *key, char *text);

int print_pdf_info(char* filename,
				   char* owner_password,
				   char* user_password,
				   char* text_enc_name)
{
	// load config
	xpdf_rc xrc(text_enc_name);
	UnicodeMap* umap = xrc.get_umap();
	if (!umap)
		return gpret_cant_get_textenc;

	// open file
	xpdf_doc xdoc(filename, owner_password, user_password);
	PDFDoc* doc = xdoc.get_doc();
	if (!doc->isOk())
		return doc->getErrorCode() == errEncrypted ?
			   gpret_pdf_encrypted :
			   gpret_cant_open_pdf;

	// print doc info
	Object info;
	doc->getDocInfo(&info);
	if (info.isDict())
	{
		Dict* dict = info.getDict();
		print_str(dict,	"Title",		"Title:          ", umap);
		print_str(dict,	"Subject",		"Subject:        ", umap);
		print_str(dict,	"Keywords",		"Keywords:       ", umap);
		print_str(dict,	"Author",		"Author:         ", umap);
		print_str(dict,	"Creator",		"Creator:        ", umap);
		print_str(dict,	"Producer",		"Producer:       ", umap);
		print_date(dict,	"CreationDate",	"Create Time:    ");
		print_date(dict,	"ModDate",		"Modify Time:    ");
	}
	info.free();

	// print page count
	printf("Pages:          %d\n", doc->getNumPages());

	// print encryption info
	printf("Encrypted:      ");
	if (doc->isEncrypted())
	{
		printf("yes (print:%s copy:%s change:%s addnotes:%s)\n",
			   doc->okToPrint(gTrue) ? "yes" : "no",
			   doc->okToCopy(gTrue) ? "yes" : "no",
			   doc->okToChange(gTrue) ? "yes" : "no",
			   doc->okToAddNotes(gTrue) ? "yes" : "no");
	}
	else
	{
		printf("no\n");
	}

	// print page size
	if (doc->getNumPages() > 0)
	{
		double w = doc->getPageCropWidth(1);
		double h = doc->getPageCropHeight(1);
		printf("Page Size:      %g x %g", w, h);
		if ((fabs(w - 612) < 0.1 && fabs(h - 792) < 0.1) ||
			(fabs(w - 792) < 0.1 && fabs(h - 612) < 0.1))
		{
			printf(" (Letter)");
		}
		else
		{
			double hISO = sqrt(sqrt(2.0)) * 7200 / 2.54;
			double wISO = hISO / sqrt(2.0);
			for (int i = 0; i <= 6; ++i)
			{
				if ((fabs(w - wISO) < 1 && fabs(h - hISO) < 1) ||
					(fabs(w - hISO) < 1 && fabs(h - wISO) < 1))
				{
					printf(" (A%d)", i);
					break;
				}
				hISO = wISO;
				wISO /= sqrt(2.0);
			}
		}
		printf("\n");
	}

	// print file size
	FILE* f = fopen(filename, "rb");
	if (f)
	{
		fseek(f, 0, SEEK_END);
		printf("File Size:      %d\n", (int)ftell(f));
		fclose(f);
	}

	// print linearization info
	printf("Optimized:      %s\n", doc->isLinearized() ? "yes" : "no");

	// print PDF version
	printf("PDF Version:    %.1f\n", doc->getPDFVersion());

	return gpret_success;
}

static void print_str(Dict *infoDict, char *key, char *text, UnicodeMap *uMap)
{
	Object obj;
	GString *s1;
	GBool isUnicode;
	Unicode u;
	char buf[8];
	int i, n;

	if (infoDict->lookup(key, &obj)->isString())
	{
		fputs(text, stdout);

		s1 = obj.getString();
		if ((s1->getChar(0) & 0xff) == 0xfe && (s1->getChar(1) & 0xff) == 0xff)
		{
			isUnicode = gTrue;
			i = 2;
		}
		else
		{
			isUnicode = gFalse;
			i = 0;
		}

		while (i < obj.getString()->getLength())
		{
			if (isUnicode)
			{
				u = ((s1->getChar(i) & 0xff) << 8) | (s1->getChar(i+1) & 0xff);
				i += 2;
			}
			else
			{
				u = pdfDocEncoding[s1->getChar(i) & 0xff];
				++i;
			}
			n = uMap->mapUnicode(u, buf, sizeof(buf));
			fwrite(buf, 1, n, stdout);
		}

		fputc('\n', stdout);
	}
	obj.free();
}

static void print_date(Dict *infoDict, char *key, char *text)
{
	Object obj;
	char *s;
	int year, mon, day, hour, min, sec, n;
	struct tm tmStruct;
	char buf[256];

	if (infoDict->lookup(key, &obj)->isString())
	{
		fputs(text, stdout);
		s = obj.getString()->getCString();
		if (s[0] == 'D' && s[1] == ':')
			s += 2;

		if ((n = sscanf(s, "%4d%2d%2d%2d%2d%2d",
						&year, &mon, &day, &hour, &min, &sec)) >= 1)
		{
			switch (n)
			{
				case 1: mon = 1;
				case 2: day = 1;
				case 3: hour = 0;
				case 4: min = 0;
				case 5: sec = 0;
			}

			tmStruct.tm_year = year - 1900;
			tmStruct.tm_mon = mon - 1;
			tmStruct.tm_mday = day;
			tmStruct.tm_hour = hour;
			tmStruct.tm_min = min;
			tmStruct.tm_sec = sec;
			tmStruct.tm_wday = -1;
			tmStruct.tm_yday = -1;
			tmStruct.tm_isdst = -1;

			if (mktime(&tmStruct) != (time_t)-1 &&
				strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmStruct))
				fputs(buf, stdout);
			else
				fputs(s, stdout);
		}
		else
		{
			fputs(s, stdout);
		}

		fputc('\n', stdout);
	}

	obj.free();
}
