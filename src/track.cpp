#include <stdio.h>
#include <algorithm>
#include <stdexcept>
#include "track.h"
#include "song.h"

Track::Track()
	: flag(0),
	ch(0),
	last_note_pos(-1),
	octave(DEFAULT_OCTAVE),
	measure_len(DEFAULT_MEASURE_LEN),
	default_duration(measure_len/4),
	quantize(DEFAULT_QUANTIZE),
	quantize_parts(DEFAULT_QUANTIZE_PARTS)
{
}

Track::~Track()
{
}

//! Enables the track.
void Track::enable()
{
	flag |= 0x80;
}

//! Return true if track is enabled.
bool Track::is_enabled()
{
	return (flag & 0x80);
}

//! Get Event::on_time after quantization.
uint16_t Track::on_time(uint16_t duration)
{
	return (duration * quantize) / quantize_parts;
}
//! Get Event::off_time after quantization.
uint16_t Track::off_time(uint16_t duration)
{
	return duration - on_time(duration);
}

//! Sets the reference to use for successive Events.
/*!
 * This is used to associate source files with events so that
 * sensible error messages can be generated outside the parser.
 */
void Track::set_reference(const std::shared_ptr<InputRef>& ref)
{
	reference = ref;
}

//! Appends an Event to the event list.
void Track::add_event(Event& new_event)
{
	events.push_back(new_event);
}

//! Appends a new Event to the event list.
void Track::add_event(Event::Type type, int16_t param, uint16_t on_time, uint16_t off_time)
{
	Event a = {type, param, on_time, off_time, reference};
	events.push_back(a);
}

//! Add an Event::NOTE Event with specified parameter and duration.
/*!
 * \param note Note number (0-11). Modified by the octave number set by set_octave().
 * \param duration Note duration. If 0, use the default duration set by set_duration().
 */
void Track::add_note(int note, uint16_t duration)
{
	duration = get_duration(duration);
	if(~flag & 0x01)
		note += octave * 12;
	last_note_pos = events.size();
	add_event(Event::NOTE, note, on_time(duration), off_time(duration));
}

//! Extends the previous note.
/*!
 * If this is not possible, add an Event::TIE Event with specified parameter and duration.
 * \param duration Extend duration. If 0, use the default duration set by set_duration().
 */
int Track::add_tie(uint16_t duration)
{
	duration = get_duration(duration);
	Event& last_note = get_event(last_note_pos);
	uint16_t old_duration = last_note.on_time + last_note.off_time;
	uint16_t new_duration = old_duration + duration;

	if(last_note_pos == events.size() - 1)
	{
		// last note event is the last event, we can just extend it.
		last_note.on_time = on_time(new_duration);
		last_note.off_time = off_time(new_duration);
		return 0;
	}
	else if(last_note_pos >= 0)
	{
		// if another event has been inserted, it's important to keep
		// the timing of that event, so we change the on/off time of the
		// last note, then insert a new event for the extended duration.
		// whether it's a tie or rest depends on the quantization.
		if(on_time(new_duration) > old_duration)
		{
			last_note_pos = events.size();
			last_note.on_time = old_duration;
			last_note.off_time = 0;
			add_event(Event::TIE, 0, on_time(new_duration)-old_duration, off_time(new_duration));
		}
		else
		{
			last_note_pos = -1; // only works once...
			last_note.on_time = on_time(new_duration);
			last_note.off_time = old_duration - on_time(new_duration);
			add_event(Event::REST, 0, 0, duration);
		}
		return 0;
	}
	else
	{
		return -1;
	}
}

//! Add an Event::REST Event with specified duration.
/*!
 * \param duration Rest duration. If 0, use the default duration set by set_duration().
 */
void Track::add_rest(uint16_t duration)
{
	duration = get_duration(duration);
	add_event(Event::REST, 0, 0, duration);
}

//! Connect two notes.
/*!
 * This is done by extending the previous note by setting it to legato,
 * then adding an Event::SLUR event.
 *
 * \retval -1 if unable to backtrack.
 * \retval 0 on success.
 */
int Track::add_slur()
{
	add_event(Event::SLUR);

	// backtrack to disable the articulation of the previous note.
	std::vector<Event>::reverse_iterator it;
	for(it=events.rbegin(); it != events.rend(); ++it)
	{
		switch(it->type)
		{
			case Event::NOTE:
			case Event::TIE:
				it->on_time += it->off_time;
				it->off_time = 0;
				return 0;
			// Don't bother reading past loop points as effects are
			// unpredictable.
			case Event::REST:
			case Event::SEGNO:
			case Event::LOOP_END:
				return -1;
			default:
				break;
		}
	}
	return -1;
}

//! Backtrack in order to shorten previous event.
/*!
 * Maybe in the future, we might be able to remove or dummy out notes in
 * order to gain extra time.
 *
 * \exception std::length_error if requested duration is greater than the
 * length of the previous event.
 * \exception std::domain_error if unable.
 */
void Track::reverse_rest(uint16_t duration)
{
	// backtrack to disable the articulation of the previous note.
	std::vector<Event>::reverse_iterator it;
	for(it=events.rbegin(); it != events.rend(); ++it)
	{
		switch(it->type)
		{
			case Event::NOTE:
			case Event::TIE:
			case Event::REST:
				// If the off_time is larger than the shorten duration,
				// we will just use that. Otherwise subtract from both
				// off_time and on_time.
				if(duration > it->off_time)
				{
					duration -= it->off_time;
					if(duration < it->on_time)
					{
						it->off_time = 0;
						it->on_time -= duration;
						return;
					}
					throw std::length_error("previous duration not long enough"); // -2
				}
				else
				{
					it->off_time -= duration;
					return;
				}
			// Don't bother reading past loop points as effects are
			// unpredictable.
			case Event::SEGNO:
			case Event::LOOP_END:
				throw std::domain_error("unable to modify previous duration"); // -1
			default:
				break;
		}
	}
	throw std::domain_error("there is no previous duration"); // -1
}

//! Finalize and validate the track.
int Track::finalize(class Song& song, uint16_t track_id)
{
	printf("Not implemented yet.\n");
	return -1;
}

//! Set the octave, which affects add_note().
void Track::set_octave(int param)
{
	octave = param;
}

//! Adds the param value to the octave.
void Track::change_octave(int param)
{
	octave += param;
}

//! Set quantization time.
/*!
 * Sets the Event::on_time of the next note events to param/parts.
 */
int Track::set_quantize(uint16_t param, uint16_t parts)
{
	if(param > parts || parts == 0)
		return -1;
	if(param == 0)
		param = -1;
	quantize = param;
	quantize_parts = parts;
	return 0;
}

//! Set the drum mode status. If param is 0, disables it.
void Track::set_drum_mode(uint16_t param)
{
	if(!param)
		flag &= ~(0x01);
	else
		flag |= 0x01;
	add_event(Event::DRUM_MODE, param);
}

//! Returns the drum mode status.
bool Track::in_drum_mode()
{
	return (flag & 0x01);
}

//! Set the default duration used by add_note() et al.
void Track::set_duration(uint16_t param)
{
	default_duration = param;
}

//! If the duration param is 0, get the default duration. Otherwise use the param value.
uint16_t Track::get_duration(uint16_t duration)
{
	if(!duration)
		return default_duration;
	else
		return duration;
}

//! Get the measure length. This is actually the length of the whole note.
/*!
 * This is actually not used internally by the track methods, rather it is
 * provided as convenience to MIDI or MML parsers.
 */
uint16_t Track::get_measure_len()
{
	return measure_len;
}

//! Get the events list.
std::vector<Event>& Track::get_events()
{
	return events;
}

//! Get the specified event.
/*!
 * \exception std::out_of_range if position exceeds event count.
 */
Event& Track::get_event(unsigned long position)
{
	return events.at(position);
}

//! Get event count.
unsigned long Track::get_event_count()
{
	return events.size();
}

