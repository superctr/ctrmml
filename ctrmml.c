#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "ctrmml.h"

void print_usage(char* exename)
{
	printf("%s <input_file.mml>\n", exename);
}

int main(int argc, char* argv[])
{
	struct song* song;
	if(argc < 2)
	{
		print_usage(argv[0]);
		printf("need to specify input file\n");
	}

	song = song_convert_mml(argv[1]);
	return 0;
}

