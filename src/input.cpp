#include "input.h"
#include <cctype>
#include <cstdio>
#include <stdexcept>

Input::Input(Song* song)
	: song(song), filename(""), fs(0)
{
}

Input::~Input()
{
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

// Return 1 if eol. Should handle windows newlines as well
bool Line_Input::iseol(int c)
{
	if(c == '\r')
		c = get();
	if(c == '\n' || c == 0)
		return 1;
	return 0;
}

int Line_Input::get()
{
	if(column == buffer.size())
		return 0;
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
	buffer[--column] = c;
}

int Line_Input::get_num()
{
	int base = 10;
	int c = get_token();
	if(std::isdigit(c))
		unget(c);
	else if(c == '$' || c == 'x')
		base = 16;
	if(column == buffer.size())
		throw new std::invalid_argument("expected number");
	const char* ptr = buffer.c_str() + column;
	const char* endptr = ptr;
	int ret = strtol(ptr, (char**)&endptr, base);
	if(ptr == endptr)
		throw new std::invalid_argument("expected number");
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
