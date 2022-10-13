#include <iostream>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <cctype>
#include <cmath>
#include <stack>

#include "md.h"
#include "mdsdrv.h"
#include "../song.h"
#include "../input.h"
#include "../stringf.h"
#include "../riff.h"
#include "../util.h"

//! Lookup register name in str and return the address, or 0 if invalid
uint8_t MDSDRV_get_register(const std::string& str)
{
	std::string s(str);
	std::transform(str.begin(), str.end(), s.begin(), [](uint8_t c){return std::tolower(c);});
	static const std::map<std::string, uint8_t> lookup = {
		{"dtml1",0x30}, {"dtml2",0x38}, {"dtml3",0x34}, {"dtml4",0x3c},
		{"tl1",0xfc}, {"tl2",0xfe}, {"tl3",0xfd}, {"tl4",0xff},
		{"ksar1",0x50}, {"ksar2",0x58}, {"ksar3",0x54}, {"ksar4",0x5c},
		{"amdr1",0x60}, {"amdr2",0x68}, {"amdr3",0x64}, {"amdr4",0x6c},
		{"sr1",0x70}, {"sr2",0x78}, {"sr3",0x74}, {"sr4",0x7c},
		{"slrr1",0x80}, {"slrr2",0x88}, {"slrr3",0x84}, {"slrr4",0x8c},
		{"ssg1",0x90}, {"ssg2",0x98}, {"ssg3",0x94}, {"ssg4",0x9c},
		{"fbal",0xb0}
	};
	auto search = lookup.find(s);
	//printf("s = '%s', match=%d\n", s.c_str(), search != lookup.end());
	if(search != lookup.end())
		return search->second;
	return 0;
}

MDSDRV_Data::MDSDRV_Data()
	: use_extended_pitch(true)
	, data_bank()
	, wave_rom(0x200000)
	, envelope_map()
	, wave_map()
	, ins_transpose()
	, ins_type()
	, pitch_map()
	, pitch_extend()
	, message("")
{
}

//! Add all instruments and envelopes from a Song to the data bank.
void MDSDRV_Data::read_song(Song& song)
{
	// clear envelope and instrument maps
	envelope_map.clear();
	ins_transpose.clear();
	pitch_map.clear();
	wave_map.clear();
	ins_type.clear();
	try
	{
		// just do this if we have this tag
		wave_rom.set_include_paths(song.get_tag("include_path"));
	}
	catch(std::exception &)
	{
	}
	// add a unique psg envelope to prevent possible errors
	envelope_map[0] = add_unique_data({0x10, 0x01, 0x1f, 0x00});
	ins_transpose[0] = 0;
	ins_type[0] = MDSDRV_Data::INS_UNDEFINED;
	Tag& tag_order = song.get_tag_order_list();

	if(song.check_tag("#option"))
	{
		Tag& tag = song.get_tag("#option");
		if(std::find(tag.begin(), tag.end(), "noextpitch") != tag.end())
		{
			use_extended_pitch = false;
		}
	}

	for(auto it = tag_order.begin(); it != tag_order.end(); it++)
	{
		uint16_t id;
		Tag& tag = song.get_tag(*it);
		if(std::sscanf(it->c_str(), "@%hu", &id) == 1)
		{
			add_instrument(id, tag);
		}
		else if(std::sscanf(it->c_str(), "@m%hu", &id) == 1)
		{
			try
			{
				add_pitch_envelope(id, tag);
				message += "read pitch envelope " + dump_data(id, pitch_map[id]) + "\n";
			}
			catch(std::invalid_argument &)
			{
				add_extended_pitch_envelope(id, tag);
				message += "read extended pitch envelope " + dump_data(id, pitch_map[id]) + "\n";
			}
		}
	}
}

//! Add an instrument to the data bank.
void MDSDRV_Data::add_instrument(uint16_t id, const Tag& tag)
{
	auto it = tag.begin();
	std::string type = *it++;
	if(iequal("fm", type))
	{
		add_ins_fm_4op(id, Tag(it, tag.end()));
		message += "read FM envelope " + dump_data(id, envelope_map[id]) + "\n";
		return;
	}
	else if(iequal("2op", type))
	{
		add_ins_fm_2op(id, Tag(it, tag.end()));
		message += "read 2op envelope " + dump_data(id, envelope_map[id]) + "\n";
		return;
	}
	else if(iequal("psg", type))
	{
		add_ins_psg(id, Tag(it, tag.end()));
		message += "read PSG envelope " + dump_data(id, envelope_map[id]) + "\n";
		return;
	}
	else if(iequal("pcm", type))
	{
		add_ins_pcm(id, Tag(it, tag.end()));
		message += "read wave sample " + dump_data(id, envelope_map[id]) + "\n";
		return;
	}
	else
	{
		throw InputError(nullptr, stringf("unknown envelope type %s\n",type.c_str()).c_str());
	}
}



//! Add 4op FM instrument. Data stored in operator order.
void MDSDRV_Data::add_ins_fm_4op(uint16_t id, const Tag& tag)
{
	std::vector<uint8_t> fm_data(30, 0);
	std::vector<uint8_t> tag_data(42, 0);
	auto it = tag.begin();
	for(int i=0; i<42; i++)
	{
		if(it == tag.end())
			throw InputError(nullptr, stringf("error: not enough parameters for fm instrument @%d", id).c_str());
		tag_data[i] = std::strtol(it->c_str(), NULL, 10);
		it++;
	}

	// Transpose
	if(it != tag.end())
		fm_data[29] = (std::strtol(it->c_str(), NULL, 10) + 24) << 1;
	else
		fm_data[29] = 24 << 1;

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
		// KS/AR
		fm_data[4 + i] = (tag_data[op + 6] << 6) | (tag_data[op + 0] & 31);
		// AM/DR
		fm_data[8 + i] = (tag_data[op + 1] & 31);
		// SR
		fm_data[12 + i] = (tag_data[op + 2] & 31);
		// SL/RR
		fm_data[16 + i] = (tag_data[op + 4] << 4) | (tag_data[op + 3] & 15);
		// SSG-EG
		fm_data[20 + i] = (tag_data[op + 9] % 100) & 15;
		// TL
		fm_data[24 + i] = tag_data[op + 5];
		// AM sensitivity = set SSG-EG to 100
		if(tag_data[op + 9] >= 100)
			fm_data[8 + i] |= 0x80;
	}
	// FB/ALG
	fm_data[28] = (tag_data[0] & 7) | (tag_data[1] << 3);
	envelope_map[id] = add_unique_data(fm_data);
	ins_transpose[id] = (fm_data[29] >> 1) - 24;
	ins_type[id] = INS_FM;
}

//! Add 2op FM instrument definition.
/*!
 * Tag format: InsID,Mul1,Mul2,Mul3,Mul4,Transpose
 */
void MDSDRV_Data::add_ins_fm_2op(uint16_t id, const Tag& tag)
{
	std::vector<uint8_t> fm_data(30, 0);
	std::vector<uint8_t> tag_data(6, 0);

	auto it = tag.begin();
	for(int i=0; i<6; i++)
	{
		if(it == tag.end())
			throw InputError(nullptr, stringf("error: not enough parameters for 2op fm instrument @%d", id).c_str());
		tag_data[i] = std::strtol(it->c_str(), NULL, 10);
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
		fm_data[24 + 3] = fm_data[24 + 2]; //op4 tl should be same as op1
		fm_data[29] = (tag_data[5] + 24) << 1;
		envelope_map[id] = add_unique_data(fm_data);
		ins_transpose[id] = tag_data[5];
		ins_type[id] = INS_FM;
	}
	catch(std::exception&)
	{
		throw InputError(nullptr, stringf("2op ins @%d is referencing instrument @%d which does not exist\n", id, ins_id).c_str());
	}
}

//! Add PSG volume envelope.
/*!
 * Envelope input format:
 *   Value
 *   Value:Length
 *   Value>Target - slide
 *   Value>Target:Length - slide
 *   | - set loop position
 *   / - wait until keyoff
 *   l:Length - default length
 * Output format:
 *   00 - end
 *   01 - sustain
 *   02 pp - loop to <p>
 *   fv - value <v> for <f> frames.
 */
void MDSDRV_Data::add_ins_psg(uint16_t id, const Tag& tag)
{
	std::vector<uint8_t> env_data;
	int loop_pos = -1, last_pos = 0, last = -1;
	unsigned int default_len = 1;
	if(!tag.size())
	{
		message += stringf("warning: empty psg instrument @%d\n", id);
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
		else if(*s == 'l' && *++s == ':' && std::isdigit(*++s))
		{
			default_len = std::strtol(s, (char**)&s, 10);
		}
		else if(std::isdigit(*s))
		{
			// TODO: make my own strtol...
			uint8_t initial = std::strtol(s, (char**)&s, 10);
			uint8_t target = initial;
			uint8_t length = default_len;
			double delta = 0, counter = 0;

			if(*s == '>' && *++s)
				target = strtol(s, (char**)&s, 10);
			target = (target > 15) ? 15 : (target < 0) ? 0 : target; // bounds check
			initial = (initial > 15) ? 15 : (initial < 0) ? 0 : initial;

			if(target != initial)
				length = std::abs(target-initial)+1;
			if(*s == ':' && *++s)
				length = strtol(s, (char**)&s, 10);
			if(length == 0)
				length++;
			else if(length > 1) // calculate slide
				delta = (double)(target-initial)/(length-1);
			counter = initial + 0.5;
			while(length--)
			{
				// last value is always the slide target
				uint8_t val = (length) ? (int)counter : target;
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

//! Add PCM instrument
void MDSDRV_Data::add_ins_pcm(uint16_t id, const Tag& tag)
{
	std::vector<uint8_t> env_data;
	int wave_header_id;

	// Insert a generic 32-byte generic Wave_Bank::Sample header.
	wave_header_id = wave_map[id] = wave_rom.add_sample(tag);
	env_data = wave_rom.get_sample_headers().at(wave_header_id).to_bytes();

	envelope_map[id] = add_unique_data(env_data);
	ins_type[id] = INS_PCM;
}

//! Read pitch envelope
/*!
 * Envelope input format:
 *   Value
 *   Value:Length
 *   Value>Target - slide
 *   Value>Target:Length - slide
 *   V<Base>:<Depth>:<Rate> - vibrato macro, automatically sets loop position
 *   | - set loop position
 * Output format:
 *   hh ll dd ss - set pitch to <hl>, change it with <d> for <s> frames.
 *                 If <s> is ff = continue forever
 *   7F <pp> - loop to <p*4>
 */
void MDSDRV_Data::add_pitch_envelope(uint16_t id, const Tag& tag)
{
	std::vector<uint8_t> env_data;
	int loop_pos = -1;
	if(!tag.size())
	{
		message += stringf("warning: empty pitch envelope @M%d\n", id);
		return;
	}
	for(auto it = tag.begin(); it != tag.end(); it++)
	{
		const char* s = it->c_str();
		if(*s == '|')
			loop_pos = env_data.size() / 4;
		else if(std::isdigit(*s) || *s == '-')
			add_pitch_node(s, false, &env_data);
		else if(*s == 'V')
		{
			loop_pos = env_data.size() / 4;
			add_pitch_vibrato(s, false, &env_data);
		}
		else
			throw InputError(nullptr,stringf("undefined envelope value '%s'", s).c_str());
	}
	// end command
	if(loop_pos == -1)
	{
		env_data.back() = 0xff;
	}
	else
	{
		env_data.push_back(0x7f);
		env_data.push_back(loop_pos);
	}
	pitch_map[id] = add_unique_data(env_data);
}

void MDSDRV_Data::add_extended_pitch_envelope(uint16_t id, const Tag& tag)
{
	std::vector<uint8_t> env_data;
	int loop_pos = -1;
	if(!tag.size())
	{
		message += stringf("warning: empty pitch envelope @M%d\n", id);
		return;
	}
	for(auto it = tag.begin(); it != tag.end(); it++)
	{
		const char* s = it->c_str();
		if(*s == '|')
			loop_pos = env_data.size() / 6;
		else if(std::isdigit(*s) || *s == '-')
			add_pitch_node(s, true, &env_data);
		else if(*s == 'V')
		{
			loop_pos = env_data.size() / 6;
			add_pitch_vibrato(s, true, &env_data);
		}
		else
			throw InputError(nullptr,stringf("undefined envelope value '%s'", s).c_str());
	}
	// end command
	if(loop_pos == -1)
	{
		env_data[env_data.size() - 2] = 0xff;
		env_data[env_data.size() - 1] -= 1;
	}
	else
	{
		env_data.back() = loop_pos;
	}
	pitch_map[id] = add_unique_data(env_data);
	pitch_extend.insert(id);
}
//! Adds a node to the pitch envelope.
void MDSDRV_Data::add_pitch_node(const char* s, bool extend, std::vector<uint8_t>* env_data)
{
	double initial = std::strtod(s, (char**)&s);
	double target = initial;
	int length = 0;
	double counter = 0;

	if(*s == '>' && *++s)
		target = strtod(s, (char**)&s);

	if(target != initial)
		length = std::lround(std::fabs(target-initial)+1.5) >> 4;
	if(*s == ':' && *++s)
		length = strtol(s, (char**)&s, 10);
	if(length < 1)
		length += 1;

	counter = initial;
	while(length > 0)
	{
		double delta = (target-counter)/length;
		uint16_t env_len = (length > 255) ? 255 : length;
		int16_t env_initial = counter * 256;
		int16_t env_delta = std::trunc(delta * 256);
		env_initial = (env_initial > 0x7eff) ? 0x7eff : env_initial;
		if(extend)
		{
			env_data->push_back(env_initial >> 8);
			env_data->push_back(env_initial & 0xff);
			env_data->push_back(env_delta >> 8);
			env_data->push_back(env_delta & 0xff);
			env_data->push_back(env_len - 1);
			env_data->push_back((env_data->size() + 1) / 6);
		}
		else
		{
			//TODO:make it possible to disable extended pitch envelopes and use the below commented code instead
			if(!use_extended_pitch)
				env_delta = (env_delta > 127) ? 127 : (env_delta < -128) ? -128 : env_delta;
			else if(env_delta > 127 || env_delta < -128)
				throw std::invalid_argument("add_pitch_node");

			env_data->push_back(env_initial >> 8);
			env_data->push_back(env_initial & 0xff);
			env_data->push_back(env_delta);
			env_data->push_back(env_len - 1);
		}
		// apply delta and decrease length counter
		counter += (env_delta * env_len) / 256;
		length -= env_len;
	}
}

//! Adds a node to the pitch envelope.
void MDSDRV_Data::add_pitch_vibrato(const char* s, bool extend, std::vector<uint8_t>* env_data)
{
	// Vibrato macro
	double vibrato_base = 0;
	double vibrato_depth = 0.5;
	int vibrato_rate = 5;

	s++;
	if(std::isdigit(*s) || *s == '-')
		vibrato_base = strtod(s, (char**)&s);
	if(*s == ':' && *++s)
		vibrato_depth = strtod(s, (char**)&s) / 2.0;
	if(*s == ':' && *++s)
		vibrato_rate = strtol(s, (char**)&s, 10);
	else // TODO: throw an exception or warning
		message += stringf("Invalid vibrato definition: '%s'\n", s);

	vibrato_depth += vibrato_base;
	add_pitch_node(stringf("%f>%f:%d", vibrato_base, vibrato_depth, vibrato_rate).c_str(), extend, env_data);
	add_pitch_node(stringf("%f>%f:%d", vibrato_depth, -vibrato_depth, vibrato_rate*2).c_str(), extend, env_data);
	add_pitch_node(stringf("%f>%f:%d", -vibrato_depth, vibrato_base, vibrato_rate).c_str(), extend, env_data);
}

//! Add unique data to the data bank and return the index.
/*!
 *  In case of a duplicate, return the index of the previously added data.
 */
int MDSDRV_Data::add_unique_data(const std::vector<uint8_t>& data)
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

//! Dump data bank index to a string for debugging purposes.
std::string MDSDRV_Data::dump_data(uint16_t id, uint16_t mapped_id)
{
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

//=====================================================================

//! Converts a Track into MDSDRV_Event stream.
MDSDRV_Track_Writer::MDSDRV_Track_Writer(MDSDRV_Converter& mdsdrv,
		int id,
		bool in_drum_mode,
		bool drum_mode_enabled,
		std::vector<MDSDRV_Event>& converted_events)
	: Basic_Player(*mdsdrv.song, mdsdrv.song->get_track(id))
	, mdsdrv(mdsdrv)
	, converted_events(converted_events)
	, in_drum_mode(in_drum_mode)
	, drum_mode_enabled(drum_mode_enabled)
	, in_loop(0)
	, rest_time(0)
{
}

//! Event conversion
void MDSDRV_Track_Writer::event_hook()
{
	int16_t param;
	if(is_inside_loop() || is_inside_jump())
	{
		return;
	}
	if(event.type != Event::REST && rest_time)
	{
		converted_events.push_back(MDSDRV_Event(MDSDRV_Event::REST, rest_time));
		rest_time = 0;
	}
	param = event.param;
	rest_time += off_time;
	switch(event.type)
	{
		default:
			break;
		case Event::TIE:
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::TIE, on_time));
			break;
		case Event::NOTE:
			if(drum_mode_enabled)
			{
				try
				{
					param = mdsdrv.get_subroutine(param, 1, 0);
				}
				catch (std::out_of_range &)
				{
					error(stringf("MDSDRV: Drum mode subroutine *%d doesn't exist", param).c_str());
				}
			}
			if(param < 0)
				param = 0;
			if(in_drum_mode)
			{
				if(param < 0 || param > 255)
					error(stringf("MDSDRV: note out of range (%d > %d)", param, 255).c_str());
				converted_events.push_back(MDSDRV_Event(MDSDRV_Event::DMFINISH, param));
				disable();
			}
			else
			{
				if(param < 0 || param >= (MDSDRV_Event::SLR - MDSDRV_Event::NOTE))
					error(stringf("MDSDRV: note out of range (%d > %d)", param, (MDSDRV_Event::SLR - MDSDRV_Event::NOTE)).c_str());
				converted_events.push_back(MDSDRV_Event(MDSDRV_Event::NOTE + param, on_time));
			}
			break;
		case Event::LOOP_START:
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::LP,0));
			break;
		case Event::LOOP_BREAK:
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::LPB,0));
			break;
		case Event::LOOP_END:
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::LPF,param));
			break;
		case Event::SEGNO:
			in_loop = true;
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::SEGNO,0));
			break;
		case Event::JUMP:
			try
			{
				converted_events.push_back(MDSDRV_Event(MDSDRV_Event::PAT,mdsdrv.get_subroutine(param, 0, drum_mode_enabled)));
			}
			catch (std::out_of_range &)
			{
				error(stringf("MDSDRV: Subroutine *%d doesn't exist", event.param).c_str());
			}
			break;
		case Event::END:
			// Gracefully handle infinite loop at the end of a track
			if(get_play_time() == get_loop_play_time())
				in_loop = false;
			if(in_loop)
				converted_events.push_back(MDSDRV_Event(MDSDRV_Event::JUMP,0));
			else
				converted_events.push_back(MDSDRV_Event(MDSDRV_Event::FINISH,0));
			break;
		case Event::SLUR:
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::SLR,0));
			break;
		case Event::PLATFORM:
			try
			{
				const Tag& tag = mdsdrv.song->get_platform_command(event.param);
				parse_platform_event(tag);
			}
			catch (std::out_of_range &)
			{
				error(stringf("MDSDRV: Platform command %d is not defined", event.param).c_str());
			}
			break;
		case Event::TRANSPOSE_REL:
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::TRSM,param));
			break;
		case Event::VOL:
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::VOL,param | 0x80));
			break;
		case Event::VOL_REL:
		case Event::VOL_FINE_REL:
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::VOLM,param));
			break;
		case Event::TEMPO_BPM:
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::TEMPO,bpm_to_delta(param)));
			break;
		case Event::INS:
			try
			{
				if(mdsdrv.data.ins_type.at(param) != MDSDRV_Data::INS_PCM)
					converted_events.push_back(MDSDRV_Event(MDSDRV_Event::INS,
								mdsdrv.get_envelope(mdsdrv.data.envelope_map.at(param))));
				else
					converted_events.push_back(MDSDRV_Event(MDSDRV_Event::PCM,
								mdsdrv.get_envelope(0x20000 + mdsdrv.data.envelope_map.at(param))));
			}
			catch (std::out_of_range &)
			{
				error(stringf("MDSDRV: Instrument @%d doesn't exist", event.param).c_str());
			}
			break;
		case Event::TRANSPOSE:
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::TRS,param));
			break;
		case Event::DETUNE:
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::DTN,param));
			break;
		case Event::VOL_FINE:
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::VOL,param & 0x7f));
			break;
		case Event::PAN:
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::PAN,param << 6));
			break;
		case Event::PAN_ENVELOPE:
			try
			{
				if(param)
					param = mdsdrv.get_macro_track(param) + 1;
				converted_events.push_back(MDSDRV_Event(MDSDRV_Event::MTAB,param));
			}
			catch (std::out_of_range &)
			{
				error(stringf("MDSDRV: Macro track *%d doesn't exist", event.param).c_str());
			}
			break;
		case Event::PITCH_ENVELOPE:
			try
			{
				if(param)
				{
					if(mdsdrv.data.pitch_extend.find(param) != mdsdrv.data.pitch_extend.end())
						param = mdsdrv.get_envelope(0x10000 + mdsdrv.data.pitch_map.at(param)) + 1;
					else
						param = mdsdrv.get_envelope(mdsdrv.data.pitch_map.at(param)) + 1;
				}
				converted_events.push_back(MDSDRV_Event(MDSDRV_Event::PEG,param));
			}
			catch (std::out_of_range &)
			{
				error(stringf("MDSDRV: Pitch envelope @M%d doesn't exist", event.param).c_str());
			}
			break;
		case Event::PORTAMENTO:
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::PTA,param));
			break;
		case Event::DRUM_MODE:
			drum_mode_enabled = param;
			if(param)
				param = 0+8;
			else
				param = 0;
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::FLG, param));
			break;
		case Event::TEMPO:
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::TEMPO,param));
			break;
	}
}

bool MDSDRV_Track_Writer::loop_hook()
{
	return 0;
}

void MDSDRV_Track_Writer::end_hook()
{
	if(rest_time)
	{
		converted_events.push_back(MDSDRV_Event(MDSDRV_Event::REST, rest_time));
		rest_time = 0;
	}

	if(in_loop)
		converted_events.push_back(MDSDRV_Event(MDSDRV_Event::JUMP,0));
	else
		converted_events.push_back(MDSDRV_Event(MDSDRV_Event::FINISH,0));
}

void MDSDRV_Track_Writer::parse_platform_event(const Tag& tag)
{
	if(iequal(tag[0], "mode")) // PSG noise mode
	{
		if(tag.size() < 2)
			error("not enough parameters for 'mode' command");
		uint16_t param = std::strtol(tag[1].c_str(), 0, 0);
		if(param == 1)
			param = 0xe7;
		else if(param == 2)
			param = 0xe3;
		else
			param = 0x100; // Should be replaced with 00 in the actual file
		converted_events.push_back(MDSDRV_Event(MDSDRV_Event::LFO, param));
	}
	else if(iequal(tag[0], "lfo")) // LFO depth
	{
		if(tag.size() < 3)
			error("not enough parameters for 'lfo' command");
		converted_events.push_back(MDSDRV_Event(MDSDRV_Event::LFO,
					((std::strtol(tag[1].c_str(), 0, 0) << 4) | (std::strtol(tag[2].c_str(), 0, 0) & 0x3f))));
	}
	else if(iequal(tag[0], "lforate")) // LFO rate
	{
		if(tag.size() < 2)
			error("not enough parameters for 'lforate' command");
		uint8_t param = std::strtol(tag[1].c_str(), 0, 0);
		if(param)
			param += 7;
		converted_events.push_back(MDSDRV_Event(MDSDRV_Event::FMREG, 0x2200 | param));
	}
	else if(iequal(tag[0], "fm3")) // FM3 mode
	{
		if(tag.size() < 2)
			error("not enough parameters for 'fm3' command");
		converted_events.push_back(MDSDRV_Event(MDSDRV_Event::FLG,
					0x80 | ((std::strtol(tag[1].c_str(), 0, 2) ^ 0x0f) & 0x0f)));
	}
	else if(iequal(tag[0], "write")) // FM register write
	{
		if(tag.size() < 3)
			error("not enough parameters for 'write' command");
		uint8_t write_addr = std::strtol(tag[1].c_str(), 0, 0);
		uint16_t write_data = (write_addr << 8) | (std::strtol(tag[2].c_str(), 0, 0) & 0xff);
		if(write_addr >= 0x30)
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::FMCREG, write_data));
		else
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::FMREG, write_data));
	}
	else if(iequal(tag[0], "pcmrate")) // PCM channel sample rate
	{
		if(tag.size() < 2)
			error("not enough parameters for 'pcmrate' command");
		uint8_t data = std::strtol(tag[1].c_str(), 0, 0);
		if(data < 1 || data > 8)
			error("pcmrate argument must be between 1 and 8");
		converted_events.push_back(MDSDRV_Event(MDSDRV_Event::PCMRATE, data));
	}
	else if(iequal(tag[0], "pcmmode")) // PCM mixing mode
	{
		if(tag.size() < 2)
			error("not enough parameters for 'pcmmode' command");
		uint8_t data = std::strtol(tag[1].c_str(), 0, 0);
		if(data < 2 || data > 3)
			error("pcmmode argument must be between 2 and 3");
		converted_events.push_back(MDSDRV_Event(MDSDRV_Event::PCMMODE, data));
	}
	else if(iequal(tag[0], "cmd")) // Direct command
	{
		if(tag.size() < 2)
			error("not enough parameters for 'cmd' command");
		MDSDRV_Event::Type type = (MDSDRV_Event::Type)std::strtol(tag[1].c_str(), 0, 0);
		uint16_t data = 0;
		if(tag.size() > 2)
			data = std::strtol(tag[2].c_str(), 0, 0);
		converted_events.push_back(MDSDRV_Event(type, data));
	}
	else if(iequal(tag[0], "carry"))
	{
		converted_events.push_back(MDSDRV_Event(MDSDRV_Event::CARRY, 0));
	}
	else if(MDSDRV_get_register(tag[0]))
	{
		if(tag.size() < 2)
			error("not enough parameters for 'write' command");

		uint8_t reg = MDSDRV_get_register(tag[0]);
		uint16_t data = (reg << 8) | (std::strtol(tag[1].c_str(), 0, 0) & 0xff);

		if(reg >= 0xfc && tag[1].size() > 0)
		{
			data -= 0xfc00;
			uint8_t sign = tag[1][0];
			if(sign == '+' || sign == '-')
				converted_events.push_back(MDSDRV_Event(MDSDRV_Event::FMTLM, data));
			else
				converted_events.push_back(MDSDRV_Event(MDSDRV_Event::FMTL, data));
		}
		else if(reg >= 0x30)
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::FMCREG, data));
		else
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::FMREG, data));
	}
}

//! Converts BPM to fractional tempo
uint8_t MDSDRV_Track_Writer::bpm_to_delta(uint16_t bpm)
{
	double base_tempo = 120. / (mdsdrv.song->get_ppqn() * (1./60.));
	double fract = (bpm / base_tempo)*256.;
	uint16_t new_tempo = (fract + 0.5) - 1;
	return std::min<uint16_t>(0xff, new_tempo);
}

//=====================================================================

//! Converts a Song into MDSDRV data, including data and sequences.
MDSDRV_Converter::MDSDRV_Converter(Song& song)
	: song(&song)
	, data()
	, used_data_map()
	, subroutine_map()
	, macro_track_map()
	, subroutine_list()
	, macro_track_list()
	, track_list()
	, sequence_data()
{
	data.read_song(song);

	for(auto it = song.get_track_map().begin(); it != song.get_track_map().end(); it++)
	{
		int id = it->first;
		if(id < 16)
			parse_track(id);
	}

	uint16_t track_header_offset = 4;
	uint16_t track_count = 0;
	uint16_t data_base = 4 + (4 * track_list.size());
	uint16_t header_size = data_base + (subroutine_list.size() + macro_track_list.size() + used_data_map.size()) * 2;
	sequence_data.insert(sequence_data.begin(), header_size, 0x00);

	// we should use something else to define the track list, i think...
	for(auto it = track_list.begin(); it != track_list.end(); it++)
	{
		track_count++;
		uint16_t offset = sequence_data.size() - data_base;
		sequence_data[track_header_offset++] = it->first;
		sequence_data[track_header_offset++] = 0;
		sequence_data[track_header_offset++] = offset >> 8;
		sequence_data[track_header_offset++] = offset;

		std::vector<uint8_t> bytes = convert_track(it->second);
		sequence_data.insert(sequence_data.end(), bytes.begin(), bytes.end());
	}

	auto vol_str = song.get_tag_front_safe("#volume");
	auto vol = 0;
	if(vol_str.size())
	{
		std::strtoul(vol_str.c_str(), NULL, 0);
		if(vol > 127)
			vol = 127;
	}

	sequence_data[0] = data_base >> 8;
	sequence_data[1] = data_base;
	sequence_data[2] = vol;
	sequence_data[3] = track_count;

	for(auto it = subroutine_list.begin(); it != subroutine_list.end(); it++)
	{
		uint16_t offset = sequence_data.size() - data_base;
		sequence_data[track_header_offset++] = offset >> 8;
		sequence_data[track_header_offset++] = offset;

		std::vector<uint8_t> bytes = convert_track(*it);
		sequence_data.insert(sequence_data.end(), bytes.begin(), bytes.end());
	}

	for(auto it = macro_track_list.begin(); it != macro_track_list.end(); it++)
	{
		uint16_t offset = sequence_data.size() - data_base;
		sequence_data[track_header_offset++] = offset >> 8;
		sequence_data[track_header_offset++] = offset;

		std::vector<uint8_t> bytes = convert_macro_track(*it);
		sequence_data.insert(sequence_data.end(), bytes.begin(), bytes.end());
	}
}

//! Output a MDSDRV RIFF container
RIFF MDSDRV_Converter::get_mds()
{
	std::vector<uint8_t> ver = {MDSDRV_SEQ_VERSION_MAJOR, MDSDRV_SEQ_VERSION_MINOR};

	auto group = song->get_tag_front_safe("#group");
	std::vector<uint8_t> group_data (group.begin(), group.end());

	RIFF riff(RIFF::TYPE_RIFF, FOURCC("MDS0"));
	riff.add_chunk(RIFF(FOURCC("ver "), ver)); // version data
	riff.add_chunk(RIFF(FOURCC("grp "), group_data)); // group id
	riff.add_chunk(RIFF(FOURCC("seq "), sequence_data));
	RIFF dblk(RIFF::TYPE_LIST, FOURCC("dblk"));
	for(auto it = used_data_map.begin(); it != used_data_map.end(); it++)
	{
		std::vector<uint8_t> d(4);
		*(uint32_t*)d.data() = get_data_id(it->second) + ((it->first & 0x10000) ? (1<<31) : 0);
		d.insert(d.end(),
				data.data_bank[it->first & 0x7fff].begin(),
				data.data_bank[it->first & 0x7fff].end());
		if(it->first < 0x20000)
			dblk.add_chunk(RIFF(FOURCC("glob"), d));
		else
			dblk.add_chunk(RIFF(FOURCC("pcmh"), d));
	}
	riff.add_chunk(dblk);
	std::vector<uint8_t> d(data.wave_rom.get_rom_data().begin(),
			data.wave_rom.get_rom_data().end() - data.wave_rom.get_free_bytes());
	riff.add_chunk(RIFF(FOURCC("pcmd"), d));
	return riff;
}

//! uses MDSDRV_Track_Writer to convert a track into an event stream
void MDSDRV_Converter::parse_track(int track_id)
{
	auto writer = MDSDRV_Track_Writer(*this, track_id, 0, 0, track_list[track_id]);
	while(writer.is_enabled())
	{
		writer.step_event();
	}
}

//! Convert an event stream (track or subroutine) to a MDSDRV byte stream.
/*!
 * This is essentially the final pass of the MML sequence data. Optimization to
 * reduce the note/rest length footprint is done here.
 */
std::vector<uint8_t> MDSDRV_Converter::convert_track(const std::vector<MDSDRV_Event>& event_list)
{
	uint16_t segno_pos = 0x0000;
	uint16_t last_rest = 0xffff; // even though we have a default rest/note time it's best not to
	uint16_t last_note = 0xffff; // rely on them, for example in the beginning of a subroutine
	auto track_data = std::vector<uint8_t>();
	uint8_t last_type = MDSDRV_Event::REST;
	std::stack<uint16_t> loop_break_address;

	for(auto it = event_list.begin(); it != event_list.end(); it++)
	{
		uint8_t type = it->type;
		uint16_t arg = it->arg;
		if(type == MDSDRV_Event::REST && arg)
		{
			arg -= 1;
			while(arg >= 128)
			{
				// If last event was a note or tie and no length was specified
				// we must add the length parameter of that event before adding any
				// rest duration, to prevent ambiguity
				if((last_type >= MDSDRV_Event::TIE) && (last_type < MDSDRV_Event::SLR)
						&& (track_data.at(track_data.size() - 1) > 0x80))
				{
					last_type = MDSDRV_Event::REST;
					track_data.push_back(last_note);
				}
				track_data.push_back(0x7f);
				last_rest = 0x7f;
				arg -= 128;
			}
			if(arg == last_rest)
			{
				track_data.push_back(MDSDRV_Event::REST);
			}
			else
			{
				if((last_type >= MDSDRV_Event::TIE) && (last_type < MDSDRV_Event::SLR)
						&& (track_data.at(track_data.size() - 1) > 0x80))
				{
					last_type = MDSDRV_Event::REST;
					track_data.push_back(last_note);
				}
				track_data.push_back(arg);
				last_rest = arg;
			}
		}
		else if(type < MDSDRV_Event::SLR && arg)
		{
			// TODO: it would be possible to optimize by replacing an argument with
			// last_note*2 with a TIE command, if the next note has last_note length.
			// could save some space for "shuffle" notes.
			arg -= 1;
			track_data.push_back(type);
			while(arg >= 128)
			{
				if(last_note != 0x7f)
					track_data.push_back(0x7f);
				last_note = 0x7f;
				arg -= 128;
				track_data.push_back(MDSDRV_Event::TIE);
			}
			if(arg != last_note)
			{
				track_data.push_back(arg);
				last_note = arg;
			}
		}
		else
		{
			switch(type)
			{
				case MDSDRV_Event::SEGNO:
					segno_pos = track_data.size();
					break;
				case MDSDRV_Event::SLR: // no argument
				case MDSDRV_Event::FINISH:
					track_data.push_back(type);
					break;
				case MDSDRV_Event::VOL: // 8-bit argument
				case MDSDRV_Event::VOLM:
				case MDSDRV_Event::TRS:
				case MDSDRV_Event::TRSM:
				case MDSDRV_Event::DTN:
				case MDSDRV_Event::PTA:
				case MDSDRV_Event::PAN:
				case MDSDRV_Event::LFO:
				case MDSDRV_Event::FLG:
				case MDSDRV_Event::DMFINISH:
				case MDSDRV_Event::COMM:
				case MDSDRV_Event::TEMPO:
				case MDSDRV_Event::PCMRATE:
				case MDSDRV_Event::PCMMODE:
					track_data.push_back(type);
					track_data.push_back(arg & 0xff);
					break;
					track_data.push_back(type);
					if(arg == 0xff)
						track_data.push_back(0);
					else
						track_data.push_back(arg);
					break;
				case MDSDRV_Event::MTAB: // 8-bit arg with offset
					track_data.push_back(type);
					if(arg)
						track_data.push_back(arg + subroutine_list.size());
					else
						track_data.push_back(0);
					break;
				case MDSDRV_Event::INS: // 8-bit arg with offset
				case MDSDRV_Event::PCM:
					track_data.push_back(type);
					track_data.push_back(get_data_id(arg));
					break;
				case MDSDRV_Event::PEG:	// 8-bit arg with offset and toggle
					track_data.push_back(type);
					if(arg)
						track_data.push_back(get_data_id(arg));
					else
						track_data.push_back(0);
					break;
				case MDSDRV_Event::FMREG: //16-bit arg
				case MDSDRV_Event::FMCREG:
				case MDSDRV_Event::FMTL:
				case MDSDRV_Event::FMTLM:
					track_data.push_back(type);
					track_data.push_back(arg >> 8);
					track_data.push_back(arg);
					break;
				case MDSDRV_Event::JUMP:
					segno_pos = (segno_pos - (track_data.size() + 3));
					track_data.push_back(MDSDRV_Event::JUMP);
					track_data.push_back(segno_pos >> 8);
					track_data.push_back(segno_pos);
					break;
				case MDSDRV_Event::PAT: // subroutine
					track_data.push_back(type);
					track_data.push_back(arg);
					last_rest = 0xffff;
					last_note = 0xffff;
					break;
				case MDSDRV_Event::LP: // loop start
					track_data.push_back(type);
					loop_break_address.push(0x0000);
					// reset counters. TODO: can be optimized by checking what the
					// values of the counters should be at the end of the loop and don't reset
					// if they match.
					last_rest = 0xffff;
					last_note = 0xffff;
					break;
				case MDSDRV_Event::LPB: // loop break
					// set the break address
					loop_break_address.top() = track_data.size();
					break;
				case MDSDRV_Event::LPF: // loop finish
					track_data.push_back(type);
					track_data.push_back(arg);
					// insert loop break command
					if(loop_break_address.top())
					{
						uint16_t offset = track_data.size() - loop_break_address.top();
						auto break_cmd = std::vector<uint8_t>();
						if(offset < 256)
						{
							// short version
							break_cmd.push_back(MDSDRV_Event::LPB);
							break_cmd.push_back(offset);
						}
						else
						{
							// long version
							break_cmd.push_back(MDSDRV_Event::LPBL);
							break_cmd.push_back(offset >> 8);
							break_cmd.push_back(offset);
						}
						track_data.insert(track_data.begin() + loop_break_address.top(),
								break_cmd.begin(), break_cmd.end());
						// reset counters. TODO: can be optimized by resetting them
						// to what they are at the loop break.
						last_rest = 0xffff;
						last_note = 0xffff;
					}
					loop_break_address.pop();
					break;
				default:
					break;
			}
		}

		last_type = type;
	}
	return track_data;
}

//! Convert an event stream (track or subroutine) to a MDSDRV macro track byte stream.
/*!
 * This is essentially the final pass of the MML sequence data.
 *
 * TODO: Not all features of this have been tested. Some commands are not supported and might
 * not be for the foreseeable future.
 */
std::vector<uint8_t> MDSDRV_Converter::convert_macro_track(const std::vector<MDSDRV_Event>& event_list)
{
	uint16_t segno_pos = 0x0000;
	auto track_data = std::vector<uint8_t>();
	std::stack<uint16_t> loop_break_address;
	std::stack<uint16_t> loop_start_address;

	for(auto it = event_list.begin(); it != event_list.end(); it++)
	{
		uint8_t type = it->type;
		uint16_t arg = it->arg;
		if(type == MDSDRV_Event::REST && arg)
		{
			arg -= 1;
			while(arg >= 256)
			{
				track_data.push_back(0x81);
				track_data.push_back(0xff);
				arg -= 256;
			}
			track_data.push_back(0x81);
			track_data.push_back(arg);
		}
		else if(type < MDSDRV_Event::SLR && arg)
		{
			uint8_t command = 0x82;
			arg -= 1;
			while(arg >= 256)
			{
				track_data.push_back(command);
				track_data.push_back(0xff);
				command = 0x81;
				arg -= 256;
			}
			track_data.push_back(command);
			track_data.push_back(arg);
		}
		else
		{
			switch(type)
			{
				case MDSDRV_Event::SLR: // Not supported by macro track
				case MDSDRV_Event::PCMMODE:
				case MDSDRV_Event::TEMPO:
				case MDSDRV_Event::MTAB:
				case MDSDRV_Event::DMFINISH:
				case MDSDRV_Event::PAT:
					printf("MDSDRV: ignoring event type %d not supported in macro track\n", type);
					break;
				case MDSDRV_Event::COMM: // May support in the future
				case MDSDRV_Event::FLG:
				case MDSDRV_Event::INS:
				case MDSDRV_Event::PCM:
				case MDSDRV_Event::PCMRATE:
				case MDSDRV_Event::PEG:
				case MDSDRV_Event::FMREG:
					printf("MDSDRV: ignoring event type %d not supported in macro track\n", type);
					break;
				case MDSDRV_Event::SEGNO:
					segno_pos = track_data.size();
					break;
				case MDSDRV_Event::CARRY:
					track_data.push_back(0x83);
					track_data.push_back(0x00);
					break;
				case MDSDRV_Event::FINISH:
					track_data.push_back(0x80);
					track_data.push_back(0x00);
					break;
				case MDSDRV_Event::VOL: // 8-bit argument
					track_data.push_back(0x00 + 0x18);
					track_data.push_back(arg);
					break;
				case MDSDRV_Event::VOLM:
					track_data.push_back(0x40 + 0x18);
					track_data.push_back(arg);
					break;
				case MDSDRV_Event::TRS:
					track_data.push_back(0x00 + 0x16);
					track_data.push_back(arg);
					break;
				case MDSDRV_Event::TRSM:
					track_data.push_back(0x40 + 0x16);
					track_data.push_back(arg);
					break;
				case MDSDRV_Event::DTN:
					track_data.push_back(0x00 + 0x11);
					track_data.push_back(arg);
					break;
				case MDSDRV_Event::PTA:
					track_data.push_back(0x00 + 0x17);
					track_data.push_back(arg);
					break;
				case MDSDRV_Event::PAN:
					track_data.push_back(0x87);
					track_data.push_back(arg);
					break;
				case MDSDRV_Event::LFO:
					if(arg < 0xc0)
					{
						track_data.push_back(0x88);
						track_data.push_back(arg);
					}
					else
					{
						track_data.push_back(0x89); // PSG noise mode
						track_data.push_back(arg & 0xff);
					}
					break;
				case MDSDRV_Event::FMCREG:
					track_data.push_back(0xc0 + (arg >> 10));
					track_data.push_back(arg & 0xff);
					break;
				case MDSDRV_Event::FMTL:
					track_data.push_back(0x00 + 0x36 + (arg >> 8));
					track_data.push_back(arg);
					break;
				case MDSDRV_Event::FMTLM:
					track_data.push_back(0x40 + 0x36 + (arg >> 8));
					track_data.push_back(arg);
					break;
				case MDSDRV_Event::JUMP:
					segno_pos = (segno_pos - (track_data.size() + 2));
					track_data.push_back(0x80);
					track_data.push_back(segno_pos/2);
					break;
				case MDSDRV_Event::LP: // loop start
					loop_break_address.push(0x0000);
					track_data.push_back(0x84);
					track_data.push_back(0); //to be filled in
					loop_start_address.push(track_data.size());
					break;
				case MDSDRV_Event::LPB: // loop break
					track_data.push_back(0x85);
					track_data.push_back(0); //to be filled in
					loop_break_address.top() = track_data.size();
					break;
				case MDSDRV_Event::LPF: // loop finish
					track_data.push_back(0x86);
					track_data.push_back((loop_start_address.top() - (track_data.size() + 1))/2);
					// insert loop break command
					if(loop_break_address.top())
					{
						uint16_t offset = track_data.size() - loop_break_address.top();
						track_data[loop_break_address.top() - 1] = offset / 2;
					}
					track_data[loop_start_address.top() - 1] = arg - 1;
					loop_break_address.pop();
					loop_start_address.pop();
					break;
				default:
					break;
			}
		}
	}
	return track_data;
}
//! Get the subroutine ID.
/*!
 * Note: if a subroutine is called from a drum mode jump, the drum mode offset
 * is not kept.
 */
int MDSDRV_Converter::get_subroutine(int track_id, bool in_drum_mode, bool drum_mode_enabled)
{
	int mapped_id = ((track_id << 2) | (in_drum_mode << 1) | (drum_mode_enabled));
	auto search = subroutine_map.find(mapped_id);
	if(search == subroutine_map.end())
	{
		int sub_id = subroutine_list.size();
		subroutine_map[mapped_id] = sub_id;
		subroutine_list.push_back({});
		// I tried using a pointer or reference to subroutine_list[sub_id] here, but
		// it turned out to be a terrible idea because the pointers move when this
		// function is recursively called (probably the push_back above this comment
		// is to blame)
		auto event_list = std::vector<MDSDRV_Event>();
		auto writer = MDSDRV_Track_Writer(*this, track_id, in_drum_mode, drum_mode_enabled, event_list);
		while(writer.is_enabled())
		{
			writer.step_event();
		}
		subroutine_list[sub_id] = event_list;
		return sub_id;
	}
	else
	{
		return search->second;
	}
}

//! Get the macro track ID.
/*!
 * Note: if a subroutine is called from a drum mode jump, the drum mode offset
 * is not kept.
 */
int MDSDRV_Converter::get_macro_track(int track_id)
{
	auto search = macro_track_map.find(track_id);
	if(search == macro_track_map.end())
	{
		int sub_id = macro_track_list.size();
		macro_track_map[track_id] = sub_id;
		macro_track_list.push_back({});
		// I tried using a pointer or reference to subroutine_list[sub_id] here, but
		// it turned out to be a terrible idea because the pointers move when this
		// function is recursively called (probably the push_back above this comment
		// is to blame)
		auto event_list = std::vector<MDSDRV_Event>();
		auto writer = MDSDRV_Track_Writer(*this, track_id, 0, 0, event_list);
		while(writer.is_enabled())
		{
			writer.step_event();
		}
		macro_track_list[sub_id] = event_list;
		return sub_id;
	}
	else
	{
		return search->second;
	}
}

//! Get the used_data_map index for the given envelope ID
/*!
 * Calling this function assumes that the envelope ID is to be used, so it is added
 * to the used_data_map and the new index is returned.
 */
int MDSDRV_Converter::get_envelope(int mapped_id)
{
	auto search = used_data_map.find(mapped_id);
	if(search == used_data_map.end())
	{
		int env_id = used_data_map.size();
		used_data_map[mapped_id] = env_id;
		return env_id;
	}
	else
	{
		return search->second;
	}
}

//=====================================================================

//! Creates a MDSDRV_Linker
MDSDRV_Linker::MDSDRV_Linker()
	: data_bank()
	, data_offset()
	, seq_bank()
	, wave_rom(0x3f8000, 0x8000)
{
}

//! Add a song (converted to MDS RIFF format).
void MDSDRV_Linker::add_song(RIFF& mds, const std::string& filename)
{
	std::vector<uint8_t> ver = {0, 0};
	std::vector<uint8_t> pcmd = {};
	std::vector<uint8_t> seq = {};
	std::vector<uint8_t> group = {};
	std::vector<std::pair<uint16_t,uint16_t>> patch_table;
	RIFF dblk = RIFF(0);
	mds.rewind();
	if(mds.get_type() != RIFF::TYPE_RIFF || mds.get_id() != FOURCC("MDS0"))
		throw InputError(nullptr, "This is not a valid .MDS version 0 file");

	while(!mds.at_end())
	{
		auto chunk = RIFF(mds.get_chunk());
		if(chunk.get_type() == FOURCC("seq "))
			seq = chunk.get_data();
		else if(chunk.get_type() == FOURCC("pcmd"))
			pcmd = chunk.get_data();
		else if(chunk.get_type() == RIFF::TYPE_LIST && chunk.get_id() == FOURCC("dblk"))
			dblk = chunk;
		else if(chunk.get_type() == FOURCC("ver "))
			ver = chunk.get_data();
		else if(chunk.get_type() == FOURCC("grp "))
			group = chunk.get_data();
	}

	check_version(ver[0], ver[1]);

	if(!seq.size() || dblk.get_type() != RIFF::TYPE_LIST)
		throw InputError(nullptr, ".MDS data is malformed");

	uint16_t seq_sdata = (seq[0] << 8) | seq[1];
	dblk.rewind();
	while(!dblk.at_end())
	{
		auto chunk = RIFF(dblk.get_chunk());
		if(chunk.get_type() == FOURCC("glob"))
		{
			// Envelope data
			auto data = chunk.get_data();
			uint32_t id = read_le32(data, 0);
			uint32_t addr = seq_sdata + id * 2;
			uint16_t offset = add_unique_data(std::vector<uint8_t>(data.begin()+4, data.end()));
			printf("replace seq+%04x with %04x (Envelope)\n", addr, offset);
			if(id & 0x80000000)
				patch_table.push_back({addr, offset | 0x8000});
			else
				patch_table.push_back({addr, offset});
		}
		else if(chunk.get_type() == FOURCC("pcmh"))
		{
			// PCM header
			auto data = chunk.get_data();
			uint32_t addr = seq_sdata + read_le32(data, 0)*2;
			Wave_Bank::Sample header;
			header.from_bytes(std::vector<uint8_t>(data.begin()+4, data.end()));
			auto begin = pcmd.begin() + header.position;
			auto end = pcmd.begin() + header.position + header.size;
			header.position = 0;
			uint16_t offset = wave_rom.add_sample(header, std::vector<uint8_t>(begin, end));

			// Get new PCM header
			header = wave_rom.get_sample_headers().at(offset);
			auto hdata = get_pcm_header(header);
			offset = add_unique_data(std::vector<uint8_t>(hdata.begin(), hdata.end()));
			printf("replace seq+%04x with %04x (PCM header)\n", addr, offset);
			patch_table.push_back({addr, offset});
		}
	}
	auto group_str = keyify_string(std::string(group.begin(), group.end()));
	if(!group_str.size()) // set default group name
		group_str = "BGM";
	seq_bank[group_str].push_back({filename, seq, patch_table});
}

std::vector<uint8_t> MDSDRV_Linker::get_pcm_header(const Wave_Bank::Sample& sample) const
{
	std::vector<uint8_t> output = {};
	float pitch = sample.rate / (MDSDRV_PCM_RATE / 8.0);
	uint8_t cp = pitch + 0.5;
	if(cp < 1)
		cp = 1;
	else if(cp > 8)
		cp = 8;
	write_be32(output, 0, (sample.position + sample.start) | (cp << 24));
	write_be32(output, 4, sample.size);
	return output;
}

//! Check that sequence version is compatible
void MDSDRV_Linker::check_version(uint8_t major, uint8_t minor)
{
	bool compatible = true;
	if(major < MDSDRV_MIN_SEQ_VERSION_MAJOR)
		compatible = false;
	if(major == MDSDRV_MIN_SEQ_VERSION_MAJOR && minor < MDSDRV_MIN_SEQ_VERSION_MINOR)
		compatible = false;
	if(major > MDSDRV_SEQ_VERSION_MAJOR)
		compatible = false;
#if (MDSDRV_MIN_SEQ_VERSION_MAJOR == 0)
	// Special case: 0.x is unstable
	if(major != 0 || minor < MDSDRV_MIN_SEQ_VERSION_MINOR || minor > MDSDRV_SEQ_VERSION_MINOR)
		compatible = false;
#endif
	if(!compatible)
		throw InputError(nullptr,
			stringf("Incompatible sequence data format version %d.%d (minimum: %d.%d)",
				major, minor,
				MDSDRV_MIN_SEQ_VERSION_MAJOR, MDSDRV_MIN_SEQ_VERSION_MINOR).c_str());

}

//! Get the number of sequences
unsigned int MDSDRV_Linker::get_seq_count() const
{
	unsigned int count = 0;
	for(auto&& i : seq_bank)
		count += i.second.size();
	return count;
}

//! Get the output mdsseq.bin
std::vector<uint8_t> MDSDRV_Linker::get_seq_data()
{
	int header_size = 12 + get_seq_count() * 4;
	auto data = std::vector<uint8_t>(header_size);

	// sdtop - 0
	int offset = header_size - 8;
	int id = 0;
	for(auto&& i : data_bank)
	{
		data.insert(data.end(), i.begin(), i.end());
		data_offset.push_back(offset);
		offset += i.size();
		if(offset & 1)
		{
			data.push_back(0);
			offset++;
		}
		if(offset >= 0x8000)
		{
			throw InputError(nullptr, "instrument data bank is too big (>32768 bytes)");
		}
		id++;
	}

	id = 1;
	for(auto&& group : seq_bank)
	{
		for(auto&& seq : group.second)
		{
			printf("put seq %02x (%s.%s) at %04x\n", id, group.first.c_str(), seq.filename.c_str(), offset);
			for(auto&& j : seq.patch_table)
			{
				write_be16(seq.data, j.first, data_offset[j.second & 0x7fff] | (j.second & 0x8000));
			}
			data.insert(data.end(), seq.data.begin(), seq.data.end());
			write_be32(data, 8 + (id * 4), offset);
			offset += seq.data.size();
			if(offset & 1)
			{
				data.push_back(0);
				offset++;
			}
			id++;
		}
	}

	// write header
	write_be32(data, 0, 0x10011f00);
	write_be16(data, 4, (MDSDRV_SEQ_VERSION_MAJOR<<8)|(MDSDRV_SEQ_VERSION_MINOR));
	write_be16(data, 6, id - 1);
	write_be32(data, 8, offset);

	// write wave table
	offset = data.size();
	write_be16(data, offset, wave_rom.get_sample_headers().size());
	offset += 2;
	for(auto&& i : wave_rom.get_sample_headers())
	{
		auto hdata = get_pcm_header(i);
		uint16_t hoffset = find_unique_data(std::vector<uint8_t>(hdata.begin(), hdata.end()));
		write_be16(data, offset, data_offset[hoffset]);
		offset += 2;
	}

	return data;
}

//! Get the output mdspcm.bin
std::vector<uint8_t> MDSDRV_Linker::get_pcm_data()
{
	auto wave = wave_rom.get_rom_data();
	return std::vector<uint8_t>(wave.begin(), wave.end() - wave_rom.get_free_bytes());
}

//! Get linker statistics
std::string MDSDRV_Linker::get_statistics()
{
	auto str = std::string();
	str += stringf("PCM data size: %d bytes (max %d)\n",
		wave_rom.get_rom_data().size() - wave_rom.get_free_bytes(),
		wave_rom.get_rom_data().size());
	str += stringf("Gaps: %d bytes, largest %d\n",
		wave_rom.get_total_gap(), wave_rom.get_largest_gap());
	return str;
}

static inline const std::string asm_define(std::string key, uint16_t value)
{
	return key + " = " + std::to_string(value) + "\n";
}

//! Get an assembly header file
std::string MDSDRV_Linker::get_asm_header() const
{
	uint16_t song_id = 0;
	std::string buf = "";
	String_Counter define_count;
	for(auto&& group : seq_bank)
	{
		buf += asm_define(unique_string(group.first + "_MIN", define_count), song_id + 1);
		for(auto&& seq : group.second)
		{
			song_id++;
			buf += asm_define(unique_string(group.first + "_" + seq.filename, define_count), song_id);
		}
		buf += asm_define(unique_string(group.first + "_MAX", define_count), song_id);
	}
	return buf;
}

static inline const std::string c_define(std::string key, uint16_t value)
{
	return "#define " + key + " " + std::to_string(value) + "\n";
}

std::string MDSDRV_Linker::get_c_header() const
{
	uint16_t song_id = 0;
	std::string buf = "";
	String_Counter define_count;
	for(auto&& group : seq_bank)
	{
		buf += c_define(unique_string(group.first + "_MIN", define_count), song_id + 1);
		for(auto&& seq : group.second)
		{
			song_id++;
			buf += c_define(unique_string(group.first + "_" + seq.filename, define_count), song_id);
		}
		buf += c_define(unique_string(group.first + "_MAX", define_count), song_id);
	}
	return buf;
}

//! Get a keyified string (all characters that would be illegal C or ASM defines stripped)
std::string MDSDRV_Linker::keyify_string(const std::string& input) const
{
	std::string out;
	for(auto && i : input)
	{
		if(std::isspace(i))
			out.push_back('_');
		else if(std::isalnum(i) || i == '_')
			out.push_back(std::toupper(i));
	}
	return out;
}

//! Get a unique keyified string
std::string MDSDRV_Linker::unique_string(const std::string& input, MDSDRV_Linker::String_Counter& map) const
{
	auto str = keyify_string(input);
	map[str]++;
	if(map[str] != 1)
		str = unique_string(str + "_" + std::to_string(map[str] - 1), map);
	return str;
}

//! Add unique data to the databank (same as MDSDRV_Data::add_unique_data())
int MDSDRV_Linker::add_unique_data(const std::vector<uint8_t>& data)
{
	unsigned int i;
	// look for previous matching data in the data bank
	for(i=0; i<data_bank.size(); i++)
	{
		if(data != data_bank[i])
			continue;
		return i;
	}
	data_bank.push_back(data);
	return data_bank.size()-1;
}

//! Find unique data in the databank. Throws out_of_range if not found.
int MDSDRV_Linker::find_unique_data(const std::vector<uint8_t>& data) const
{
	unsigned int i;
	// look for previous matching data in the data bank
	for(i=0; i<data_bank.size(); i++)
	{
		if(data != data_bank[i])
			continue;
		return i;
	}
	throw std::out_of_range("MDSDRV_Linker::find_unique_data");
}

//=====================================================================

MDSDRV_Platform::MDSDRV_Platform(int pcm_mode)
	: pcm_mode(pcm_mode)
{
}

std::shared_ptr<Driver> MDSDRV_Platform::get_driver(unsigned int rate, VGM_Interface* vgm_interface) const
{
	return std::static_pointer_cast<Driver>(std::make_shared<MD_Driver>(rate, vgm_interface, pcm_mode));
}

const Platform::Format_List& MDSDRV_Platform::get_export_formats() const
{
	static const Platform::Format_List out = {{"vgm", "VGM"}, {"mds", "MDS song data"}};
	return out;
}

std::vector<uint8_t> MDSDRV_Platform::get_export_data(Song& song, int format) const
{
	if(format == 0)
	{
		return vgm_export(song);
	}
	else if(format == 1)
	{
		MDSDRV_Converter converter(song);
		return converter.get_mds().to_bytes();
	}
	else
	{
		throw std::logic_error("no such exporter");
	}
}
