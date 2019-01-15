#ifndef CTRMML_H
#include <stdint.h>

enum atom_command {
	// basic commands
	ATOM_NOP = 0,
	ATOM_REST, // read durations (off time probably best)
	ATOM_NOTE, // read durations (on time and off time)
	ATOM_TIE, // param is ignored, read durations
	// track commands
	ATOM_CMD_LOOP_START,
	ATOM_CMD_LOOP_BREAK,
	ATOM_CMD_LOOP_END,
	ATOM_CMD_SEGNO,
	ATOM_CMD_JUMP,
	ATOM_CMD_END,
	ATOM_CMD_SLUR, // flag to indicate the next note is legato
	// special channel commands. These affect the same memory as another command
	ATOM_CMD_TRANSPOSE_REL,
	ATOM_CMD_VOL,
	ATOM_CMD_VOL_REL,
	ATOM_CMD_VOL_FINE_REL,
	ATOM_CMD_TEMPO_BPM,
	// channel commands
	ATOM_CMD_CHANNEL_MODE, // always the first channel command
	ATOM_CMD_INS,
	ATOM_CMD_TRANSPOSE,
	ATOM_CMD_DETUNE,
	ATOM_CMD_VOL_FINE,
	ATOM_CMD_PAN,
	ATOM_CMD_VOL_ENVELOPE,
	ATOM_CMD_PITCH_ENVELOPE,
	ATOM_CMD_PAN_ENVELOPE,
	ATOM_CMD_DRUM_MODE,
	ATOM_CMD_TEMPO,
	// special
	ATOM_CMD_COUNT, // last command ID
	ATOM_CHANNEL_CMD = ATOM_CMD_CHANNEL_MODE, // first channel cmd ID
	ATOM_CHANNEL_CMD_COUNT = ATOM_CMD_COUNT - ATOM_CHANNEL_CMD, // channel command count
	ATOM_CMD_INVALID = -1,
};

// MML intermediatory language
struct atom
{
	enum atom_command type;
	int16_t param;
	uint16_t on_duration; // only for note and rest
	uint16_t off_duration;
};

struct track
{
	uint8_t flag; // 0x80 if track is used, 0x01 for drum mode
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

struct tag
{
	char *key;
	struct tag *property;
	struct tag *next;
};

struct song
{
	struct track *track[256];
	uint8_t track_map[256];
	struct tag *tag;
};

typedef void (*atom_cb_t)(void* cb_state, struct atom* atom);

// track player status
#define PLAYER_MAX_STACK 8
struct track_player
{
	struct song* song;
	struct track* track;
	enum {
		CHANNEL_DISABLED,
		CHANNEL_INACTIVE,
		CHANNEL_KEYON,
		CHANNEL_KEYOFF
	} state;
	int delay;
	int position;
	int loop_position;
	uint32_t ch_flag; // flag set if ch_state changed
	int ch_state[ATOM_CHANNEL_CMD_COUNT];
	uint32_t loop_ch_flag;
	int loop_ch_state[ATOM_CHANNEL_CMD_COUNT];
	struct {
		int loop_count;
		struct track* track;
		int position;
		int end_position;
	} stack[PLAYER_MAX_STACK + 1];
	int stack_frame;
	int accumulated_length;

	void* cb_state;
	atom_cb_t atom_callback;
	unsigned int cb_unroll_loops : 1;
	unsigned int cb_unroll_jumps : 1;
	unsigned int cb_keyoff : 1; // send rest instead of off_time
	unsigned int cb_optimized : 1; // send all atoms or just optimized
	int inside_loop;
	int inside_jump;
};

// Functions for use by song parsers
// track module
struct track* track_init();
void track_free(struct track* track);
void track_atom(struct track* track, enum atom_command type, int16_t param);
void track_note(struct track* track, int note, int duration);
int  track_slur(struct track* track);
void track_tie(struct track* track, int duration);
void track_rest(struct track* track, int duration);
int  track_reverse_rest(struct track* track, int duration);
void track_quantize(struct track* track, int param);
void track_drum_mode(struct track* track, int param);
void track_octave(struct track* track, int octave);
void track_octave_up(struct track* track);
void track_octave_down(struct track* track);
void track_finalize(struct song* song, struct track* track, int id);
void track_enable(struct track* track, int ch);
int track_is_enabled(struct track* track);
int track_in_drum_mode(struct track* track);

// track playback
struct track_player* track_player_init(struct song* song, struct track *track);
int track_player_reset(struct track_player* player, struct song* song, int track_id);
void track_player_free(struct track_player *player);
int track_player_step(struct track_player *player);

// song module
struct tag* tag_create(char* value, struct tag* property);
void tag_delete_recursively(struct tag* tag);
struct tag* tag_get_next(struct tag* tag);
struct tag* tag_append(struct tag* tag, struct tag* new);
struct tag* tag_get_property(struct tag* tag);
struct tag* tag_add_property(struct tag* tag, struct tag* new);
struct tag* tag_set_property(struct tag* tag, struct tag* new);
struct tag* tag_find(struct tag* tag, char* key);

void add_tag(struct tag* tag, char* s);
void add_tag_list(struct tag* tag, char* s);

struct song* song_create();
int song_open(struct song* song, char* filename);
int song_finalize(struct song* song);
void song_free(struct song* song);

int song_convert_mml(struct song* song, char* filename);

void md_create_vgm(struct song* song, char* filename);

#define CTRMML_H
#endif
