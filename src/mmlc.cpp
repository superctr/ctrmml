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
#include <stdexcept>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage(const char* exename)
{
	std::cout << "ctrmml Music Compiler, version " CTRMML_VERSION "\n";
	std::cout << "(C) 2019-2020 Ian Karlsson.\n";
	std::cout << "Licensed under GPLv2, see COPYING for details.\n\n";
	std::cout << "Usage: " << exename << " [options] <input_file.mml>\n";
	std::cout << "Options:\n";
	std::cout << "\t--output / -o <filename> : Set output filename\n";
	std::cout << "\t--format / -f <format> : Set output file format\n";
}

std::string get_extension(const char* input_filename)
{
	static char str[256];
	char *last_dot;
	strncpy(str,input_filename, 256);
	last_dot = strrchr(str, '.');
	if(last_dot && *last_dot == '.')
		return last_dot + 1;
	else
		return "";
}

std::string output_filename(const char* input_filename, const char* extension)
{
	static char str[256];
	char *last_dot;
	strncpy(str,input_filename, 256);
	last_dot = strrchr(str, '.');
	if(last_dot)
		*last_dot = 0;
	strncat(last_dot, ".", 256);
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

int main(int argc, char* argv[])
{
	std::string in_filename = "";
	std::string out_filename = "";
	std::string format = "";

	for(int arg = 1, default_arguments = 0; arg < argc; arg++)
	{
		if((!strcmp(argv[arg], "-o") || !strcmp(argv[arg], "--output")) && arg < argc)
			out_filename = argv[++arg];
		else if((!strcmp(argv[arg], "-f") || !strcmp(argv[arg], "--format")) && arg < argc)
			format = argv[++arg];
		else if((!strcmp(argv[arg], "-h") || !strcmp(argv[arg], "--help")) && arg < argc)
		{
			print_usage(argv[0]);
			return -1;
		}
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
		// Parse MML
		Song song = convert_file(in_filename.c_str());

		// Get available formats
		unsigned int format_id = 0;
		auto format_list = song.get_platform()->get_export_formats();

		// Find matching format
		for(auto&& i : format_list)
		{
			if(format == "")
				format = i.first;
			if(iequal(i.first, format))
				break;
			format_id ++;
		}

		// No available format
		if(format_id == format_list.size())
		{
			std::cerr << "Format not available!\n";
			if(format_list.size())
			{
				std::cerr << "\nAvailable formats:\n";
				for(auto&& i : format_list)
					std::cerr << "\t'" << i.first << "': " << i.second << "\n";
			}
			return -1;
		}

		// Generate output filename if not already specified
		if(!out_filename.size())
			out_filename = output_filename(in_filename.c_str(), format.c_str());

		// Export data
		std::vector<uint8_t> bytes = song.get_platform()->get_export_data(song, format_id);

		// Write to file
		if(bytes.size())
		{
			std::ofstream out(out_filename, std::ios::binary);
			out.write((char*)bytes.data(), bytes.size());
			std::cout << "Wrote " << bytes.size() << " bytes to " << out_filename << "\n";
		}
	}
	catch (InputError& error)
	{
		std::cerr << error.what() << "\n";
		return -1;
	}
	catch (std::logic_error& error) // in case get_driver is not found
	{
		std::cerr << error.what() << "\n";
		return -1;
	}
}

