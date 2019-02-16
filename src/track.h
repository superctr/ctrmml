#ifndef TRACK_H
#define TRACK_H
#include <stdint.h>
#include <vector>
#include "core.h"

//! Track event.
struct Event
{
	enum Type {
		// basic commands
		NOP = 0,
		REST, //!< Reads off_time.
		NOTE, //!< Key on, Param defines note, Reads on_time and off_time.
		TIE, //!< Reads on_time and off_time.
		// track commands
		CMD_LOOP_START,
		CMD_LOOP_BREAK,
		CMD_LOOP_END, //!< Param defines loop count
		CMD_SEGNO, //!< Set the track loop position.
		CMD_JUMP, //!< Jump to a track. Param specifies the track number. Previous position is stored in stack.
		CMD_END, //!< Jump to the stack position, alternatively the loop position, alternatively stops the track.
		CMD_SLUR, //!< Indicates that the next note is legato
		// special channel commands. These affect the same memory as another command
		CMD_TRANSPOSE_REL, //!< Relative transpose.
		CMD_VOL, //!< Coarse volume. Param is between 0-15, should correspond to -2dB per step.
		CMD_VOL_REL, //!< Relative coarse volume.
		CMD_VOL_FINE_REL, //!< Relative fine volume. Platform-specific.
		CMD_TEMPO_BPM, //!< Param defines tempo in BPM.
		// channel commands
		CMD_CHANNEL_MODE, //!< Platform-specific, always the first channel command
		CMD_INS, //!< Set instrument
		CMD_TRANSPOSE, //!< Set transpose
		CMD_DETUNE, //!< Set detune
		CMD_VOL_FINE, //!< Fine volume. Platform-specific.
		CMD_PAN, //!< Set panning. Signed parameter.
		CMD_VOL_ENVELOPE, //!< Set volume envelope ID.
		CMD_PITCH_ENVELOPE, //!< Set pitch envelope ID.
		CMD_PAN_ENVELOPE, //!< Set pan envelope ID.
		CMD_DRUM_MODE, //!< Set drum mode.
		CMD_TEMPO, //!< Set platform-specific tempo.
		// special
		CMD_COUNT, //!< Command ID count.
		CHANNEL_CMD = CMD_CHANNEL_MODE, //!< first channel cmd ID
		CHANNEL_CMD_COUNT = CMD_COUNT - CHANNEL_CMD, //!< channel command count
		CMD_INVALID = -1,
	};

	//! The event type.
	Event::Type type;
	//! Optional parameter.
	int16_t param;
	//! Key-on time (for NOTE and TIE types only)
	uint16_t on_time;
	//! Key-off time (for NOTE, REST and TIE types only)
	uint16_t off_time;
};

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

		void enable();
		uint16_t on_time(uint16_t duration);
		uint16_t off_time(uint16_t duration);

	public:
		Track();
		~Track();

		//! Default octave setting.
		static const int DEFAULT_OCTAVE = 5;
		//! Default measure length (whole note duration).
		static const uint16_t DEFAULT_MEASURE_LEN = 96;
		//! Default quantize dividend.
		static const uint16_t DEFAULT_QUANTIZE = 8;
		//! Default quantize divisor.
		static const uint16_t DEFAULT_QUANTIZE_PARTS = 8;

		bool is_enabled();
		bool in_drum_mode();
		std::vector<Event>& get_events();
		Event& get_event(unsigned long position);
		unsigned long get_event_count();

		void add_event(Event& new_event);
		void add_event(Event::Type type, int16_t param = 0, uint16_t on_time = 0, uint16_t off_time = 0);
		void add_note(int note, uint16_t duration = 0);
		int  add_tie(uint16_t duration = 0);
		void add_rest(uint16_t duration = 0);
		int  add_slur();

		void reverse_rest(uint16_t duration = 0);
		int  finalize(Song& song);

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

