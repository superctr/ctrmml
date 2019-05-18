#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <cctype>
#include <climits>
#include <cmath>
#include <algorithm>

#include "md.h"
#include "../song.h"
#include "../input.h"
#include "../stringf.h"

MD_Data::MD_Data()
	: data_bank()
	, wave_rom(0x200000)
	, envelope_map()
	, wave_map()
	, ins_transpose()
	, pitch_map()
	, ins_type()
{
}

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
 *                 If <s> is ff, this is the final value
 *   7F <pp> - loop to <p*4>
 */
void MD_Data::read_pitch(uint16_t id, const Tag& tag)
{
	std::vector<uint8_t> env_data;
	int loop_pos = -1;
	if(!tag.size())
	{
		std::cerr << stringf("warning: empty pitch envelope @M%d\n", id);
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
void MD_Data::add_pitch_node(const char* s, std::vector<uint8_t>* env_data)
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
	//std::cout << stringf("initial:%.2f, target=%.2f, len=%d", initial, target, length) << "\n";
	while(length > 0)
	{
		double delta = (target-counter)/length;
		uint16_t env_len = (length > 255) ? 255 : length;
		int16_t env_initial = counter * 256;
		int16_t env_delta = std::trunc(delta * 256);
		env_initial = (env_initial > 0x7eff) ? 0x7eff : env_initial;
		env_delta = (env_delta > 127) ? 127 : (env_delta < -128) ? -128: env_delta;
		//std::cout << stringf("len:%d, from:%04x, chg:%d", env_len, env_initial, env_delta) << "\n";
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
void MD_Data::add_pitch_vibrato(const char* s, std::vector<uint8_t>* env_data)
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
	else
		printf("Invalid vibrato definition: '%s'\n", s);

	vibrato_depth += vibrato_base;
	add_pitch_node(stringf("%f>%f:%d", vibrato_base, vibrato_depth, vibrato_rate).c_str(), env_data);
	add_pitch_node(stringf("%f>%f:%d", vibrato_depth, -vibrato_depth, vibrato_rate*2).c_str(), env_data);
	add_pitch_node(stringf("%f>%f:%d", -vibrato_depth, vibrato_base, vibrato_rate).c_str(), env_data);
}

void MD_Data::read_wave(uint16_t id, const Tag& tag)
{
	std::vector<uint8_t> env_data;
	int wave_header_id;
	Wave_Rom::Sample sample;

	wave_header_id = wave_map[id] = wave_rom.add_sample(tag);
	sample = wave_rom.get_sample_headers().at(wave_header_id);
	// TODO: rate. VGM playback reads directly from the wave_header.
	env_data.push_back((sample.start_pos >> 16) & 0xff);
	env_data.push_back((sample.start_pos >> 8) & 0xff);
	env_data.push_back((sample.start_pos >> 0) & 0xff);
	env_data.push_back((sample.size >> 16) & 0xff);
	env_data.push_back((sample.size >> 8) & 0xff);
	env_data.push_back((sample.size >> 0) & 0xff);
	envelope_map[id] = add_unique_data(env_data);
	ins_type[id] = INS_PCM;
}


std::string MD_Data::dump_data(uint16_t id, uint16_t mapped_id)
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

void MD_Data::read_envelope(uint16_t id, const Tag& tag)
{
	auto it = tag.begin();
	std::string type = *it++;
	if(iequal("psg", type))
	{
		read_psg(id, Tag(it, tag.end()));
		std::cout << "read PSG envelope " << dump_data(id, envelope_map[id]) << "\n";
		return;
	}
	else if(iequal("fm", type))
	{
		read_fm_4op(id, Tag(it, tag.end()));
		std::cout << "read FM envelope " << dump_data(id, envelope_map[id]) << "\n";
		return;
	}
	else if(iequal("2op", type))
	{
		read_fm_2op(id, Tag(it, tag.end()));
		std::cout << "read 2op envelope " << dump_data(id, envelope_map[id]) << "\n";
		return;
	}
	else if(iequal("pcm", type))
	{
		read_wave(id, Tag(it, tag.end()));
		std::cout << "read wave sample " << dump_data(id, envelope_map[id]) << "\n";
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
	pitch_map.clear();
	wave_map.clear();
	ins_type.clear();
	wave_rom.set_include_paths(song.get_tag("include_path"));
	// add a unique psg envelope to prevent possible errors
	envelope_map[0] = add_unique_data({0x10, 0x01, 0x1f, 0x00});
	ins_transpose[0] = 0;
	ins_type[0] = MD_Data::INS_UNDEFINED;
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

//! Constructs a MD_Channel.
MD_Channel::MD_Channel(MD_Driver& driver, int id)
	: Player(*driver.song, driver.song->get_track(id)),
	driver(&driver),
	channel_id(id),
	slur_flag(0),
	key_on_flag(0),
	note_pitch(0xffff),
	porta_value(0),
	last_pitch(0),
	pitch_env_data(0),
	pitch_env_delay(0),
	pitch_env_pos(0),
	pitch(0),
	ins_transpose(0),
	con(0),
	tl()
{
}

//! Platform-exclusive command parser
uint32_t MD_Channel::parse_platform_event(const Tag& tag, int16_t* platform_state)
{
	if(iequal(tag[0], "mode"))
	{
		if(tag.size() < 2)
			error("not enough parameters for 'mode' command");
		platform_state[EVENT_CHANNEL_MODE] = std::strtol(tag[1].c_str(), 0, 0);
		return (1 << EVENT_CHANNEL_MODE);
	}
	else if(iequal(tag[0], "lfo"))
	{
		if(tag.size() < 3)
			error("not enough parameters for 'lfo' command");
		platform_state[EVENT_LFO] = (std::strtol(tag[1].c_str(), 0, 0) << 3) | (std::strtol(tag[2].c_str(), 0, 0));
		return (1 << EVENT_LFO);
	}
	else if(iequal(tag[0], "lfodelay"))
	{
		if(tag.size() < 2)
			error("not enough parameters for 'lfodelay' command");
		platform_state[EVENT_LFO_DELAY] = std::strtol(tag[1].c_str(), 0, 0);
		return (1 << EVENT_LFO_DELAY);
	}
	else if(iequal(tag[0], "lfoconfig"))
	{
		if(tag.size() < 2)
			error("not enough parameters for 'lfoconfig' command");
		platform_state[EVENT_LFO_CONFIG] = std::strtol(tag[1].c_str(), 0, 0);
		return (1 << EVENT_LFO_CONFIG);
	}
	else if(iequal(tag[0], "fm3"))
	{
		if(tag.size() < 2)
			error("not enough parameters for 'fm3' command");
		platform_state[EVENT_FM3] = (std::strtol(tag[1].c_str(), 0, 2) ^ 0x0f) & 0x0f;
		return (1 << EVENT_FM3);
	}
	else if(iequal(tag[0], "write"))
	{
		if(tag.size() < 3)
			error("not enough parameters for 'write' command");
		platform_state[EVENT_WRITE_ADDR] = std::strtol(tag[1].c_str(), 0, 0);
		platform_state[EVENT_WRITE_DATA] = std::strtol(tag[2].c_str(), 0, 0);
		return (1 << EVENT_WRITE_ADDR);
	}
	return 0;
}

//! Event handler
void MD_Channel::write_event()
{
	switch(event.type)
	{
		case Event::SEGNO:
			driver->loop_trigger = true;
			break;
		case Event::TIE:
			slur_flag = true;
		case Event::NOTE:
			if(event.type == Event::NOTE)
				key_on_flag = true;
			if(get_update_flag(Event::INS))
			{
				v_set_ins();
				set_vol();
				clear_update_flag(Event::INS);
				clear_update_flag(Event::VOL_FINE);
			}
			else if(get_update_flag(Event::VOL_FINE))
			{
				set_vol();
				clear_update_flag(Event::VOL_FINE);
			}
			if(event.type != Event::TIE)
			{
				note_pitch = (event.param + get_var(Event::TRANSPOSE) + ins_transpose)<<8;
				note_pitch += get_var(Event::DETUNE);
			}
			if(!slur_flag)
			{
				key_off();
			}
			break;
		case Event::END:
			driver->loop_trigger = true;
		case Event::REST: // continue
			key_off();
			break;
		case Event::SLUR:
			slur_flag = 1;
			break;
		case Event::TEMPO:
		case Event::TEMPO_BPM:
			if(bpm_flag())
			{
				driver->tempo_delta = driver->tempo_convert(get_var(Event::TEMPO));
				printf("set tempo to %02x (%d bpm)\n", driver->tempo_delta, get_var(Event::TEMPO));
			}
			else
			{
				driver->tempo_delta = get_var(Event::TEMPO);
				printf("set tempo to %02x (direct)\n", driver->tempo_delta);
			}
			break;
		case Event::PLATFORM:
			if(get_platform_flag(EVENT_FM3))
			{
				last_pitch = 0xffff;
				v_key_off();
				clear_platform_flag(EVENT_FM3);
			}
			v_set_type();
			break;
		case Event::PAN:
			v_set_pan();
			break;
		default:
			break;
	}
}

void MD_Channel::update_pitch()
{
	if(get_var(Event::PORTAMENTO))
	{
		int16_t difference = note_pitch - porta_value;
		int16_t step = difference >> 8;
		if(difference < 0)
			step--;
		else
			step++;
		porta_value += (step*get_var(Event::PORTAMENTO)) >> 1;
		if(((note_pitch - porta_value) ^ difference) < 0)
			porta_value = note_pitch;
		pitch = porta_value;
	}
	else
	{
		porta_value = note_pitch;
	}
	pitch = porta_value;
	if(get_var(Event::PITCH_ENVELOPE))
	{
		if(key_on_flag || !pitch_env_data)
		{
			int pitch_id = driver->data.pitch_map[get_var(Event::PITCH_ENVELOPE)];
			if(!pitch_id)
			{
				error("Invalid pitch envelope");
			}
			else
			{
				pitch_env_data = &driver->data.data_bank.at(pitch_id);
				pitch_env_pos = 0;
				pitch_env_delay = 0;
			}
		}
		uint16_t pos = pitch_env_pos << 2;
		uint16_t command = (pitch_env_data->at(pos) << 8) | (pitch_env_data->at(pos+1));
		int8_t delta = pitch_env_data->at(pos+2);
		uint8_t length = pitch_env_data->at(pos+3);
		//std::cout << stringf("pos=%d, time=%d/%d, delta=%d, ", pos, pitch_env_delay, length, delta);
		if(pitch_env_delay == 0)
			pitch_env_value = command;

		if(pitch_env_delay == length)
		{
			int16_t next_command = (pitch_env_data->at(pos+4) << 8) | (pitch_env_data->at(pos+5));
			if(next_command >= 0x7f00)
				pitch_env_pos = next_command & 0xff;
			else
				pitch_env_pos++;
			pitch_env_delay = 0;
		}
		else if(pitch_env_delay < 0xfe)
			pitch_env_delay++;

		pitch_env_value += delta;
		//std::cout << stringf("val=%04x, next_pos=%d\n", pitch_env_value, pitch_env_pos);
		pitch += pitch_env_value;
	}
}

//! Write a single FM operator
uint8_t MD_Channel::write_fm_operator(int idx, int bank, int id, const std::vector<uint8_t>& idata)
{
	uint8_t retval;
	driver->ym2612_w(bank, 0x30, id, idx, idata[idx]);
	retval = idata[4+idx];
	driver->ym2612_w(bank, 0x50, id, idx, idata[8+idx]);
	driver->ym2612_w(bank, 0x60, id, idx, idata[12+idx]);
	driver->ym2612_w(bank, 0x70, id, idx, idata[16+idx]);
	driver->ym2612_w(bank, 0x80, id, idx, idata[20+idx]);
	driver->ym2612_w(bank, 0x90, id, idx, idata[24+idx]);
	return retval;
}

//! Write a 4op FM instrument
void MD_Channel::write_fm_4op(int bank, int id)
{
	uint16_t ins_id = get_var(Event::INS);
	if(driver->data.ins_type[ins_id] != MD_Data::INS_FM)
		return;

	const std::vector<uint8_t>& idata = driver->data.data_bank.at(driver->data.envelope_map[ins_id]);
	driver->ym2612_w(bank, 0x40, id, 0, 0x7f); // tl=max
	driver->ym2612_w(bank, 0x40, id, 1, 0x7f);
	driver->ym2612_w(bank, 0x40, id, 2, 0x7f);
	driver->ym2612_w(bank, 0x40, id, 3, 0x7f);
	driver->ym2612_w(bank, 0x28, id, 0, 0); // key off
	for(int op=0; op<4; op++)
	{
		// load instrument parameters
		tl[op] = write_fm_operator(op, bank, id, idata);
	}
	// set fb/con
	driver->ym2612_w(bank, 0xb0, id, 0, idata[28]);
	con = idata[28] & 7;
	// set ins transpose
	ins_transpose = driver->data.ins_transpose[ins_id];
}

uint16_t MD_Channel::get_fm_pitch(uint16_t pitch) const
{
	static const uint16_t freqtab[13] = {644, 681, 722, 765, 810, 858, 910, 964, 1021, 1081, 1146, 1214, 1288};
	uint8_t note = (pitch >> 8);
	uint8_t octave = note / 12;
	uint8_t detune = pitch & 0xff;
	const uint16_t* base = &freqtab[note % 12];
	uint16_t set_pitch = base[0] + (((base[1] - base[0]) * detune) >> 8);
	set_pitch += (octave & 7) << 11;
	return set_pitch;
}

uint16_t MD_Channel::get_psg_pitch(uint16_t pitch) const
{
	static const uint16_t freqtab[13] = {851, 803, 758, 715, 675, 637, 601, 568, 536, 506, 477, 450, 425};
	uint8_t note = (pitch >> 8);
	uint8_t octave = (note / 12)-1;
	uint8_t detune = pitch & 0xff;
	const uint16_t* base = &freqtab[note % 12];
	uint16_t set_pitch = base[0] + (((base[1] - base[0]) * detune) >> 8);
	set_pitch >>= octave;
	return set_pitch;
}

void MD_Channel::key_on()
{
	if(get_platform_var(EVENT_FM3))
	{
		driver->fm3_mask |= ~get_platform_var(EVENT_FM3);
		driver->ym2612_w(0, 0x28, 2, 0, driver->fm3_mask);
	}
	else
	{
		key_on_pcm();
		v_key_on();
	}
}

void MD_Channel::key_off()
{
	if(get_platform_var(EVENT_FM3))
	{
		driver->fm3_mask &= get_platform_var(EVENT_FM3);
		driver->ym2612_w(0, 0x28, 2, 0, driver->fm3_mask);
	}
	else
	{
		key_off_pcm();
		v_key_off();
	}
}

void MD_Channel::set_pitch()
{
	if(get_platform_var(EVENT_FM3))
		set_pitch_fm3();
	else
		v_set_pitch();
}

void MD_Channel::set_vol()
{
	if(get_platform_var(EVENT_FM3))
		set_vol_fm3();
	else
		v_set_vol();
}

void MD_Channel::set_pitch_fm3()
{
	int mask = get_platform_var(EVENT_FM3);
	for(int op=0; op<4; op++)
	{
		uint16_t val = get_fm_pitch(pitch);
		if(~mask & 1)
			driver->ym2612_w(0, 0xa8, 0, op, val);
		mask >>= 1;
	}
}

void MD_Channel::set_vol_fm3()
{
	static const uint8_t opn_con_op[8] = {3,3,3,3,2,1,1,0};
	static const uint8_t fm3_op_mask[4] = {1,4,2,8};
	int vol = get_var(Event::VOL_FINE);
	if(coarse_volume_flag())
	{
		if(vol > 15)
			vol = 0;
		else
			vol = 15-vol;
		// 2db per step
		vol = 2 + vol*3 - vol/3;
	}

	int mask = get_platform_var(EVENT_FM3);
	for(int op=3; op>=0; op--)
	{
		uint8_t max_tl = driver->fm3_tl[op];
		if(op >= opn_con_op[driver->fm3_con])
			max_tl += vol;
		if(max_tl > 127)
			max_tl = 127;
		if(fm3_op_mask[op] & ~mask)
			driver->ym2612_w(0, 0x40, 2, op, max_tl);
	}
}

void MD_Channel::key_on_pcm()
{
	int16_t ins_id = get_var(Event::INS);
	if(driver->data.ins_type[ins_id] == MD_Data::INS_PCM)
	{
		int wave_header_id = driver->data.wave_map[ins_id];
		Wave_Rom::Sample sample = driver->data.wave_rom.get_sample_headers().at(wave_header_id);
		driver->last_pcm_channel = channel_id;
		driver->ym2612_w(0, 0x2b, 0, 0, 0x80); // DAC enable
		if(driver->vgm_writer)
		{
			driver->vgm_writer->dac_start(0x00, sample.start_pos, sample.size, sample.rate);
		}
	}
}

void MD_Channel::key_off_pcm()
{
	if(driver->last_pcm_channel == channel_id)
	{
		driver->ym2612_w(0, 0x2b, 0, 0, 0x00); // DAC disable
		if(driver->vgm_writer)
		{
			driver->vgm_writer->dac_stop(0x00);
		}
		driver->last_pcm_channel = -1;
	}
}

//! Update a channel
void MD_Channel::update(int seq_ticks)
{
	while(seq_ticks--)
		play_tick();
	v_update_envelope();
	update_pitch();

	if(pitch != last_pitch)
		set_pitch();
	last_pitch = pitch;

	if(key_on_flag)
	{
		if(!slur_flag)
			key_on();
		slur_flag = false;
		key_on_flag = false;
	}
}

//! Constructs a MD_FM.
MD_FM::MD_FM(MD_Driver& driver, int track_id, int channel_id)
	: MD_Channel(driver, track_id),
	type(MD_FM::NORMAL),
	bank(channel_id / 3),
	id(channel_id % 3),
	pan_lfo(0xc0) // L/R enabled
{
	driver.ym2612_w(bank, 0x40, id, 0, 0x7f); //tl=max
	driver.ym2612_w(bank, 0x40, id, 1, 0x7f);
	driver.ym2612_w(bank, 0x40, id, 2, 0x7f);
	driver.ym2612_w(bank, 0x40, id, 3, 0x7f);
	driver.ym2612_w(bank, 0x28, id, 0, 0); //key off
	driver.ym2612_w(bank, 0xb4, id, 0, pan_lfo); //enable panning
}

void MD_FM::v_set_ins()
{
	write_fm_4op(bank, id);
	if(bank == 0 && id == 2)
	{
		driver->fm3_con = con;
		std::memcpy(driver->fm3_tl, tl, sizeof(driver->fm3_tl));
	}
}

void MD_FM::v_set_pan()
{
	pan_lfo &= 0x0f; // mask out panning
	int8_t mml_pan = get_var(Event::PAN);
	if(mml_pan >= 0 && mml_pan < 4)
		pan_lfo |= mml_pan << 6; // 1=right, 2=left, 3=both
	else
		error("Unsupported panning value");
	driver->ym2612_w(bank, 0xb4, id, 0, pan_lfo);
}

void MD_FM::v_set_vol()
{
	static const uint8_t opn_con_op[8] = {3,3,3,3,2,1,1,0};
	int vol = get_var(Event::VOL_FINE);
	if(coarse_volume_flag())
	{
		if(vol > 15)
			vol = 0;
		else
			vol = 15-vol;
		// 2db per step
		vol = 2 + vol*3 - vol/3;
	}

	for(int op=3; op>=0; op--)
	{
		uint8_t max_tl = tl[op];
		if(op >= opn_con_op[con])
			max_tl += vol;
		if(max_tl > 127)
			max_tl = 127;
		driver->ym2612_w(bank, 0x40, id, op, max_tl);
	}
}

void MD_FM::v_key_on()
{
	driver->ym2612_w(bank, 0x28, id, 0, 15);
}

void MD_FM::v_key_off()
{
	driver->ym2612_w(bank, 0x28, id, 0, 0);
}

void MD_FM::v_set_pitch()
{
	uint16_t val = get_fm_pitch(pitch);
	driver->ym2612_w(bank, 0xa0, id, 0, val);
}

void MD_FM::v_set_type()
{
	// FM3 special mode
	if(bank == 0 && id == 2)
	{
		if(get_platform_var(EVENT_FM3))
			driver->ym2612_w(bank, 0x27, id, 0, 0x40);
		else
			driver->ym2612_w(bank, 0x27, id, 0, 0x00);
	}
}

void MD_FM::v_update_envelope()
{
	// Nothing needs to be done for FM
}

//! Constructs a MD_PSG.
MD_PSG::MD_PSG(MD_Driver& driver, int track_id, int channel_id)
	: MD_Channel(driver, track_id),
	id(channel_id % 4),
	env_data(&driver.data.data_bank.at(0)),
	env_keyoff(false),
	env_pos(3),
	env_delay(0)
{
	driver.sn76489_w(1, id, 15); // disable output
}

void MD_PSG::set_envelope(std::vector<uint8_t>* idata)
{
	env_data = idata;
	env_pos = 0;
	env_delay = 15;
}

void MD_PSG::v_key_on()
{
}

void MD_PSG::v_key_off()
{
	// Mute channel if at the end.
	if(event.type == Event::END || get_platform_var(EVENT_FM3))
		driver->sn76489_w(1, id, 15);
	event.type = Event::REST;
	env_keyoff = true;
}


void MD_PSG::v_update_envelope()
{
	if(!is_enabled())
		return;
	// reset envelope on keyon
	if(key_on_flag && !get_platform_var(EVENT_FM3))
	{
		env_pos = 0;
		env_delay = 0x1f;
		env_keyoff = 0;
	}
	// faster decay if key off
	if(env_delay < 0x20 || env_keyoff)
	{
		// sustain command
		if(env_data->at(env_pos) == 0x01 && env_keyoff)
		{
			env_pos++;
			env_keyoff = 0;
		}
		// jump command
		else if(env_data->at(env_pos) == 0x02 && !env_keyoff)
		{
			env_pos = env_data->at(env_pos+1);
		}
		// volume + length
		if(env_data->at(env_pos) > 0x0f)
		{
			env_delay = env_data->at(env_pos);
			v_set_vol();
			env_pos++;
		}
		// unknown command or stop command
		else if(event.type == env_keyoff)
		{
			driver->sn76489_w(1, id, 15); // mute
			env_keyoff = false; // remove keyoff flag to optimize writes
		}
	}
	else
	{
		env_delay -= 0x10;
	}
}

void MD_PSG::v_set_pan()
{
	error("Panning not supported for PSG channels");
}

//! Constructs a MD_PSGMelody.
MD_PSGMelody::MD_PSGMelody(MD_Driver& driver, int track_id, int channel_id)
	: MD_PSG(driver, track_id, channel_id),
	type(MD_PSGMelody::NORMAL)
{
}

void MD_PSGMelody::v_set_ins()
{
	int16_t ins_id = get_var(Event::INS);
	if(driver->data.ins_type[ins_id] != MD_Data::INS_PSG)
		return;
	std::vector<uint8_t>* idata = &driver->data.data_bank.at(driver->data.envelope_map[ins_id]);
	set_envelope(idata);
}

void MD_PSGMelody::v_set_vol()
{
	uint8_t vol = 15 - get_var(Event::VOL_FINE);
	vol += env_delay & 0x0f;
	if(vol > 15)
		vol = 15;
	driver->sn76489_w(1, id, vol);
}

void MD_PSGMelody::v_set_pitch()
{
	uint16_t val = get_psg_pitch(pitch);
	driver->sn76489_w(0, id, val);
}

void MD_PSGMelody::v_set_type()
{
}

//! Constructs a MD_PSGNoise.
MD_PSGNoise::MD_PSGNoise(MD_Driver& driver, int track_id, int channel_id)
	: MD_PSG(driver, track_id, channel_id),
	type(MD_PSGNoise::NORMAL)
{
}

void MD_PSGNoise::v_set_ins()
{
	int16_t ins_id = get_var(Event::INS);
	if(driver->data.ins_type[ins_id] != MD_Data::INS_PSG)
		return;
	std::vector<uint8_t>* idata = &driver->data.data_bank.at(driver->data.envelope_map[ins_id]);
	set_envelope(idata);
}

void MD_PSGNoise::v_set_vol()
{
	uint8_t vol = 15 - get_var(Event::VOL_FINE);
	vol += env_delay & 0x0f;
	if(vol > 15)
		vol = 15;
	driver->sn76489_w(1, id, vol);
}

void MD_PSGNoise::v_set_pitch()
{
	if(type == 1)
	{
		uint16_t val = get_psg_pitch(pitch);
		driver->sn76489_w(0, 2, val);
		driver->sn76489_w(0, 3, 7);
	}
	else
	{
		driver->sn76489_w(0, 3, (pitch>>8)&7);
	}
}

void MD_PSGNoise::v_set_type()
{
	if(get_platform_flag(EVENT_CHANNEL_MODE))
	{
		type = get_platform_var(EVENT_CHANNEL_MODE);
		clear_platform_flag(EVENT_CHANNEL_MODE);
	}
}

//! Constructs a dummy channel.
MD_Dummy::MD_Dummy(MD_Driver& driver, int track_id, int channel_id)
	: MD_Channel(driver, track_id),
	id(channel_id)
{
}

void MD_Dummy::v_set_ins()
{
}

void MD_Dummy::v_set_pan()
{
}

void MD_Dummy::v_set_vol()
{
}

void MD_Dummy::v_key_on()
{
}

void MD_Dummy::v_key_off()
{
}

void MD_Dummy::v_set_pitch()
{
}

void MD_Dummy::v_set_type()
{
}

void MD_Dummy::v_update_envelope()
{
}



//! constructs a MD_Driver.
/*!
 * \param rate Sample rate.
 * \param vgm Optional VGM writer. Set to nullptr to disable VGM logging.
 * \param is_pal Use 50hz sequence update rate
 */
MD_Driver::MD_Driver(unsigned int rate, VGM_Writer* vgm, bool is_pal)
	: Driver(rate, vgm)
	, vgm_writer(vgm)
	, tempo_delta(255)
	, tempo_counter(0)
	, fm3_mask(0)
	, fm3_con(0)
	, fm3_tl()
	, last_pcm_channel(-1)
	, loop_trigger(0)
{
	if(vgm)
	{
		vgm->poke32(0x2c, 7670454); // YM2612
		vgm->poke32(0x0c, 3579575); // SN76489 (SEGA PSG)
		vgm->poke16(0x28, 0x0009);
		vgm->poke8(0x2a, 0x10);
		vgm->poke8(0x2b, 0x03);
	}
	seq_rate = (is_pal) ? 50.0 : 60.0;
	seq_counter = seq_delta = rate/seq_rate;
	pcm_counter = pcm_delta = rate/100.0; //not used anyway
}

//! Converts BPM to fractional tempo
uint8_t MD_Driver::tempo_convert(uint16_t bpm)
{
	double base_tempo = 120. / (song->get_ppqn() * (1./seq_rate));
	double fract = (bpm / base_tempo)*256.;
	uint16_t new_tempo = (fract + 0.5) - 1;
	return std::min<uint16_t>(0xff, new_tempo);
}

//! Initiate playback
void MD_Driver::play_song(Song& song)
{
	this->song = &song;
	channels.clear();
	data.read_song(song);
	if(vgm_writer)
	{
		const std::vector<uint8_t>& dbdata = data.wave_rom.get_rom_data();
		vgm_writer->datablock(0x00,
			dbdata.size() - data.wave_rom.get_free_bytes(),
			dbdata.data(),
			dbdata.size());
		vgm_writer->dac_setup(0x00, 0x02, 0x00, 0x2a, 0x00);
	}
	// setup tempo
	tempo_delta = 128;
	tempo_counter = 0;
	loop_trigger = 0;
	// setup channels
	for(auto it=song.get_track_map().begin(); it != song.get_track_map().end(); it++)
	{
		int id = it->first;
		if(id < 6)
			channels.push_back(std::make_unique<MD_FM>(*this, id, id));
		else if(id < 9)
			channels.push_back(std::make_unique<MD_PSGMelody>(*this, id, id-6));
		else if(id < 10)
			channels.push_back(std::make_unique<MD_PSGNoise>(*this, id, id-6));
		else if(id < 16)
			channels.push_back(std::make_unique<MD_Dummy>(*this, id, id-10));
	}
}

//! Reset sound chips, etc.
void MD_Driver::reset()
{
	channels.clear();
}

void MD_Driver::seq_update()
{
	uint16_t next_counter = tempo_counter + tempo_delta + 1;
	uint8_t tempo_step = next_counter >> 7;
	tempo_counter = next_counter & 0x7f;
	for(auto it = channels.begin(); it != channels.end(); it++)
	{
		MD_Channel* ch = it->get();
		if(ch->is_enabled())
			ch->update(tempo_step);
	}
}

//! Reset loop count
void MD_Driver::reset_loop_count()
{
	if(!channels.size())
		return;

	for(auto it = channels.begin(); it != channels.end(); it++)
	{
		it->get()->reset_loop_count();
	}
}

//! Return true if driver is currently playing a song, false otherwise.
bool MD_Driver::is_playing()
{
	if(!channels.size())
		return false;

	for(auto it = channels.begin(); it != channels.end(); it++)
	{
		MD_Channel* ch = it->get();
		if(ch->is_enabled())
			return true;
	}
	return false;
}

//! Get loop count (untested)
int MD_Driver::loop_count()
{
	int loop_count = INT_MAX;
	if(!channels.size())
		return 0;

	for(auto it = channels.begin(); it != channels.end(); it++)
	{
		MD_Channel* ch = it->get();
		if(ch->is_enabled() && ch->get_loop_count() < loop_count)
			loop_count = ch->get_loop_count();
	}
	return loop_count;
}

//! Updates the sound driver state and return delta until the next event.
double MD_Driver::play_step()
{
	if(seq_counter < 1)
	{
		// update tracks
		seq_counter += seq_delta;
		seq_update();
	}
	if(pcm_counter < 1)
	{
		// update pcm
		pcm_counter += pcm_delta;
	}
	if(loop_trigger && loop_count() == 0)
	{
		set_loop();
		reset_loop_count();
		loop_trigger = 0;
	}
	// get the time to the next event
	double next_delta = std::min(seq_counter, pcm_counter);
	seq_counter -= next_delta;
	pcm_counter -= next_delta;
	return next_delta;
}
