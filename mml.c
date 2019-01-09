#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "ctrmml.h"

// globals for the MML module
	static FILE* f;
	static char* fn; // filename
	static int line; // current line
	static int line_pos; // ftell at the beginning of the line

	static char buffer[1024];

	static struct song *song; // working area
	static void (*last_cmd)();

	static struct track *trk;
	static int trk_id;
	static int trk_offset; // will be used for the {} command
	static char trk_list[128]; // track batching

#define PARSE_ERROR(msg) error(msg, 1, 1)
#define PARSE_WARN(msg) error(msg, 1, 0)
#define ERROR(msg) error(msg, 0, 1)
#define WARN(msg) error(msg, 0, 0)
static void error(char* error_msg, int column, int fatal)
{
	int curr_pos = ftell(f);
	char* error_s = fatal ? "Error: " : "Warning: ";
	fseek(f, line_pos, SEEK_SET);
	fgets(buffer, 1024, f);
	if(column)
		fprintf(stderr,"%s:%d:%d: %s%s\n", fn, line, curr_pos-line_pos, error_s, error_msg);
	else
		fprintf(stderr,"%s:%d: %s%s\n", fn, line, error_s, error_msg);
	fprintf(stderr,"%s", buffer);
	if(strlen(buffer) > 0 && buffer[strlen(buffer)-1] != '\n')
		putc('\n', stderr);
	if(column)
	{
		column = curr_pos;
		while(--column > line_pos)
			putc(' ', stderr);
		fputs("^\n",stderr);
	}
	if(fatal)
		exit(-1);
	else
		fseek(f, curr_pos, SEEK_SET);
}

static int iseol(int c)
{
	if(c == '\r')
		c = getc(f);
	if(c == '\n' || c == EOF)
	{
		return 1;
	}
	return 0;
}

// return first non-blank character
static int my_getc(FILE *f)
{
	int c;
	do c = getc(f);
	while(isblank(c));
	return c;
}

// wrapper around scanf to make sure newlines are not skipped
// also allows hex prefix
static int scannum(int* num)
{
	int c = my_getc(f); // skips spaces, but not newlines
	if(isdigit(c))
	{
		ungetc(c,f);
		return fscanf(f,"%d",num);
	}
	else if(c == '$' || c == 'x')
	{
		return fscanf(f,"%x",num);
	}
	else
	{
		ungetc(c,f);
		return -1;
	}
}

static void mml_default_duration()
{
	int duration = 0, c = 0;
	if(scannum(&duration) == 1)
		duration = trk->measure_len / duration;
	else
		PARSE_ERROR("missing parameter");
	c = my_getc(f);
	if(c == '.') // dotted duration
		duration += duration>>1;
	else
		ungetc(c, f);

	trk->duration = duration;
}
static int get_duration()
{
	int duration = 0, c = 0;
	if(scannum(&duration) != 1)
		duration = trk->duration;
	else
		duration = trk->measure_len / duration;
	c = my_getc(f);
	if(c == '.') // dotted duration
		duration += duration>>1;
	else
		ungetc(c, f);

	return duration;
}

static void mml_note(int c)
{
	static const int note_values[8] = {9,11,0,2,4,5,7}; // abcdefg
	int val = note_values[c - 'a'];
	c = my_getc(f);
	if(c == '+') // sharp
		val++;
	else if(c == '-') // flat
		val--;
	else
		ungetc(c, f);
	track_note(trk, val, get_duration());
}

static void mml_slur()
{
	if(track_slur(trk))
		PARSE_WARN("slur may not affect articulation of previous note");
}

static void mml_rest()
{
	track_rest(trk, get_duration());
}

static void mml_tie()
{
	track_tie(trk, get_duration());
}

static void mml_octave()
{
	int octave;
	if(scannum(&octave) == 1)
		track_octave(trk, octave);
	else
		PARSE_ERROR("missing parameter");
}

static void mml_quantize()
{
	int param;
	if(scannum(&param) == 1)
		track_quantize(trk, param);
	else
		PARSE_ERROR("missing parameter");
}

static void mml_generic(int type)
{
	int param;
	if(scannum(&param) == 1)
		track_atom(trk, type, param);
	else
		PARSE_ERROR("missing parameter");
}

static void mml_generic_default(int type, int param)
{
	scannum(&param);
	track_atom(trk, type, param);
}

static void mml_volume_rel(int inc)
{
	int multiplier = 1;
	if(scannum(&multiplier) == 1)
		inc *= multiplier;
	track_atom(trk, ATOM_CMD_VOL_REL, inc);
}

// read MML commands
static void parse_mml(int conditional_block)
{
	int c;

	if(conditional_block)
	{
		int offset = trk_offset;
		while(offset)
		{
			// skip to break
			while(!iseol(c) && c != '/' && c != ';')
				c = getc(f);
			if(c != '/')
				ERROR("unterminated conditional block");
			c = 0;
			offset--;
		}
	}

	while(1)
	{
		c = my_getc(f);

		if(c == '|')
			continue;
		if(c == ';')
			break;

		if(c >= 'a' && c <= 'g')
			mml_note(c);
		else if(c == 'r')
			mml_rest();
		else if(c == '^')
			mml_tie();
		else if(c == '&')
			mml_slur();
		else if(c == 'o')
			mml_octave();
		else if(c == '>')
			track_octave_up(trk);
		else if(c == '<')
			track_octave_down(trk);
		else if(c == 'l')
			mml_default_duration();
		else if(c == 'q')
			mml_quantize();
		else if(c == 't')
			mml_generic(ATOM_CMD_TEMPO_BPM);
		else if(c == 'T')
			mml_generic(ATOM_CMD_TEMPO);
		else if(c == '@') // may handle alnum ids in the future...
			mml_generic(ATOM_CMD_INS);
		else if(c == 'k')
			mml_generic(ATOM_CMD_TRANSPOSE);
		else if(c == 'K')
			mml_generic(ATOM_CMD_DETUNE);
		else if(c == 'v')
			mml_generic(ATOM_CMD_VOL);
		else if(c == '(')
			mml_volume_rel(-1);
		else if(c == ')')
			mml_volume_rel(1);
		else if(c == 'V')
			mml_generic(ATOM_CMD_VOL_FINE);
		else if((c == '/' || c == '}') && conditional_block)
			break;
		else if(c == '{')
			parse_mml(1); // enable conditional block
		else if(c == '[')
			track_atom(trk, ATOM_CMD_LOOP_START, 0);
		else if(c == '/')
			track_atom(trk, ATOM_CMD_LOOP_BREAK, 0);
		else if(c == ']')
			mml_generic_default(ATOM_CMD_LOOP_END,2);
		else if(c == 'L')
			track_atom(trk, ATOM_CMD_SEGNO, 0);

		else if(iseol(c))
			return;
		else
			PARSE_ERROR("unknown command");
	}

	if(conditional_block)
	{
		// skip to terminator
		while(!iseol(c) && c != '}' && c != ';')
			c = getc(f);
		if(c != '}')
			ERROR("unterminated conditional block");
		return;
	}

	// comment found, skipping to eol
	while(!iseol(c))
		c = getc(f);
}

static void parse_trk_list()
{
	int saved_line = line;
	long file_pos = ftell(f);
	int count = strlen(trk_list);
	int i;

	if(!count)
		PARSE_ERROR("no track(s) specified");

	for(i = 0; i < count; i++)
	{
		fseek(f, file_pos, SEEK_SET);
		trk_id = trk_list[i] - 'A';
		trk = song->track[trk_id];
		track_enable(trk, trk_id);
		line = saved_line;
		trk_offset = i;
		parse_mml(0);
	}
}

static void parse_line()
{
	int c;
	line++;
	line_pos = ftell(f);

	c = getc(f);
	if(c >= 'A' && c <= 'P')
	{
		int i = 0;
		// read the channel mask
		while(isalpha(c) && c >= 'A' && c <= 'P')
		{
			if(i < 127)
				trk_list[i++] = c;
			c = getc(f);
		}
		trk_list[i] = 0;
		last_cmd = parse_trk_list;
	}
	else if(c == '#')
	{
		ungetc(c, f);
		fgets(buffer,1024,f);
		WARN("meta commands unhandled right now");
		return;
	}
	else if(iseol(c))
		return;

	if(isblank(c))
	{
		// skip all non blank characters
		while(isblank(c))
			c = getc(f);
		ungetc(c, f);

		if(iseol(c))
			return;
		if(last_cmd)
		{
			printf("now on %s\n", trk_list);
			last_cmd(); // also responsible for advancing to the next line
			return;
		}
	}

	// nothing found, finish this line
	while(!iseol(c))
		c = getc(f);
}

static void finalize_tracks()
{
	for(trk_id=0; trk_id<256; trk_id++)
		track_finalize(song->track[trk_id]);
}

struct song* song_convert_mml(char* filename)
{
	int i;
	fn = filename;
	f = fopen(filename, "r");
	if(!f)
	{
		perror(filename);
		exit(-1);
	}

	song = calloc(1, sizeof(*song));

	for(i=0; i<256; i++)
		song->track[i] = track_init();

	last_cmd = NULL;
	line = 0;

	while(!feof(f))
		parse_line();

	finalize_tracks();

	return song;
}

void song_free(struct song* song)
{
	int i;
	for(i=0; i<256; i++)
		track_free(song->track[i]);
}
