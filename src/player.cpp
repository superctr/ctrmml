#include <algorithm>
#include <stdexcept>
#include <climits>

#include "player.h"
#include "input.h"
#include "song.h"
#include "track.h"
#include "stringf.h"

//! Creates a Basic_Player.
/*!
 *  \param[in,out] song reference to a Song.
 *  \param[in,out] track reference to a Track.
 */
Basic_Player::Basic_Player(Song& song, Track& track)
	: play_time(0)
	, on_time(0)
	, off_time(0)
	, song(&song)
	, track(&track)
	, enabled(true)
	, position(0)
	, loop_position(-1)
	, loop_reset_position(-1)
	, loop_count(-1)
	, loop_reset_count(0)
	, stack()
	, stack_depth()
	, max_stack_depth(10)
	, loop_begin_depth(0)
{
}

//! Basic_Player destructor
Basic_Player::~Basic_Player()
{
}

//! Play one event.
/*!
 *  This function first reads an event from the track. Then calls the
 *  abstract virtual function event_hook().
 *
 *  After that, loops and jump events are handled to set the next
 *  event position.
 */
void Basic_Player::step_event()
{
	// Set accumulated time
	play_time += on_time + off_time;
	on_time = 0;
	off_time = 0;
	if(position == loop_reset_position)
		loop_reset_count = loop_count;
	try
	{
		// Read the next event
		track_event = &track->get_event(position++);
		// Set the event time
		if(track_event->play_time > play_time)
			track_event->play_time = play_time;
		event = *track_event;
	}
	catch(std::out_of_range&)
	{
		// reached the end
		event = {Event::END, 0, 0, 0, UINT_MAX, reference};
		track_event = nullptr;
	}
	// Set new on/off time
	on_time = event.on_time;
	off_time = event.off_time;
	reference = event.reference;
	// Handle events
	switch(event.type)
	{
		case Event::LOOP_START:
			loop_begin_depth++;
			stack_push({Player_Stack::LOOP, track, position, 0, 0});
			event_hook();
			break;
		case Event::LOOP_BREAK:
			// verify
			stack_top(Player_Stack::LOOP);
			// set param to end position to help with conversion
			track_event->param = stack.top().end_position;
			// Break if at the final loop iteration
			if(stack.top().loop_count == 1)
			{
				// make sure event_hook sees a LOOP_END on the final iteration
				event = track->get_event(stack.top().end_position - 1);
				position = stack_pop(Player_Stack::LOOP).end_position;
			}
			event_hook();
			break;
		case Event::LOOP_END:
			stack_top(Player_Stack::LOOP);
			stack.top().end_position = position;
			// Set loop count if zero
			if(stack.top().loop_count == 0)
			{
				stack.top().loop_count = event.param;
				loop_begin_depth--;
			}
			if(stack.top().loop_count < 0)
				error("Invalid loop count");
			// Jump back
			if(--stack.top().loop_count > 0)
				position = stack.top().position;
			else
				stack_pop(Player_Stack::LOOP);
			event_hook();
			break;
		case Event::SEGNO:
			loop_count = 0;
			loop_reset_count = 0;
			loop_position = position;
			loop_reset_position = position;
			event_hook();
			break;
		case Event::JUMP:
			try
			{
				Track& new_track = song->get_track(event.param);
				// Event hook should be sent before pushing the stack
				event_hook();
				// Push old position
				stack_push({Player_Stack::JUMP, track, position, 0, 0});
				// Set new position
				track = &new_track;
				position = 0;
			}
			catch(std::exception& ex)
			{
				error("jump destination doesn't exist");
			}
			break;
		case Event::END:
			if(stack.size())
			{
				// Pop old position
				track = stack_top(Player_Stack::JUMP).track;
				position = stack_pop(Player_Stack::JUMP).position;
			}
			else
			{
				if(loop_position != -1 && loop_hook())
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
			break;
		default:
			event_hook();
			break;
	}
}

//! Resets the loop count.
void Basic_Player::reset_loop_count()
{
	if(loop_count != -1)
	{
		loop_count = 0;
		loop_reset_count = 0;
		loop_reset_position = (position) ? position-1 : 0;
	}
}

//! Return false when playback is completed.
bool Basic_Player::is_enabled() const
{
	return enabled;
}

//! Check if the playback position is inside a loop.
/*!
 *  The first iteration of each loop is not counted.
 *
 *  \todo More documentation based on unit tests.
 *
 *  \retval true Inside a loop.
 *  \retval false Not inside a loop.
 */
bool Basic_Player::is_inside_loop() const
{
	return stack_depth[Player_Stack::LOOP] && (stack_depth[Player_Stack::LOOP] != loop_begin_depth);
}

//! Check if the playback position is inside a jump (subroutine).
/*!
 *  This also includes drum mode subroutines.
 *
 *  \todo More documentation based on unit tests.
 *
 *  \retval true Inside a jump.
 *  \retval false Not inside a jump.
 */
bool Basic_Player::is_inside_jump() const
{
	return stack_depth[Player_Stack::JUMP];
}

//! Gets the timestamp of the last played event.
/*!
 *  This function can be used to get the track duration if called in a
 *  Track_Validator.
 */
unsigned int Basic_Player::get_play_time() const
{
	return play_time;
}

//! Gets the current loop count.
/*!
 *  \return Current loop count.
 *  \retval -1 If the track has reached the loop point or if it's a
 *             non-looping track.
 */
int Basic_Player::get_loop_count() const
{
	return std::min(loop_reset_count, loop_count);
}

//! Gets the last parsed event.
/*!
 *  \note Includes events added automatically, for example an
 *        Event::REST added for a note event with both \p on_time
 *        and \p off_time parameters.
 */
const Event& Basic_Player::get_event() const
{
	return event;
}

//! Get a list of references to the current track position and calling commands.
std::vector<std::shared_ptr<InputRef>> Basic_Player::get_references()
{
	std::vector<std::shared_ptr<InputRef>> reflist;
	reflist.push_back(track->get_events().at(position-1).reference);

	std::stack<Player_Stack> copy_stack = stack;
	while(!copy_stack.empty())
	{
		auto frame = copy_stack.top();
		if(frame.type != Player_Stack::LOOP)
			reflist.push_back(frame.track->get_events().at(frame.position-1).reference);
		copy_stack.pop();
	}
	return reflist;
}

//! Indicate that track processing is finished.
/*!
 *  This is typically done at the end of the track, and can also be
 *  used by player classes to stop track processing early.
 */
void Basic_Player::disable()
{
	enabled = false;
}

//! Push a stack frame.
/*!
 *  \param frame Stack frame to be pushed.
 *  \exception InputError if the stack size has reached the maximum allowed
 *                     stack depth.
 */
void Basic_Player::stack_push(const Player_Stack& frame)
{
	if(stack.size() >= max_stack_depth)
		error("stack overflow (depth limit reached)");
	stack_depth[frame.type]++;
	stack.push(frame);
}

//! Get the top stack frame, with type checking.
/*!
 *  \param type Expected stack frame type.
 *  \return Top stack frame.
 *  \exception InputError if the top stack frame does not match \p type.
 */
Player_Stack& Basic_Player::stack_top(Player_Stack::Type type)
{
	if(!stack.size())
		stack_underflow(type);
	Player_Stack& frame = stack.top();
	if(frame.type != type)
		stack_underflow(frame.type);
	return frame;
}

//! Pop a stack frame, with type checking.
/*!
 *  \param type Expected stack frame type.
 *  \return The stack frame being popped.
 *  \exception InputError if the top stack frame does not match \p type.
 */
Player_Stack Basic_Player::stack_pop(Player_Stack::Type type)
{
	Player_Stack frame = stack.top();
	stack.pop();
	stack_depth[type]--;
	if(frame.type != type)
		stack_underflow(frame.type);
	return frame;
}

//! Get the type of the top stack frame.
/*!
 *  \return A stack type.
 *  \retval Player_Stack::MAX_STACK_TYPE if the stack is empty.
 */
Player_Stack::Type Basic_Player::get_stack_type()
{
	if(!stack.size())
		return Player_Stack::MAX_STACK_TYPE;
	return stack.top().type;
}

//! Get the stack depth for the specified type.
/*!
 *  \param[in] type A stack type.
 *  \return         The stack depth.
 */
unsigned int Basic_Player::get_stack_depth(Player_Stack::Type type)
{
	return stack_depth[type];
}


//! Throws an InputError.
/*!
 *  \param message Error message.
 *  \exception InputError Always.
 */
void Basic_Player::error(const char* message) const
{
	throw InputError(reference, message);
}


//! Throw an error with appropriate message for a stack underflow.
void Basic_Player::stack_underflow(int type)
{
	if(type == Player_Stack::LOOP)
		error("unterminated '[]' loop");
	else if(type == Player_Stack::JUMP)
		error("unexpected ']' loop end");
	else if(type == Player_Stack::DRUM_MODE)
		error("drum routine contains no note");
	else
		error("unknown stack type (BUG, please report)");
}

//=====================================================================

//! Creates a Player.
/*!
 *  \param[in,out] song reference to a Song.
 *  \param[in,out] track reference to a Track.
 */
Player::Player(Song& song, Track& track)
	: Basic_Player(song, track),
	skip_flag(false),
	note_count(0),
	rest_count(0),
	platform_state(),
	platform_update_mask(0),
	track_state(),
	track_update_mask(0)
{
}

//! Player destructor
Player::~Player()
{
}

//! TODO: move to inline functions / constants members ...
#define FLAG(flag) (track_update_mask & (1<<(flag - Event::CHANNEL_CMD)))
#define FLAG_SET(flag) { track_update_mask |= (1<<(flag - Event::CHANNEL_CMD)); }
#define FLAG_CLR(flag) { track_update_mask &= ~(1<<(flag - Event::CHANNEL_CMD)); }
#define CH_STATE(var) track_state[var - Event::CHANNEL_CMD]
#define VOL_BIT 30 + Event::CHANNEL_CMD
#define BPM_BIT 31 + Event::CHANNEL_CMD

//! Skip a number of ticks.
/*!
 *  \param ticks Number of ticks to skip, counting from the current
 *               position.
 */
void Player::skip_ticks(unsigned int ticks)
{
	if(!is_enabled())
	{
		play_time += ticks;
		return;
	}
	skip_flag = true;
	while(ticks && is_enabled())
	{
		if(on_time)
		{
			if(on_time > ticks)
			{
				on_time -= ticks;
				break;
			}
			play_time += on_time;
			ticks -= on_time;
			on_time = 0;
		}
		else if(off_time)
		{
			if(off_time > ticks)
			{
				off_time -= ticks;
				break;
			}
			play_time += off_time;
			ticks -= off_time;
			off_time = 0;
		}
		while (!on_time && !off_time && is_enabled())
		{
			if(ticks == 0)
				skip_flag = false;
			step_event();
		}
	}
	play_time += ticks;
	skip_flag = false;
}

//! Play a single tick.
/*!
 *  Reads and decrements the on_time and off_time of the previous Event.
 *
 *  When there is remaining on_time, only the on_time is decremented.
 *  After it becomes 0 and if there is remaining off_time, an Event::REST
 *  is automatically created and passed to write_event().
 *
 *  When both counters are 0, the next event is read using step_event().
 */
void Player::play_tick()
{
	if(on_time)
	{
		on_time--;
		play_time++;
		// key off
		if(!on_time && off_time)
		{
			event = {Event::REST, 0, 0, 0, UINT_MAX, reference};
			write_event();
		}
	}
	else if(off_time)
	{
		off_time--;
		play_time++;
	}
	while(is_enabled() && !on_time && !off_time)
	{
		step_event();
	}
}

//! Return the coarse volume flag.
/*!
 *  The coarse volume flag determines the scaling of the event variable
 *  Event::VOL, coarse or fine. The definition of either is
 *  platform-specific, but typically the coarse volume is logarithmic
 *  in 15 steps of 2 dB while fine volume can have more steps and be
 *  either linear or logarithmic.
 *
 *  In MML, the `v` command would emit a coarse volume setting while
 *  `V` is a fine volume setting.
 *
 *  \return false if volume setting is fine.
 *  \return true if volume setting is coarse
 */
bool Player::coarse_volume_flag() const
{
	return FLAG(VOL_BIT);
}

//! Return the tempo BPM flag.
/*!
 *  The tempo BPM flag determines if the event variable Event::TEMPO
 *  contains the direct tempo (platform-specific) or the BPM.
 *
 *  In MML, the `t` command would emit a BPM tempo setting while `T` is
 *  a platform-specific setting (for example the periods of an FM chip's
 *  timer)
 *
 *  \return false if tempo setting is platform-specific.
 *  \return true if tempo setting is in BPM.
 */
bool Player::bpm_flag() const
{
	return FLAG(BPM_BIT);
}

//! Get event variable.
/*!
 *  \param[in] type The event type.
 *
 *  \return The event variable corresponding to that type.
 */
int16_t Player::get_platform_var(int type) const
{
	if(type > 31)
		return 0;
	return platform_state[type];
}

//! Get event variable.
/*!
 *  \param[in] type The event type.
 *
 *  \return The event variable corresponding to that type.
 */
int16_t Player::get_var(Event::Type type) const
{
	if(type < Event::CHANNEL_CMD || type >= Event::CMD_COUNT)
		error("BUG: Unsupported event type");
	return CH_STATE(type);
}

//! Check if a platform variable has been updated.
/*!
 *  \param[in] type Specify the variable to check.
 *
 *  \retval true Variable was updated.
 *  \retval false Variable was not updated.
 */
bool Player::get_platform_flag(unsigned int type) const
{
	if(type > 31)
		return 0; // silently ignored
	return (platform_update_mask >> type) & 1;
}

//! Clear the update flag for a platform variable
/*!
 *  \param[in] type Specify the variable to clear.
 */
void Player::clear_platform_flag(unsigned int type)
{
	if(type > 31)
		return;
	platform_update_mask &= ~(1 << type);
}

//! Check if a channel variable has been updated.
/*!
 *  \param[in] type Specify the variable to check.
 *
 *  \retval true Variable was updated.
 *  \retval false Variable was not updated.
 */
bool Player::get_update_flag(Event::Type type) const
{
	if(type < Event::CHANNEL_CMD || type >= Event::CMD_COUNT)
		error("BUG: Unsupported event type");
	return FLAG(type);
}

//! Clear the update flag for a channel variable
/*!
 *  \param[in] type Specify the variable to clear.
 */
void Player::clear_update_flag(Event::Type type)
{
	if(type < Event::CHANNEL_CMD || type >= Event::CMD_COUNT)
		error("BUG: Unsupported event type");
	FLAG_CLR(type);
}

//! Custom platform event parser.
/*!
 *  The override function should modify the \p platform_state as appropriate
 *  and then return a bitmask indicating the changed state.
 *
 *  \param[in] tag Reference to a Tag containing the platform command,
 *                 as created by Song::register_platform_command().
 *  \param[in,out] platform_state Pointer to the internal platform
 *                 command state array.
 *  \return Override functions should return a bitmask representing the
 *          event state variables that were modified by the functions.
 *          Each bit corresponds to the index in the array of the
 *          variables being modified.
 *
 *  \see Song::register_platform_command()
 */
uint32_t Player::parse_platform_event(const Tag& tag, int16_t* platform_state)
{
	return 0;
}

//! Event handler.
/*!
 *  This is the Player equivalent to event_hook().
 *
 *  The default write_event handler simply increments a note and rest
 *  counter.
 *
 *  \note skip_ticks() can be used to skip events. Because
 *        events are not sent to write_event() during this time, the
 *        derived write_event() should mainly check for Event::NOTE
 *        and Event::REST types (other Event types in special cases)
 *        and get the state of the channel variables using the
 *        \c get_flag and \c get_var (and platform event equivalents)
 *        respectively.
 */
void Player::write_event()
{
	// Handle NOTE and REST events here.
	if(event.type == Event::NOTE)
		note_count++;
	else if(event.type == Event::REST || event.type == Event::END)
		rest_count++;
}

//! Handle an Event::NOTE in drum mode
void Player::handle_drum_mode()
{
	if(get_stack_type() != Player_Stack::DRUM_MODE)
	{
		// First note event enters subroutine
		int offset = CH_STATE(Event::DRUM_MODE);
		try
		{
			Track& new_track = song->get_track(offset + event.param);
			// Push old position
			stack_push({Player_Stack::DRUM_MODE, track, position, (int)on_time, (int)off_time});
			// Set new position
			track = &new_track;
			position = 0;
			// Replace note
			on_time = 0;
			off_time = 0;
			event.type = Event::NOP;
		}
		catch(std::exception& ex)
		{
			error(stringf("drum mode error: track *%d is not defined (base %d, note %d)", event.param + offset, offset, event.param).c_str());
		}
	}
	else
	{
		// Second note exits the subroutine
		on_time = stack_top(Player_Stack::DRUM_MODE).end_position;
		off_time = stack_top(Player_Stack::DRUM_MODE).loop_count;
		position = stack_top(Player_Stack::DRUM_MODE).position;
		track = stack_pop(Player_Stack::DRUM_MODE).track;
	}
}

//! Handles an event inside the player.
/*!
 *  Updates the internal channel variables and update flags. If a
 *  platform event is encountered, calls override functions to parse
 *  and execute those events.
 *
 *  This function should be called inside the player's event_hook().
 */
void Player::handle_event()
{
	switch(event.type)
	{
		case Event::NOTE:
			if(CH_STATE(Event::DRUM_MODE))
				handle_drum_mode();
			break;
		case Event::PLATFORM:
			try
			{
				const Tag& tag = song->get_platform_command(event.param);
				platform_update_mask |= parse_platform_event(tag, platform_state);
			}
			catch (std::out_of_range &)
			{
				error(stringf("Platform command %d is not defined", event.param).c_str());
			}
			break;
		case Event::TRANSPOSE_REL:
			CH_STATE(Event::TRANSPOSE) += event.param;
			FLAG_SET(Event::TRANSPOSE);
			break;
		case Event::VOL:
		case Event::VOL_REL:
			if(event.type != Event::VOL_REL)
				CH_STATE(Event::VOL_FINE) = 0;
			CH_STATE(Event::VOL_FINE) += event.param;
			FLAG_SET(Event::VOL_FINE);
			FLAG_SET(VOL_BIT);
			break;
		case Event::VOL_FINE_REL:
			CH_STATE(Event::VOL_FINE) += event.param;
			FLAG_SET(Event::VOL_FINE);
			FLAG_CLR(VOL_BIT);
			break;
		case Event::TEMPO_BPM:
			CH_STATE(Event::TEMPO) = event.param;
			FLAG_SET(Event::TEMPO);
			FLAG_SET(BPM_BIT);
			break;
		default:
			if(event.type >= Event::CHANNEL_CMD && event.type < Event::CMD_COUNT)
			{
				int type = event.type - Event::CHANNEL_CMD;
				track_state[type] = event.param;
				track_update_mask |= 1<<type;
				if(type == Event::VOL_FINE)
					FLAG_CLR(VOL_BIT);
				if(type == Event::TEMPO)
					FLAG_CLR(BPM_BIT);
			}
			break;
	}
}

void Player::event_hook()
{
	handle_event();
	if(!skip_flag)
		write_event();
}

bool Player::loop_hook()
{
	return 1;
}

void Player::end_hook()
{
	event = {Event::END, 0, 0, 0, UINT_MAX, reference};
	write_event();
}

//=====================================================================

//! Validates a track by playing it.
/*!
 * \exception InputError if any validation errors occur. These should be displayed to the user.
 */
Track_Validator::Track_Validator(Song& song, Track& track)
	: Basic_Player(song, track), segno_time(-1), loop_time(0)
{
	// step all the way to the end
	while(is_enabled())
		step_event();
}

//! Gets the length of the loop section
/*!
 *  The loop section strats from the position of the Event::SEGNO to
 *  the end of the track.
 *
 *  \return If there is no loop, 0 is returned. Otherwise, the loop
 *          section as defined above.
 */
unsigned int Track_Validator::get_loop_length() const
{
	return loop_time;
}

void Track_Validator::event_hook()
{
	// Record timestamp of segno event.
	if(event.type == Event::SEGNO)
		segno_time = get_play_time();
}

bool Track_Validator::loop_hook()
{
	// do not loop
	return 0;
}

void Track_Validator::end_hook()
{
	if(segno_time >= 0)
		loop_time = get_play_time() - segno_time;
}

//=====================================================================

//! Creates a Song_Validator.
/*!
 *  \exception InputError if any validation errors occur.
 *             These should be displayed to the user.
 */
Song_Validator::Song_Validator(Song& song)
{
	for(auto it = song.get_track_map().begin(); it != song.get_track_map().end(); it++)
	{
		track_map.insert(std::make_pair(it->first, Track_Validator(song, it->second)));
	}
}

//! Gets a reference to the Track_Validator map.
/*!
 *  The track validators contain the length and loop lengths of each
 *  Track, which you can get with Player::get_play_time() and
 *  Track_Validator::get_loop_length() respectively.
 */
const std::map<uint16_t,Track_Validator>& Song_Validator::get_track_map() const
{
	return track_map;
}
