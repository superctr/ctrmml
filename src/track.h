//! \file track.h
#ifndef TRACK_H
#define TRACK_H
#include <stdint.h>
#include <vector>
#include <memory>
#include "core.h"

//! Track event.
struct Event
{
	enum Type {
		// basic commands
		NOP = 0,		//!< Does nothing and ignores all parameters.
		REST,			//!< Key off. Reads off_time.
		NOTE,			//!< Key on, Param defines note, Reads on_time and off_time.
		TIE, 			//!< Extends the previous note or rest. Reads on_time and off_time.
		// track commands
		LOOP_START,		//!< Start of a loop block.
		LOOP_BREAK,		//!< Skip to the end of the loop block.
		LOOP_END,		//!< End of the loop block. Param defines loop count
		SEGNO,			//!< Set the track loop position.
		JUMP,			//!< Jump to a track. Param specifies the track number. Previous position is stored in stack.
		END,			//!< Jump to the stack position, alternatively the loop position, alternatively stops the track.
		SLUR,			//!< Indicates that the next note is legato
		// special channel commands. These affect the same memory as another command
		TRANSPOSE_REL,	//!< Relative transpose.
		VOL,			//!< Coarse volume. Param is between 0-15, should correspond to -2dB per step.
		VOL_REL,		//!< Relative coarse volume.
		VOL_FINE_REL,	//!< Relative fine volume. Platform-specific.
		TEMPO_BPM,		//!< Set tempo in quarter notes per minute
		// channel commands
		CHANNEL_MODE,	//!< Platform-specific, always the first channel command
		INS,			//!< Set instrument
		TRANSPOSE,		//!< Set transpose
		DETUNE,			//!< Set detune
		VOL_FINE,		//!< Fine volume. Platform-specific.
		PAN,			//!< Set panning. Signed parameter.
		VOL_ENVELOPE,	//!< Set volume envelope ID.
		PITCH_ENVELOPE, //!< Set pitch envelope ID.
		PAN_ENVELOPE,	//!< Set pan envelope ID.
		DRUM_MODE,		//!< Set drum mode.
		TEMPO,			//!< Set platform-specific tempo.
		// special
		CMD_COUNT,		//!< Command ID count.
		CHANNEL_CMD = CHANNEL_MODE, //!< first channel cmd ID
		CHANNEL_CMD_COUNT = CMD_COUNT - CHANNEL_CMD, //!< channel command count
		INVALID = -1, //!< represents an invalid or unknown command.
	};

	//! The event type.
	Event::Type type;
	//! Optional parameter.
	int16_t param;
	//! Key-on time (for NOTE and TIE types only)
	uint16_t on_time;
	//! Key-off time (for NOTE, REST and TIE types only)
	uint16_t off_time;
	//! Pointer to a reference.
	std::shared_ptr<InputRef> reference;
};

//! Track structure.
/*!
 * Each song consists of an indeterminate number of tracks which are used for channels as well as individual phrases
 * that may be referenced by a channel.
 *
 * The functions of this class facilitate easier insertion of events to a track.
 */
class Track
{
	private:
		uint8_t flag;
		uint8_t ch;
		std::vector<Event> events;
		uint16_t last_note_pos; // last event id that was a note
		int octave;
		uint16_t measure_len;
		uint16_t default_duration; // default duration
		uint16_t quantize;
		uint16_t quantize_parts;
		std::shared_ptr<InputRef> reference;

		void enable();
		uint16_t on_time(uint16_t duration);
		uint16_t off_time(uint16_t duration);

	public:
		//! Default octave setting.
		static const int DEFAULT_OCTAVE = 5;
		//! Default measure length (whole note duration).
		static const uint16_t DEFAULT_MEASURE_LEN = 96;
		//! Default quantize dividend.
		static const uint16_t DEFAULT_QUANTIZE = 8;
		//! Default quantize divisor.
		static const uint16_t DEFAULT_QUANTIZE_PARTS = 8;

		Track(uint16_t ppqn = DEFAULT_MEASURE_LEN/4);

		bool is_enabled();
		bool in_drum_mode();
		std::vector<Event>& get_events();
		Event& get_event(unsigned long position);
		unsigned long get_event_count();

		void set_reference(const std::shared_ptr<InputRef>& ref);
		void add_event(Event& new_event);
		void add_event(Event::Type type, int16_t param = 0, uint16_t on_time = 0, uint16_t off_time = 0);
		void add_note(int note, uint16_t duration = 0);
		int  add_tie(uint16_t duration = 0);
		void add_rest(uint16_t duration = 0);
		int  add_slur();

		void reverse_rest(uint16_t duration = 0);
		int  finalize(Song& song, uint16_t track_id);

		void set_octave(int param);
		void change_octave(int param);
		int  set_quantize(uint16_t param, uint16_t parts = 8);
		void set_drum_mode(uint16_t param);

		// These are for use by input handlers but can also be used if duration passed to add_note is 0.
		void set_duration(uint16_t duration);
		uint16_t get_duration(uint16_t duration = 0);
		uint16_t get_measure_len();
};
#endif

