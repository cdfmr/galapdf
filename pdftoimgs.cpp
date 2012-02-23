#include "galapdf.h"
#include "pdfutil.h"
#include <Object.h>
#include <SplashOutputDev.h>
#include <SplashBitmap.h>

static FIBITMAP* splash_to_fibitmap(SplashBitmap* bitmap, int resolution);
static BOOL save_splash_bitmap(SplashBitmap* bitmap, char* filename,
							   int resolution, FREE_IMAGE_FORMAT fif,
							   int jpg_quality, GBool tiff_jpg);

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
						  GBool tiff_jpg)
{
	if (user_cancelled)
		return gpret_user_cancelled;

	// check output format
	FREE_IMAGE_FORMAT fif = FIF_BMP;
	if (!strcmp(format, "jpg"))
		fif = FIF_JPEG;
	else if (!strcmp(format, "png"))
		fif = FIF_PNG;
	else if (!strcmp(format, "gif"))
		fif = FIF_GIF;
	else if (!strcmp(format, "tif"))
		fif = FIF_TIFF;
	else
		strcpy(format, "bmp");

	// get base file name
	char file_base[512];
	strcpy(file_base, target);

	// check resolution
	if (resolution <= 0 || resolution > 1200)
		resolution = 150;

	// check jpeg quality
	if (jpg_quality <= 0 || jpg_quality > 100)
		jpg_quality = 75;

	// load free image
	fi_loader fi;

	// load config
	xpdf_rc xrc;
	globalParams->setupBaseFonts(NULL);

	// open file
	xpdf_doc xdoc(filename, owner_password, user_password);
	PDFDoc* doc = xdoc.get_doc();
	if (!doc->isOk())
		return doc->getErrorCode() == errEncrypted ?
			   gpret_pdf_encrypted :
			   gpret_cant_open_pdf;

	// get page range
	page_range range_list(range);
	if (*range == '\0')
	{
		range_list.add_item(range_item(1, doc->getNumPages()));
	}

	if (user_cancelled)
		return gpret_user_cancelled;

	// create splash output device
	SplashColor color;
	SplashOutputDev* splash_out;
	if (mono)
	{
		color[0] = 0xff;
		splash_out = new SplashOutputDev(splashModeMono1, 1, gFalse, color);
	}
	else if (gray)
	{
		color[0] = 0xff;
		splash_out = new SplashOutputDev(splashModeMono8, 1, gFalse, color);
	}
	else
	{
		color[0] = color[1] = color[2] = 0xff;
		splash_out = new SplashOutputDev(splashModeBGR8, 1, gFalse, color);
	}

	splash_out->startDoc(doc->getXRef());

	if (user_cancelled)
	{
		delete splash_out;
		return gpret_user_cancelled;
	}

	// write image files
	if (multi_page && fif == FIF_TIFF)
	{
		char file_name[512];
		sprintf(file_name, "%s.tif", file_base);
		FIMULTIBITMAP* fmb = FreeImage_OpenMultiBitmap(FIF_TIFF, file_name,
													   TRUE, FALSE, FALSE, 0);
		if (!fmb)
		{
			delete splash_out;
			return gpret_save_image_error;
		}

		int progress = 0;
		for (int i = 0; i < range_list.item_count(); i++)
		{
			range_item& item = range_list.get_item(i);
			for (int pg = item.first;
				 pg <= min(item.last, doc->getNumPages());
				 pg++)
			{
				if (user_cancelled)
				{
					FreeImage_CloseMultiBitmap(
						fmb, (tiff_jpg && !mono) ? TIFF_JPEG : 0);
					delete splash_out;
					return gpret_user_cancelled;
				}

				doc->displayPage(splash_out, pg, resolution, resolution,
								 0, gFalse, gTrue, gFalse);
				FIBITMAP* fib = splash_to_fibitmap(splash_out->getBitmap(),
												   resolution);
				if (!fib)
				{
					delete splash_out;
					return gpret_save_image_error;
				}
				FreeImage_AppendPage(fmb, fib);
				FreeImage_Unload(fib);
				printf("progress: %d\n",
					   ++progress * 95 / range_list.page_count());
			}
		}
		FreeImage_CloseMultiBitmap(fmb, (tiff_jpg && !mono) ? TIFF_JPEG : 0);
		printf("progress: 100\n");
	}
	else
	{
		int progress = 0;
		for (int i = 0; i < range_list.item_count(); i++)
		{
			range_item& item = range_list.get_item(i);
			for (int pg = item.first;
				 pg <= min(item.last, doc->getNumPages());
				 pg++)
			{
				if (user_cancelled)
				{
					delete splash_out;
					return gpret_user_cancelled;
				}

				doc->displayPage(splash_out, pg, resolution, resolution,
								 0, gFalse, gTrue, gFalse);
				char file_name[512];
				sprintf(file_name, "%s%05d.%s", file_base, pg, format);
				if (!save_splash_bitmap(splash_out->getBitmap(), file_name,
										resolution, fif, jpg_quality, tiff_jpg))
				{
					delete splash_out;
					return gpret_save_image_error;
				}
				printf("progress: %d\n",
					   ++progress * 100 / range_list.page_count());
			}
		}
	}

	delete splash_out;

	return gpret_success;
}

static FIBITMAP* splash_to_fibitmap(SplashBitmap* bitmap, int resolution)
{
	FIBITMAP* fib = NULL;
	switch (bitmap->getMode())
	{
	case splashModeMono1:
		{
			fib = FreeImage_ConvertFromRawBits(bitmap->getDataPtr(),
				bitmap->getWidth(), bitmap->getHeight(), bitmap->getRowSize(),
				1, 0, 0, 0, 1);
			RGBQUAD* pal = FreeImage_GetPalette(fib);
			pal[0].rgbRed = pal[0].rgbGreen = pal[0].rgbBlue = 0;
			pal[1].rgbRed = pal[1].rgbGreen = pal[1].rgbBlue = 255;
		}
		break;
	case splashModeMono8:
		{
			fib = FreeImage_ConvertFromRawBits(bitmap->getDataPtr(),
				bitmap->getWidth(), bitmap->getHeight(), bitmap->getRowSize(),
				8, 0, 0, 0, 1);
			RGBQUAD* pal = FreeImage_GetPalette(fib);
			for ( int i = 0 ; i < 256 ; i++)
				pal[i].rgbRed = pal[i].rgbGreen = pal[i].rgbBlue = i;
		}
		break;
	case splashModeBGR8:
		{
			fib = FreeImage_ConvertFromRawBits(bitmap->getDataPtr(),
				bitmap->getWidth(), bitmap->getHeight(), bitmap->getRowSize(),
				24, 0, 0, 0, 1);
		}
		break;
	}

	if (fib)
	{
		FreeImage_SetDotsPerMeterX(fib, resolution * 10000 / 254);
		FreeImage_SetDotsPerMeterY(fib, resolution * 10000 / 254);
	}

	return fib;
}

static BOOL save_splash_bitmap(SplashBitmap* bitmap, char* filename,
							   int resolution, FREE_IMAGE_FORMAT fif,
							   int jpg_quality, GBool tiff_jpg)
{
	// read raw data from memory
	FIBITMAP* fib = splash_to_fibitmap(bitmap, resolution);
	if (!fib)
		return false;

	// adjust color depth
	if (fif == FIF_GIF && bitmap->getMode() == splashModeBGR8)
	{
		FIBITMAP* fib_temp = fib;
		fib = FreeImage_ColorQuantize(fib_temp, FIQ_WUQUANT);
		FreeImage_Unload(fib_temp);
	}
	else if (fif == FIF_JPEG && bitmap->getMode() == splashModeMono1)
	{
		FIBITMAP* fib_temp = fib;
		fib = FreeImage_ConvertTo8Bits(fib_temp);
		FreeImage_Unload(fib_temp);
	}

	// save
	int flag = 0;
	if (fif == FIF_JPEG)
		flag = jpg_quality;
	else if (fif == FIF_TIFF && tiff_jpg &&
			 bitmap->getMode() != splashModeMono1)
		flag = TIFF_JPEG;
	BOOL save_ok = FreeImage_Save(fif, fib, filename, flag);
	FreeImage_Unload(fib);

	return save_ok;
}
