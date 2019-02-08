#include "mml_input.h"

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
}

void MML_Input::parse_mml()
{
}

void MML_Input::parse_tag()
{
}

int MML_Input::convert_track_id(char c)
{
}

MML_Input::MML_Input(Song* song)
	: Line_Input(song), track_list(0)
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
	return 0;
}

