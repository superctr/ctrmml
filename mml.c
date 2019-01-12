#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "ctrmml.h"

// todo: move these to struct
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
	static char trk_list[256]; // track batching
	static int trk_count; // tracks in the batch list

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
	// add a newline if missing
	if(strlen(buffer) > 0 && buffer[strlen(buffer)-1] != '\n')
		putc('\n', stderr);
	if(column)
	{
		// display an arrow pointing to the problematic column
		column = 0;
		while(column < curr_pos-line_pos-1)
			putc((buffer[column++] == '\t') ? '\t' : ' ', stderr);
		fputs("^\n",stderr);
	}
	if(fatal)
		exit(-1);
	else
		fseek(f, curr_pos, SEEK_SET);
}

// return 1 if eol (or EOF). should handle windows newlines as well (untested yet)
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

static int my_ungetc(int c, FILE *f)
{
	return ungetc(c,f);
}

// wrapper around scanf to make sure newlines are not skipped
// also allows hex prefix
static int scannum(int* num)
{
	int c = my_getc(f); // skips spaces, but not newlines
	if(isdigit(c))
	{
		my_ungetc(c,f);
		return fscanf(f,"%d",num);
	}
	else if(c == '$' || c == 'x')
	{
		return fscanf(f,"%x",num);
	}
	else
	{
		my_ungetc(c,f);
		return -1;
	}
}

// Parser functions
// Read duration, if none found use the default value from the track
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
		my_ungetc(c, f);

	return duration;
}

// Read parameter
static int parameter(int default_param)
{
	scannum(&default_param);
	return default_param;
}

// Read parameter and throw error if none found
static int expect_parameter()
{
	int param;
	if(scannum(&param) == 1)
		return param;
	else
		PARSE_ERROR("missing parameter");
}

// Read signed parameter
static int expect_signed()
{
	int param, c=my_getc(f);
	if(c != '+' && c != '-')
		my_ungetc(c,f);
	if(scannum(&param) == 1)
	{
		if(c == '-')
			param = -param;
		return param;
	}
	else
		PARSE_ERROR("missing parameter");
}

// Convert note and read sharps / flats
static int note(int c)
{
	static const int note_values[8] = {9,11,0,2,4,5,7,11}; // abcdefgh
	int val = c - 'a';
	if(!track_in_drum_mode(trk))
		val = note_values[val];
	c = my_getc(f);
	if(c == '+') // sharp
		val++;
	else if(c == '-') // flat
		val--;
	else
		my_ungetc(c, f);
	return val;
}

// Handlers for MML commands
static void mml_slur()
{
	if(track_slur(trk))
		PARSE_WARN("slur may not affect articulation of previous note");
}

static void mml_reverse_rest(int duration)
{
	int status = track_reverse_rest(trk, duration);
	if(status == -1)
		PARSE_ERROR("unable to modify previous duration (insert a loop break?)");
	else if(status == -2)
		PARSE_ERROR("command duration too long (delete previous note?");
}

static void mml_grace()
{
	int c = note(my_getc(f));
	int duration = get_duration();
	mml_reverse_rest(duration);
	track_note(trk, c, duration);
}

// combination command that allows for two atom_types depending on
// if sign prefix is found.
static void mml_atom_relative(enum atom_command type, enum atom_command subtype)
{
	int param, c=my_getc(f);
	if(c == '+' || c == '-')
		type == subtype;
	else
		my_ungetc(c,f);
	if(scannum(&param) == 1)
	{
		if(type == ATOM_CMD_INVALID)
			PARSE_ERROR("parameter must be relative (+ or - prefix)");
		if(c == '-')
			param = -param;
		track_atom(trk, type, param);
	}
	else
		PARSE_ERROR("missing parameter");
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

		if(c >= 'a' && c <= 'h')
			c = note(c), track_note(trk, c, get_duration());
		else if(c == 'r')
			track_rest(trk, get_duration());
		else if(c == '^')
			track_tie(trk, get_duration());
		else if(c == '&')
			mml_slur();
		else if(c == 'o')
			track_octave(trk, expect_parameter());
		else if(c == '<')
			track_octave_down(trk);
		else if(c == '>')
			track_octave_up(trk);
		else if(c == 'l')
			trk->duration = get_duration();
		else if(c == 'q')
			track_quantize(trk, expect_parameter());
		else if(c == '@') // may handle alnum ids in the future...
			track_atom(trk, ATOM_CMD_INS, expect_parameter());
		else if(c == 'k')
			track_atom(trk, ATOM_CMD_TRANSPOSE, expect_signed());
		else if(c == 'K')
			track_atom(trk, ATOM_CMD_DETUNE, expect_signed());
		else if(c == 'v')
			track_atom(trk, ATOM_CMD_VOL, expect_parameter());
		else if(c == '(')
			track_atom(trk, ATOM_CMD_VOL_REL, -parameter(1));
		else if(c == ')')
			track_atom(trk, ATOM_CMD_VOL_REL, parameter(1));
		else if(c == 'V')
			mml_atom_relative(ATOM_CMD_VOL_FINE, ATOM_CMD_VOL_FINE_REL);
		else if(c == 'p')
			track_atom(trk, ATOM_CMD_PAN, expect_signed());
		else if(c == 'E')
			track_atom(trk, ATOM_CMD_VOL_ENVELOPE, expect_parameter());
		else if(c == 'M')
			track_atom(trk, ATOM_CMD_PITCH_ENVELOPE, expect_parameter());
		else if(c == 'P')
			track_atom(trk, ATOM_CMD_PAN_ENVELOPE, expect_parameter());
		else if(c == 'D')
			track_drum_mode(trk, expect_parameter());
		else if(c == 't')
			track_atom(trk, ATOM_CMD_TEMPO_BPM, expect_parameter());
		else if(c == 'T')
			track_atom(trk, ATOM_CMD_TEMPO, expect_parameter());
		else if((c == '/' || c == '}') && conditional_block)
			break;
		else if(c == '{' && !conditional_block)
			parse_mml(1); // enable conditional block
		else if(c == '[')
			track_atom(trk, ATOM_CMD_LOOP_START, 0);
		else if(c == '/')
			track_atom(trk, ATOM_CMD_LOOP_BREAK, 0);
		else if(c == ']')
			track_atom(trk, ATOM_CMD_LOOP_END, parameter(2));
		else if(c == 'L')
			track_atom(trk, ATOM_CMD_SEGNO, 0);
		else if(c == 'R')
			mml_reverse_rest(get_duration());
		else if(c == 'G')
			mml_grace();
		else if(c == '*')
			track_atom(trk, ATOM_CMD_JUMP, expect_parameter());

		else if(iseol(c))
			return;
		else
			PARSE_ERROR("unknown command");
	}

	if(conditional_block)
	{
		// skip to terminator
		while(!iseol(c) && c != '}' && c != ';')
			c = my_getc(f);
		if(c != '}')
			ERROR("unterminated conditional block");
		return;
	}

	// comment found, skipping to eol
	while(!iseol(c))
		c = my_getc(f);
}

static void parse_trk_list()
{
	int saved_line = line;
	long file_pos = ftell(f);
	int i;

	if(!trk_count)
		PARSE_ERROR("no track(s) specified");

	for(i = 0; i < trk_count; i++)
	{
		fseek(f, file_pos, SEEK_SET);
		trk_id = trk_list[i];
		trk = song->track[trk_id];
		track_enable(trk, trk_id);
		line = saved_line;
		trk_offset = i;
		parse_mml(0);
	}
}

static int get_track_id(char c)
{
	if(c >= 'A' && c <= 'Z')
		return c - 'A';
	else if(isdigit(c))
		return c - '0' + 26;
	else if(c == '*')
		return -1;
	else
		return -2;
}

static void parse_line()
{
	int c;
	line++;
	line_pos = ftell(f);

	c = getc(f);
	if(get_track_id(c) > -2)
	{
		int i = 0;
		// read the channel mask
		while((trk_id = get_track_id(c)) > -2)
		{
			if(trk_id == -1 && scannum(&trk_id) != 1)
				PARSE_ERROR("could not parse track ID");
			if(i < 256)
				trk_list[i++] = trk_id;
			c = getc(f);
		}
		trk_count = i;
		last_cmd = parse_trk_list;
	}
	else if(c == '#' || c == '@')
	{
		my_ungetc(c, f);
		fgets(buffer,1024,f);
		WARN("meta commands unhandled right now");
		return;
	}
	else if(iseol(c))
		return;

	if(isblank(c))
	{
		// skip all non blank characters
		c = my_getc(f);
		my_ungetc(c, f);

		if(iseol(c))
			return;
		if(last_cmd)
		{
			last_cmd(); // also responsible for advancing to the next line
			return;
		}
	}

	// nothing found, finish this line
	while(!iseol(c))
		c = getc(f);
}

//struct song* song_convert_mml(char* filename)
int song_convert_mml(struct song* s, char* filename)
{
	fn = filename;
	f = fopen(filename, "r");
	if(!f)
	{
		perror(filename);
		exit(-1);
	}

	song = s;

	last_cmd = NULL;
	trk_count = 0;
	line = 0;

	while(!feof(f))
		parse_line();

	return 0;
}
