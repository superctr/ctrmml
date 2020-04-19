#include <iostream>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <cctype>
#include <cmath>
#include <stack>

#include "mdsdrv.h"
#include "../song.h"
#include "../input.h"
#include "../stringf.h"
#include "../riff.h"
#include "../util.h"

MDSDRV_Data::MDSDRV_Data()
	: data_bank()
	, wave_rom(0x200000)
	, envelope_map()
	, wave_map()
	, ins_transpose()
	, pitch_map()
	, ins_type()
	, message("")
{
}

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

//! adds a 4op FM instrument. Data stored in operator order.
void MDSDRV_Data::read_fm_4op(uint16_t id, const Tag& tag)
{
	std::vector<uint8_t> fm_data(30, 0);
	std::vector<uint8_t> tag_data(42, 0);
	auto it = tag.begin();
	for(int i=0; i<42; i++)
	{
		if(it == tag.end())
			throw InputError(nullptr, stringf("error: not enough parameters for fm instrument @%d", id).c_str());
		tag_data[i] = std::strtol(it->c_str(), NULL, 0);
		it++;
	}

	// Transpose
	if(it != tag.end())
		fm_data[29] = (std::strtol(it->c_str(), NULL, 0) + 24) << 1;
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
		// AM/DR (AM sensitivity not supported yet)
		fm_data[8 + i] = (tag_data[op + 1] & 31);
		// SR
		fm_data[12 + i] = (tag_data[op + 2] & 31);
		// SL/RR
		fm_data[16 + i] = (tag_data[op + 4] << 4) | (tag_data[op + 3] & 15);
		// SSG-EG
		fm_data[20 + i] = tag_data[op + 9] & 15;
		// TL
		fm_data[24 + i] = tag_data[op + 5];
	}
	// FB/ALG
	fm_data[28] = (tag_data[0] & 7) | (tag_data[1] << 3);
	envelope_map[id] = add_unique_data(fm_data);
	ins_transpose[id] = (fm_data[29] >> 1) - 24;
	ins_type[id] = INS_FM;
}

//! Read 2op instrument definition.
/*!
 * Tag format: InsID,Mul1,Mul2,Mul3,Mul4,Transpose
 */
void MDSDRV_Data::read_fm_2op(uint16_t id, const Tag& tag)
{
	std::vector<uint8_t> fm_data(30, 0);
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

//! Read PSG volume envelope.
/*!
 * Envelope input format:
 *   Value
 *   Value:Length
 *   Value>Target - slide
 *   Value>Target:Length - slide
 *   | - set loop position
 *   / - wait until keyoff
 * Output format:
 *   00 - end
 *   01 - sustain
 *   02 pp - loop to <p>
 *   fv - value <v> for <f> frames.
 */
void MDSDRV_Data::read_psg(uint16_t id, const Tag& tag)
{
	std::vector<uint8_t> env_data;
	int loop_pos = -1, last_pos = 0, last = -1;
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
		else if(std::isdigit(*s))
		{
			uint8_t initial = std::strtol(s, (char**)&s, 0);
			uint8_t target = initial;
			uint8_t length = 0;
			double delta = 0, counter = 0;

			if(*s == '>' && *++s)
				target = strtol(s, (char**)&s, 0);
			target = (target > 15) ? 15 : (target < 0) ? 0 : target; // bounds check
			initial = (initial > 15) ? 15 : (initial < 0) ? 0 : initial;

			if(target != initial)
				length = std::abs(target-initial)+1;
			if(*s == ':' && *++s)
				length = strtol(s, (char**)&s, 0);
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
void MDSDRV_Data::read_pitch(uint16_t id, const Tag& tag)
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
			add_pitch_node(s, &env_data);
		else if(*s == 'V')
		{
			loop_pos = env_data.size() / 4;
			add_pitch_vibrato(s, &env_data);
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

//! Adds a node to the pitch envelope.
void MDSDRV_Data::add_pitch_node(const char* s, std::vector<uint8_t>* env_data)
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
		length = strtol(s, (char**)&s, 0);
	if(length < 1)
		length += 1;

	counter = initial;
	//message += stringf("initial:%.2f, target=%.2f, len=%d", initial, target, length) + "\n";
	while(length > 0)
	{
		double delta = (target-counter)/length;
		uint16_t env_len = (length > 255) ? 255 : length;
		int16_t env_initial = counter * 256;
		int16_t env_delta = std::trunc(delta * 256);
		env_initial = (env_initial > 0x7eff) ? 0x7eff : env_initial;
		env_delta = (env_delta > 127) ? 127 : (env_delta < -128) ? -128: env_delta;
		//message += stringf("len:%d, from:%04x, chg:%d", env_len, env_initial, env_delta) + "\n";
		env_data->push_back(env_initial >> 8);
		env_data->push_back(env_initial & 0xff);
		env_data->push_back(env_delta);
		env_data->push_back(env_len - 1);
		// apply delta and decrease length counter
		counter += (env_delta * env_len) / 256;
		length -= env_len;
	}
}

//! Adds a node to the pitch envelope.
void MDSDRV_Data::add_pitch_vibrato(const char* s, std::vector<uint8_t>* env_data)
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
		vibrato_rate = strtol(s, (char**)&s, 0);
	else // TODO: throw an exception or warning
		message += stringf("Invalid vibrato definition: '%s'\n", s);

	vibrato_depth += vibrato_base;
	add_pitch_node(stringf("%f>%f:%d", vibrato_base, vibrato_depth, vibrato_rate).c_str(), env_data);
	add_pitch_node(stringf("%f>%f:%d", vibrato_depth, -vibrato_depth, vibrato_rate*2).c_str(), env_data);
	add_pitch_node(stringf("%f>%f:%d", -vibrato_depth, vibrato_base, vibrato_rate).c_str(), env_data);
}

void MDSDRV_Data::read_wave(uint16_t id, const Tag& tag)
{
	std::vector<uint8_t> env_data;
	int wave_header_id;

	// Insert a generic 32-byte generic Wave_Rom::Sample header.
	wave_header_id = wave_map[id] = wave_rom.add_sample(tag);
	env_data = wave_rom.get_sample_headers().at(wave_header_id).to_bytes();

	envelope_map[id] = add_unique_data(env_data);
	ins_type[id] = INS_PCM;
}


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

void MDSDRV_Data::read_envelope(uint16_t id, const Tag& tag)
{
	auto it = tag.begin();
	std::string type = *it++;
	if(iequal("psg", type))
	{
		read_psg(id, Tag(it, tag.end()));
		message += "read PSG envelope " + dump_data(id, envelope_map[id]) + "\n";
		return;
	}
	else if(iequal("fm", type))
	{
		read_fm_4op(id, Tag(it, tag.end()));
		message += "read FM envelope " + dump_data(id, envelope_map[id]) + "\n";
		return;
	}
	else if(iequal("2op", type))
	{
		read_fm_2op(id, Tag(it, tag.end()));
		message += "read 2op envelope " + dump_data(id, envelope_map[id]) + "\n";
		return;
	}
	else if(iequal("pcm", type))
	{
		read_wave(id, Tag(it, tag.end()));
		message += "read wave sample " + dump_data(id, envelope_map[id]) + "\n";
		return;
	}
	else
	{
		throw InputError(nullptr, stringf("unknown envelope type %s\n",type.c_str()).c_str());
	}
}

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
	for(auto it = tag_order.begin(); it != tag_order.end(); it++)
	{
		uint16_t id;
		Tag& tag = song.get_tag(*it);
		if(std::sscanf(it->c_str(), "@%hu", &id) == 1)
		{
			read_envelope(id, tag);
		}
		else if(std::sscanf(it->c_str(), "@m%hu", &id) == 1)
		{
			read_pitch(id, tag);
			std::cout << "read pitch envelope " << dump_data(id, pitch_map[id]) << "\n";
		}
	}
}

//! Converts a Track into MDSDRV_Event stream.
MDSDRV_Track_Writer::MDSDRV_Track_Writer(MDSDRV_Converter& mdsdrv,
		int id,
		bool in_drum_mode,
		std::vector<MDSDRV_Event>& converted_events)
	: Basic_Player(*mdsdrv.song, mdsdrv.song->get_track(id))
	, mdsdrv(mdsdrv)
	, converted_events(converted_events)
	, in_drum_mode(in_drum_mode)
	, drum_mode_offset(0)
	, in_loop(0)
	, rest_time(0)
{
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
			param = 0x00;
		converted_events.push_back(MDSDRV_Event(MDSDRV_Event::LFO, param));
	}
	else if(iequal(tag[0], "lfo")) // LFO depth
	{
		if(tag.size() < 3)
			error("not enough parameters for 'lfo' command");
		converted_events.push_back(MDSDRV_Event(MDSDRV_Event::LFO,
					(std::strtol(tag[1].c_str(), 0, 0) << 3) | (std::strtol(tag[2].c_str(), 0, 0))));
	}
	else if(iequal(tag[0], "lfodelay")) // LFO delay
	{
		if(tag.size() < 2)
			error("not enough parameters for 'lfodelay' command");
		converted_events.push_back(MDSDRV_Event(MDSDRV_Event::LFOD, std::strtol(tag[1].c_str(), 0, 0)));
	}
	else if(iequal(tag[0], "lforate")) // LFO rate
	{
		if(tag.size() < 2)
			error("not enough parameters for 'lforate' command");
		uint8_t param = std::strtol(tag[1].c_str(), 0, 0);
		if(!param)
			param = 0;
		else
			param = (param-1) | 0x8;
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
		uint8_t write_data = std::strtol(tag[2].c_str(), 0, 0);
		if(write_addr >= 0x30)
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::FMCREG, (write_addr << 8) | write_data));
		else
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::FMREG, (write_addr << 8) | write_data));
	}
}

//! Converts BPM to fractional tempo
uint8_t MDSDRV_Track_Writer::tempo_convert(uint16_t bpm)
{
	double base_tempo = 120. / (mdsdrv.song->get_ppqn() * (1./60.));
	double fract = (bpm / base_tempo)*256.;
	uint16_t new_tempo = (fract + 0.5) - 1;
	return std::min<uint16_t>(0xff, new_tempo);
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
			if(drum_mode_offset)
			{
				try
				{
					param = mdsdrv.get_subroutine(param + drum_mode_offset, 1);
				}
				catch (std::out_of_range &)
				{
					error(stringf("MDSDRV: Drum mode subroutine *%d doesn't exist", param + drum_mode_offset).c_str());
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
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::LP,0));
			break;
		case Event::JUMP:
			try
			{
				converted_events.push_back(MDSDRV_Event(MDSDRV_Event::PAT,mdsdrv.get_subroutine(param, 0)));
			}
			catch (std::out_of_range &)
			{
				error(stringf("MDSDRV: Subroutine *%d doesn't exist", event.param).c_str());
			}
			break;
		case Event::END:
			if(in_loop)
				converted_events.push_back(MDSDRV_Event(MDSDRV_Event::LPF,0));
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
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::TEMPO,tempo_convert(param)));
			break;
		case Event::INS:
			if(mdsdrv.data.ins_type.at(param) != MDSDRV_Data::INS_PCM)
				converted_events.push_back(MDSDRV_Event(MDSDRV_Event::INS,
							mdsdrv.get_envelope(mdsdrv.data.envelope_map.at(param))));
			else
				converted_events.push_back(MDSDRV_Event(MDSDRV_Event::PCM,
							mdsdrv.get_envelope(0x10000 + mdsdrv.data.envelope_map.at(param))));
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
		case Event::PITCH_ENVELOPE:
			if(param)
				param = mdsdrv.get_envelope(mdsdrv.data.pitch_map.at(param)) + 1;
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::PEG,param));
			break;
		case Event::PORTAMENTO:
			converted_events.push_back(MDSDRV_Event(MDSDRV_Event::PTA,param));
			break;
		case Event::DRUM_MODE:
			drum_mode_offset = param;
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
		converted_events.push_back(MDSDRV_Event(MDSDRV_Event::LPF,0));
	else
		converted_events.push_back(MDSDRV_Event(MDSDRV_Event::FINISH,0));
}

//! Converts a Song into MDSDRV data, including data and sequences.
MDSDRV_Converter::MDSDRV_Converter(Song& song)
	: song(&song)
	, data()
	, used_data_map()
	, subroutine_map()
	, subroutine_list()
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

	uint16_t track_mask = 0;
	uint16_t track_header_offset = 4;
	uint16_t track_count = 0;
	uint16_t data_base = 4 + (4 * track_list.size());
	uint16_t header_size = data_base + (subroutine_list.size() + used_data_map.size()) * 2;
	sequence_data.insert(sequence_data.begin(), header_size, 0x00);

	// we should use something else to define the track list, i think...
	for(auto it = track_list.begin(); it != track_list.end(); it++)
	{
		uint16_t offset = sequence_data.size() - data_base;
		track_mask |= 0x8000 >> track_count++;
		sequence_data[track_header_offset++] = it->first;
		sequence_data[track_header_offset++] = 0;
		sequence_data[track_header_offset++] = offset >> 8;
		sequence_data[track_header_offset++] = offset;

		std::vector<uint8_t> bytes = convert_track(it->second);
		sequence_data.insert(sequence_data.end(), bytes.begin(), bytes.end());
	}

	sequence_data[0] = data_base >> 8;
	sequence_data[1] = data_base;
	sequence_data[2] = track_mask >> 8;
	sequence_data[3] = track_mask;

	for(auto it = subroutine_list.begin(); it != subroutine_list.end(); it++)
	{
		uint16_t offset = sequence_data.size() - data_base;
		sequence_data[track_header_offset++] = offset >> 8;
		sequence_data[track_header_offset++] = offset;

		std::vector<uint8_t> bytes = convert_track(*it);
		sequence_data.insert(sequence_data.end(), bytes.begin(), bytes.end());
	}
}

//! uses MDSDRV_Track_Writer to convert a track into an event stream
void MDSDRV_Converter::parse_track(int track_id)
{
	auto writer = MDSDRV_Track_Writer(*this, track_id, 0, track_list[track_id]);
	while(writer.is_enabled())
	{
		writer.step_event();
	}
}

//! Get the subroutine ID.
/*!
 * Note: if a subroutine is called from a drum mode jump, the drum mode offset
 * is not kept.
 */
int MDSDRV_Converter::get_subroutine(int track_id, bool in_drum_mode)
{
	int mapped_id = (track_id << 1) | in_drum_mode;
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
		auto writer = MDSDRV_Track_Writer(*this, track_id, in_drum_mode, event_list);
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

//! Convert an event stream (track or subroutine) to a MDSDRV byte stream.
/*!
 * This is essentially the final pass of the MML sequence data. Optimization to
 * reduce the note/rest length footprint is done here.
 */
std::vector<uint8_t> MDSDRV_Converter::convert_track(const std::vector<MDSDRV_Event>& event_list)
{
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
			while(arg > 128)
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
			while(arg > 128)
			{
				if(last_note != 0x7f)
					track_data.push_back(0x7f);
				last_note = 0x7f;
				arg -= 128;
				if(arg != 0)
				{
					track_data.push_back(MDSDRV_Event::TIE);
				}
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
				case MDSDRV_Event::SLR: // no argument
				case MDSDRV_Event::FINISH:
					track_data.push_back(type);
					break;
				case MDSDRV_Event::VOL: // 8-bit argument
				case MDSDRV_Event::VOLM:
				case MDSDRV_Event::TRS:
				case MDSDRV_Event::DTN:
				case MDSDRV_Event::PTA:
				case MDSDRV_Event::PAN:
				case MDSDRV_Event::LFO:
				case MDSDRV_Event::LFOD:
				case MDSDRV_Event::FLG:
				case MDSDRV_Event::DMFINISH:
				case MDSDRV_Event::COMM:
				case MDSDRV_Event::TEMPO:
					track_data.push_back(type);
					track_data.push_back(arg);
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
						if(offset < 254)
						{
							// short version
							offset += 2;
							break_cmd.push_back(MDSDRV_Event::LPB);
							break_cmd.push_back(offset);
						}
						else
						{
							// long version
							offset += 3;
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

//! Output a MDSDRV RIFF container
RIFF MDSDRV_Converter::get_mds()
{
	RIFF riff(RIFF::TYPE_RIFF, FOURCC("MDS0"));
	riff.add_chunk(RIFF(FOURCC("seq "), sequence_data)); // seq
	RIFF dblk(RIFF::TYPE_LIST, FOURCC("dblk"));
	for(auto it = used_data_map.begin(); it != used_data_map.end(); it++)
	{
		std::vector<uint8_t> d(4);
		*(uint32_t*)d.data() = get_data_id(it->second);
		d.insert(d.end(),
				data.data_bank[it->first & 0x7fff].begin(),
				data.data_bank[it->first & 0x7fff].end());
		if(it->first < 0x10000)
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

MDSDRV_Linker::MDSDRV_Linker()
	: data_bank()
	, data_offset()
	, seq_bank()
	, wave_rom(0x3f8000, 0x8000)
{
}

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

void MDSDRV_Linker::add_song(RIFF& mds)
{
	std::vector<uint8_t> pcmd = {};
	std::vector<uint8_t> seq = {};
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
	}

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
			uint32_t addr = seq_sdata + read_le32(data, 0)*2;
			uint16_t offset = add_unique_data(std::vector<uint8_t>(data.begin()+4, data.end()));
			printf("replace seq+%04x with %04x\n", addr, offset);
			patch_table.push_back({addr, offset});
		}
		else if(chunk.get_type() == FOURCC("pcmh"))
		{
			// PCM header
			auto data = chunk.get_data();
			uint32_t addr = seq_sdata + read_le32(data, 0)*2;
			Wave_Rom::Sample header;
			header.from_bytes(std::vector<uint8_t>(data.begin()+4, data.end()));
			auto begin = pcmd.begin() + header.position;
			auto end = pcmd.begin() + header.position + header.size;
			header.position = 0;
			uint16_t offset = wave_rom.add_sample(header, std::vector<uint8_t>(begin, end)) | 0x8000;
			printf("replace seq+%04x with %04x\n", addr, offset);
			patch_table.push_back({addr, offset});
		}
	}
	seq_bank.push_back({seq, patch_table});
}

std::vector<uint8_t> MDSDRV_Linker::get_seq_data()
{
	int header_size = 6 + seq_bank.size() * 4;
	auto data = std::vector<uint8_t>(header_size);

	// sdtop - 0
	int offset = header_size - 6;
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

	id = 0;
	for(auto&& i : seq_bank)
	{
		printf("put seq %02x at %04x\n", id, offset);
		for(auto&& j : i.patch_table)
		{
			if(j.second & 0x8000)
				write_be16(i.data, j.first, j.second & 0x7fff);
			else
				write_be16(i.data, j.first, data_offset[j.second]);
		}
		data.insert(data.end(), i.data.begin(), i.data.end());
		write_be32(data, 6 + (id * 4), offset);
		offset += i.data.size();
		if(offset & 1)
		{
			data.push_back(0);
			offset++;
		}
		id++;
	}

	// write header
	write_be32(data, 0, offset);
	write_be16(data, 4, id);

	// write wave table
	offset = data.size();
	write_be16(data, offset, wave_rom.get_sample_headers().size());
	offset += 2;
	for(auto&& i : wave_rom.get_sample_headers())
	{
		write_be32(data, offset, i.position + i.start);
		write_be16(data, offset + 4, i.size);
		offset += 6;
	}

	return data;
}

std::vector<uint8_t> MDSDRV_Linker::get_pcm_data()
{
	auto wave = wave_rom.get_rom_data();
	return std::vector<uint8_t>(wave.begin(), wave.end() - wave_rom.get_free_bytes());
}

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
