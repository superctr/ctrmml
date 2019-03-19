#include "player.h"
#include <stdexcept>
#include "song.h"
#include "track.h"

Basic_Player::Basic_Player(Song& song, Track& track)
	: song(&song),
	track(&track),
	enabled(track.is_enabled()),
	position(0),
	loop_position(-1),
	play_time(0),
	loop_count(0),
	max_stack_depth(10),
	on_time(0),
	off_time(0)
{
}

void Basic_Player::stack_push(const Player_Stack& frame)
{
	if(stack.size() >= max_stack_depth)
		throw std::invalid_argument("stack depth limit reached");
	stack_depth[frame.type]++;
	stack.push(frame);
}

Player_Stack& Basic_Player::stack_top(Player_Stack::Type type)
{
	if(!stack.size())
		throw std::invalid_argument("stack underflow");
	Player_Stack& frame = stack.top();
	if(frame.type != type)
		throw std::invalid_argument("incorrect stack frame");
	return frame;
}

Player_Stack Basic_Player::stack_pop(Player_Stack::Type type)
{
	Player_Stack frame = stack.top();
	stack.pop();
	if(frame.type != type)
		throw std::invalid_argument("incorrect stack frame");
	return frame;
}

unsigned int Basic_Player::get_stack_depth(Player_Stack::Type type)
{
	return stack_depth[type];
}

unsigned int Basic_Player::get_play_time() const
{
	return play_time;
}

unsigned int Basic_Player::get_loop_count() const
{
	return loop_count;
}

const Event& Basic_Player::get_event() const
{
	return event;
}

void Basic_Player::step_event()
{
	// Set accumulated time
	play_time += on_time + off_time;
	on_time = 0;
	off_time = 0;
	try
	{
		// Read the next event
		track_event = &track->get_event(position++);
		event = *track_event;
	}
	catch(std::out_of_range&)
	{
		// reached the end
		event = {Event::END, 0, 0, 0};
		track_event = nullptr;
	}
	// Set new on/off time
	on_time = event.on_time;
	off_time = event.off_time;
	event_hook();
	// Handle events
	switch(event.type)
	{
		case Event::LOOP_START:
			stack_push({Player_Stack::LOOP, track, position, 0, 0});
			break;
		case Event::LOOP_BREAK:
			// verify
			stack_top(Player_Stack::LOOP);
			// set param to end position to help with conversion
			track_event->param = stack.top().end_position;
			// Break if at the final loop iteration
			if(stack.top().loop_count == 1)
				position = stack_pop(Player_Stack::LOOP).end_position;
			break;
		case Event::LOOP_END:
			stack_top(Player_Stack::LOOP);
			stack.top().end_position = position;
			// Set loop count if zero
			if(stack.top().loop_count == 0)
				stack.top().loop_count = event.param;
			// Jump back
			if(--stack.top().loop_count > 0)
				position = stack.top().position;
			else
				stack_pop(Player_Stack::LOOP);
			break;
		case Event::SEGNO:
			loop_position = position;
			break;
		case Event::JUMP:
			try
			{
				Track& new_track = song->get_track(event.param);
				// Push old position
				stack_push({Player_Stack::JUMP, track, position, 0, 0});
				// Set new position
				track = &new_track;
				position = 0;
			}
			catch(std::exception& ex)
			{
				throw std::invalid_argument("jump destination doesn't exist");
			}
			break;
		case Event::END:
			try
			{
				// Pop old position
				track = stack_top(Player_Stack::JUMP).track;
				position = stack_pop(Player_Stack::JUMP).position;
			}
			catch(std::exception&)
			{
				if(loop_hook())
				{
					position = loop_position;
					loop_count++;
				}
				else
				{
					enabled = false;
					// send a rest event here?
					end_hook();
				}
			}
		default:
			break;
	}
}

bool Basic_Player::is_enabled() const
{
	return enabled;
}

Player::Player(Song& song, Track& track, Player_Flag flag)
	: Basic_Player(song, track), flag(flag)
{
}

void Player::event_hook()
{
}

bool Player::loop_hook()
{
	return 1;
}

void Player::end_hook()
{
}

void Player::write_event()
{
}

void Player::skip_ticks(unsigned int ticks)
{
}

void Player::play_ticks(unsigned int ticks)
{
}

