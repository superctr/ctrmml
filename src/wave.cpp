/*
	This code is a bit ugly and should be refactored when I have the time...
*/
#include <iostream>
#include <fstream>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wave.h"
#include "input.h"
#include "vgm.h"
#include "stringf.h"

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
	//printf("Parse the %c%c%c%c chunk with %d size\n", fdata[0],fdata[1],fdata[2],fdata[3], chunksize);
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
			if(data.size() != channels)
				data.resize(channels);
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
						data[ch].push_back((*d ^ 0x80) << 8);
						d++;
					}
					else if(sbits == 16)
					{
						data[ch].push_back(*(int16_t*)d);
						d += 2;
					}
				}
				pos++;
			}
			slength = data[0].size();
			lstart = 0;
			lend = 0;
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
				slength = lend;
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
	, current_size(0)
	, include_paths{""}
	, rom_data()
	, gaps()
{
	rom_data.resize(max_size, 0);
}

Wave_Rom::~Wave_Rom()
{
	return;
}

//! Encode the sample, convert it from 16-bit data to 8-bit.
std::vector<uint8_t> Wave_Rom::encode_sample(const std::string& encoding_type, const std::vector<int16_t>& input)
{
	// default encoder, simply convert 16-bit to 8-bit unsigned
	std::vector<uint8_t> output;
	for(auto&& i : input)
	{
		output.push_back((i >> 8) ^ 0x80);
	}
	return output;
}

//! This function simply returns the next possible start address for the rom.
uint32_t Wave_Rom::fit_sample(unsigned long loop_start, unsigned long sample_size)
{
	return current_size;
}

void Wave_Rom::set_include_paths(const Tag& tag)
{
	include_paths = tag;
}

//! Add sample to the waverom.
unsigned int Wave_Rom::add_sample(const Tag& tag)
{
	int status = -1;
	if(!tag.size())
	{
		error_message = "Incomplete sample definition";
		throw InputError(nullptr, error_message.c_str());
	}
	std::string filename = tag[0];
	Wave_File wf;
	for(auto&& i : include_paths)
	{
		std::string fn = i + "/" + filename;
		//std::cout << "attempt to load " << fn << "\n";
		status = wf.read(fn);
		if(status == 0)
			break;
	}
	if(status)
	{
		error_message = filename + " not found";
		throw InputError(nullptr, error_message.c_str());
	}

	// convert sample
	std::vector<uint8_t> sample = encode_sample("", wf.data[0]);
	unsigned long start_pos = fit_sample(wf.lstart, sample.size());

	// TODO: we should call fit_sample here instead
	if((start_pos + sample.size()) > max_size)
	{
		error_message = "Sample does not fit in remaining ROM size";
		throw InputError(nullptr, error_message.c_str());
	}
	current_size = start_pos + sample.size();

	std::copy_n(sample.begin(),wf.slength, rom_data.begin() + start_pos);
	samples.push_back({
		start_pos,
		wf.slength,
		wf.lstart,
		wf.lend,
		wf.srate,
		wf.transpose});

	return samples.size() - 1;
}

unsigned int Wave_Rom::get_free_bytes()
{
	return max_size - current_size;
}

const std::vector<Wave_Rom::Sample>& Wave_Rom::get_sample_headers()
{
	return samples;
}

const std::vector<uint8_t>& Wave_Rom::get_rom_data()
{
	return rom_data;
}

const std::string& Wave_Rom::get_error()
{
	return error_message;
}
