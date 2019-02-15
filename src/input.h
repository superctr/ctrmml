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
		std::string line_contents;
		unsigned int line;
		unsigned int column;
	public:
		InputRef(const std::string &filename = "", const std::string &line = "", int line_no = 0, int column = 0);
		const std::string& get_filename();
		const unsigned int& get_line();
		const unsigned int& get_column();
		const std::string& get_line_contents();
		std::string get_column_arrow();
};

// Abstract class for the song data parser.
class Input
{
	protected:
		Song* song;
		std::string filename;
		std::fstream fs;

		virtual bool parse_file() = 0;
		virtual std::shared_ptr<InputRef> get_reference();

		bool include_file(const std::string filename);
		void parse_error(const char* msg);
		void parse_warning(const char* msg);

	public:
		Input(Song* song);
		virtual ~Input();

		bool open_file(const std::string &filename);
		static Input& get_input(const std::string &filename); // Get appropriate input type based on the filename
};

class Line_Input: public Input
{
	protected:
		std::vector<std::string> lines;
		std::string buffer; // current line used by get/unget functions, etc.
		unsigned int line;
		unsigned int column;
		bool eof_flag;

		// Provide reference for line commands
		std::shared_ptr<InputRef> get_reference();

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

