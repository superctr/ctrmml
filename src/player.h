/*! \file src/player.h
 *  \brief Track player class
 *
 *  Includes base classes for the Track player.
 *
 *  \see Basic_Player
 */
#ifndef PLAYER_H
#define PLAYER_H
#include "core.h"
#include "track.h"
#include <stack>
#include <memory>

//! Player stack frame.
struct Player_Stack
{
	//! Defines the type of stack frame.
	enum Type {
		LOOP = 0,
		JUMP = 1,
		DRUM_MODE = 2,
		MAX_STACK_TYPE = 3
	} type;
	//! Referenced track.
	Track* track;
	//! Event position
	int position;
	//! If \ref LOOP, points to the end position of the loop.
	//! May not be filled in until the loop has iterated once.
	int end_position;
	//! If \ref LOOP, remaining loop count.
	int loop_count;
};

//! Abstract basic track player.
/*!
 *  The player class is used to iterate Track events, handling basic
 *  track events such as looping and jumping to subroutines.
 *
 *  All events are forwarded to the derived classes with the
 *  event_hook(), loop_hook() and end_hook() virtual functions.
 *
 *  At the end of the track, when an Event::END is encountered,
 *  loop_hook() is called. Depending on the return value, the track
 *  is stopped and end_hook() is called.
 *
 *  Typical usage of Basic_Player is to call step_event() until
 *  is_enabled() returns False.
 *
 *  \see Player
 */
class Basic_Player
{
	friend Player; // needed to access position
	friend class Player_Test;

	public:
		Basic_Player(Song& song, Track& track);
		virtual ~Basic_Player();

		void step_event();
		void reset_loop_count();

		bool is_enabled() const;
		bool is_inside_loop() const;
		bool is_inside_jump() const;
		unsigned int get_play_time() const;
		int get_loop_count() const;
		const Event& get_event() const;

		virtual std::vector<std::shared_ptr<InputRef>> get_references();

	protected:
		void disable();
		void stack_push(const Player_Stack& stack_obj);
		Player_Stack& stack_top(Player_Stack::Type type);
		Player_Stack stack_pop(Player_Stack::Type type);
		Player_Stack::Type get_stack_type();
		unsigned int get_stack_depth(Player_Stack::Type type);

		Song* get_song();

		void error(const char* message) const;

		//! Called at every event.
		virtual void event_hook() = 0;
		//! Called at the loop position. Return 1 to continue loop, 0 to end playback (end_hook will be called)
		virtual bool loop_hook() = 0;
		//! Called at the end position.
		virtual void end_hook() = 0;

		//! Current event.
		Event event;
		//! Pointer to current event in the track.
		Event *track_event;
		//! Current reference
		std::shared_ptr<InputRef> reference;
		//! Playing time
		unsigned int play_time;
		//! Keyon time from current event
		unsigned int on_time;
		//! Keyoff time from current event
		unsigned int off_time;

	private:
		void stack_underflow(int type);

		Song* song;
		Track* track;
		bool enabled;
		int position;
		int loop_position;
		int loop_reset_position; // Position to increment the loop count
		int loop_count;
		int loop_reset_count;
		std::stack<Player_Stack> stack;
		unsigned int stack_depth[Player_Stack::MAX_STACK_TYPE];
		unsigned int max_stack_depth;
		// # of loops in the stack where the loop count is 0.
		unsigned int loop_begin_depth;
};

//! Generic track player.
/*!
 *  This handles the channel events using an internal track state
 *  array. Drum mode events are also handled here.
 *
 *  The event is then passed to write_event().
 *
 *  This player also keeps track of note and rest durations (in ticks)
 *  and so if play_tick() is called at a regular interval, playback
 *  of multiple tracks can be synchronized.
 *
 *  skip_ticks() can be used to skip events. Because
 *  events are not sent to write_event() during this time, the derived
 *  write_event() should mainly check for Event::NOTE and Event::REST types
 *  (other Event types in special cases) and get the state of the channel
 *  variables using the \c get_flag and \c get_var (and platform event
 *  equivalents) respectively.
 *
 *  The Player class is intended for actual playback. For conversion,
 *  directly inheriting Basic_Player might be a better idea.
 *
 *  \see Basic_Player
 */
class Player : public Basic_Player
{
	friend class Player_Test;

	public:
		Player(Song& song, Track& track);
		virtual ~Player();

		void skip_ticks(unsigned int ticks);
		void play_tick();

		bool coarse_volume_flag() const;
		bool bpm_flag() const;
		int16_t get_platform_var(int type) const;
		int16_t get_var(Event::Type type) const;

		void set_var(Event::Type type, int16_t val);
		void set_coarse_volume_flag(bool state);

		void platform_update(const Tag& tag);

	protected:
		bool get_platform_flag(unsigned int type) const;
		void clear_platform_flag(unsigned int type);
		bool get_update_flag(Event::Type type) const;
		void set_update_flag(Event::Type type);
		void clear_update_flag(Event::Type type);
		int16_t get_last_note() const;
		virtual uint32_t parse_platform_event(const Tag& tag, int16_t* platform_state);
		virtual void write_event();

	private:
		void handle_drum_mode();
		void handle_event();
		virtual void event_hook() override;
		virtual bool loop_hook() override;
		virtual void end_hook() override;

		int16_t last_note;
		bool skip_flag;
		int note_count;
		int rest_count;
		int16_t platform_state[Event::CHANNEL_CMD_COUNT];
		uint32_t platform_update_mask;
		int16_t track_state[Event::CHANNEL_CMD_COUNT];
		uint32_t track_update_mask;

};

//! Track validator
/*!
 *  The track validator is used to perform a "sanity check" of a Track,
 *  detecting playback errors while also calculating the play and loop
 *  duration.
 */
class Track_Validator : public Basic_Player
{
	public:
		Track_Validator(Song& song, Track& track);

		unsigned int get_loop_length() const;

	private:
		void event_hook() override;
		bool loop_hook() override;
		void end_hook() override;

		int segno_time;
		unsigned int loop_time;
};

//! Song validator
/*!
 *  Validates all tracks in a song using Track_Validator.
 */
class Song_Validator
{
	public:
		Song_Validator(Song& song);

		const std::map<uint16_t,Track_Validator>& get_track_map() const;

	private:
		std::map<uint16_t,Track_Validator> track_map;
};

#endif
