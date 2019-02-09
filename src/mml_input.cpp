#include "mml_input.h"
#include <iostream>
#include <cctype>

int MML_Input::read_duration()
{
}

int MML_Input::read_parameter(int default_parameter)
{
}

int MML_Input::expect_parameter()
{
}

int MML_Input::expect_signed()
{
}

int MML_Input::read_note(int c)
{
}


void MML_Input::mml_slur()
{
}

void MML_Input::mml_reverse_rest(int duration)
{
}

void MML_Input::mml_grace()
{
}

void MML_Input::atom_relative(Atom_Command type, Atom_Command subtype)
{
}

void MML_Input::parse_mml_track(int conditional_block)
{
	std::cout << "parse_mml_track() "<<track_id<<"\n";
}

void MML_Input::parse_mml()
{
	unsigned long col = column; // to be replaced with tell()
	for(unsigned int i = 0; i < track_list.size(); i++)
	{
		column = col; // to be replaced with seek()
		track_id = track_list[i];
		track_offset = i;
		track = &song->get_track_map()[track_id];
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
			// Perhaps this should be replaced with a string
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

