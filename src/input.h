#ifndef INPUT_H
#define INPUT_H
#include <fstream>
#include <string>
#include "core.h"

class InputError : public std::exception
{
	private:
		std::shared_ptr<InputRef> reference;
		char buf[200];

	public:
		InputError(std::shared_ptr<InputRef> ref, const char* message);
		const char* what();
};

class InputRef
{
	private:
		std::string filename;
		std::string line;
		int line_nr;
		int column;
	public:
		InputRef(std::string &filename, std::string &line, int line_no = 0, int column = 0);
		std::string& get_filename();
		int& get_line();
		int& get_column();
		std::string& get_line_contents();
		std::string get_column_arrow();
};

class Input
{
	protected:
		Song* song;
		std::shared_ptr<InputRef> reference;
		std::string filename;
		std::fstream fs;
		virtual bool parse_file() = 0;
		bool include_file(std::string filename);
		void set_reference(InputRef& ref);
		void throw_error(const char* msg);
	public:
		Input(Song* song);
		virtual ~Input();
		bool open_file(std::string filename);
		std::shared_ptr<InputRef> get_reference();

		static Input& get_input(std::string filename); // Get appropriate input type based on the filename
};

class Line_Input: public Input
{
	protected:
		std::vector<std::string> lines;
		std::string buffer; // current line used by get/unget functions, etc.
		unsigned int line;
		unsigned int column;
		bool eof_flag;

		// Display error message.
		// column=1 to show the column number, fatal=1 to throw an exception
		void error(char* error_msg, bool column, bool fatal);

		// Text buffer commands
		int get();
		int get_token();
		void unget(int c = 0);
		unsigned long tell();
		void seek(unsigned long pos);

		int get_num();

		bool parse_file();
		virtual bool parse_line() = 0;

	public:
		Line_Input(Song* song);
		virtual ~Line_Input();
		bool read_line(const std::string& input_line);
};

#endif

