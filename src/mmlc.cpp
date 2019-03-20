#include "song.h"
#include "input.h"
#include "mml_input.h"
#include "player.h"
#include "vgm.h"
#include "platform/md.h"
#include "stringf.h"

#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage(char* exename)
{
	std::cout << "mmlc (pre-alpha version)\n";
	std::cout << "(C) 2019 ian karlsson\n";
	std::cout << exename << " <input_file.mml>\n";
}

std::string output_filename(char* input_filename, char* extension)
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

void generate_vgm(Song& song, const char* filename)
{
	VGM_Writer vgm(filename, 0x61, 0x100);
	MD_Driver driver(44100, &vgm);
	driver.play_song(song);

}

int main(int argc, char* argv[])
{
	if(argc < 2)
	{
		print_usage(argv[0]);
		std::cerr << "no input specified\n";
		return -1;
	}

	try
	{
		Song song = convert_file(argv[1]);
		generate_vgm(song, "");
	}
	catch (InputError& error)
	{
		std::cerr << error.what() << "\n";
		return -1;
	}
}
