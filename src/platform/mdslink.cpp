#include "../song.h"
#include "../input.h"
#include "../mml_input.h"
#include "../stringf.h"
#include "mdsdrv.h"

#include <iostream>
#include <fstream>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage(const char* exename)
{
	std::cout << "mdslink - MDSDRV Linker and MML Compiler\n";
	std::cout << "(C) 2019-2020 ian karlsson\n\n";
	std::cout << "Usage: " << exename << " [options] <list of input files ...>\n\n";
	std::cout << "Options:\n";
	std::cout << "\t-o <mdsseq.bin> <mdsbin.bin> : Specify output filenames\n";
	std::cout << "Note:\n";
	std::cout << "\tInput files can be in .mml or .mds format\n";
}

std::string get_extension(const char* input_filename)
{
	static char str[256];
	char *last_dot;
	strncpy(str,input_filename, 256);
	last_dot = strrchr(str, '.');
	if(last_dot)
		return last_dot;
	else
		return "";
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
	auto input = std::vector<std::string>();
	std::string seq_filename = "mdsseq.bin";
	std::string pcm_filename = "mdspcm.bin";

	for(int arg = 1; arg < argc; arg++)
	{
		if((!strcmp(argv[arg], "-o") || !strcmp(argv[arg], "--output")) && (arg+1) < argc)
		{
			seq_filename = argv[++arg];
			pcm_filename = argv[++arg];
		}
		else
			input.push_back(argv[arg]);
	}

	if(!input.size())
	{
		print_usage(argv[0]);
		std::cerr << "no input specified\n";
		return -1;
	}

	try
	{
		auto linker = MDSDRV_Linker();
		for(auto it = input.begin(); it != input.end(); it++)
		{
			auto extension = get_extension(it->c_str());
			RIFF mds = RIFF(0);
			printf("[%ld/%ld] %s\n", 1+it-input.begin(), input.size(), it->c_str());
			if(iequal(extension, ".mds"))
			{
				if (std::ifstream in{*it, std::ios::binary | std::ios::ate})
				{
					auto size = in.tellg();
					auto data = std::vector<uint8_t>(size, 0);
					in.seekg(0);
					if(in.read((char*)&data[0], size))
					{
						mds = RIFF(data);
					}
					else
					{
						throw InputError(nullptr, stringf("Couldn't read %s", it->c_str()).c_str());
					}
				}
				else
				{
					throw InputError(nullptr, stringf("Couldn't open %s", it->c_str()).c_str());
				}
			}
			else
			{
				auto song = convert_file(it->c_str());
				auto converter = MDSDRV_Converter(song);
				mds = converter.get_mds();
			}
			// pass to linker
			linker.add_song(mds);
		}
		if(seq_filename.size())
		{
			printf("writing %s ...\n", seq_filename.c_str());
			auto bytes = linker.get_seq_data();
			std::ofstream out(seq_filename, std::ios::binary);
			out.write((char*)bytes.data(), bytes.size());
		}
		if(pcm_filename.size())
		{
			printf("writing %s ...\n", pcm_filename.c_str());
			auto bytes = linker.get_pcm_data();
			std::ofstream out(pcm_filename, std::ios::binary);
			out.write((char*)bytes.data(), bytes.size());
			std::cout << linker.get_statistics();
		}
		return 0;
	}
	catch (InputError& error)
	{
		std::cerr << error.what() << "\n";
		return -1;
	}
}
