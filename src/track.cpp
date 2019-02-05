#include <stdio.h>
#include <algorithm>
#include "track.h"

Track::Track()
	: last_note_pos(-1),
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

void Track::enable()
{
	flag |= 0x80;
}

// get quantized on time / off time respectively
uint16_t Track::on_time(uint16_t duration)
{
	return (duration * quantize) / quantize_parts;
}
uint16_t Track::off_time(uint16_t duration)
{
	return duration - on_time(duration);
}

void Track::add_atom(Atom *new_atom)
{
	atoms.push_back(*new_atom);
}

void Track::add_atom(Atom_Command type, int16_t param, uint16_t on_time, uint16_t off_time)
{
	Atom a = {type, param, on_time, off_time};
	atoms.push_back(a);
}

void Track::add_note(int note, uint16_t duration)
{
	duration = get_duration(duration);
	if(~flag & 0x01)
		note += octave * 12;
	last_note_pos = atoms.size();
	add_atom(ATOM_NOTE, note, on_time(duration), off_time(duration));
}

int Track::add_tie(uint16_t duration)
{
	duration = get_duration(duration);
	Atom *last_note = get_atom(last_note_pos);
	uint16_t old_duration = last_note->on_time + last_note->off_time;
	uint16_t new_duration = old_duration + duration;

	if(last_note_pos == atoms.size() - 1)
	{
		// last note event is the last atom, we can just extend it.
		last_note->on_time = on_time(new_duration);
		last_note->off_time = off_time(new_duration);
		return 0;
	}
	else if(last_note_pos >= 0)
	{
		// if another atom has been inserted, it's important to keep
		// the timing of that atom, so we change the on/off time of the
		// last note, then insert a new atom for the extended duration.
		// whether it's a tie or rest depends on the quantization.
		if(on_time(new_duration) > old_duration)
		{
			last_note_pos = atoms.size();
			last_note->on_time = old_duration;
			last_note->off_time = 0;
			add_atom(ATOM_TIE, 0, on_time(new_duration)-old_duration, off_time(new_duration));
		}
		else
		{
			last_note_pos = -1; // only works once...
			last_note->on_time = on_time(new_duration);
			last_note->off_time = old_duration - on_time(new_duration);
			add_atom(ATOM_REST, 0, 0, duration);
		}
		return 0;
	}
	else
	{
		return -1;
	}
}

void Track::add_rest(uint16_t duration)
{
	duration = get_duration(duration);
	add_atom(ATOM_REST, 0, 0, duration);
}

// Return -1 if unable to backtrack.
int Track::add_slur()
{
	add_atom(ATOM_CMD_SLUR);

	// backtrack to disable the articulation of the previous note.
	std::vector<Atom>::reverse_iterator it;
	for(it=atoms.rbegin(); it != atoms.rend(); ++it)
	{
		switch(it->type)
		{
			case ATOM_NOTE:
			case ATOM_TIE:
				it->on_time += it->off_time;
				it->off_time = 0;
				return 0;
			// Don't bother reading past loop points as effects are
			// unpredictable.
			case ATOM_REST:
			case ATOM_CMD_SEGNO:
			case ATOM_CMD_LOOP_END:
				return -1;
			default:
				break;
		}
	}
	return -1;
}

// Backtrack in order to shorten previous note.
// Maybe in the future, we might be able to remove or dummy out notes in
// order to gain more time
// Return 0 if successful, -1 if unable to backtrack, -2 if overflow.
int Track::reverse_rest(uint16_t duration)
{
	// backtrack to disable the articulation of the previous note.
	std::vector<Atom>::reverse_iterator it;
	for(it=atoms.rbegin(); it != atoms.rend(); ++it)
	{
		switch(it->type)
		{
			case ATOM_NOTE:
			case ATOM_TIE:
			case ATOM_REST:
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
						return 0;
					}
					return -2; // Overflow condition.
				}
				else
				{
					it->off_time -= duration;
					return 0;
				}
			// Don't bother reading past loop points as effects are
			// unpredictable.
			case ATOM_CMD_SEGNO:
			case ATOM_CMD_LOOP_END:
				return -1;
			default:
				break;
		}
	}
	return -1;
}

int Track::finalize(class Song *song)
{
	printf("Not implemented yet.\n");
	return -1;
}

void Track::set_octave(int param)
{
	octave = param;
}

void Track::change_octave(int param)
{
	octave += param;
}

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

void Track::set_drum_mode(uint16_t param)
{
	if(!param)
		flag &= ~(0x01);
	else
		flag |= 0x01;
	add_atom(ATOM_CMD_DRUM_MODE, param);
}

void Track::set_duration(uint16_t param)
{
	default_duration = param;
}

uint16_t Track::get_duration(uint16_t duration)
{
	if(!duration)
		return default_duration;
	else
		return duration;
}

std::vector<Atom> *Track::get_atoms()
{
	return &atoms;
}

Atom *Track::get_atom(unsigned long position)
{
	return &atoms.at(position);
}
