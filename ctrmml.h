#ifndef CTRMML_H
#include <stdint.h>

// MML intermediatory language
struct atom
{
	enum {
		ATOM_NOP = 0,
		ATOM_REST, // read durations (off time probably best)
		ATOM_NOTE, // read durations (on time and off time)
		ATOM_TIE, // param is ignored, read durations
		ATOM_CMD_SLUR, // flag to indicate the next note is legato
		ATOM_CMD_INS,
		ATOM_CMD_VOL_FINE,
		ATOM_CMD_VOL_FINE_REL,
		ATOM_CMD_VOL,
		ATOM_CMD_VOL_REL,
		ATOM_CMD_TRANSPOSE,
		ATOM_CMD_TRANSPOSE_REL,
		ATOM_CMD_DETUNE,
		ATOM_CMD_LOOP_START,
		ATOM_CMD_LOOP_BREAK,
		ATOM_CMD_LOOP_END,
		ATOM_CMD_TEMPO_BPM,
		ATOM_CMD_TEMPO,
		ATOM_CMD_SEGNO,
		ATOM_CMD_END
	} type;
	int16_t param;
	uint16_t on_duration; // only for note and rest
	uint16_t off_duration;
};

struct track
{
	uint8_t flag; // 0x80 if track is used
	uint8_t ch;
	int atom_count;
	int atom_alloc;
	struct atom *atom;
	int last_note; // last atom id that was a note
	int octave;
	int measure_len;
	int duration; // default duration
	int quantize;
};

struct song
{
	struct track *track[256];
	uint8_t track_map[256];
};

// stuff to remember when looping
struct atom_state
{
	int vol;
	int ins;
	int pan;
	int transpose;
};


// functions
struct track* track_init();
void track_free(struct track* track);
void track_atom(struct track* track, int type, int16_t param);
void track_note(struct track* track, int note, int duration);
int  track_slur(struct track* track);
void track_tie(struct track* track, int duration);
void track_rest(struct track* track, int duration);
void track_quantize(struct track* track, int param);
void track_octave(struct track* track, int octave);
void track_octave_up(struct track* track);
void track_octave_down(struct track* track);
void track_finalize(struct track* track);
void track_enable(struct track* track, int ch);
int track_is_enabled(struct track* track);

struct song* song_convert_mml(char* filename);
void song_free(struct song* song);

#define CTRMML_H
#endif
