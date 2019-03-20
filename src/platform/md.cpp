#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <cctype>

#include "md.h"
#include "../song.h"
#include "../input.h"
#include "../stringf.h"

int MD_Data::add_unique_data(const std::vector<uint8_t>& data)
{
	unsigned int i;
	// look for previous matching data in the data bank
	for(i=0; i<data_bank.size(); i++)
	{
		if(data != data_bank[i])
			continue;
		return i;
	}
	i = data_bank.size();
	if(i >= data_count_max)
	{
		throw InputError(nullptr, "error: maximum amount of data table entries reached\n");
	}
	data_bank.push_back(data);
	return i;
}

//! adds a 4op FM instrument. Data stored in operator order.
void MD_Data::read_fm_4op(uint16_t id, const Tag& tag)
{
	std::vector<uint8_t> fm_data(29, 0);
	std::vector<uint8_t> tag_data(42, 0);
	auto it = tag.begin();
	for(int i=0; i<42; i++)
	{
		if(it == tag.end())
			throw InputError(nullptr, stringf("error: not enough parameters for fm instrument @%d", id).c_str());
		tag_data[i] = std::strtol(it->c_str(), NULL, 0);
		it++;
	}

	for(int i=0; i<4; i++)
	{
		int op = 2;

		// physical operator order is 1,3,2,4
		if(i & 2)
			op += 10;
		if(i & 1)
			op += 20;
		// DT,MUL
		fm_data[0 + i] = (tag_data[op + 8] << 4) | (tag_data[op + 7] & 15);
		// TL
		fm_data[4 + i] = tag_data[op + 5];
		// KS/AR
		fm_data[8 + i] = (tag_data[op + 6] << 6) | (tag_data[op + 0] & 31);
		// AM/DR (AM sensitivity not supported yet)
		fm_data[12 + i] = (tag_data[op + 1] & 31);
		// SR
		fm_data[16 + i] = (tag_data[op + 2] & 31);
		// SL/RR
		fm_data[20 + i] = (tag_data[op + 4] << 4) | (tag_data[op + 3] & 15);
		// SSG-EG
		fm_data[24 + i] = tag_data[op + 9] & 15;
	}
	// FB/ALG
	fm_data[28] = (tag_data[0] & 7) | (tag_data[1] << 3);
	envelope_map[id] = add_unique_data(fm_data);
	ins_transpose[id] = 0;
	ins_type[id] = INS_FM;
}

//! Read 2op instrument definition.
/*!
 * Tag format: InsID,Mul1,Mul2,Mul3,Mul4,Transpose
 */
void MD_Data::read_fm_2op(uint16_t id, const Tag& tag)
{
	std::vector<uint8_t> fm_data(29, 0);
	std::vector<uint8_t> tag_data(6, 0);

	auto it = tag.begin();
	for(int i=0; i<6; i++)
	{
		if(it == tag.end())
			throw InputError(nullptr, stringf("error: not enough parameters for 2op fm instrument @%d", id).c_str());
		tag_data[i] = std::strtol(it->c_str(), NULL, 0);
		it++;
	}

	int ins_id = tag_data[0];
	try
	{
		fm_data = data_bank.at(envelope_map.at(ins_id));
		for(int i=0; i<4; i++)
		{
			uint8_t mul = tag_data[1 + ((i&1)<<1) + ((i&2)>>1)];
			uint8_t dt = fm_data[0+i] & 0xf0;
			fm_data[0+i] = dt | (mul & 15);
		}
		fm_data[4 + 3] = fm_data[4 + 2]; //op4 tl should be same as op1
		envelope_map[id] = add_unique_data(fm_data);
		ins_transpose[id] = tag_data[5];
		ins_type[id] = INS_FM;
	}
	catch(std::exception&)
	{
		throw InputError(nullptr, stringf("2op ins @%d is referencing instrument @%d which does not exist\n", id, ins_id).c_str());
	}
}

//! Read PSG volume envelope.
/*!
 * Format:
 *   Value or Value:Length or | or /
 * Output format:
 *   00 - end
 *   01 - sustain
 *   02 <pos> - loop
 *   fv - value <v> for <f> frames.
 */
void MD_Data::read_psg(uint16_t id, const Tag& tag)
{
	std::vector<uint8_t> env_data;
	int loop_pos = -1, last_pos = 0, last = -1;
	if(!tag.size())
	{
		std::cerr << stringf("warning: empty psg instrument @%d\n", id);
		return;
	}
	for(auto it = tag.begin(); it != tag.end(); it++)
	{
		const char* s = it->c_str();
		if(*s == '|')
			loop_pos = env_data.size();
		else if(*s == '/')
		{
			// insert sustain
			if(last == -1)
			{
				// max volume
				env_data.push_back(0x10);
			}
			env_data.push_back(0x01);
			last = -1;
		}
		else if(std::isdigit(*s))
		{
			uint8_t initial = std::strtol(s, (char**)&s, 0);
			uint8_t final = initial;
			uint8_t length = 0;
			double delta = 0, counter = 0;
			if(*s == '>' && *++s)
				final = strtol(s, (char**)&s, 0);
			final = (final > 15) ? 15 : (final < 0) ? 0 : final; // bounds check
			initial = (initial > 15) ? 15 : (initial < 0) ? 0 : initial;
			if(final != initial)
				length = std::abs(final-initial)+1;
			if(*s == ':' && *++s)
				length = strtol(s, (char**)&s, 0);
			if(length == 0)
				length++;
			else if(length > 1) // calculate slide
				delta = (double)(final-initial)/(length-1);
			counter = initial + 0.5;
			while(length--)
			{
				// last value is always the final
				uint8_t val = (length) ? (int)counter : final;
				// add to duration of previous value if it's the same
				if((int)counter == last && env_data[last_pos] < 0xf0)
					env_data[last_pos] += 0x10;
				else
				{
					last_pos = env_data.size();
					env_data.push_back(0x1f - val);
				}
				last = val;
				counter += delta;
			}
		}
		else
		{
			throw InputError(nullptr,stringf("undefined envelope value '%s'", s).c_str());
		}
	}
	if(loop_pos == -1)
	{
		// end command
		env_data.push_back(0x00);
	}
	else
	{
		// loop command
		env_data.push_back(0x02);
		env_data.push_back(loop_pos);
	}
	envelope_map[id] = add_unique_data(env_data);
	ins_transpose[id] = 0;
	ins_type[id] = INS_PSG;
}

std::string MD_Data::dump_data(uint16_t id)
{
	int mapped_id = envelope_map[id];
	std::string out = stringf("%d = %d [%d]{", id, mapped_id, data_bank[mapped_id].size());
	for(auto it = data_bank[mapped_id].begin(); it != data_bank[mapped_id].end(); it++)
	{
		if(it != data_bank[mapped_id].begin())
		{
			out += ", ";
		}
		out += stringf("%02x", *it);
	}
	out += "}";
	return out;
}

void MD_Data::read_envelope(uint16_t id, const Tag& tag)
{
	auto it = tag.begin();
	std::string type = *it++;
	if(iequal("psg", type))
	{
		read_psg(id, Tag(it, tag.end()));
		std::cout << "read PSG envelope " << dump_data(id) << "\n";
		return;
	}
	else if(iequal("fm", type))
	{
		read_fm_4op(id, Tag(it, tag.end()));
		std::cout << "read FM envelope " << dump_data(id) << "\n";
		return;
	}
	else if(iequal("2op", type))
	{
		read_fm_2op(id, Tag(it, tag.end()));
		std::cout << "read 2op envelope " << dump_data(id) << "\n";
		return;
	}
	else
	{
		throw InputError(nullptr, stringf("unknown envelope type %s\n",type.c_str()).c_str());
	}
}

void MD_Data::read_song(Song& song)
{
	// clear envelope and instrument maps
	envelope_map.clear();
	ins_transpose.clear();
	ins_type.clear();
	// add a unique psg envelope to prevent possible errors
	envelope_map[0] = add_unique_data({0x10, 0x01, 0x1f, 0x00});
	ins_transpose[0] = 0;
	ins_type[0] = MD_Data::INS_UNDEFINED;
	for(auto it = song.get_tag_map().begin(); it != song.get_tag_map().end(); it++)
	{
		uint16_t id;
		if(std::sscanf(it->first.c_str(), "@%hu", &id) == 1)
		{
			read_envelope(id, it->second);
		}
	}
}

MD_Channel::MD_Channel(Song& song, int id)
	: Player(song, song.get_track(id))
{

}

void MD_Channel::write_event()
{

}

MD_Driver::MD_Driver(unsigned int rate, VGM_Writer* vgm, bool is_pal)
	: Driver(rate, vgm), tempo_delta(0), tempo_counter(0)
{
	seq_rate = rate/60.0;
	pcm_rate = rate/16000.0;
}

//! Initiate playback
void MD_Driver::play_song(Song& song)
{
	data.read_song(song);
}

//! Reset sound chips, etc.
void MD_Driver::reset()
{
}

//! Return delta until the next event.
double MD_Driver::play_step()
{
	if(seq_delta < seq_rate)
	{
		// update tracks
		seq_delta += seq_rate;
	}
	if(pcm_delta < pcm_rate)
	{
		// update pcm
		pcm_delta += pcm_rate;
	}
	// get the time to the next event
	if(seq_delta < pcm_delta)
	{
		pcm_delta -= seq_delta;
		return seq_delta;
	}
	else
	{
		seq_delta -= pcm_delta;
		return pcm_delta;
	}
}

