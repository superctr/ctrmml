#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ctrmml.h"

void print_usage(char* exename)
{
	printf("ctrmml (pre-alpha version)\n");
	printf("(C) 2019 ian karlsson\n");
	printf("%s <input_file.mml>\n", exename);
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

int main(int argc, char* argv[])
{
	struct song* song;
	if(argc < 2)
	{
		print_usage(argv[0]);
		printf("need to specify input file\n");
		exit(-1);
	}

	song = song_create();
	song_open(song, argv[1]);
	song_finalize(song);
	md_create_vgm(song, output_filename(argv[1], ".vgm"));
	song_free(song);

	return 0;
}
