#ifndef _pdfutil_h_
#define _pdfutil_h_

#include <ErrorCodes.h>
#include <PDFDoc.h>
#include <GlobalParams.h>
#include <UnicodeMap.h>
#include <FreeImage.h>

#include <string>
#include <list>
#include <algorithm>
#include <cassert>

using namespace std;

class xpdf_rc
{
public:

	xpdf_rc(char* text_enc_name = NULL);
	~xpdf_rc();

	UnicodeMap* get_umap()
	{
		return umap;
	}

private:

	UnicodeMap* umap;
};

class xpdf_doc
{
public:

	xpdf_doc(char* filename, char* owner_password, char* user_password);
	~xpdf_doc();

	PDFDoc* get_doc()
	{
		return doc;
	}

private:

	PDFDoc* doc;
};

class fi_loader
{
public:

	fi_loader()
	{
		FreeImage_Initialise(TRUE);
	}

	~fi_loader()
	{
		FreeImage_DeInitialise();
	}
};

class range_item
{
public:

	range_item(int a, int b)
	{
		first = a;
		last = b;
		valid = (first > 0) && (last >= first);
	}

	range_item(const string& range)
	{
		valid = false;
		size_t pos = range.find('-');
		if (pos == string::npos)
		{
			first = atoi(range.c_str());
			if (first > 0)
			{
				last = first;
				valid = true;
			}
		}
		else if (pos != range.size() - 1)
		{
			first = atoi(range.substr(0, pos).c_str());
			last = atoi(range.substr(pos + 1).c_str());
			valid = (first > 0) && (last >= first);
		}
	}

	bool isvalid() const
	{
		return valid;
	}

	int first;
	int last;

private:

	bool valid;
};

class range_sort
{
public:
	bool operator() (range_item& a, range_item& b) const
	{
		return a.first < b.first;
	}
};

class page_range
{
public:

	page_range(char* range)
		: count(0)
	{
		string srange = range;
		size_t pos = srange.find(',');
		while (pos != string::npos)
		{
			add_item(range_item(srange.substr(0, pos)), false);
			srange = srange.substr(pos + 1);
			pos = srange.find(',');
		}
		add_item(range_item(srange), false);

		range_list.sort(range_sort());

		calculate_count();
	}

	~page_range()
	{
	}

	int page_count() const
	{
		return count;
	}

	int item_count()
	{
		return range_list.size();
	}

	range_item& get_item(int i)
	{
		assert(i >= 0 && i < item_count());
		list<range_item>::iterator ite = range_list.begin();
		for (int j = 0; j < i; j++, ite++) {};
		return *ite;
	}

	void add_item(const range_item& item, bool update_count = true)
	{
		if (!item.isvalid())
			return;

		list<range_item>::iterator ite;
		for (ite = range_list.begin(); ite != range_list.end(); ite++)
		{
			if (combine_item(*ite, item))
			{
				if (update_count)
					calculate_count();
				return;
			}
		}
		range_list.push_back(item);

		if (update_count)
			calculate_count();
	}

private:

	bool combine_item(range_item& dest, const range_item& source)
	{
		if (source.last < dest.first - 1 || source.first > dest.last + 1)
			return false;

		dest.first = min(dest.first, source.first);
		dest.last = max(dest.last, source.last);
		return true;
	}

	void calculate_count()
	{
		count = 0;
		list<range_item>::iterator ite;
		for (ite = range_list.begin(); ite != range_list.end(); ite++)
			count += (ite->last - ite->first + 1);
	}

	list<range_item> range_list;
	int count;
};

#endif // _pdfutil_h_
