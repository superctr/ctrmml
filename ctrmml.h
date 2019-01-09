#ifndef CTRMML_H
#include <stdint.h>

struct atom
{
	enum {
		ATOM_NOP = 0,
		ATOM_NOTE,
		ATOM_REST,
		ATOM_TIE,
		ATOM_LEGATO_NOTE_AND_DURATION,
		ATOM_CMD_INS,
		ATOM_CMD_VOL,
		ATOM_CMD_VOL_REL,
		ATOM_CMD_VOL_QUICK,
		ATOM_CMD_TRANSPOSE,
		ATOM_CMD_TRANSPOSE_REL,
		ATOM_CMD_OCTAVE_SET,
		ATOM_CMD_OCTAVE_UP,
		ATOM_CMD_OCTAVE_DOWN,
		ATOM_CMD_LOOP_START,
		ATOM_CMD_LOOP_BREAK,
		ATOM_CMD_LOOP_END,
		ATOM_CMD_SEGNO,
		ATOM_CMD_END
	} type;
	uint16_t param;
}

struct channel
{
	int atom_count;
	int atom_alloc;
	struct atom *atoms;
}

#define CTRMML_H
#endif
