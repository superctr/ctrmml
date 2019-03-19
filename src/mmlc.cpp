#include "song.h"
#include "input.h"
#include "mml_input.h"

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

char* output_filename(char* input_filename, char* extension)
{
	static char str[256];
	char *last_dot;
	strncpy(str,input_filename, 256);
	last_dot = strrchr(str, '.');
	if(last_dot)
		*last_dot = 0;
	strncat(last_dot, extension, 256);
	return strdup(str);
}

Song convert_file(const char* filename)
{
	Song song;
	MML_Input input = MML_Input(&song);
	input.open_file(filename);
	return song;
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
		convert_file(argv[1]);
	}
	catch (InputError& error)
	{
		std::cerr << "input error: " << error.what();
		return -1;
	}
}

