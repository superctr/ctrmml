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
		std::snprintf(buf,200,"%s:%d:%d: %s", ref->get_filename().c_str(), ref->get_line()+1, ref->get_column()+1, message);
	}
}

//! Return exception information.
const char* InputError::what()
{
	return buf;
}

InputRef::InputRef(const std::string &fn, const std::string &ln, int lno, int col)
	: filename(fn), line_contents(ln), line(lno), column(col)
{
}

const std::string& InputRef::get_filename()
{
	return filename;
}

const unsigned int& InputRef::get_line()
{
	return line;
}
const unsigned int& InputRef::get_column()
{
	return column;
}
const std::string& InputRef::get_line_contents()
{
	return line_contents;
}
std::string InputRef::get_column_arrow()
{
	std::string s;
	return s;
}

Input::Input(Song* song)
	: song(song), filename("")
{
}

Input::~Input()
{
}

//! Get the current associated Song object, where all tags and track should be added.
Song& Input::get_song()
{
	return *song;
}

//! Get current filename
const std::string &Input::get_filename()
{
	return filename;
}

//! Get an InputRef.
/*! This can be overridden by derived classes to support column/file
 * numbers where this is relevant.
 */
std::shared_ptr<InputRef> Input::get_reference()
{
	InputRef r = InputRef(filename);
	return std::make_shared<InputRef>(r);
}

//! Throw an InputError.
void Input::parse_error(const char* msg)
{
	throw InputError(get_reference(), msg);
}

//! Raise a parse warning.
/*! In the future this will be added to a warning buffer...
 */
void Input::parse_warning(const char* msg)
{
	throw InputError(get_reference(), msg);
}

Line_Input::Line_Input(Song* song)
	: Input(song), lines(0), line(0), column(0)
{
}

Line_Input::~Line_Input()
{
}

//! Open file and parse lines.
bool Line_Input::parse_file()
{
	return 0;
}

#if 0 // not used. use parse_error or parse_warning instead
void Line_Input::error(char* error_msg, bool show_column, bool fatal)
{
}
#endif

#if 0 // So far not used. Maybe it's not needed, as I currently read one line at a time.
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

std::shared_ptr<InputRef> Line_Input::get_reference()
{
	InputRef r = InputRef(get_filename(), buffer, line, column);
	return std::make_shared<InputRef>(r);
}

//! Get the next character from the buffer and increase the column number
/*! If at the end of the current buffer, 0 is returned and column number is still incremented.
 */
int Line_Input::get()
{
	if(column == buffer.size())
	{
		column++;
		return 0;
	}
	return buffer[column++];
}

//! Get the next non-blank character from the buffer.
/*! Blank characters are skipped until the next non-blank character is found.
 */
int Line_Input::get_token()
{
	int c;
	do c = get();
	while (std::isblank(c));
	return c;
}

//! Get a number from the buffer.
/*! Blank characters are skipped until the next number is found.
 * '$' or 'x' prefix indicates hexadecimal number.
 * \exception std::invalid_argument if no number could be read.
 */
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

//! Put back the character to the buffer, decrementing the buffer position.
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

//! Get current buffer position.
unsigned long Line_Input::tell()
{
	return column;
}

//! Set buffer position.
void Line_Input::seek(unsigned long pos)
{
	column = pos;
}

//! Read input line and parse it.
bool Line_Input::read_line(const std::string& input_line)
{
	buffer = input_line;
	return parse_line();
}

