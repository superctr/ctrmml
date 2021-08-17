/*! \file src/mml_input.h
 *  \brief MML (Music Macro Language) parser
 *
 *  For more info about the MML dialect used here, see
 *  the [MML reference](mml_ref.md).
 */
#ifndef MML_INPUT_H
#define MML_INPUT_H
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include "input.h"
#include "track.h"

//! MML (Music Macro Language) parser class
/*!
 *  For more info about the MML dialect used here, see
 *  the [MML reference](mml_ref.md).
 *
 *  \todo it should be possible to derive this in order to
 *        better support different MML dialects.
 */
class MML_Input: public Line_Input
{
	public:
		typedef std::map<uint16_t, unsigned long> Track_Position_Map;

		MML_Input(Song* song);
		~MML_Input();

		Track_Position_Map get_track_map();

	private:
		// MML read helpers
		unsigned int read_duration();
		int read_parameter(int default_parameter);
		int expect_parameter();
		int expect_signed();
		int read_note(int c); // c is the first character
		void platform_exclusive();

		// Wrappers that provide error/warning messages
		// or other functions
		void mml_slur();
		void mml_reverse_rest(int duration);
		void mml_grace();
		void mml_transpose();
		void mml_echo();
		void event_relative(Event::Type type, Event::Type subtype);
		void conditional_block_begin();
		void conditional_block_end(int c);

		// MML command parsers. Returns 0 and increments position if parsing succeeds.
		// the idea is that these can be swapped out for different MML dialects or platforms.
		bool mml_basic(); // Notes, length, octave, etc.
		bool mml_control(); // Loop control
		bool mml_envelope(); // Instrument, volume, envelope etc.

		// Parsers for various parts of the MML file
		void parse_mml_track();
		void parse_mml();
		void parse_tag();

		// Convert track id from character
		int get_track_id();

		// Virtual function override from Line_Input
		void parse_line();

		std::string tag_key;
		Track* track;
		uint16_t track_id;
		uint16_t track_offset;
		std::vector<uint16_t> track_list;
		void (MML_Input::*last_cmd)();
		bool conditional_block;
};
#endif

