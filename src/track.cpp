#include <cstdio>
#include <cstring>
#include <cctype>
#include <climits>
#include <algorithm>
#include <stdexcept>
#include "track.h"
#include "song.h"

//! Constructs a Track.
/*!
 *  \param ppqn Pulses per quarter note, used to set the default duration
 *              and can also be used as a reference for input handlers.
 */
Track::Track(uint16_t ppqn)
	: flag(0)
	, ch(0)
	, last_note_pos(-1)
	, octave(DEFAULT_OCTAVE)
	, measure_len(ppqn * 4)
	, default_duration(measure_len / 4)
	, quantize(DEFAULT_QUANTIZE)
	, quantize_parts(DEFAULT_QUANTIZE_PARTS)
	, early_release(0)
	, shuffle(0)
	, sharp_mask(0)
	, flat_mask(0)
	, echo_delay(0)
	, echo_volume(0)
{
}

//! Appends an Event to the event list.
/*!
 *  \param new_event Reference to the Event to be appended.
 */
void Track::add_event(Event& new_event)
{
	events.push_back(new_event);
}

//! Appends a new Event to the event list.
/*!
 *  \param type     Event type.
 *  \param param    Event parameter
 *  \param on_time  On time (Event::NOTE, Event::REST, Event::TIE only)
 *  \param off_time Off time (Event::NOTE, Event::TIE only)
 */
void Track::add_event(Event::Type type, int16_t param, uint16_t on_time, uint16_t off_time)
{
	Event a = {type, param, on_time, off_time, UINT_MAX, reference};
	events.push_back(a);
}

//! Add an Event::NOTE Event with specified parameter and duration.
/*!
 *  \param note     Note number (0-11). Modified by the octave number
 *                  set by set_octave().
 *  \param duration Note duration. If 0, use the default duration set
 *                  by set_duration().
 */
void Track::add_note(int note, uint16_t duration)
{
	duration = add_shuffle(get_duration(duration));
	shuffle = -shuffle;

	if(!in_drum_mode())
		note += octave * 12;
	push_echo_note(note);

	last_note_pos = events.size();
	add_event(Event::NOTE, note, on_time(duration), off_time(duration));
}

//! Extends the previous note.
/*!
 *  If preceded by a Event::NOTE or Event::REST, simply extend the
 *  on_time or off_time of the previous event.
 *
 *  Otherwise, add a new Event::TIE Event with specified parameter
 *  and duration.
 *
 *  \param duration Extend duration. If 0, use the default
 *                  duration set by set_duration().
 */
int Track::add_tie(uint16_t duration)
{
	duration = add_shuffle(get_duration(duration));
	shuffle = -shuffle;

	if(last_note_pos >= 0)
	{
		Event& last_note = get_event(last_note_pos);
		uint16_t old_duration = last_note.on_time + last_note.off_time;
		uint16_t new_duration = old_duration + duration;
		int last_event_pos = events.size() - 1;

		if(last_note_pos == last_event_pos)
		{
			// last note event is the last event, we can just extend it.
			last_note.on_time = on_time(new_duration);
			last_note.off_time = off_time(new_duration);
			return 0;
		}
		else
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
	}
	else
	{
		add_event(Event::TIE, 0, on_time(duration), off_time(duration));
		return 0;
	}
}

//! Add an Event::REST Event with specified duration.
/*!
 *  \param[in] duration Rest duration. If 0, use the default duration
 *                      set by set_duration().
 */
void Track::add_rest(uint16_t duration)
{
	duration = add_shuffle(get_duration(duration));
	shuffle = -shuffle;

	push_echo_note(0);

	add_event(Event::REST, 0, 0, duration);
}

//! Connect two notes.
/*!
 *  This is done by extending the previous note by setting it to legato,
 *  then adding an Event::SLUR event.
 *
 *  \retval -1 if unable to backtrack.
 *  \retval 0 on success.
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

//! Add a note from the echo buffer.
/*!
 *  Set the parameters with set_echo() before calling this.
 *
 *  Adds a rest if the echo delay parameter is 0 or larger
 *  than the echo buffer.
 */
void Track::add_echo(uint16_t duration)
{
	duration = add_shuffle(get_duration(duration));
	shuffle = -shuffle;

	if(echo_volume)
		add_event(Event::VOL_REL, -echo_volume, 0, 0);

	if(echo_delay == 0 || echo_buffer.size() < echo_delay)
	{
		add_event(Event::REST, 0, 0, duration);
	}
	else
	{
		uint16_t note = echo_buffer[echo_delay - 1];
		if(note == 0)
		{
			add_event(Event::REST, 0, 0, duration);
		}
		else
		{
			last_note_pos = events.size();
			add_event(Event::NOTE, note, on_time(duration), off_time(duration));
		}
	}

	if(echo_volume)
		add_event(Event::VOL_REL, echo_volume, 0, 0);
}

//! Backtrack in order to shorten previous event.
/*!
 *  \note Duration must not exceed the combined length of on_time and
 *        off_time of the previous note, rest or tie. See todo below.
 *  \todo Currently this command cannot remove notes that have already
 *       been added. Maybe in the future...
 *  \param[in] duration Number of ticks to subtract from the previous
 *                      event.
 *  \exception std::length_error if requested duration is greater than the
 *                               length of the previous event.
 *  \exception std::domain_error if unable.
 */
void Track::reverse_rest(uint16_t duration)
{
	// Undo shuffle (I guess this is the best behavior?)
	shuffle = -shuffle;

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
					throw std::length_error("Track::reverse_rest: previous duration not long enough"); // -2
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
				throw std::domain_error("Track::reverse_rest: unable to modify previous duration"); // -1
			default:
				break;
		}
	}
	throw std::domain_error("Track::reverse_rest: there is no previous duration"); // -1
}

//! Sets the reference to use for successive Events.
/*!
 *  This is used to associate source files with events so that
 *  sensible error messages can be generated outside the parser.
 */
void Track::set_reference(const std::shared_ptr<InputRef>& ref)
{
	reference = ref;
}

//! Set the octave, affecting subsequent calls to add_note().
/*!
 *  \param[in] param Octave number (typically 0-8)
 */
void Track::set_octave(int param)
{
	octave = param;
}

//! Modify the octave, affecting subsequent calls to add_note().
/*!
 *  \param[in] param Octave number displacement.
 */
void Track::change_octave(int param)
{
	octave += param;
}

//! Set the default duration.
/*!
 *  \param param Duration value in ticks.
 */
void Track::set_duration(uint16_t param)
{
	default_duration = param;
}

//! Set quantization time.
/*!
 *  Sets the Event::on_time of the next note events to \p param
 *  divided by \p parts. Replaces the early release setting.
 *
 *  \param[in] param Quantization dividend.
 *  \param[in] parts Quantization divider. Default value is 8.
 */
int Track::set_quantize(uint16_t param, uint16_t parts)
{
	if(param > parts || parts == 0)
		return -1;
	if(param == 0)
		param = parts;
	quantize = param;
	quantize_parts = parts;
	early_release = 0;
	return 0;
}

//! Set early release time.
/*!
 *  Sets the Event::on_time of the next note events to \p param.
 *  Replaces the quantization setting.
 *
 *  \param[in] param Early release time in ticks.
 */
void Track::set_early_release(uint16_t param)
{
	quantize = quantize_parts;
	early_release = param;
}

//! Set the drum mode parameters.
/*!
 *  \param[in] param Drum mode note offset.
 *               If 0, drum mode is disabled.
 */
void Track::set_drum_mode(uint16_t param)
{
	if(!param)
		flag &= ~(0x01);
	else
		flag |= 0x01;
	add_event(Event::DRUM_MODE, param);
}

//! Set the echo parameters.
/*!
 *  \param[in] delay Note offset in the buffer
 *  \param[in] volume Volume reduction.
 */
void Track::set_echo(uint16_t delay, int16_t volume)
{
	if(delay > ECHO_BUFFER_SIZE)
		delay = ECHO_BUFFER_SIZE;

	echo_delay = delay;
	echo_volume = volume;
}

//! Get the events list.
std::vector<Event>& Track::get_events()
{
	return events;
}

//! Get the Event at the specified position.
/*!
 * \param position Position of event in the track.
 * \exception std::out_of_range if position exceeds event count.
 */
Event& Track::get_event(unsigned long position)
{
	return events.at(position);
}

//! Get the total number of events in the track.
unsigned long Track::get_event_count() const
{
	return events.size();
}

//! Return true if track is enabled.
/*!
 *  This returns bit 7 of the track flag. It is to be used as a
 *  convenience by players to signify that a track has been marked as
 *  used by the input routine.
 */
bool Track::is_enabled() const
{
	return (flag & 0x80);
}

//! Returns the drum mode status.
/*!
 *  \retval true  Drum mode is enabled.
 *  \retval false Drum mode is disabled.
 */
bool Track::in_drum_mode() const
{
	return (flag & 0x01);
}

//! Get the default duration.
/*!
 *  \param[in] duration If 0, get the default duration as set by
 *                  set_duration(). Otherwise use the param value.
 */
uint16_t Track::get_duration(uint16_t duration) const
{
	if(!duration)
		return default_duration;
	else
		return duration;
}

//! Set the length of a whole note.
/*!
 *  The measure length is not internally used by the track writer, but it
 *  is stored with the track state as a convenience to MIDI or MML parsers.
 */
void Track::set_measure_len(uint16_t param)
{
	measure_len = param;
}

//! Get the length of a whole note.
/*!
 *  This is actually not used internally by the track methods, rather it is
 *  provided as convenience to MIDI or MML parsers.
 */
uint16_t Track::get_measure_len() const
{
	return measure_len;
}

//! Set the shuffle modifier.
void Track::set_shuffle(int16_t param)
{
	shuffle = param;
}

//! Get the current shuffle modifier.
int16_t Track::get_shuffle() const
{
	return shuffle;
}

//! Enables the track.
/*!
 *  This sets bit 7 of the track flag. It is to be used as a convenience
 *  by input routines (by using is_enabled()) to signify that a track is
 *  to be included in the final output.
 */
void Track::enable()
{
	flag |= 0x80;
}

//! Set the key signature
/*!
 *  Takes a string input and sets the key signature (only used with
 *  get_key_signature()).
 *
 *  The string can be formatted in one of two forms:
 *  - using the name of a scale. Lower case indicates a minor scale,
 *    upper case indicates a major scale. Using this method will reset
 *    the key signature.
 *  - adding a sharp ("+"), flat ("-"), or natural ("=") sign followed
 *    by a list of notes that are to be modified.
 *
 *  \except std::invalid_argument Scale or modifier string is invalid.
 */
void Track::set_key_signature(const char* key)
{
	static const struct {
		uint8_t sharp;
		uint8_t flat;
		const char* major;
		const char* minor;
	} scales[15] =
	{ //  +hgfedcba   -hgfedcba  maj   min
	   { 0b00000000, 0b00000000, "C",  "a"  },
	   { 0b00100000, 0b00000000, "G",  "e"  },
	   { 0b00100100, 0b00000000, "D",  "b"  },
	   { 0b01100100, 0b00000000, "A",  "f+" },
	   { 0b01101100, 0b00000000, "E",  "c+" },
	   { 0b01101101, 0b00000000, "B",  "g+" },
	   { 0b01111101, 0b00000000, "F+", "d+" },
	   { 0b01111111, 0b00000000, "C+", "a+" },
	   { 0b00000000, 0b00000010, "F",  "d"  },
	   { 0b00000000, 0b00010010, "B-", "g"  },
	   { 0b00000000, 0b00010011, "E-", "c"  },
	   { 0b00000000, 0b00011011, "A-", "f"  },
	   { 0b00000000, 0b01011011, "D-", "b-" },
	   { 0b00000000, 0b01011111, "G-", "e-" },
	   { 0b00000000, 0b01111111, "C-", "a-" }
	};

	char k = *key;
	int8_t modifier = 0;
	if(std::isalpha(k))
	{
		for(int i=0; i<15; i++)
		{
			if(!std::strcmp(scales[i].major, key) || !std::strcmp(scales[i].minor, key))
			{
				sharp_mask = scales[i].sharp;
				flat_mask = scales[i].flat;
				return;
			}
		}
		throw std::invalid_argument("Track::set_key_signature : unknown key");
	}
	else
	{
		do
		{
			if(k == '+')
				modifier = 1;
			else if(k == '-')
				modifier = -1;
			else if(k == '=')
				modifier = 0;
			else if(isalpha(k))
				modify_key_signature(k, modifier);
			else
				throw std::invalid_argument("Track::set_key_signature : unknown modifier");
			k = *key++;
		}
		while(k);
	}
}

//! Modify the key signature
/*!
 *  This will directly modify the key signature modifier for the
 *  given note. The \p modifier must be either -1, 0 or 1.
 *
 *  \except std::invalid_argument Note or modifier parameter is
 *          invalid.
 */
void Track::modify_key_signature(char note, int8_t modifier)
{
	note = std::tolower(note) - 'a';
	if(note > 7)
		throw std::invalid_argument("Track::modify_key_signature - invalid note");

	sharp_mask &= ~(1 << note);
	flat_mask &= ~(1 << note);
	if(modifier == 1)
		sharp_mask |= (1 << note);
	else if(modifier == -1)
		flat_mask |= (1 << note);
	else if(modifier != 0)
		throw std::invalid_argument("Track::modify_key_signature - invalid modifier");
}

//! Get the key signature modifier for the note.
/*!
 *  \except std::invalid_argument Note is invalid.
 */
int8_t Track::get_key_signature(char note)
{
	note = std::tolower(note) - 'a';
	if(note > 7)
		throw std::invalid_argument("Track::get_key_signature - invalid note");

	if(sharp_mask & (1 << note))
		return 1;
	else if(flat_mask & (1 << note))
		return -1;
	return 0;
}

//! Get Event::on_time after quantization.
uint16_t Track::on_time(uint16_t duration) const
{
	if(early_release)
	{
		if(early_release >= duration)
			return 1;
		else
			return duration - early_release;
	}
	return (duration * quantize) / quantize_parts;
}

//! Get Event::off_time after quantization.
uint16_t Track::off_time(uint16_t duration) const
{
	return duration - on_time(duration);
}

//! Add shuffle, with underflow clamping
uint16_t Track::add_shuffle(uint16_t duration) const
{
	if((duration + shuffle) < 0)
		return 0;
	else
		return duration + shuffle;
}

//! Push notes to the echo buffer
void Track::push_echo_note(uint16_t note)
{
	echo_buffer.push_front(note);
	if(echo_buffer.size() > ECHO_BUFFER_SIZE)
		echo_buffer.pop_back();
}
