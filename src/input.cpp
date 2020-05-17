#include "input.h"
#include "song.h"
#include <cctype>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <stdexcept>

//! Creates an InputError exception.
/*!
 *  \param ref a reference pointing at the error. If this is nullptr, a generic
 *         error message is generated.
 */
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

//=============================================================================

//! Creates an InputRef.
InputRef::InputRef(const std::string &fn, const std::string &ln, int lno, int col)
	: filename(fn), line_contents(ln), line(lno), column(col)
{
}

//! Return the file name
const std::string& InputRef::get_filename() const
{
	return filename;
}

//! Return the line number
const unsigned int& InputRef::get_line() const
{
	return line;
}

//! Return the column number
const unsigned int& InputRef::get_column() const
{
	return column;
}

//! Return the contents of the line.
const std::string& InputRef::get_line_contents() const
{
	return line_contents;
}

//! Print a formatted InputRef.
std::ostream& operator<<(std::ostream& os, const class InputRef& ref)
{
	os << ref.get_filename() << ":" << ref.get_line() << ":" << ref.get_column();
	return os;
}

//=============================================================================

//! Creates an Input.
Input::Input(Song* song)
	: song(song), filename("")
{
}

Input::~Input()
{
}

//! Open a file and parse it.
/*!
 *  This adds the filepath to the include_path tag,
 *  sets the filename and calls parse_file().
 *
 *  \exception InputError in case of a read or parse error.
 */
void Input::open_file(const std::string& fn)
{
	int path_break = fn.find_last_of("/\\");
	if(path_break != -1)
		song->add_tag("include_path", fn.substr(0, path_break + 1));
	filename = fn;
	parse_file();
}

//! Get the target Song object
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
/*!
 *  This can be overridden by derived classes to support column/file
 *  numbers where this is relevant.
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
/*!
 *  \todo this should be added to a warning buffer...
 */
void Input::parse_warning(const char* msg)
{
	std::cerr << *get_reference() << ": " << msg << "\n";
	std::cerr << get_reference()->get_line_contents() << std::endl;
}

//=============================================================================

//! Creates a Line_Buffer
Line_Buffer::Line_Buffer(std::string line, unsigned int column)
	: column(column)
{
	buffer = std::make_shared<std::string>(line);
}

//! Duplicates a Line_Buffer
Line_Buffer::Line_Buffer(const class Line_Buffer& original)
	: buffer(original.buffer)
	, column(original.column)
{
}

//! Line_Buffer destructor
Line_Buffer::~Line_Buffer()
{
}

//! Get the next character from the buffer
/*!
 *  Also increments the buffer position.
 *
 *  \retval 0 if at the end of the current buffer. The column number
 *          will still be incremented.
 */
int Line_Buffer::get()
{
	if(column >= buffer->size())
	{
		column++;
		return 0;
	}
	return (*buffer)[column++];
}

//! Get the next non-blank character from the buffer.
/*!
 *  Blank characters are skipped until the next non-blank character is
 *  found.
 */
int Line_Buffer::get_token()
{
	int c;
	do c = get();
	while (std::isblank(c));
	return c;
}

//! Get a number from the buffer.
/*!
 *  Blank characters are skipped until the next number is found.
 *  A `$` or `x` prefix indicates hexadecimal number.
 *
 *  \exception std::invalid_argument if no number could be read.
 */
int Line_Buffer::get_num()
{
	int base = 10;
	int c = get_token();
	if(c == '$' || c == 'x')
		base = 16;
	else
		unget(c);
	if(column == buffer->size())
		throw std::invalid_argument("expected number");
	const char* ptr = buffer->c_str() + column;
	const char* endptr = ptr;
	int ret = strtol(ptr, (char**)&endptr, base);
	if(ptr == endptr)
		throw std::invalid_argument("expected number");
	column += endptr - ptr;
	return ret;
}

//! Return a substring starting from the current position.
std::string Line_Buffer::get_line()
{
	return std::string(*buffer, column);
}

//! Put back the character to the buffer, decrementing the buffer position.
/*!
 *  The buffer position is decremented by this call.
 *
 *  \param c character to put back. If this is 0, no character will be put
 *           back and the buffer contents are unchanged. Effectively doing
 *           the same as a seek(tell-1);
 *
 *  \exception std::out_of_range If the buffer position is already at 0.
 */
void Line_Buffer::unget(int c)
{
	if(column == 0)
		throw std::out_of_range("unget too many");
	if(c == 0)
	{
		column--;
		return;
	}
	(*buffer)[--column] = c;
}

//! Get current buffer position.
unsigned long Line_Buffer::tell()
{
	return column;
}

//! Set the buffer position.
void Line_Buffer::seek(unsigned long pos)
{
	column = pos;
}

//! Set the contents of the buffer and reset the position.
void Line_Buffer::set_buffer(std::string line, unsigned int new_column)
{
	buffer = std::make_shared<std::string>(line);
	if(new_column >= 0)
		column = new_column;
}

//=============================================================================

//! Creates a Line_Input.
Line_Input::Line_Input(Song* song)
	: Input(song), Line_Buffer("", 0), line(0)
{
}

Line_Input::~Line_Input()
{
}

//! Open file and parse lines.
void Line_Input::parse_file()
{
	std::ifstream inputfile = std::ifstream(get_filename());
	buffer = std::make_shared<std::string>("");
	if(!inputfile)
		parse_error("failed to open file");
	line = 0;
	for(std::string str; std::getline(inputfile, str);)
	{
		read_line(str);
		line++;
	}
}

std::shared_ptr<InputRef> Line_Input::get_reference()
{
	InputRef r = InputRef(get_filename(), *buffer, line, column);
	return std::make_shared<InputRef>(r);
}

//! Read a single input line and parse it.
/*!
 *  Optionally also set the line number.
 */
void Line_Input::read_line(const std::string& input_line, int line_number)
{
	if (line_number >= 0)
		line = line_number;
	column = 0;
	buffer = std::make_shared<std::string>(input_line);
	parse_line();
}
