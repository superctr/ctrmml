#include "stringf.h"
#include <cstdarg>
#include <cstdio>

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

