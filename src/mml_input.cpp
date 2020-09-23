#include <iostream>
#include <cctype>
#include <stdexcept>
#include "mml_input.h"
#include "song.h"
#include "track.h"
#include "stringf.h"

unsigned int MML_Input::read_duration()
{
	int duration = 0, dot;
	try
	{
		int c = get();
		if(c == ':')
		{
			duration = get_num();
		}
		else
		{
			unget(c);
			int div = get_num();
			if(div < 1)
				parse_error("illegal duration");
			duration = track->get_measure_len() / div;
		}
		if(duration < 0)
			parse_error("illegal duration");
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
	static const int note_values[8] = {9,11,0,2,4,5,7,11}; // abcdefgh
	int8_t val = c - 'a';
	int8_t sig = 0;
	// linear if in drum mode
	if(!track->in_drum_mode())
	{
		val = note_values[val & 7];
		sig = track->get_key_signature(c);
	}
	// sharps/flats
	c = get();
	if(c == '+')
		sig = 1;
	else if(c == '-')
		sig = -1;
	else if(c == '=')
		sig = 0;
	else
		unget(c);
	return val + sig;
}

//! Platform-exclusive messages ('<key> <value> ...')
void MML_Input::platform_exclusive()
{
	std::string str = "";
	char c = get();
	while(c && c != '\'')
	{
		str.push_back(c);
		c = get();
	}
	if(c != '\'')
		parse_error("unterminated platform-exclusive message");
	int16_t param = get_song().register_platform_command(-1, str);
	track->add_event(Event::PLATFORM, param);
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
	catch (std::length_error&)
	{
		parse_error("previous note is not long enough");
	}
}

void MML_Input::mml_grace()
{
	int c = read_note(get_token());
	int duration = read_duration();
	mml_reverse_rest(duration);
	track->add_note(c, duration);
}

void MML_Input::mml_transpose()
{
	int c = get_token();
	if(c == '_')
	{
		track->add_event(Event::TRANSPOSE_REL, expect_signed());
	}
	else if(c == '{')
	{
		try
		{
			// Read key signature
			std::string str = "";
			do
			{
				c = get();
				if(c && c != '}' && !std::isspace(c))
					str.push_back(c);
			}
			while(c && c != '}');
			track->set_key_signature(str.c_str());
		}
		catch(std::invalid_argument&)
		{
			parse_error("invalid key signature");
		}
	}
	else
	{
		unget();
		track->add_event(Event::TRANSPOSE, expect_signed());
	}
}

//! combination command that allows for two Event::Type depending on
//! if a sign prefix is found.
void MML_Input::event_relative(Event::Type type, Event::Type subtype)
{
	int c = get_token();
	if(c == '+' || c == '-')
		type = subtype;
	if(c != '+')
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
		track->set_octave(expect_parameter() - 1);
	else if(c == '<')
		track->change_octave(-1);
	else if(c == '>')
		track->change_octave(1);
	else if(c == 'l')
		track->set_duration(read_duration());
	else if(c == 'Q')
		track->set_quantize(expect_parameter());
	else if(c == 'q')
		track->set_early_release(expect_parameter());
	else if(c == 'R')
		mml_reverse_rest(read_duration());
	else if(c == '~')
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
	else if(c == '\'')
		platform_exclusive();
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
	else if(c == '_')
		mml_transpose();
	else if(c == 'k') // TODO: ktype command to set compile-time transpose?
		mml_transpose();
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
	else if(c == 'G')
		track->add_event(Event::PORTAMENTO, expect_parameter());
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

void MML_Input::conditional_block_begin()
{
	int c;
	int offset = track_offset;
	conditional_block = 1;
	while(offset)
	{
		do c = get_token();
		while(c != 0 && c != '/' && c != ';');
		if(c != '/')
			parse_error("unterminated conditonal block");
		c = 0;
		offset--;
	}
}

void MML_Input::conditional_block_end(int c)
{
	while(c != 0 && c != '}' && c != ';')
		c = get_token();
	if(c != '}')
		parse_error("unterminated conditional block");
	conditional_block = 0;
}

void MML_Input::parse_mml_track()
{
	int c;
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
			conditional_block_end(c);
		else if(c == '{' && !conditional_block)
			conditional_block_begin();
		else if(c == '%')
			track->add_event(Event::PLATFORM, expect_parameter());
		else if(c == 0)
			return;
		else
		{
			unget(c);
			// Set reference
			track->set_reference(get_reference());
			// Here i can read a list of command handlers and call them
			// until one returns false
			if(mml_basic() == false)
				continue;
			else if(mml_control() == false)
				continue;
			else if(mml_envelope() == false)
				continue;
			parse_error("unknown MML command");
		}
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
		track = &get_song().make_track(track_id);
		conditional_block = false;
		parse_mml_track();
		if(conditional_block)
			parse_error("unterminated conditional block");
	}
}

void MML_Input::parse_tag()
{
	//std::cout << "parse_tag() "<<tag_key<<":" << get_line() << "\n";
	if(tag_key[0] == '#')
	{
		// Special cases for "include" etc commands go here
		// #platform = set output format
		// #format = set MML format. Handle internally
		if(iequal(tag_key, "#platform"))
			get_song().set_platform(get_line());
		else
			get_song().set_tag(tag_key, get_line());
		last_cmd = nullptr; // Only read a single line
	}
	else
	{
		get_song().add_tag_list(tag_key, get_line());
	}
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
	: Line_Input(song),
	track_id(0),
	track_offset(0),
	track_list(0),
	last_cmd(nullptr),
	conditional_block(0)
{
	// Perhaps the initial state of mml_input should be track A.
	// Or maybe it can be initialized by a previous MML_Input during
	// an "include" command.
}

MML_Input::~MML_Input()
{
}

//! Get a list of tracks that were affected by the previous read_line()
MML_Input::Track_Position_Map MML_Input::get_track_map()
{
	Track_Position_Map out = {};
	if(last_cmd == &MML_Input::parse_mml)
	{
		for(auto && i : track_list)
			out[i] = get_song().get_track(i).get_event_count();
	}
	return out;
}

void MML_Input::parse_line()
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
				tag_key.push_back(std::tolower(c));
				c = get();
			}
			while (c != 0 && !std::isspace(c));
			last_cmd = &MML_Input::parse_tag;
		}
		else if(c == ';')
		{
			// Comment
			return;
		}
		else if(!std::isblank(c))
		{
			// Not at the end of the line
			if(c != 0)
			{
				parse_error("Expected track or tag identifier");
			}
			// At the end of the line, we can stop parsing
			return;
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
			return;
		if(last_cmd != nullptr)
		{
			(this->*last_cmd)();
			return;
		}
	}
	return;
}

