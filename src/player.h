//! \file player.h
#ifndef PLAYER_H
#define PLAYER_H
#include "core.h"
#include "track.h"
#include <stack>
#include <memory>

//! Track player flags.
//! This controls when Player calls Player::write_event().
enum Player_Flag
{
	PLAYER_NO_FLAG = 0,
	PLAYER_UNROLL_LOOPS = 1<<1, //!< Do not send events inside a loop.
	PLAYER_UNROLL_JUMPS = 1<<2, //!< Do not send events inside a jump.
	PLAYER_SEND_REST = 1<<3, //!< Send Event::REST after an Event::NOTE or Event::TIE event with an off_time defined.
	PLAYER_OPTIMIZE = 1<<4, //!< Filter redundant events.
	PLAYER_SKIP = 1<<5, //!< Skip sending events
};

//! Player stack structure.
struct Player_Stack
{
	enum Type {
		LOOP = 0,
		JUMP = 1,
		DRUM_MODE = 2,
		MAX_STACK_TYPE = 3
	} type;
	Track* track;
	int position;
	int end_position;
	int loop_count;
};

//! Basic track player.
/*! This class implements the basic Event commands only.
 */
class Basic_Player
{
	friend Player; // needed to access position
	private:
		Song* song;
		Track* track;
		bool enabled;
		int position;
		int loop_position;
		std::stack<Player_Stack> stack;
		unsigned int loop_count;
		unsigned int stack_depth[Player_Stack::MAX_STACK_TYPE];
		unsigned int max_stack_depth;
		void stack_underflow(int type);

	protected:
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
		//! Called at every event.
		virtual void event_hook() = 0;
		//! Called at the loop position. Return 1 to continue loop, 0 to end playback (end_hook will be called)
		virtual bool loop_hook() = 0;
		//! Called at the end position.
		virtual void end_hook() = 0;
		void error(const char* message) const;
		void stack_push(const Player_Stack& stack_obj);
		Player_Stack& stack_top(Player_Stack::Type type);
		Player_Stack stack_pop(Player_Stack::Type type);
		Player_Stack::Type get_stack_type();
		unsigned int get_stack_depth(Player_Stack::Type type);

	public:
		Basic_Player(Song& song, Track& track);
		bool is_enabled() const;
		unsigned int get_play_time() const;
		unsigned int get_loop_count() const;
		const Event& get_event() const;
		void step_event();
};

//! Generic track player.
/*! This handles the channel-mode commands using the track_state array.
 * Depending on the flags set (see PlayerFlag), the event is then passed to write_event.
 */
class Player : public Basic_Player
{
	friend class Player_Test;
	private:
		uint32_t flag;
		int note_count;
		int rest_count;
		int16_t platform_state[Event::CHANNEL_CMD_COUNT];
		uint32_t platform_update_mask;
		int16_t track_state[Event::CHANNEL_CMD_COUNT];
		uint32_t track_update_mask;
		void handle_drum_mode();
		void handle_event();
		void event_hook() override;
		bool loop_hook() override;
		void end_hook() override;
	protected:
		bool get_platform_flag(unsigned int type) const;
		void clear_platform_flag(unsigned int type);
		bool get_update_flag(Event::Type type) const;
		void clear_update_flag(Event::Type type);
		virtual uint32_t parse_platform_event(const Tag& tag, int16_t* platform_state);
		virtual void write_event();
	public:
		Player(Song& song, Track& track, Player_Flag flag = PLAYER_NO_FLAG);
		bool coarse_volume_flag() const;
		bool bpm_flag() const;
		int16_t get_var(Event::Type type) const;
		int16_t get_platform_var(int type) const;
		void play_tick();
		void skip_ticks(unsigned int ticks);
};

//! Track validator and length calculator
/*!
 * Used to detect basic playback errors and to get the track lengths
 */
class Track_Validator : public Basic_Player
{
	private:
		void event_hook() override;
		bool loop_hook() override;
		void end_hook() override;
		int segno_time;
		unsigned int loop_time;
	public:
		Track_Validator(Song& song, Track& track);
		unsigned int get_loop_length() const;
};

//! This validates and calculates length for each track.
class Song_Validator
{
	private:
		std::map<uint16_t,Track_Validator> track_map;
	public:
		Song_Validator(Song& song);
		const std::map<uint16_t,Track_Validator>& get_track_map() const;
};

#endif

