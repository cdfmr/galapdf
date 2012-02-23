#include "galapdf.h"
#include "pdfutil.h"
#include <Object.h>
#include <GfxFont.h>
#include <Annot.h>

static void scan_fonts(Dict *resDict, PDFDoc *doc);
static void scan_font(GfxFont *font, PDFDoc *doc);

static Ref* fonts;
static int fontsLen;
static int fontsSize;

// NB: this must match the definition of GfxFontType in GfxFont.h.
static char *fontTypeNames[] =
{
	"unknown",
	"Type 1",
	"Type 1C",
	"Type 1C (OT)",
	"Type 3",
	"TrueType",
	"TrueType (OT)",
	"CID Type 0",
	"CID Type 0C",
	"CID Type 0C (OT)",
	"CID TrueType",
	"CID TrueType (OT)"
};

int print_pdf_fonts(char* filename, char* owner_password, char* user_password)
{
	// load config
	xpdf_rc xrc;

	// open file
	xpdf_doc xdoc(filename, owner_password, user_password);
	PDFDoc* doc = xdoc.get_doc();
	if (!doc->isOk())
		return doc->getErrorCode() == errEncrypted ?
			   gpret_pdf_encrypted :
			   gpret_cant_open_pdf;

	printf("name                                 type              emb sub uni object id\n");
	printf("------------------------------------ ----------------- --- --- --- ---------\n");

	fonts = NULL;
	fontsLen = fontsSize = 0;

	for (int pg = 1; pg <= doc->getNumPages(); pg++)
	{
		Page* page = doc->getCatalog()->getPage(pg);
		Dict* res_dict = page->getResourceDict();
		if (res_dict)
			scan_fonts(res_dict, doc);

		Object obj1, obj2;
		Annots* annots = new Annots(doc->getXRef(), doc->getCatalog(),
									page->getAnnots(&obj1));
		obj1.free();
		for (int i = 0; i < annots->getNumAnnots(); ++i)
		{
			if (annots->getAnnot(i)->getAppearance(&obj1)->isStream())
			{
				obj1.streamGetDict()->lookup("Resources", &obj2);
				if (obj2.isDict())
					scan_fonts(obj2.getDict(), doc);
				obj2.free();
			}
			obj1.free();
		}
		delete annots;
	}

	// clean up
	gfree(fonts);

	return gpret_success;
}

static void scan_fonts(Dict *resDict, PDFDoc *doc)
{
	Object obj1, obj2, xObjDict, xObj, resObj;
	Ref r;
	GfxFontDict *gfxFontDict;
	GfxFont *font;
	int i;

	// scan the fonts in this resource dictionary
	gfxFontDict = NULL;
	resDict->lookupNF("Font", &obj1);
	if (obj1.isRef())
	{
		obj1.fetch(doc->getXRef(), &obj2);
		if (obj2.isDict())
		{
			r = obj1.getRef();
			gfxFontDict = new GfxFontDict(doc->getXRef(), &r, obj2.getDict());
		}
		obj2.free();
	}
	else if (obj1.isDict())
	{
		gfxFontDict = new GfxFontDict(doc->getXRef(), NULL, obj1.getDict());
	}

	if (gfxFontDict)
	{
		for (i = 0; i < gfxFontDict->getNumFonts(); ++i)
		{
			if ((font = gfxFontDict->getFont(i)))
				scan_font(font, doc);
		}
		delete gfxFontDict;
	}

	obj1.free();

	// recursively scan any resource dictionaries in objects in this
	// resource dictionary
	resDict->lookup("XObject", &xObjDict);
	if (xObjDict.isDict())
	{
		for (i = 0; i < xObjDict.dictGetLength(); ++i)
		{
			xObjDict.dictGetVal(i, &xObj);
			if (xObj.isStream())
			{
				xObj.streamGetDict()->lookup("Resources", &resObj);
				if (resObj.isDict())
					scan_fonts(resObj.getDict(), doc);
				resObj.free();
			}
			xObj.free();
		}
	}
	xObjDict.free();
}

static void scan_font(GfxFont *font, PDFDoc *doc)
{
	Ref fontRef, embRef;
	Object fontObj, toUnicodeObj;
	GString *name;
	GBool emb, subset, hasToUnicode;
	int i;

	fontRef = *font->getID();

	// check for an already-seen font
	for (i = 0; i < fontsLen; ++i)
		if (fontRef.num == fonts[i].num && fontRef.gen == fonts[i].gen)
			return;

	// font name
	name = font->getOrigName();

	// check for an embedded font
	if (font->getType() == fontType3)
		emb = gTrue;
	else
		emb = font->getEmbeddedFontID(&embRef);

	// look for a ToUnicode map
	hasToUnicode = gFalse;
	if (doc->getXRef()->fetch(fontRef.num, fontRef.gen, &fontObj)->isDict())
	{
		hasToUnicode =
			fontObj.dictLookup("ToUnicode", &toUnicodeObj)->isStream();
		toUnicodeObj.free();
	}
	fontObj.free();

	// check for a font subset name: capital letters followed by a '+' sign
	subset = gFalse;
	if (name)
	{
		for (i = 0; i < name->getLength(); ++i)
			if (name->getChar(i) < 'A' || name->getChar(i) > 'Z')
				break;
		subset = i > 0 && i < name->getLength() && name->getChar(i) == '+';
	}

	// print the font info
	printf("%-36s %-17s %-3s %-3s %-3s",
		   name ? name->getCString() : "[none]",
		   fontTypeNames[font->getType()],
		   emb ? "yes" : "no",
		   subset ? "yes" : "no",
		   hasToUnicode ? "yes" : "no");
	if (fontRef.gen >= 100000)
		printf(" [none]\n");
	else
		printf(" %6d %2d\n", fontRef.num, fontRef.gen);

	// add this font to the list
	if (fontsLen == fontsSize)
	{
		fontsSize += 32;
		fonts = (Ref *)greallocn(fonts, fontsSize, sizeof(Ref));
	}
	fonts[fontsLen++] = *font->getID();
}
