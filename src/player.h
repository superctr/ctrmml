//! \file player.h
#ifndef PLAYER_H
#define PLAYER_H
#include "core.h"
#include "track.h"
#include <stack>

//! Track player flags.
//! This controls when Player calls Player::write_event().
enum Player_Flag
{
	PLAYER_NO_FLAG = 0,
	PLAYER_UNROLL_LOOPS = 1<<1, //!< Do not send events inside a loop.
	PLAYER_UNROLL_JUMPS = 1<<2, //!< Do not send events inside a jump.
	PLAYER_SEND_REST = 1<<3, //!< Send Event::REST after an Event::NOTE or Event::TIE event with an off_time defined.
	PLAYER_OPTIMIZE = 1<<4, //!< Filter redundant events.
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
	private:
		Song* song;
		Track* track;
		bool enabled;
		int position;
		int loop_position;
		std::stack<Player_Stack> stack;
		unsigned int play_time;
		unsigned int loop_count;
		unsigned int stack_depth[Player_Stack::MAX_STACK_TYPE];

	protected:
		Event event;
		Event *track_event;
		unsigned int on_time;
		unsigned int off_time;
		virtual void handle_event() = 0;
		void stack_push(const Player_Stack& stack_obj);
		Player_Stack& stack_top(Player_Stack::Type type);
		Player_Stack stack_pop(Player_Stack::Type type);
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
	private:
		Player_Flag flag;
		void handle_event();
	protected:
		int track_state[Event::CHANNEL_CMD_COUNT];
		uint32_t track_update_mask;
		virtual void write_event();
	public:
		Player(Song& song, Track& track, Player_Flag flag = PLAYER_NO_FLAG);
		void play_ticks(unsigned int ticks);
		void skip_ticks(unsigned int ticks);
};

#endif

