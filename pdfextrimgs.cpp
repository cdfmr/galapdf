#include "galapdf.h"
#include "pdfutil.h"
#include <stdio.h>
#include <GfxState.h>
#include <OutputDev.h>
#include <Error.h>

class image_extractor: public OutputDev
{
public:
	image_extractor(char* target_p, GBool dump_jpeg_p, char* format_p,
					int jpg_quality_p, GBool tiff_jpeg_p)
	{
		strcpy(file_base, target_p);

		// check output format
		strcpy(format, format_p);
		if (!strcmp(format, "jpg"))
			fif = FIF_JPEG;
		else if (!strcmp(format, "png"))
			fif = FIF_PNG;
		else if (!strcmp(format, "gif"))
			fif = FIF_GIF;
		else if (!strcmp(format, "tif"))
			fif = FIF_TIFF;
		else
		{
			fif = FIF_BMP;
			strcpy(format, "bmp");
		}

		// check jpeg quality
		jpg_quality = jpg_quality_p;
		if (jpg_quality <= 0 || jpg_quality > 100)
			jpg_quality = 75;

		dump_jpeg = dump_jpeg_p;
		tiff_jpeg = tiff_jpeg_p;
		img_num = 0;
	}

	virtual ~image_extractor()
	{
	}

	virtual int get_image_number()
	{
		return img_num;
	}

	int getImageNum() { return img_num; }
	virtual GBool isOk() { return gTrue; }
	virtual GBool interpretType3Chars() { return gFalse; }
	virtual GBool needNonText() { return gTrue; }
	virtual GBool upsideDown() { return gTrue; }
	virtual GBool useDrawChar() { return gFalse; }

	void save_stream_to_file(Stream* str, char* file_name)
	{
		FILE *f;

		if (!(f = fopen(file_name, "wb")))
		{
			error(-1, "Couldn't create file '%s'", file_name);
			return;
		}

		str = ((DCTStream *)str)->getRawStream();
		str->reset();
		int c;
		while ((c = str->getChar()) != EOF)
			fputc(c, f);

		str->close();
		fclose(f);
	}

	void save_stream_to_image(Stream* str, GfxImageColorMap* color_map,
							  int width, int height, GBool mono,
							  char* file_name, GBool mask)
	{
		// read raw data from memory
		FIBITMAP* fib;
		if (mono)
		{
			int size = height * ((width + 7) / 8);
			unsigned char* buf = (unsigned char*)malloc(size);
			if (!buf) return;
			unsigned char* p = buf;
			str->reset();
			for (int i = 0; i < size; i++)
				*p++ = str->getChar();
			str->close();

			fib = FreeImage_ConvertFromRawBits(buf, width, height,
											   (width + 7) / 8, 1, 0, 0, 0, 1);
			RGBQUAD* pal = FreeImage_GetPalette(fib);
			pal[0].rgbRed = pal[0].rgbGreen = pal[0].rgbBlue = (mask ? 0 : 255);
			pal[1].rgbRed = pal[1].rgbGreen = pal[1].rgbBlue = (mask ? 255 : 0);
			free(buf);
		}
		else
		{
			int row_size = width * 3;
			if (row_size % 4)
				row_size += (4 - row_size % 4);
			unsigned char* buf = (unsigned char*)malloc(height * row_size);
			if (!buf) return;

			ImageStream* img_str =
				new ImageStream(str, width, color_map->getNumPixelComps(),
								color_map->getBits());
			img_str->reset();

			Guchar* p;
			unsigned char* pbuf;
			GfxRGB rgb;

			for (int y = 0; y < height; ++y)
			{
				p = img_str->getLine();
				pbuf = buf + row_size * y;
				for (int x = 0; x < width; ++x)
				{
					color_map->getRGB(p, &rgb);
					pbuf[0] = colToByte(rgb.b);
					pbuf[1] = colToByte(rgb.g);
					pbuf[2] = colToByte(rgb.r);
					p += color_map->getNumPixelComps();
					pbuf += 3;
				}
			}

			fib = FreeImage_ConvertFromRawBits(buf, width, height, row_size,
											   24, 0, 0, 0, 1);
			delete img_str;
			free(buf);
		}

		// adjust color depth
		if (fif == FIF_GIF && !mono)
		{
			FIBITMAP* fib_temp = fib;
			fib = FreeImage_ColorQuantize(fib_temp, FIQ_WUQUANT);
			FreeImage_Unload(fib_temp);
		}
		else if (fif == FIF_JPEG && mono)
		{
			FIBITMAP* fib_temp = fib;
			fib = FreeImage_ConvertTo8Bits(fib_temp);
			FreeImage_Unload(fib_temp);
		}

		// save
		FreeImage_SetDotsPerMeterX(fib, 72 * 10000 / 254);
		FreeImage_SetDotsPerMeterY(fib, 72 * 10000 / 254);
		int flag = 0;
		if (fif == FIF_JPEG) flag = jpg_quality;
		else if (fif == FIF_TIFF && tiff_jpeg && !mono) flag = TIFF_JPEG;
		if (!FreeImage_Save(fif, fib, file_name, flag))
			error(-1, "Couldn't create file '%s'", file_name);
		FreeImage_Unload(fib);
	}
	
	void save_stream_to_alpha_image(Stream* str, GfxImageColorMap* colorMap,
									int width, int height, Stream* maskStr,
									GfxImageColorMap* maskColorMap,
									int maskWidth, int maskHeight,
									char* file_name)
	{
		if (fif != FIF_PNG ||
			width == 0 || height == 0 || maskWidth == 0 || maskHeight == 0)
		{
			save_stream_to_image(str, colorMap, width, height,
								 colorMap->getNumPixelComps() == 1 &&
								 colorMap->getBits() == 1,
								 file_name, gFalse);
			return;
		}
		
		// allocate buffer
		int row_size = width * 4;
		unsigned char* buf = (unsigned char*)malloc(height * row_size);
		if (!buf) return;
		
		// read mask data
		ImageStream* mask_str =
		new ImageStream(maskStr, maskWidth,
						maskColorMap->getNumPixelComps(),
						maskColorMap->getBits());
		mask_str->reset();
		Guchar* mask_line = mask_str->getLine();
		int old_mask_row = 0;
		for (int y = 0; y < height; ++y)
		{
			unsigned char* pbuf = buf + row_size * y;
			int new_mask_row = y * maskHeight / height;
			for (int i = old_mask_row; i < new_mask_row; ++i)
			{
				mask_line = mask_str->getLine();
			}
			old_mask_row = new_mask_row;
			
			int old_mask_column = 0;
			for (int x = 0; x < width; ++x)
			{
				int new_mask_column = x * maskWidth / width;
				mask_line += maskColorMap->getNumPixelComps() *
				(new_mask_column - old_mask_column);
				old_mask_column = new_mask_column;
				
				GfxGray gray;
				maskColorMap->getGray(mask_line, &gray);
				pbuf[3] = colToByte(gray);
				pbuf += 4;
			}
		}
		delete mask_str;
		
		// read image data
		ImageStream* img_str =
		new ImageStream(str, width, colorMap->getNumPixelComps(),
						colorMap->getBits());
		img_str->reset();
		for (int y = 0; y < height; ++y)
		{
			unsigned char* pbuf = buf + row_size * y;
			Guchar* line = img_str->getLine();
			for (int x = 0; x < width; ++x)
			{
				GfxRGB rgb;
				colorMap->getRGB(line, &rgb);
//				int alpha = pbuf[3];
//				if (alpha < 255 && alpha > 0)
//				{
//					pbuf[0] = (colToByte(rgb.b) + alpha - 255) * 255 / alpha;
//					pbuf[1] = (colToByte(rgb.g) + alpha - 255) * 255 / alpha;
//					pbuf[2] = (colToByte(rgb.r) + alpha - 255) * 255 / alpha;
//				}
//				else
				{
					pbuf[0] = colToByte(rgb.b);
					pbuf[1] = colToByte(rgb.g);
					pbuf[2] = colToByte(rgb.r);
				}
				line += colorMap->getNumPixelComps();
				pbuf += 4;
			}
		}
		delete img_str;
		
		// create image
		FIBITMAP* fib = FreeImage_ConvertFromRawBits(buf, width, height,
													 row_size, 32, 0, 0, 0, 1);
		free(buf);
		
		// save
		FreeImage_SetDotsPerMeterX(fib, 72 * 10000 / 254);
		FreeImage_SetDotsPerMeterY(fib, 72 * 10000 / 254);
		int flag = 0;
		if (fif == FIF_JPEG) flag = jpg_quality;
		else if (fif == FIF_TIFF && tiff_jpeg) flag = TIFF_JPEG;
		if (!FreeImage_Save(fif, fib, file_name, flag))
			error(-1, "Couldn't create file '%s'", file_name);
		FreeImage_Unload(fib);
	}

	virtual void drawImageMask(GfxState *state, Object *ref, Stream *str,
							   int width, int height, GBool invert,
							   GBool inlineImg)
	{
		// dump jpeg
		if (dump_jpeg && str->getKind() == strDCT && !inlineImg)
		{
			sprintf(file_name, "%s%05d.jpg", file_base, ++img_num);
			save_stream_to_file(str, file_name);
		}

		// dump pbm
		else
		{
			sprintf(file_name, "%s%05d.%s", file_base, ++img_num, format);
			save_stream_to_image(str, NULL, width, height, gTrue, file_name,
								 gTrue);
		}
	}

	virtual void drawImage(GfxState *state, Object *ref, Stream *str,
						   int width, int height, GfxImageColorMap *colorMap,
						   int *maskColors, GBool inlineImg)
	{
		// dump jpeg
		if (dump_jpeg && str->getKind() == strDCT &&
			(colorMap->getNumPixelComps() == 1 ||
			 colorMap->getNumPixelComps() == 3) &&
			!inlineImg)
		{
			sprintf(file_name, "%s%05d.jpg", file_base, ++img_num);
			save_stream_to_file(str, file_name);
		}

		// dump pbm
		else if (colorMap->getNumPixelComps() == 1 && colorMap->getBits() == 1)
		{
			sprintf(file_name, "%s%05d.%s", file_base, ++img_num, format);
			save_stream_to_image(str, NULL, width, height, gTrue, file_name,
								 gFalse);
		}

		// dump ppm
		else
		{
			sprintf(file_name, "%s%05d.%s", file_base, ++img_num, format);
			save_stream_to_image(str, colorMap, width, height, gFalse,
								 file_name, gFalse);
		}
	}
	
	virtual void drawSoftMaskedImage(GfxState *state, Object *ref,
									 Stream *str, int width, int height,
									 GfxImageColorMap *colorMap,
									 Stream *maskStr, int maskWidth,
									 int maskHeight,
									 GfxImageColorMap *maskColorMap)
	{
		sprintf(file_name, "%s%05d.%s", file_base, ++img_num, format);
		save_stream_to_alpha_image(str, colorMap, width, height,
								   maskStr, maskColorMap, maskWidth, maskHeight,
								   file_name);
	}

	private:
		char file_base[1024];
		char file_name[1024];
		GBool dump_jpeg;
		int img_num;
		FREE_IMAGE_FORMAT fif;
		char format[16];
		int jpg_quality;
		GBool tiff_jpeg;
};

int extract_images_from_pdf(char* filename,
							char* target,
							char* owner_password,
							char* user_password,
							char* range,
							char* format,
							int jpg_quality,
							GBool dump_jpg,
							GBool tiff_jpg)
{
	if (user_cancelled)
		return gpret_user_cancelled;

	// load config
	xpdf_rc xrc;

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

	// get page range
	page_range range_list(range);
	if (*range == '\0')
	{
		range_list.add_item(range_item(1, doc->getNumPages()));
	}

	if (user_cancelled)
		return gpret_user_cancelled;

	// write image files
	fi_loader fi;
	int progress = 0;
	image_extractor img_out(target, dump_jpg, format, jpg_quality, tiff_jpg);
	for (int i = 0; i < range_list.item_count(); i++)
	{
		range_item& item = range_list.get_item(i);
		for (int pg = item.first;
			 pg <= min(item.last, doc->getNumPages());
			 pg++)
		{
			if (user_cancelled)
				return gpret_user_cancelled;
			doc->displayPage(&img_out, pg, 72, 72, 0, gFalse, gTrue, gFalse);
			printf("progress: %d\n",
				   ++progress * 100 / range_list.page_count());
		}
	}

	printf("image count: %d\n", img_out.get_image_number());

	return gpret_success;
}
