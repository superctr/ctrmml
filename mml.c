#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "ctrmml.h"

struct mml_file {
	FILE* file;
	char* filename; // filename
	int line; // current line
	int line_pos; // ftell at the beginning of the line

	char buffer[1024];

	struct song *song; // working area
	void (*last_cmd)(struct mml_file*);

	struct track *trk;
	int trk_id;
	int trk_offset; // will be used for the {} command
	char trk_list[256]; // track batching
	int trk_count; // tracks in the batch list

	struct tag* tag;
};

#define PARSE_ERROR(mml,msg) error(mml, msg, 1, 1)
#define PARSE_WARN(mml,msg) error(mml, msg, 1, 0)
#define ERROR(mml,msg) error(mml, msg, 0, 1)
#define WARN(mml,msg) error(mml, msg, 0, 0)
static void error(struct mml_file *mml, char* error_msg, int column, int fatal)
{
	char* error_s = fatal ? "Error: " : "Warning: ";
	int error_column = ftell(mml->file) - mml->line_pos;
	fseek(mml->file, mml->line_pos, SEEK_SET);
	fgets(mml->buffer, 1024, mml->file);
	if(column)
		fprintf(stderr,"%s:%d:%d: %s%s\n", mml->filename, mml->line, error_column, error_s, error_msg);
	else
		fprintf(stderr,"%s:%d: %s%s\n", mml->filename, mml->line, error_s, error_msg);
	fprintf(stderr,"%s", mml->buffer);
	// add a newline if missing
	if(strlen(mml->buffer) > 0 && mml->buffer[strlen(mml->buffer)-1] != '\n')
		putc('\n', stderr);
	if(column)
	{
		// display an arrow pointing to the problematic column
		column = 0;
		while(column < error_column-1)
			putc((mml->buffer[column++] == '\t') ? '\t' : ' ', stderr);
		fputs("^\n",stderr);
	}
	if(fatal)
		exit(-1);
	else
		fseek(mml->file, error_column + mml->line_pos, SEEK_SET);
}

// return 1 if eol (or EOF). should handle windows newlines as well (untested yet)
static int iseol(struct mml_file* mml, int c)
{
	if(c == '\r')
		c = getc(mml->file);
	if(c == '\n' || c == EOF)
		return 1;
	return 0;
}

// return first non-blank character
static int my_getc(struct mml_file* mml)
{
	int c;
	do c = getc(mml->file);
	while(isblank(c));
	return c;
}

static int my_ungetc(int c, struct mml_file* mml)
{
	return ungetc(c,mml->file);
}

// wrapper around scanf to make sure newlines are not skipped
// also allows hex prefix
static int scannum(struct mml_file* mml, int* num)
{
	int c = my_getc(mml); // skips spaces, but not newlines
	if(isdigit(c))
	{
		my_ungetc(c,mml);
		return fscanf(mml->file,"%d",num);
	}
	else if(c == '$' || c == 'x')
	{
		return fscanf(mml->file,"%x",num);
	}
	else
	{
		my_ungetc(c,mml);
		return -1;
	}
}

// Parser functions
// Read duration, if none found use the default value from the track
static int get_duration(struct mml_file* mml)
{
	int duration = 0, c = 0;
	if(scannum(mml, &duration) != 1)
		duration = mml->trk->duration;
	else
		duration = mml->trk->measure_len / duration;
	c = my_getc(mml);
	if(c == '.') // dotted duration
		duration += duration>>1;
	else
		my_ungetc(c, mml);

	return duration;
}

// Read parameter
static int parameter(struct mml_file* mml, int default_param)
{
	scannum(mml, &default_param);
	return default_param;
}

// Read parameter and throw error if none found
static int expect_parameter(struct mml_file* mml)
{
	int param;
	if(scannum(mml, &param) == 1)
		return param;
	else
		PARSE_ERROR(mml,"missing parameter");
	return 0;
}

// Read signed parameter
static int expect_signed(struct mml_file* mml)
{
	int param, c=my_getc(mml);
	if(c != '+' && c != '-')
		my_ungetc(c,mml);
	if(scannum(mml, &param) == 1)
	{
		if(c == '-')
			param = -param;
		return param;
	}
	else
		PARSE_ERROR(mml,"missing parameter");
	return 0;
}

// Convert note and read sharps / flats
static int note(struct mml_file* mml, int c)
{
	static const int note_values[8] = {9,11,0,2,4,5,7,11}; // abcdefgh
	int val = c - 'a';
	if(!track_in_drum_mode(mml->trk))
		val = note_values[val];
	c = my_getc(mml);
	if(c == '+') // sharp
		val++;
	else if(c == '-') // flat
		val--;
	else
		my_ungetc(c, mml);
	return val;
}

// Handlers for MML commands
static void slur(struct mml_file *mml)
{
	if(track_slur(mml->trk))
		PARSE_WARN(mml, "slur may not affect articulation of previous note");
}

static void reverse_rest(struct mml_file *mml, int duration)
{
	int status = track_reverse_rest(mml->trk, duration);
	if(status == -1)
		PARSE_ERROR(mml, "unable to modify previous duration (insert a loop break?)");
	else if(status == -2)
		PARSE_ERROR(mml, "command duration too long (delete previous note?");
}

static void grace(struct mml_file *mml)
{
	int c = note(mml, my_getc(mml));
	int duration = get_duration(mml);
	reverse_rest(mml, duration);
	track_note(mml->trk, c, duration);
}

// combination command that allows for two atom_types depending on
// if sign prefix is found.
static void atom_relative(struct mml_file *mml, enum atom_command type, enum atom_command subtype)
{
	int param, c=my_getc(mml);
	if(c == '+' || c == '-')
		type = subtype;
	else
		my_ungetc(c,mml);
	if(scannum(mml, &param) == 1)
	{
		if(type == ATOM_CMD_INVALID)
			PARSE_ERROR(mml, "parameter must be relative (+ or - prefix)");
		if(c == '-')
			param = -param;
		track_atom(mml->trk, type, param);
	}
	else
		PARSE_ERROR(mml,"missing parameter");
}

// read MML commands
static void parse_mml(struct mml_file *mml, int conditional_block)
{
	int c;

	if(conditional_block)
	{
		int offset = mml->trk_offset;
		while(offset)
		{
			// skip to break
			do c = getc(mml->file);
			while(!iseol(mml, c) && c != '/' && c != ';');
			if(c != '/')
				ERROR(mml, "unterminated conditional block");
			c = 0;
			offset--;
		}
	}

	while(1)
	{
		c = my_getc(mml);

		if(c == '|')
			continue;
		if(c == ';')
			break;

		if(c >= 'a' && c <= 'h')
			c = note(mml, c), track_note(mml->trk, c, get_duration(mml));
		else if(c == 'r')
			track_rest(mml->trk, get_duration(mml));
		else if(c == '^')
			track_tie(mml->trk, get_duration(mml));
		else if(c == '&')
			slur(mml);
		else if(c == 'o')
			track_octave(mml->trk, expect_parameter(mml));
		else if(c == '<')
			track_octave_down(mml->trk);
		else if(c == '>')
			track_octave_up(mml->trk);
		else if(c == 'l')
			mml->trk->duration = get_duration(mml);
		else if(c == 'q')
			track_quantize(mml->trk, expect_parameter(mml));
		else if(c == '@') // may handle alnum ids in the future...
			track_atom(mml->trk, ATOM_CMD_INS, expect_parameter(mml));
		else if(c == 'k')
			track_atom(mml->trk, ATOM_CMD_TRANSPOSE, expect_signed(mml));
		else if(c == 'K')
			track_atom(mml->trk, ATOM_CMD_DETUNE, expect_signed(mml));
		else if(c == 'v')
			track_atom(mml->trk, ATOM_CMD_VOL, expect_parameter(mml));
		else if(c == '(')
			track_atom(mml->trk, ATOM_CMD_VOL_REL, -parameter(mml, 1));
		else if(c == ')')
			track_atom(mml->trk, ATOM_CMD_VOL_REL, parameter(mml, 1));
		else if(c == 'V')
			atom_relative(mml, ATOM_CMD_VOL_FINE, ATOM_CMD_VOL_FINE_REL);
		else if(c == 'p')
			track_atom(mml->trk, ATOM_CMD_PAN, expect_signed(mml));
		else if(c == 'E')
			track_atom(mml->trk, ATOM_CMD_VOL_ENVELOPE, expect_parameter(mml));
		else if(c == 'M')
			track_atom(mml->trk, ATOM_CMD_PITCH_ENVELOPE, expect_parameter(mml));
		else if(c == 'P')
			track_atom(mml->trk, ATOM_CMD_PAN_ENVELOPE, expect_parameter(mml));
		else if(c == 'D')
			track_drum_mode(mml->trk, expect_parameter(mml));
		else if(c == 't')
			track_atom(mml->trk, ATOM_CMD_TEMPO_BPM, expect_parameter(mml));
		else if(c == 'T')
			track_atom(mml->trk, ATOM_CMD_TEMPO, expect_parameter(mml));
		else if((c == '/' || c == '}') && conditional_block)
			break;
		else if(c == '{' && !conditional_block)
			parse_mml(mml, 1); // enable conditional block
		else if(c == '[')
			track_atom(mml->trk, ATOM_CMD_LOOP_START, 0);
		else if(c == '/')
			track_atom(mml->trk, ATOM_CMD_LOOP_BREAK, 0);
		else if(c == ']')
			track_atom(mml->trk, ATOM_CMD_LOOP_END, parameter(mml, 2));
		else if(c == 'L')
			track_atom(mml->trk, ATOM_CMD_SEGNO, 0);
		else if(c == 'R')
			reverse_rest(mml, get_duration(mml));
		else if(c == 'G')
			grace(mml);
		else if(c == '*')
			track_atom(mml->trk, ATOM_CMD_JUMP, expect_parameter(mml));
		else if(c == '%')
			track_atom(mml->trk, ATOM_CMD_CHANNEL_MODE, expect_parameter(mml));

		else if(iseol(mml, c))
			return;
		else
			PARSE_ERROR(mml, "unknown command");
	}

	if(conditional_block)
	{
		// skip to terminator
		while(!iseol(mml, c) && c != '}' && c != ';')
			c = my_getc(mml);
		if(c != '}')
			ERROR(mml, "unterminated conditional block");
		return;
	}

	// comment found, skipping to eol
	while(!iseol(mml, c))
		c = my_getc(mml);
}

static void parse_trk_list(struct mml_file *mml)
{
	int saved_line = mml->line;
	long file_pos = ftell(mml->file);
	int i;

	if(!mml->trk_count)
		PARSE_ERROR(mml, "no track(s) specified");

	for(i = 0; i < mml->trk_count; i++)
	{
		fseek(mml->file, file_pos, SEEK_SET);
		mml->trk_id = mml->trk_list[i];
		mml->trk = mml->song->track[mml->trk_id];
		track_enable(mml->trk, mml->trk_id);
		mml->line = saved_line;
		mml->trk_offset = i;
		parse_mml(mml,0);
	}
}

static void parse_tag(struct mml_file *mml)
{
	fgets(mml->buffer,1024,mml->file);
	if(mml->tag->key[0] == '#')
		add_tag(mml->tag, mml->buffer);
	else
		add_tag_list(mml->tag, mml->buffer);
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

static void parse_line(struct mml_file *mml)
{
	int c;
	mml->line++;
	mml->line_pos = ftell(mml->file);

	c = getc(mml->file);
	if(get_track_id(c) > -2)
	{
		int i = 0;
		// read the channel mask
		while((mml->trk_id = get_track_id(c)) > -2)
		{
			if(mml->trk_id == -1 && scannum(mml, &mml->trk_id) != 1)
				PARSE_ERROR(mml,"could not parse track ID");
			if(i < 256)
				mml->trk_list[i++] = mml->trk_id;
			c = getc(mml->file);
		}
		mml->trk_count = i;
		mml->last_cmd = parse_trk_list;
	}
	else if(c == '#' || c == '@')
	{
		my_ungetc(c, mml);
		fscanf(mml->file, "%s", mml->buffer);
		mml->tag = tag_find(mml->song->tag, mml->buffer);
		if(!mml->tag)
			mml->tag = tag_append(mml->song->tag, tag_create(mml->buffer, NULL));
		mml->last_cmd = parse_tag;
		c = getc(mml->file); // get the last character
		my_ungetc(c, mml);
	}
	else if(iseol(mml, c))
		return;

	if(isblank(c))
	{
		// skip all non blank characters
		c = my_getc(mml);
		my_ungetc(c, mml);

		if(iseol(mml, c))
			return;
		if(mml->last_cmd)
		{
			mml->last_cmd(mml); // also responsible for advancing to the next line
			return;
		}
	}

	// nothing found, finish this line
	while(!iseol(mml, c))
		c = getc(mml->file);
}

//struct song* song_convert_mml(char* filename)
int song_convert_mml(struct song* s, char* filename)
{
	struct mml_file *mml;
	mml = calloc(1,sizeof(*mml));
	mml->song = s;
	mml->filename = filename;
	mml->file = fopen(filename, "r");
	mml->last_cmd = NULL;
	mml->trk_count = 0;
	mml->line = 0;

	if(!mml->file)
	{
		perror(filename);
		exit(-1);
	}
	while(!feof(mml->file))
		parse_line(mml);

	fclose(mml->file);
	free(mml);
	return 0;
}
