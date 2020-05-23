#include "stringf.h"
#include <cstdarg>
#include <cstdio>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#include <stdexcept>
//! Wrapper to get the native filename on windows (needed for unicode support)
std::vector<wchar_t> get_native_filename(const std::string &s, unsigned int cp, int max)
{
	std::vector<wchar_t> wcs (max, 0);
	wchar_t* ptr = wcs.data();
	int l = MultiByteToWideChar(cp,0,s.c_str(),-1,ptr,max-1);
	if(l == 0)
	{
		throw std::runtime_error("get_native_filename");
	}
	return wcs;
}
#else
#endif

std::string stringf(const char* format, ...)
{
	using namespace std;
	char* buf;
	va_list arg1,arg2;
	va_start(arg1,format);
	va_copy(arg2,arg1);
	int size = vsnprintf(NULL,0,format,arg2) + 1;
	buf = new char[size];
	vsnprintf(buf,size,format,arg1);
	std::string out = std::string(buf);
	delete[] buf;
	va_end(arg1);
	va_end(arg2);
	return out;
}

class iequal_class
{
public:
	bool operator() (int c1, int c2) const
	{
		return std::tolower(c1) == std::tolower(c2);
	}
};

bool iequal(const std::string &s1, const std::string &s2)
{
	return s1.size() == s2.size() && std::equal(s1.begin(), s1.end(), s2.begin(), iequal_class());
}
