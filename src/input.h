/*! \file src/input.h
 *  \brief Input file formats base
 *
 *  Includes base classes for input file formats.
 */
#ifndef INPUT_H
#define INPUT_H
#include <ostream>
#include <fstream>
#include <string>
#include "core.h"

//! Exception class for input file errors
/*!
 *  This exception is thrown whenever an error occurs while reading
 *  or parsing an Input file.
 *
 *  It should not be thrown directly, but rather by using the
 *  Input::parse_error() function.
 *
 *  \see Input::parse_error()
 */
class InputError : public std::exception
{
	public:
		InputError(std::shared_ptr<InputRef> ref, const char* message);
		const char* what();
		inline std::shared_ptr<InputRef> get_reference() { return reference; }

	private:
		std::shared_ptr<InputRef> reference;
		char buf[200];
};

//! Reference to input data
/*!
 *  The reference includes the filename, and if applicable, line and
 *  column numbers as well as the contents of the line.
 *
 *  \todo this class could also be abstracted or extended to
 *        better support tracker file formats.
 */
class InputRef
{
	public:
		InputRef(const std::string& filename = "", const std::string& line = "", int line_no = 0, int column = 0);

		const std::string& get_filename() const;
		const unsigned int& get_line() const;
		const unsigned int& get_column() const;
		const std::string& get_line_contents() const;

	private:
		std::string filename;
		std::string line_contents;
		unsigned int line;
		unsigned int column;
};

std::ostream& operator<<(std::ostream& os, const class InputRef& ref);

//! Abstract input file format class.
/*!
 *  The general purpose of this class (and derived) is to convert
 *  files to Song objects.
 *
 *  A binary file format might inherit directly from the Input class.
 *  Text-based formats such as MML can use Line_Input that provides
 *  helper functions for reading text lines.
 */
class Input
{
	public:
		Input(Song* song);
		virtual ~Input();

		void open_file(const std::string& filename);
		static Input& get_input(const std::string& filename); // Get appropriate input type based on the filename

	protected:
		Song& get_song();
		const std::string& get_filename();
		virtual std::shared_ptr<InputRef> get_reference();

		void parse_error(const char* msg);
		void parse_warning(const char* msg);
		void include_file(const std::string filename);

		//! Used by derived classes to open and parse a file.
		virtual void parse_file() = 0;

	private:
		Song* song;
		std::string filename;
};

//! Line buffer interface
/*!
 *  This provides a stdio-style interface to a buffer as if it was a file.
 */
class Line_Buffer
{
	friend class Line_Input_Test; // needs access to internal variables.

	public:
		Line_Buffer(std::string line, unsigned int column = -1);
		Line_Buffer(const class Line_Buffer& original);
		virtual ~Line_Buffer();

		int get();
		int get_token();
		int get_num();
		std::string get_line();
		void unget(int c = 0);
		unsigned long tell();
		void seek(unsigned long pos);

	protected:
		std::shared_ptr<std::string> buffer; // current line used by get/unget functions, etc.
		void set_buffer(std::string line, unsigned int new_column = 0);

		unsigned int column;
};

//! Abstract class for text line-based input formats (such as MML)
/*!
 *  Reads the input files, one line at a time, parsing them using
 *  the virtual function parse_line().
 *
 *  To help with parsing, a C stdio-style interface to lines of texts
 *  is provided. (see Line_Buffer)
 *
 *  Because data is stored in an internal buffer and the position
 *  is kept track of, a call to get_reference() (and thus parse_error())
 *  will create an InputRef with the correct line number and column.
 */
class Line_Input: public Input, protected Line_Buffer
{
	friend class Line_Input_Test; // needs access to internal variables.

	public:
		Line_Input(Song* song);
		virtual ~Line_Input();

		void read_line(const std::string& input_line, int line_number = -1);

	protected:
		std::shared_ptr<InputRef> get_reference();

		//! Used by derived classes to read the input lines.
		virtual void parse_line() = 0;

	private:
		void parse_file();

		unsigned int line;
};

#endif

