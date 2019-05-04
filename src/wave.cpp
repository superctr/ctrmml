#include "wave.h"
#include "vgm.h"
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Wave_File::Wave_File(uint16_t channels, uint32_t rate, uint16_t bits)
	: channels(channels), sbits(bits), srate(rate)
{
	stype = 1;
	step = channels*(bits/8);
}

Wave_File::~Wave_File()
{
	return;
}

int Wave_File::load_file(const std::string& filename, uint8_t** buffer, uint32_t* filesize)
{
	if(std::ifstream is{filename, std::ios::binary|std::ios::ate})
	{
		auto size = is.tellg();
		*filesize = size;
		*buffer = (uint8_t*)calloc(1,size);
		is.seekg(0);
		if(is.read((char*)*buffer, size))
			return 0;
	}
	return -1;
}

uint32_t Wave_File::parse_chunk(const uint8_t *fdata)
{
	uint32_t chunkid = *(uint32_t*)(fdata);
	uint32_t chunksize = *(uint32_t*)(fdata+4);
	printf("Parse the %c%c%c%c chunk with %d size\n", fdata[0],fdata[1],fdata[2],fdata[3], chunksize);
	uint32_t pos = 0;
	switch(chunkid)
	{
		default:
			break;
		case 0x20746d66: // 'fmt '
			if(chunksize < 0x10)
				return 0;
			stype = *(uint16_t*)(fdata+0x08);
			channels = *(uint16_t*)(fdata+0x0a);
			sbits = *(uint16_t*)(fdata+0x16);
			step = (sbits * channels) / 8;
			srate = *(uint32_t*)(fdata+0x0c);
			slength = 0;
			if(stype != 1 || channels > 2 || step == 0)
			{
				fprintf(stderr,"unsupported format\n");
				return 0;
			}
			break;
		case 0x61746164: // 'data'
			if(step == 0)
				return 0;
			for(const uint8_t* d = fdata+8; d < (fdata+8+chunksize);)
			{
				for(int ch=0; ch<channels; ch++)
				{
					if(sbits == 8)
					{
						data[ch][pos] = (*d ^ 0x80) << 8;
						d++;
					}
					else if(sbits == 16)
					{
						data[ch][pos] = *(int16_t*)d;
						d += 2;
					}
				}
				pos++;
			}
			slength = pos;
			break;
		case 0x6c706d73: // 'smpl'
			use_smpl_chunk = 1;
			if(chunksize >= 0x10)
			{
				transpose = *(uint32_t*)(fdata+0x14);
				if(!transpose)
					transpose -= 60;
			}
			if(chunksize >= 0x2c && *(uint32_t*)(fdata+0x24))
			{
				lstart = *(uint32_t*)(fdata+0x2c+8);
				lend = *(uint32_t*)(fdata+0x2c+12) + 1;
			}
			break;
	}
	return chunksize;
}

int Wave_File::read(const std::string& filename)
{
	uint8_t* filebuf;
	uint32_t filesize, pos=0, wavesize;
	channels = 0;

	if(load_file(filename,&filebuf,&filesize))
	{
		return -1;
	}
	if(filesize < 13)
	{
		fprintf(stderr,"Malformed wav file '%s'\n", filename.c_str());
		return -1;
	}
	if(memcmp(&filebuf[0],"RIFF",4))
	{
		fprintf(stderr,"Riff header not found in '%s'\n", filename.c_str());
		return -1;
	}
	wavesize = (*(uint32_t*)(filebuf+4)) + 8;
	pos += 8;
	if(filesize != wavesize)
	{
		fprintf(stderr,"Warning: reported file size and actual file size do not match.\n"
				"Reported %d, actual %d\n", wavesize, filesize);

	}
	if(memcmp(&filebuf[pos],"WAVE",4))
	{
		fprintf(stderr,"'%s' is not a WAVE format file.\n", filename.c_str());
		return -1;
	}
	pos += 4;
	while(pos < wavesize)
	{
		uint32_t chunksize, ret;
		if(pos & 1)
			pos++;

		chunksize = *(uint32_t*)(filebuf+pos+4) + 8;
		if(pos+chunksize > filesize)
		{
			printf("Illegal chunk size (%d, %d)\n", pos+chunksize+8, filesize);
			return -1;
		}
		ret = parse_chunk(filebuf+pos);
		if(!ret)
		{
			printf("Failed to parse chunk %c%c%c%c.\n",filebuf[pos],filebuf[pos+1],filebuf[pos+2],filebuf[pos+3]);
			return -1;
		}
		pos += chunksize;
	}
	free(filebuf);
	return 0;
}


Wave_Rom::Wave_Rom(unsigned long max_size)
	: max_size(max_size)
{
}

Wave_Rom::~Wave_Rom()
{
	return;
}
