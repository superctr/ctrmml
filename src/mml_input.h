#ifndef MML_INPUT_H
#define MML_INPUT_H
#include <fstream>
#include <vector>
#include "track.h"
#include "song.h"

class MML_Input
{
	private:
		std::string filename;
		std::fstream fs;
		unsigned int line;
		unsigned int column;

		std::string buffer;
		bool eof_flag;

		Song* song;
		Tag* tag;
		Track* track;
		uint16_t track_id;
		uint16_t track_offset;
		std::vector<uint16_t> track_list;

		void (MML_Input::*last_cmd)();

		// Text buffer commands
		void error(char* error_msg, int column, int fatal);
		void iseol(int c);
		void my_getc();
		void my_ungetc();
		int scannum(int* ptr);

		// MML read helpers
		int read_duration();
		int read_parameter(int default_parameter);
		int expect_parameter();
		int expect_signed();
		int read_note(int c); // c is the first character
		
		// Wrappers that provide error/warning messages
		// or other functions
		void mml_slur();
		void mml_reverse_rest(int duration);
		void mml_grace();
		void atom_relative(Atom_Command type, Atom_Command subtype);

		// Parsers for various parts of the MML file
		void parse_mml_track(int conditional_block);
		void parse_mml();
		void parse_tag();

		// Convert track id from character
		static int convert_track_id(char c);

	public:
		MML_Input(Song* song);
		~MML_Input();

		bool open_file(std::string filename);
		bool parse_line(std::string line);
};
#endif

