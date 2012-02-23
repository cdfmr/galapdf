#include "galapdf.h"
#include "pdfutil.h"

xpdf_rc::xpdf_rc(char* text_enc_name /*=NULL*/)
{
	globalParams = new GlobalParams(cfg_file);
	if (text_enc_name && text_enc_name[0])
		globalParams->setTextEncoding(text_enc_name);
	umap = globalParams->getTextEncoding();
}

xpdf_rc::~xpdf_rc()
{
	if (umap)
		umap->decRefCnt();
	delete globalParams;
}

xpdf_doc::xpdf_doc(char* filename, char* owner_password, char* user_password)
{
	GString* file_name	= new GString(filename);
	GString* owner_pwd	= owner_password[0] != '\001' ?
						  new GString(owner_password) :
						  NULL;
	GString* user_pwd	= user_password[0] != '\001' ?
						  new GString(user_password) :
						  NULL;

	doc = new PDFDoc(file_name, owner_pwd, user_pwd);
	delete owner_pwd;
	delete user_pwd;
}

xpdf_doc::~xpdf_doc()
{
	delete doc;
}
