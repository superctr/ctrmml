#include "input.h"
#include <cctype>
#include <cstring>
#include <cstdio>
#include <stdexcept>

InputError::InputError(std::shared_ptr<InputRef> ref, const char* message)
	: reference(ref)
{
	if(ref == nullptr)
	{
		std::strncpy(buf,message,200);
	}
	else
	{
		std::snprintf(buf,200,"%s:%d:%d: %s", ref->get_filename().c_str(), ref->get_line(), ref->get_column(), message);
	}
}

const char* InputError::what()
{
	return buf;
}

InputRef::InputRef(std::string &fn, std::string &ln, int lno, int col)
	: filename(fn), line(ln), line_nr(lno), column(col)
{
}

std::string& InputRef::get_filename()
{
	return filename;
}

int& InputRef::get_line()
{
	return line_nr;
}
int& InputRef::get_column()
{
	return column;
}
std::string& InputRef::get_line_contents()
{
	return line;
}
std::string InputRef::get_column_arrow()
{
	std::string s;
	return s;
}

Input::Input(Song* song)
	: song(song), reference(nullptr), filename(""), fs(0)
{
}

Input::~Input()
{
}

void Input::set_reference(InputRef& in_ref)
{
	reference = std::make_shared<InputRef>(in_ref);
}

void Input::throw_error(const char* msg)
{
	throw InputError(reference, msg);
}

Line_Input::Line_Input(Song* song)
	: Input(song), lines(0), line(0), column(0), eof_flag(false)
{
}

Line_Input::~Line_Input()
{
}

void Line_Input::error(char* error_msg, bool show_column, bool fatal)
{
}

#if 0
// Return 1 if eol. Should handle windows newlines as well
bool Line_Input::iseol(int c)
{
	if(c == '\r')
		c = get();
	if(c == '\n' || c == 0)
		return 1;
	return 0;
}
#endif

int Line_Input::get()
{
	if(column == buffer.size())
	{
		column++;
		return 0;
	}
	return buffer[column++];
}

// get next token (non-blank character)
int Line_Input::get_token()
{
	int c;
	do c = get();
	while (std::isblank(c));
	return c;
}

void Line_Input::unget(int c)
{
	if(column == 0)
		throw std::out_of_range("unget too many");
	if(c == 0)
	{
		column--;
		return;
	}
	buffer[--column] = c;
}

unsigned long Line_Input::tell()
{
	return column;
}

void Line_Input::seek(unsigned long pos)
{
	column = pos;
}

int Line_Input::get_num()
{
	int base = 10;
	int c = get_token();
	if(c == '$' || c == 'x')
		base = 16;
	else
		unget(c);
	if(column == buffer.size())
		throw std::invalid_argument("expected number");
	const char* ptr = buffer.c_str() + column;
	const char* endptr = ptr;
	int ret = strtol(ptr, (char**)&endptr, base);
	if(ptr == endptr)
		throw std::invalid_argument("expected number");
	column += endptr - ptr;
	return ret;
}

bool Line_Input::parse_file()
{
	return 0;
}

bool Line_Input::read_line(const std::string& input_line)
{
	buffer = input_line;
	return parse_line();
}
