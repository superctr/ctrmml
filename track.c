#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "ctrmml.h"

#define DEBUG_PRINT(fmt,...) { printf("ch %02x: " fmt, track->ch, __VA_ARGS__); }
static char* note_debug[12] = {"c","c#","d","d#","e","f","f#","g","g#","a","a#","b"};

struct track* track_init()
{
	struct track* track = malloc(sizeof(*track));
	track->atom_count = 0;
	track->atom_alloc = 1000;
	track->atom = malloc(sizeof(*track->atom) * track->atom_alloc);
	track->last_note = -100;
	track->octave = 5;
	track->measure_len = 96;
	track->duration = track->measure_len / 4;
	track->quantize = 8;
	return track;
}

void track_free(struct track* track)
{
	free(track->atom);
	free(track);
}

static void track_atom_add(struct track* track, enum atom_command type, uint16_t param,
		uint16_t on_time, uint16_t off_time)
{
	struct atom a = {type, param, on_time, off_time};
	track->atom[track->atom_count++] = a;
	// resize buffer if needed
	if(track->atom_count == track->atom_alloc)
	{
		track->atom_alloc *= 2;
		track->atom = realloc(track->atom, sizeof(track->atom)*track->atom_alloc);
	}
}

void track_note(struct track* track, int note, int duration)
{
	track->last_note = track->atom_count;

	int quantize = (duration * track->quantize) / 8;
	if(!quantize)
		quantize++;

	track_atom_add(track, ATOM_NOTE, note + track->octave*12, quantize, duration-quantize);

	DEBUG_PRINT("add note %s%d, %d (%d, %d)\n", note_debug[note], track->octave, duration, quantize, duration-quantize);
}

void track_tie(struct track* track, int duration)
{
	int new_duration, old_duration, quantize;
	old_duration = track->atom[track->last_note].on_duration;
	old_duration += track->atom[track->last_note].off_duration;
	new_duration = old_duration + duration;

	if(track->last_note == track->atom_count-1)
	{
		quantize = (new_duration * track->quantize) / 8;
		track->atom[track->last_note].on_duration = quantize;
		track->atom[track->last_note].off_duration = duration-quantize;
		DEBUG_PRINT("extend note %d A (%d, %d)\n", duration, duration, duration-quantize);
	}
	else if(track->last_note >= 0)
	{
		// quantization probably won't work properly if more than one tie is done like this
		quantize = (new_duration * track->quantize) / 8;
		if(quantize > old_duration)
		{
			track->atom[track->last_note].on_duration = old_duration;
			track->atom[track->last_note].off_duration = 0;
			track_atom_add(track, ATOM_TIE, 0, quantize-old_duration, new_duration-quantize);
			DEBUG_PRINT("extend note %d B (%d, %d)\n", duration, quantize-old_duration, new_duration-quantize);
		}
		else
		{
			track->atom[track->last_note].on_duration = quantize;
			track->atom[track->last_note].off_duration = old_duration-quantize;
			track_atom_add(track, ATOM_REST, 0, 0, duration);
			DEBUG_PRINT("extend note %d C (%d, %d)\n", duration, quantize, old_duration-quantize);
		}
	}
}

void track_rest(struct track *track, int duration)
{
	track_atom_add(track, ATOM_REST, 0, 0, duration);
}

// backtrack in order to shorten previous note
// return 0 if successful or -1 if unable to backtrack, -2 if overflow
int track_reverse_rest(struct track *track, int duration)
{
	int index = track->atom_count;
	while(index > 0)
	{
		enum atom_command type = track->atom[--index].type;
		if(type == ATOM_NOTE || type == ATOM_TIE || type == ATOM_REST)
		{
			if(duration > track->atom[index].off_duration)
			{
				duration -= track->atom[index].off_duration;
				track->atom[index].off_duration = 0;
			}
			else
			{
				track->atom[index].off_duration -= duration;
				DEBUG_PRINT("shortened off_duration to %d\n", track->atom[index].off_duration);
				return 0;
			}
			if(duration > track->atom[index].on_duration)
			{
				duration -= track->atom[index].on_duration;
				track->atom[index].on_duration = 0;
			}
			else
			{
				track->atom[index].on_duration -= duration;
				DEBUG_PRINT("shortened on_duration to %d\n", track->atom[index].on_duration);
				return 0;
			}
			return -2;
		}
		// don't bother read past loop points, as effects are unpredictable
		else if(type == ATOM_CMD_SEGNO ||  type == ATOM_CMD_LOOP_END)
			return -1;
	}
	return -1;
}

// backtrack in order to disable the articulation of the previous note (or tie)
// return 0 if successful or -1 if unable to backtrack
// you may run into problems if you don't put this command immediately after another note.
int track_slur(struct track* track)
{
	int index = track->atom_count;
	track_atom_add(track, ATOM_CMD_SLUR, 0, 0, 0);
	while(index > 0)
	{
		enum atom_command type = track->atom[--index].type;
		if(type == ATOM_NOTE || type == ATOM_TIE)
		{
			track->atom[index].on_duration += track->atom[index].off_duration;
			track->atom[index].off_duration = 0;
			return 0;
		}
		// don't bother read past loop points, as effects are unpredictable
		else if(type == ATOM_REST || type == ATOM_CMD_SEGNO ||  type == ATOM_CMD_LOOP_END)
			return -1;
	}
	return -1;
}

void track_quantize(struct track* track, int param)
{
	track->quantize = param;
}

void track_octave(struct track* track, int octave)
{
	track->octave = octave + 1;
}

void track_octave_up(struct track* track)
{
	track->octave += 1;
}

void track_octave_down(struct track* track)
{
	track->octave -= 1;
}

void track_atom(struct track* track, enum atom_command type, int16_t param)
{
	struct atom a = {type, param, 0, 0};
	track->atom[track->atom_count++] = a;
	// resize buffer if needed
	if(track->atom_count == track->atom_alloc)
	{
		track->atom_alloc *= 2;
		track->atom = realloc(track->atom, track->atom_alloc);
	}
	DEBUG_PRINT("atom %d = %d\n", type, param);
}

void track_drum_mode(struct track* track, int param)
{
	if(!param)
		track->flag &= ~(0x01);
	else
		track->flag |= 0x01;
	track_atom(track, ATOM_CMD_DRUM_MODE, param);
}

void track_enable(struct track* track, int ch)
{
	track->flag |= 0x80;
	track->ch = ch;
}

int track_is_enabled(struct track* track)
{
	return (track->flag & 0x80);
}

int track_in_drum_mode(struct track* track)
{
	return (track->flag & 0x01);
}

void track_finalize(struct track* track)
{
	if(track->flag & 0x80)
		track_atom(track, ATOM_CMD_END, 0);
}

