#ifndef MML_INPUT_H
#define MML_INPUT_H
#include <fstream>
#include <string>
#include <vector>
#include "input.h"
#include "track.h"

class MML_Input: public Line_Input
{
	private:
		Tag* tag;
		Track* track;
		uint16_t track_id;
		uint16_t track_offset;
		std::vector<uint16_t> track_list;
		void (MML_Input::*last_cmd)();

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
		bool parse_line();
};
#endif

