#ifndef INPUT_H
#define INPUT_H
#include <fstream>
#include <string>
#include "song.h"

class Input
{
	protected:
		Song* song;
		std::string filename;
		std::fstream fs;
		virtual bool parse_file() = 0;
		bool include_file(std::string filename);
	public:
		Input(Song* song);
		virtual ~Input();
		bool open_file(std::string filename);
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
		bool iseol(int c);
		int get();
		int get_token();
		void unget(int c);
		int get_num();
		bool parse_file();
		virtual bool parse_line() = 0;

	public:
		Line_Input(Song* song);
		virtual ~Line_Input();
		bool read_line(const std::string& input_line);
};

#endif

