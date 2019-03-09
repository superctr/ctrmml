#include <iostream>
#include <cctype>
#include <stdexcept>
#include "mml_input.h"
#include "song.h"
#include "track.h"

unsigned int MML_Input::read_duration()
{
	int duration = 0, dot;
	try
	{
		duration = track->get_measure_len() / get_num();
		if(duration < 0)
			throw std::out_of_range("MMLInput::read_duration()");
	}
	catch(std::invalid_argument&)
	{
		duration = track->get_duration();
	}
	dot = duration>>1;
	while(1)
	{
		if(get() == '.')
		{
			duration += dot;
			dot >>= 1;
		}
		else
		{
			unget();
			break;
		}
	}
	return (unsigned)duration;
}

int MML_Input::read_parameter(int default_parameter)
{
	try
	{
		return get_num();
	}
	catch(std::invalid_argument&)
	{
		return default_parameter;
	}
}

int MML_Input::expect_parameter()
{
	try
	{
		return get_num();
	}
	catch(std::invalid_argument&)
	{
		parse_error("missing parameter");
		return 0;
	}
}

int MML_Input::expect_signed()
{
	// is this function necessary anymore?
	return expect_parameter();
}

int MML_Input::read_note(int c)
{
	// would be nice to support key signatures...
	static const int note_values[8] = {8,11,0,2,4,5,7,11}; // abcdefgh
	int val = c - 'a';
	// linear if in drum mode
	if(!track->in_drum_mode())
		val = note_values[val];
	// sharps/flats
	c = get();
	if(c == '+')
		val++;
	else if(c == '-')
		val--;
	else
		unget(c);
	return val;
}

void MML_Input::mml_slur()
{
	if(track->add_slur())
		parse_warning("slur may not affect articulation of previous note");
}

void MML_Input::mml_reverse_rest(int duration)
{
	try
	{
		track->reverse_rest(duration);
	}
	catch (std::domain_error&)
	{
		parse_error("unable to backtrack");
	}
}

void MML_Input::mml_grace()
{
	int c = read_note(get_token());
	int duration = read_duration();
	mml_reverse_rest(duration);
	track->add_note(c, duration);
}

//! combination command that allows for two Event::Type depending on
//! if a sign prefix is found.
void MML_Input::event_relative(Event::Type type, Event::Type subtype)
{
	int c = get_token();
	if(c == '+' || c == '-')
		type = subtype;
	else
		unget();
	if(type == Event::INVALID)
		parse_error("parameter must be relative (+ or - prefix)");
	track->add_event(type, expect_parameter());
}

// Basic MML command parser. Commands defined here are the ones
// unlikely to change in different MML dialects.
bool MML_Input::mml_basic()
{
	int c = get_token();
	if(c >= 'a' && c <= 'h')
	{
		c = read_note(c);
		track->add_note(c, read_duration());
	}
	else if(c == 'r')
		track->add_rest(read_duration());
	else if(c == '^')
		track->add_tie(read_duration());
	else if(c == '&')
		mml_slur();
	else if(c == 'o')
		track->set_octave(expect_parameter());
	else if(c == '<')
		track->change_octave(-1);
	else if(c == '>')
		track->change_octave(1);
	else if(c == 'l')
		track->set_duration(read_duration());
	else if(c == 'q')
		track->set_quantize(expect_parameter());
	else if(c == 'R')
		mml_reverse_rest(read_duration());
	else if(c == 'G')
		mml_grace();
	else
	{
		unget(c);
		return true;
	}
	return false;
}

bool MML_Input::mml_control()
{
	int c = get_token();
	if(c == '[')
		track->add_event(Event::LOOP_START);
	else if(c == '/')
		track->add_event(Event::LOOP_BREAK);
	else if(c == ']')
		track->add_event(Event::LOOP_END, read_parameter(2));
	else if(c == 'L')
		track->add_event(Event::SEGNO);
	else if(c == '*')
		track->add_event(Event::JUMP, expect_parameter());
	else
	{
		unget(c);
		return true;
	}
	return false;
}

// Dialect specific MML commands
bool MML_Input::mml_envelope()
{
	int c = get_token();
	if(c == '@')
		track->add_event(Event::INS, expect_parameter());
	else if(c == 'k')
		track->add_event(Event::TRANSPOSE, expect_signed());
	else if(c == 'K')
		track->add_event(Event::DETUNE, expect_signed());
	else if(c == 'v')
		track->add_event(Event::VOL, expect_parameter());
	else if(c == '(')
		track->add_event(Event::VOL_REL, -read_parameter(1));
	else if(c == ')')
		track->add_event(Event::VOL_REL, read_parameter(1));
	else if(c == 'V')
		event_relative(Event::VOL_FINE, Event::VOL_FINE_REL);
	else if(c == 'p')
		track->add_event(Event::PAN, expect_signed());
	else if(c == 'E')
		track->add_event(Event::VOL_ENVELOPE, expect_parameter());
	else if(c == 'M')
		track->add_event(Event::PITCH_ENVELOPE, expect_parameter());
	else if(c == 'P')
		track->add_event(Event::PAN_ENVELOPE, expect_parameter());
	else if(c == 'D')
		track->set_drum_mode(expect_parameter());
	else if(c == 't')
		track->add_event(Event::TEMPO_BPM, expect_parameter());
	else if(c == 'T')
		track->add_event(Event::TEMPO, expect_parameter());
	else
	{
		unget(c);
		return true;
	}
	return false;
}

void MML_Input::parse_mml_track(int conditional_block)
{
	int c;
	if(conditional_block)
	{
		int offset = track_offset;
		while(offset)
		{
			do c = get_token();
			while(c != 0 && c != '}' && c != ';');
			if(c != '/')
				throw std::invalid_argument("unterminated conditonal block");
			c = 0;
			offset--;
		}
	}
	while(1)
	{
		// could this be rewritten using a command map?
		// it would be nice to at least split the MML commands
		// into a few functions...
		c = get_token();
		if(c == '|') // Separator
			continue;
		else if(c == ';') // Comment
			break;
		else if((c == '/' || c == '}') && conditional_block)
			break;
		else if(c == '{' && !conditional_block)
			parse_mml_track(1);
		else if(c == '%')
			track->add_event(Event::CHANNEL_MODE, expect_parameter());
		else if(c == 0)
			return;
		else
		{
			unget(c);
			// Here i can read a list of command handlers and call them
			// until one returns false
			if(mml_basic() == false)
				continue;
			else if(mml_control() == false)
				continue;
			else if(mml_envelope() == false)
				continue;
			throw std::invalid_argument("unknown MML command...");
		}
	}
	if(conditional_block)
	{
		while(c != 0 && c != '}' && c != ';')
			c = get_token();
		if(c != '}')
			throw std::invalid_argument("unexpected conditional block");
	}
}

void MML_Input::parse_mml()
{
	unsigned long col = tell();
	for(unsigned int i = 0; i < track_list.size(); i++)
	{
		seek(col);
		track_id = track_list[i];
		track_offset = i;
		track = &get_song().get_track_map()[track_id];
		parse_mml_track(0);
	}
}

void MML_Input::parse_tag()
{
	std::cout << "parse_tag() "<<tag_key<<"\n";
}

// may throw std::invalid_argument
int MML_Input::get_track_id()
{
	int c = get();
	if(c >= 'A' && c <= 'Z')
		return c - 'A';
	else if(std::isdigit(c))
		return c - '0' + 26;
	else if(c == '*')
		return get_num();
	// No match
	unget(c);
	return -1;
}

MML_Input::MML_Input(Song* song)
	: Line_Input(song), track_list(0), last_cmd(nullptr)
{
	// Perhaps the initial state of mml_input should be track A.
	// Or maybe it can be initialized by a previous MML_Input during
	// an "include" command.
}

MML_Input::~MML_Input()
{
}

bool MML_Input::parse_line()
{
	int c = get_track_id();
	if(c != -1)
	{
		// Read track list
		track_list.clear();
		do
		{
			track_list.push_back(c);
			c = get_track_id();
		}
		while (c != -1);
		last_cmd = &MML_Input::parse_mml;
	}
	else
	{
		c = get();
		if(c == '#' || c == '@')
		{
			// This could maybe be more efficient
			tag_key.clear();
			do
			{
				tag_key.push_back(c);
				c = get();
			}
			while (c != 0 && !std::isspace(c));
			last_cmd = &MML_Input::parse_tag;
		}
		else if(c == ';')
		{
			// Comment
			return false;
		}
		else
		{
			// I guess an exception could be thrown here instead
			return true;
		}
		unget(c);
	}

	c = get();
	if(std::isblank(c))
	{
		// Skip non blank characters
		c = get_token();
		unget(c);
		if(c == 0)
			return false;
		if(last_cmd != nullptr)
		{
			(this->*last_cmd)();
			return false;
		}
	}
	return false;
}

