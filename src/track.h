#ifndef TRACK_H
#define TRACK_H
#include <stdint.h>
#include <vector>
#include "song.h"

enum Atom_Command {
	// basic commands
	ATOM_NOP = 0,
	ATOM_REST, // read durations (off time probably best)
	ATOM_NOTE, // read durations (on time and off time)
	ATOM_TIE, // param is ignored, read durations
	// track commands
	ATOM_CMD_LOOP_START,
	ATOM_CMD_LOOP_BREAK,
	ATOM_CMD_LOOP_END,
	ATOM_CMD_SEGNO,
	ATOM_CMD_JUMP,
	ATOM_CMD_END,
	ATOM_CMD_SLUR, // flag to indicate the next note is legato
	// special channel commands. These affect the same memory as another command
	ATOM_CMD_TRANSPOSE_REL,
	ATOM_CMD_VOL,
	ATOM_CMD_VOL_REL,
	ATOM_CMD_VOL_FINE_REL,
	ATOM_CMD_TEMPO_BPM,
	// channel commands
	ATOM_CMD_CHANNEL_MODE, // always the first channel command
	ATOM_CMD_INS,
	ATOM_CMD_TRANSPOSE,
	ATOM_CMD_DETUNE,
	ATOM_CMD_VOL_FINE,
	ATOM_CMD_PAN,
	ATOM_CMD_VOL_ENVELOPE,
	ATOM_CMD_PITCH_ENVELOPE,
	ATOM_CMD_PAN_ENVELOPE,
	ATOM_CMD_DRUM_MODE,
	ATOM_CMD_TEMPO,
	// special
	ATOM_CMD_COUNT, // last command ID
	ATOM_CHANNEL_CMD = ATOM_CMD_CHANNEL_MODE, // first channel cmd ID
	ATOM_CHANNEL_CMD_COUNT = ATOM_CMD_COUNT - ATOM_CHANNEL_CMD, // channel command count
	ATOM_CMD_INVALID = -1,
};

struct Atom
{
	Atom_Command type;
	int16_t param;
	uint16_t on_time; // only for note and rest
	uint16_t off_time;
};

class Track
{
	protected:
		uint8_t flag;
		uint8_t ch;
		std::vector<Atom> atoms;
		uint16_t last_note_pos; // last atom id that was a note
		int octave;
		uint16_t measure_len;
		uint16_t default_duration; // default duration
		uint16_t quantize;
		uint16_t quantize_parts;

		void enable();
		uint16_t on_time(uint16_t duration);
		uint16_t off_time(uint16_t duration);

	public:
		Track();
		~Track();

		static const int DEFAULT_OCTAVE = 5;
		static const uint16_t DEFAULT_MEASURE_LEN = 96;
		static const uint16_t DEFAULT_QUANTIZE = 8;
		static const uint16_t DEFAULT_QUANTIZE_PARTS = 8;

		bool is_enabled();
		bool in_drum_mode();
		std::vector<Atom> *get_atoms();
		Atom *get_atom(unsigned long position);

		void add_atom(Atom *new_atom);
		void add_atom(Atom_Command type, int16_t param = 0, uint16_t on_time = 0, uint16_t off_time = 0);
		void add_note(int note, uint16_t duration = 0);
		int  add_tie(uint16_t duration = 0);
		void add_rest(uint16_t duration = 0);
		int  add_slur();

		int  reverse_rest(uint16_t duration = 0);
		int  finalize(class Song *song);

		void set_octave(int param);
		void change_octave(int param);
		int  set_quantize(uint16_t param, uint16_t parts = 8);
		void set_drum_mode(uint16_t param);

		// These are for use by input handlers but can also be used if duration passed to add_note is 0.
		void set_duration(uint16_t duration);
		uint16_t get_duration(uint16_t duration = 0);
};
#endif

