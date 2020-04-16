/*
	This code is a bit ugly and should be refactored when I have the time...
	2020-04-11: This code has now been slightly refactored. It is however still a little bit ugly.
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
#include "util.h"

Wave_File::Wave_File(uint16_t channels, uint32_t rate, uint16_t bits)
	: channels(channels)
	, sbits(bits)
	, srate(rate)
	, use_smpl_chunk(0)
	, transpose(0)
	, lstart(0)
	, lend(0)
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

Wave_Rom::Wave_Rom(unsigned long max_size, unsigned long bank_size)
	: max_size(max_size)
	, current_size(0)
	, bank_size(bank_size)
	, include_paths{""}
	, rom_data()
	, gaps()
{
	rom_data.resize(max_size, 0);
	if(!bank_size)
		this->bank_size = max_size;
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

//! Returns the next possible aligned start address for the rom.
/*!
 *  Given a sample header, a proposed start address and end address,
 *  return the appropriate end address of the sample. If the sample
 *  cannot fit within the boundaries. return NO_FIT.
 */
uint32_t Wave_Rom::fit_sample(const Wave_Rom::Sample& header, uint32_t start, uint32_t end) const
{
	uint32_t sample_end = start + header.size;
	uint32_t start_bank = start / bank_size;
	uint32_t end_bank = sample_end / bank_size;
	// Sample must fit in the same bank
	if (start_bank != end_bank)
		start = end_bank * bank_size;
	// Crossed end boundary?
	if ((start + header.size) > end)
		return NO_FIT;
	return start;
}

//! Look for duplicates in the sample ROM.
/*!
 *  If a duplicate is found, return the index to the duplicate wave entry.
 *  Otherwise, return -1.
 */
int Wave_Rom::find_duplicate(const Wave_Rom::Sample& header, const std::vector<uint8_t>& sample) const
{
	int id = 0;
	for(auto&& i : samples)
	{
		// The reason for the loop start check is that some sound chips (like C352)
		// require that the looping part of the sample fit in the same bank.
		if(   i.position + sample.size() <= rom_data.size()
		   && i.loop_start <= header.loop_start
		   && !memcmp(&sample[0], &rom_data[i.position], sample.size()))
			return id;
		id++;
	}
	return -1;
}

//! Set a list of include paths to check when reading samples from a Tag.
void Wave_Rom::set_include_paths(const Tag& tag)
{
	include_paths = tag;
}

//! Check if the sample fits in an existing alignment gap
/*!
 *  If the sample cannot fit in any gap, return NO_FIT. Otherwise, return
 *  the index of the smallest gap that fits the sample.
 *
 *  \p gap_start will be set with the aligned start position of the gap.
 */
unsigned int Wave_Rom::find_gap(const Wave_Rom::Sample& header, uint32_t& gap_start) const
{
	unsigned int best_gap = NO_FIT;
	if(gaps.size())
	{
		// Look for the smallest gap that fits our sample
		int best_gap_size = max_size;
		uint32_t start_pos;
		for(unsigned int i = 0; i < gaps.size(); i++)
		{
			int gap_size = gaps[i].end - gaps[i].start;
			start_pos = fit_sample(header, gaps[i].start, gaps[i].end);
			if(start_pos != NO_FIT && gap_size < best_gap_size)
			{
				best_gap = i;
				best_gap_size = gap_size;
				gap_start = start_pos;
			}
		}
	}
	return best_gap;
}
//! Quick and dirty comparison operator (used below)
static bool operator==(const Wave_Rom::Sample& s1, const Wave_Rom::Sample& s2)
{
	// Can't directly compare structs properly, so we convert to bytes instead
	// and then compare the arrays.
	return s1.to_bytes() == s2.to_bytes();
}

//! Convert and add sample to the waverom.
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
		std::string fn = i + filename;
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
	Wave_Rom::Sample header = {
		0,
		0,
		wf.slength,
		wf.lstart,
		wf.lend,
		wf.srate,
		wf.transpose,
		0};

	return add_sample(header, sample);
};

//! Add sample to the waverom in raw format.
unsigned int Wave_Rom::add_sample(Wave_Rom::Sample header, const std::vector<uint8_t>& sample)
{
	// Find duplicates of sample data and selected header parameters if needed
	int duplicate = find_duplicate(header, sample);

	if(duplicate != -1)
	{
		header.position = samples[duplicate].position;
		auto result = std::find(samples.begin(), samples.end(), header);
		if(result != samples.end())
		{
			// Header is similar to an existing one, we reuse it
			return result - samples.begin();
		}
		else
		{
			// Create a new header, while using the same sample data
			samples.push_back(header);
			return samples.size() - 1;
		}
	}
	else
	{
		// Create a new entry.
		uint32_t start_pos = -1; // Aligned start position
		uint32_t start = current_size; // Proposed start position
		// Check if the sample fits in a gap.
		unsigned int gap_id = find_gap(header, start_pos);
		if(gap_id != NO_FIT)
		{
			start = gaps[gap_id].start;
			gaps[gap_id].start = start_pos + header.size;
		}
		else
		{
			// Append sample to the end
			start_pos = fit_sample(header, start, max_size);
		}
		// Check if sample fits in ROM
		if(start_pos == NO_FIT)
		{
			error_message = stringf("Sample does not fit in remaining ROM space (%d bytes remaining, sample size is %d)", max_size - start_pos, sample.size());
			throw InputError(nullptr, error_message.c_str());
		}
		// Add a new gap if needed
		if(start_pos > start)
		{
			gaps.push_back({start, start_pos});
		}
		// Move the end position if needed
		if(start_pos >= current_size)
		{
			current_size = start_pos + header.size;
		}

		printf("Append sample %d to ROM at %08x (size %08x)\n", samples.size(), start_pos, header.size);
		std::copy_n(sample.begin(), header.size, rom_data.begin() + start_pos);
		header.position = start_pos;
		samples.push_back(header);
		return samples.size() - 1;
	}
}

//! Get the number of unused allocated bytes in the Wave_Rom.
unsigned int Wave_Rom::get_free_bytes()
{
	return max_size - current_size;
}

//! Get the total size of alignment gaps.
unsigned int Wave_Rom::get_total_gap()
{
	unsigned int gap_size = 0;
	for(auto&& gap : gaps)
	{
		gap_size += gap.end - gap.start;
	}
	return gap_size;
}

//! Get the size of the largest gap.
/*!
 *  If there are no gaps, return 0.
 */
unsigned int Wave_Rom::get_largest_gap()
{
	unsigned int largest_gap = 0;
	for(auto&& gap : gaps)
	{
		unsigned int gap_size = gap.end - gap.start;
		if(gap_size > largest_gap)
			largest_gap = gap_size;
	}
	return largest_gap;
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

//! Fill a sample header with values from a byte vector.
/*!
 *  The format matches the data created by to_bytes().
 *
 *  \exception std::out_of_range Input too small
 */
void Wave_Rom::Sample::from_bytes(std::vector<uint8_t> input)
{
	position = read_le32(input, 0);
	start = read_le32(input, 4);
	size = read_le32(input, 8);
	loop_start = read_le32(input, 12);
	loop_end = read_le32(input, 16);
	rate = read_le32(input, 20);
	transpose = read_le32(input, 24);
	flags = read_le32(input, 28);
}

//! Return a sample header as a byte vector.
/*!
 *  Output can be made as a header again using to_bytes().
 */
std::vector<uint8_t> Wave_Rom::Sample::to_bytes() const
{
	auto output = std::vector<uint8_t>();
	write_le32(output, 0, position);
	write_le32(output, 4, start);
	write_le32(output, 8, size);
	write_le32(output, 12, loop_start);
	write_le32(output, 16, loop_end);
	write_le32(output, 20, rate);
	write_le32(output, 24, transpose);
	write_le32(output, 28, flags); // Reserved.
	return output;
}
