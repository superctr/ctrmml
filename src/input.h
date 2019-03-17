//! \file input.h
#ifndef INPUT_H
#define INPUT_H
#include <ostream>
#include <fstream>
#include <string>
#include "core.h"

//! Parser error exception.
class InputError : public std::exception
{
	private:
		std::shared_ptr<InputRef> reference;
		char buf[200];

	public:
		InputError(std::shared_ptr<InputRef> ref, const char* message);
		const char* what();
};

//! Reference to input data
class InputRef
{
	private:
		std::string filename;
		std::string line_contents;
		unsigned int line;
		unsigned int column;

	public:
		InputRef(const std::string& filename = "", const std::string& line = "", int line_no = 0, int column = 0);

		const std::string& get_filename() const;
		const unsigned int& get_line() const;
		const unsigned int& get_column() const;
		const std::string& get_line_contents() const;
		std::string get_column_arrow() const;
};

std::ostream& operator<<(std::ostream& os, const class InputRef& ref);

//! Abstract class for the song data parser.
class Input
{
	private:
		Song* song;
		std::string filename;

	protected:
		//! Used by derived classes to open and parse a file.
		virtual void parse_file() = 0;

		Song& get_song();
		const std::string& get_filename();
		virtual std::shared_ptr<InputRef> get_reference();

		void parse_error(const char* msg);
		void parse_warning(const char* msg);
		void include_file(const std::string filename);

	public:
		Input(Song* song);
		virtual ~Input();

		void open_file(const std::string& filename);
		static Input& get_input(const std::string& filename); // Get appropriate input type based on the filename
};

//! Abstract class for text line-based input formats (such as MML)
class Line_Input: public Input
{
	friend class Line_Input_Test; // needs access to internal variables.
	private:
		std::vector<std::string> lines;
		std::string buffer; // current line used by get/unget functions, etc.
		unsigned int line;
		unsigned int column;

		void parse_file();

	protected:
		//! Used by derived classes to read the input lines.
		virtual void parse_line() = 0;

		std::shared_ptr<InputRef> get_reference();

		int get();
		int get_token();
		int get_num();
		std::string get_line();
		void unget(int c = 0);
		unsigned long tell();
		void seek(unsigned long pos);

	public:
		Line_Input(Song* song);
		virtual ~Line_Input();
		void read_line(const std::string& input_line);
};

#endif

