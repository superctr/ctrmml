#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <cctype>
#include <climits>

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

//! Constructs a MD_Channel.
MD_Channel::MD_Channel(MD_Driver& driver, int id)
	: Player(*driver.song, driver.song->get_track(id)),
	driver(&driver),
	slur_flag(0),
	pitch(0),
	ins_transpose(0),
	pan_lfo(0xc0), // L/R enabled
	con(0)
{
}

//! Event handler
void MD_Channel::write_event()
{
	switch(event.type)
	{
		case Event::TIE:
			slur_flag = true;
		case Event::NOTE:
			if(get_update_flag(Event::INS))
			{
				set_ins();
				clear_update_flag(Event::INS);
			}
			if(get_update_flag(Event::VOL_FINE))
			{
				set_ins();
				clear_update_flag(Event::VOL_FINE);
			}
			pitch = (event.param + get_var(Event::TRANSPOSE))<<8;
			pitch += get_var(Event::DETUNE);
			if(!slur_flag)
			{
				key_off();
				set_pitch();
				key_on();
			}
			break;
		case Event::REST:
		case Event::END:
			key_off();
			break;
		case Event::TEMPO:
		case Event::TEMPO_BPM:
			// todo: correct BPM calc
			if(bpm_flag())
				driver->tempo_delta = (get_var(Event::TEMPO)*256 / 150) - 1;
			else
				driver->tempo_delta = get_var(Event::TEMPO);
			printf("set tempo to %02x\n", driver->tempo_delta);
			break;
		case Event::CHANNEL_MODE:
			set_type();
			break;
		default:
			break;
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
	const std::vector<uint8_t>& idata = driver->data.data_bank.at(driver->data.envelope_map[get_var(Event::INS)]);
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
}

uint16_t MD_Channel::get_fm_pitch() const
{
	static const uint16_t freqtab[13] = {644, 681, 722, 765, 810, 858, 910, 964, 1021, 1081, 1146, 1214, 1288};
	uint8_t note = (pitch >> 8) + ins_transpose;
	uint8_t octave = note / 12;
	uint8_t detune = pitch & 0xff;
	const uint16_t* base = &freqtab[note % 12];
	uint16_t set_pitch = base[0] + (((base[1] - base[0]) * detune) >> 8);
	set_pitch += (octave & 7) << 11;
	return set_pitch;
}

uint16_t MD_Channel::get_psg_pitch() const
{
	static const uint16_t freqtab[13] = {851, 803, 758, 715, 675, 637, 601, 568, 536, 506, 477, 450, 425};
	uint8_t note = (pitch >> 8) + ins_transpose;
	uint8_t octave = note / 12;
	uint8_t detune = pitch & 0xff;
	const uint16_t* base = &freqtab[note % 12];
	uint16_t set_pitch = base[0] + (((base[1] - base[0]) * detune) >> 8);
	set_pitch >>= octave;
	return set_pitch;
}

//! Update a channel
void MD_Channel::update()
{
	play_tick();
	update_envelope();
}

//! Constructs a MD_FM.
MD_FM::MD_FM(MD_Driver& driver, int track_id, int channel_id)
	: MD_Channel(driver, track_id),
	type(MD_FM::NORMAL),
	bank(channel_id / 3),
	id(channel_id % 3)
{
	driver.ym2612_w(bank, 0x40, id, 0, 0x7f); //tl=max
	driver.ym2612_w(bank, 0x40, id, 1, 0x7f);
	driver.ym2612_w(bank, 0x40, id, 2, 0x7f);
	driver.ym2612_w(bank, 0x40, id, 3, 0x7f);
	driver.ym2612_w(bank, 0x28, id, 0, 0); //key off
	driver.ym2612_w(bank, 0xb4, id, 0, pan_lfo); //enable panning
}

void MD_FM::set_ins()
{
	write_fm_4op(bank, id);
}

void MD_FM::set_vol()
{
	static const uint8_t opn_con_op[8] = {3,3,3,3,2,1,1,0};
	int vol = get_var(Event::VOL_FINE);
	if(coarse_volume_flag())
	{
		if(vol > 15)
			vol = 15;
		else // 2db per step
			vol = 2 + (15-vol)*3 - (15-vol)/3;
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

void MD_FM::key_on()
{
	driver->ym2612_w(bank, 0x28, id, 0, 15);
}

void MD_FM::key_off()
{
	driver->ym2612_w(bank, 0x28, id, 0, 0);
}

void MD_FM::set_pitch()
{
	uint16_t val = get_fm_pitch();
	driver->ym2612_w(bank, 0xa0, id, 0, val);
}

void MD_FM::set_type()
{
	type = get_var(Event::CHANNEL_MODE);
	printf("<FM%d> set type to %d\n", bank*3 + id, type);
}

void MD_FM::update_envelope()
{
	// Nothing needs to be done for FM
}

//! Constructs a MD_PSG.
MD_PSG::MD_PSG(MD_Driver& driver, int track_id, int channel_id)
	: MD_Channel(driver, track_id),
	id(channel_id % 4),
	env_data(&driver.data.data_bank.at(0)),
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

void MD_PSG::update_envelope()
{
	// faster decay if key off
	if(env_delay < 0x20 || event.type == Event::REST)
	{
		// sustain command
		if(env_data->at(env_pos) == 0x01 && event.type == Event::REST)
		{
			env_pos++;
			event.type = Event::TIE; // remove keyoff flag to set normal delay
		}
		// jump command
		else if(env_data->at(env_pos) == 0x02 && event.type != Event::REST)
		{
			env_pos = env_data->at(env_pos+1);
		}
		// volume + length
		if(env_data->at(env_pos) > 0x0f)
		{
			env_delay = env_data->at(env_pos);
			set_vol();
			env_pos++;
		}
		// unknown command or stop command
		else if(event.type == Event::REST)
		{
			driver->sn76489_w(1, id, 15); // mute
			event.type = Event::TIE; // remove keyoff flag to optimize writes
		}
	}
	else
	{
		env_delay -= 0x10;
	}
}

//! Constructs a MD_PSGMelody.
MD_PSGMelody::MD_PSGMelody(MD_Driver& driver, int track_id, int channel_id)
	: MD_PSG(driver, track_id, channel_id),
	type(MD_PSGMelody::NORMAL)
{
}

void MD_PSGMelody::set_ins()
{
	std::vector<uint8_t>* idata = &driver->data.data_bank.at(driver->data.envelope_map[get_var(Event::INS)]);
	set_envelope(idata);
}

void MD_PSGMelody::set_vol()
{
	uint8_t vol = 15 - get_var(Event::VOL_FINE);
	vol += env_delay & 0x0f;
	if(vol > 15)
		vol = 15;
	driver->sn76489_w(1, id, vol);
}

void MD_PSGMelody::key_on()
{
	env_pos = 0;
	env_delay = 0x1f;
}

void MD_PSGMelody::key_off()
{
	// Mute channel if at the end.
	if(event.type == Event::END)
		driver->sn76489_w(1, id, 15);
	event.type = Event::REST;
}

void MD_PSGMelody::set_pitch()
{
	uint16_t val = get_psg_pitch();
	driver->sn76489_w(0, id, val);
}

void MD_PSGMelody::set_type()
{
	type = get_var(Event::CHANNEL_MODE);
	printf("<PSG%d> set type to %d\n", id, type);
}

//! Constructs a MD_PSGNoise.
MD_PSGNoise::MD_PSGNoise(MD_Driver& driver, int track_id, int channel_id)
	: MD_PSG(driver, track_id, channel_id),
	type(MD_PSGNoise::NORMAL)
{
}

void MD_PSGNoise::set_ins()
{
	std::vector<uint8_t>* idata = &driver->data.data_bank.at(driver->data.envelope_map[get_var(Event::INS)]);
	set_envelope(idata);
}

void MD_PSGNoise::set_vol()
{
	uint8_t vol = 15 - get_var(Event::VOL_FINE);
	vol += env_delay & 0x0f;
	if(vol > 15)
		vol = 15;
	driver->sn76489_w(1, id, vol);
}

void MD_PSGNoise::key_on()
{
	env_pos = 0;
	env_delay = 0x1f;
}

void MD_PSGNoise::key_off()
{
	// Mute channel if at the end.
	if(event.type == Event::END)
		driver->sn76489_w(1, id, 15);
	event.type = Event::REST;
}

void MD_PSGNoise::set_pitch()
{
	if(type == 1)
	{
		uint16_t val = get_psg_pitch();
		driver->sn76489_w(0, 2, val);
		driver->sn76489_w(0, 3, 7);
	}
	else
	{
		driver->sn76489_w(0, 3, (pitch>>8)&7);
	}
}

void MD_PSGNoise::set_type()
{
	type = get_var(Event::CHANNEL_MODE);
	printf("<PSGN> set type to %d\n", type);
}

//! Driver constructor
MD_Driver::MD_Driver(unsigned int rate, VGM_Writer* vgm, bool is_pal)
	: Driver(rate, vgm), tempo_delta(0), tempo_counter(0)
{
	seq_rate = rate/60.0;
	pcm_rate = rate/16000.0;
}

//! Initiate playback
void MD_Driver::play_song(Song& song)
{
	this->song = &song;
	channels.clear();
	data.read_song(song);
	// setup channels
	for(auto it=song.get_track_map().begin(); it != song.get_track_map().end(); it++)
	{
		int id = it->first;
		if(id < 6)
			channels.push_back(std::make_unique<MD_FM>(*this, id, id));
		else if(id < 9)
			channels.push_back(std::make_unique<MD_PSGMelody>(*this, id, id-6));
		else if(id == 9)
			channels.push_back(std::make_unique<MD_PSGNoise>(*this, id, id-6));
	}
}

//! Reset sound chips, etc.
void MD_Driver::reset()
{
	channels.clear();
}

void MD_Driver::seq_update()
{
	for(auto it = channels.begin(); it != channels.end(); it++)
	{
		MD_Channel* ch = it->get();
		if(ch->is_enabled())
			ch->update();
	}
}

bool MD_Driver::is_playing()
{
	if(!channels.size())
		return false;

	for(auto it = channels.begin(); it != channels.end(); it++)
	{
		MD_Channel* ch = it->get();
		if(!ch->is_enabled())
			return false;
	}
	return true;
}

int MD_Driver::loop_count()
{
	unsigned int loop_count = INT_MAX;
	if(!channels.size())
		return 0;

	for(auto it = channels.begin(); it != channels.end(); it++)
	{
		MD_Channel* ch = it->get();
		if(ch->get_loop_count() < loop_count)
			loop_count = ch->get_loop_count();
	}
	return loop_count;
}

//! Return delta until the next event.
double MD_Driver::play_step()
{
	if(seq_delta < seq_rate)
	{
		// update tracks
		seq_delta += seq_rate;
		seq_update();
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

