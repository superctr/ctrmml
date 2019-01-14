#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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

void track_finalize_cb(void* cb_state, struct atom* atom)
{
	struct track_player *player = cb_state;
	if(atom->type == ATOM_CMD_SEGNO)
	{
		printf("at loop pos\n");
		memcpy(player->loop_ch_state, player->ch_state, sizeof(player->loop_ch_state));
		player->loop_ch_flag = player->ch_flag;
	}
}

void track_finalize(struct song* song, struct track* track, int id)
{
	// read track, calculate track lengths and add commands before loop
	if(track->flag & 0x80)
	{
		struct track_player *player = track_player_init(song, track);
		player->atom_callback = track_finalize_cb;
		player->cb_state = player;
		int res;
		do;
		while((res = track_player_step(player)) >= 0);

		if(res == -2)
		{
			fprintf(stderr, "an error occured while processing track %02x", id);
			exit(-1);
		}

		if(player->loop_position >= 0)
		{
			int i;
			// restore register values to as what they were at the beginning of the loop
			// this feature could be made optional, maybe
			for(i = 0; i < ATOM_CHANNEL_CMD_COUNT; i++)
			{
				// we want to send 0 value to regs that were set in the loop part only
				uint32_t unset_at_loop = player->loop_ch_flag ^ player->ch_flag;
				if(player->ch_flag & (1<<i))
				{
					int type = i + ATOM_CMD_CHANNEL_MODE;
					// special case for coarse volume and tempo bpm as they use same memory
					// as the fine volume and timer value respectively
					if(type == ATOM_CMD_VOL_FINE && player->loop_ch_flag & (1<<30))
						type = ATOM_CMD_VOL;
					else if(type == ATOM_CMD_TEMPO && player->loop_ch_flag & (1<<31))
						type = ATOM_CMD_TEMPO_BPM;
					if(unset_at_loop & (1<<i))
						track_atom(track, type, 0);
					else if(player->ch_state[i] != player->loop_ch_state[i])
						track_atom(track, type, player->loop_ch_state[i]);
					printf("restore %02x = %d\n", type, player->loop_ch_state[i]);
				}
			}
		}

		printf("Track %02x length = %6d\n", id, player->accumulated_length);

		track_atom(track, ATOM_CMD_END, 0);
	}
}

// Initialize track player
struct track_player* track_player_init(struct song* song, struct track *track)
{
	struct track_player* player = calloc(1, sizeof(*player));
	player->track = track;
	player->song = song;
	player->loop_position = -1;
	return player;
}

void track_player_free(struct track_player *player)
{
	free(player);
}

static void atom_send_cb(struct track_player *player, struct atom* atom)
{
	if(!player->cb_unroll_loops && player->inside_loop)
		return;
	if(!player->cb_unroll_jumps && player->inside_jump)
		return;
	if(player->atom_callback)
		player->atom_callback(player->cb_state, atom);
}

// step the track player. this should work for both file generation and playback/logging
// by setting the "expand jump/loop" bits as needed and using the callback.
int track_player_step(struct track_player *player)
{
	struct atom* atom = &player->track->atom[player->position];
	player->accumulated_length += player->delay;
	player->delay = 0;
	if(player->state == CHANNEL_KEYON)
	{
		player->state = CHANNEL_INACTIVE;
		struct atom* old_atom = &player->track->atom[player->position-1];
		if(old_atom->off_duration)
		{
			player->delay = old_atom->off_duration;
			player->state = CHANNEL_KEYOFF;
			return player->delay;
		}
	}
	if(player->position++ >= player->track->atom_count)
	{
		if(player->stack_frame)
		{
			player->track = player->stack[player->stack_frame].track;
			player->position = player->stack[player->stack_frame--].position;
			player->inside_jump--;
		}
		else if(player->inside_loop)
			return -2;
		else
			return -1;
	}
	atom_send_cb(player, atom);
	// todo: move below code to a jumptable
	switch(atom->type)
	{
		case ATOM_REST:
			player->delay = atom->off_duration;
			player->state = CHANNEL_KEYOFF;
			break;
		case ATOM_NOTE:
		case ATOM_TIE:
			if(atom->on_duration)
			{
				player->state = CHANNEL_KEYON;
				player->delay = atom->on_duration;
			}
			else if(atom->off_duration)
			{
				player->state = CHANNEL_KEYOFF;
				player->delay = atom->off_duration;
			}
			else
			{
				printf("error: orphan atom\n");
				return -2;
			}
			break;
		case ATOM_CMD_LOOP_START:
			if(player->stack_frame == PLAYER_MAX_STACK)
			{
				printf("error: loop stack overflow\n");
				return -2;
			}
			player->stack[++player->stack_frame].loop_count = 0;
			player->stack[player->stack_frame].position = player->position;
			break;
		case ATOM_CMD_LOOP_BREAK:
			if(player->stack_frame == 0)
			{
				printf("error: loop stack underflow\n");
				return -2;
			}
			// set param to end position to make it easier later
			atom->param = player->stack[player->stack_frame].end_position;
			if(player->stack[player->stack_frame].loop_count == 1)
			{
				player->position = player->stack[player->stack_frame--].end_position;
				player->inside_loop--;
			}
			break;
		case ATOM_CMD_LOOP_END:
			if(player->stack_frame == 0)
			{
				printf("error: loop stack underflow\n");
				return -2;
			}
			// set end position in stack
			player->stack[player->stack_frame].end_position = player->position;
			if(player->stack[player->stack_frame].loop_count == 0)
			{
				player->stack[player->stack_frame].loop_count = atom->param;
				player->inside_loop++;
			}
			if(--player->stack[player->stack_frame].loop_count > 0)
			{
				player->position = player->stack[player->stack_frame].position;
			}
			else
			{
				player->stack_frame--;
				player->inside_loop--;
			}
			break;
		case ATOM_CMD_SEGNO:
			player->loop_position = player->position;
			break;
		case ATOM_CMD_JUMP:
			if(player->stack_frame == PLAYER_MAX_STACK)
			{
				printf("error: jump stack overflow\n");
				return -2;
			}
			player->stack[++player->stack_frame].position = player->position;
			player->stack[player->stack_frame].track = player->track;
			player->track = player->song->track[atom->param];
			player->position = 0;
			player->inside_jump++;
			break;
		case ATOM_CMD_END:
			if(player->stack_frame)
			{
				player->track = player->stack[player->stack_frame].track;
				player->position = player->stack[player->stack_frame--].position;
				break;
			}
			else if(player->loop_position >= 0)
				player->position = player->loop_position;
			else
				return -1;
			break;
		case ATOM_CMD_SLUR:
			break;
#define FLAG_SET(flag) { player->ch_flag |= (1<<(flag - ATOM_CMD_CHANNEL_MODE)); }
#define FLAG_CLR(flag) { player->ch_flag &= ~(1<<(flag - ATOM_CMD_CHANNEL_MODE)); }
#define CH_STATE(var) player->ch_state[var - ATOM_CMD_CHANNEL_MODE]
#define VOL_BIT 30 + ATOM_CMD_CHANNEL_MODE
#define BPM_BIT 31 + ATOM_CMD_CHANNEL_MODE
		case ATOM_CMD_TRANSPOSE_REL:
			CH_STATE(ATOM_CMD_TRANSPOSE) += atom->param;
			FLAG_SET(ATOM_CMD_TRANSPOSE);
			break;
		case ATOM_CMD_VOL:
		case ATOM_CMD_VOL_REL:
			if(atom->type != ATOM_CMD_VOL_REL)
				CH_STATE(ATOM_CMD_VOL_FINE) = 0;
			CH_STATE(ATOM_CMD_VOL_FINE) += atom->param;
			FLAG_CLR(ATOM_CMD_VOL_FINE);
			FLAG_SET(VOL_BIT);
			break;
		case ATOM_CMD_VOL_FINE_REL:
			CH_STATE(ATOM_CMD_VOL_FINE) += atom->param;
			FLAG_SET(ATOM_CMD_VOL_FINE);
			FLAG_CLR(VOL_BIT);
			break;
		case ATOM_CMD_TEMPO_BPM:
			CH_STATE(ATOM_CMD_TEMPO) = atom->param;
			FLAG_CLR(ATOM_CMD_TEMPO);
			FLAG_SET(BPM_BIT);
			break;
		default:
			if(atom->type > ATOM_CMD_CHANNEL_MODE && atom->type < ATOM_CMD_COUNT)
			{
				int type = atom->type - ATOM_CMD_CHANNEL_MODE;
				player->ch_state[type] = atom->param;
				player->ch_flag |= 1<<type;
				if(type == ATOM_CMD_VOL_FINE)
					FLAG_CLR(VOL_BIT);
				if(type == ATOM_CMD_TEMPO)
					FLAG_CLR(BPM_BIT);
			}
			break;
	}
	return player->delay;
}

