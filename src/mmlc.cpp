#include "song.h"
#include "input.h"
#include "mml_input.h"
#include "player.h"
#include "vgm.h"
#include "platform/md.h"
#include "platform/mdsdrv.h"
#include "stringf.h"

#include <iostream>
#include <fstream>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage(const char* exename)
{
	std::cout << "mmlc - ctr MML Compiler\n";
	std::cout << "(C) 2019-2020 ian karlsson\n\n";
	std::cout << "Usage: " << exename << " <input_file.mml>\n";
}

std::string output_filename(const char* input_filename, const char* extension)
{
	static char str[256];
	char *last_dot;
	strncpy(str,input_filename, 256);
	last_dot = strrchr(str, '.');
	if(last_dot)
		*last_dot = 0;
	strncat(last_dot, extension, 256);
	return str;
}

Song convert_file(const char* filename)
{
	Song song;
	MML_Input input = MML_Input(&song);
	input.open_file(filename);
	auto validator = Song_Validator(song);
	for(auto it = validator.get_track_map().begin(); it != validator.get_track_map().end(); it++)
	{
		std::cout << stringf("Track%3d:%7d", it->first, it->second.get_play_time());
		if(auto length = it->second.get_loop_length())
			std::cout << stringf(" (loop %7d)", length);
		std::cout << "\n";
	}
	return song;
}

std::string safe_get_tag(Song& song, const std::string& tagname)
{
	if(song.get_tag_map()[tagname].size())
		return song.get_tag_map()[tagname].front();
	else
		return "";
}

VGM_Tag get_tags(Song& song)
{
	VGM_Tag tag;
	Tag_Map tag_map = song.get_tag_map();

	tag.title = safe_get_tag(song,"#title");
	tag.title_j = safe_get_tag(song,"#titlej");
	tag.author = safe_get_tag(song,"#composer");
	tag.author_j = safe_get_tag(song,"#composerj");
	tag.system = safe_get_tag(song,"#system");
	tag.system_j = safe_get_tag(song,"#systemj");
	tag.game = safe_get_tag(song,"#game");
	tag.game_j = safe_get_tag(song,"#gamej");
	tag.creator = safe_get_tag(song,"#programmer");
	tag.notes = safe_get_tag(song,"#comment");
	tag.date = safe_get_tag(song,"#vgmdate");

	// Fallback tags
	if(!tag.author.size())
		tag.creator = safe_get_tag(song,"#author");
	if(!tag.creator.size())
		tag.creator = safe_get_tag(song,"#programer");
	if(!tag.creator.size())
		tag.creator = tag.author;

	return tag;
}

void generate_vgm(Song& song, const std::string& filename, int max_seconds)
{
	VGM_Writer vgm(filename.c_str(), 0x61, 0x100);
	MD_Driver driver(44100, &vgm);
	long max_time = max_seconds * 44100;
	driver.play_song(song);
	double elapsed_time = 0;
	double delta = 0;
	bool looped_or_finished = 0;
	while(elapsed_time < max_time)
	{
		vgm.delay(delta);
		delta = driver.play_step();
		elapsed_time += delta;
		if(!driver.is_playing() || driver.loop_count() > 0)
		{
			looped_or_finished = 1;
			break;
		}
	}
	if(!looped_or_finished)
		vgm.delay(max_time-elapsed_time);
	vgm.stop();
	vgm.write_tag(get_tags(song));
}

void generate_mds(Song& song, const std::string& filename)
{
	MDSDRV_Converter converter(song);

	auto bytes = converter.get_mds().to_bytes();

	std::ofstream out(filename, std::ios::binary);
	out.write((char*)bytes.data(), bytes.size());
}

int main(int argc, char* argv[])
{
	enum
	{
		ACTION_VGM,
		ACTION_COMPILE,
	} action = ACTION_VGM;
	std::string in_filename = "";
	std::string out_filename = "";
	uint32_t max_seconds = 300;

	for(int arg = 1, default_arguments=0; arg < argc; arg++)
	{
		if((!strcmp(argv[arg], "-o") || !strcmp(argv[arg], "--output")) && arg < argc)
			out_filename = argv[++arg];
		else if((!strcmp(argv[arg], "-s") || !strcmp(argv[arg], "--max-seconds")) && arg < argc)
			max_seconds = strtoul(argv[++arg], NULL, 0);
		else if(!strcmp(argv[arg], "-c") || !strcmp(argv[arg], "--compile"))
			action = ACTION_COMPILE;
		else if(default_arguments < 1)
		{
			default_arguments++;
			in_filename = argv[arg];
		}
	}

	if(!in_filename.size())
	{
		print_usage(argv[0]);
		std::cerr << "no input specified\n";
		return -1;
	}

	try
	{
		Song song = convert_file(in_filename.c_str());
		switch(action)
		{
			case ACTION_VGM:
				if(!out_filename.size())
					out_filename = output_filename(in_filename.c_str(), ".vgm");
				generate_vgm(song, out_filename, max_seconds);
				break;
			case ACTION_COMPILE:
				if(!out_filename.size())
					out_filename = output_filename(in_filename.c_str(), ".mds");
				generate_mds(song, out_filename);
				break;
		}
	}
	catch (InputError& error)
	{
		std::cerr << error.what() << "\n";
		return -1;
	}
}

