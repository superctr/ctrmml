#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <cctype>
#include <climits>
#include <algorithm>
#include <cmath>

#include "md.h"
#include "mdsdrv.h"
#include "../song.h"
#include "../input.h"
#include "../stringf.h"


//! Constructs a MD_Channel.
MD_Channel::MD_Channel(MD_Driver& driver, int id)
	: Player(*driver.song, driver.song->get_track(id)),
	driver(&driver),
	channel_id(id),
	pcm_channel_enable(0),
	pcm_channel_valid(0),
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
	tl(),
	macro_carry(0)
{
	if(channel_id == 5)
	{
		pcm_channel_valid = true;
		pcm_channel_id = 0;
	}
	else if(channel_id == 10 || channel_id == 11)
	{
		pcm_channel_valid = true;
		pcm_channel_id = channel_id - 9;
	}

	set_var(Event::VOL_FINE, 15);
	set_var(Event::PAN, 3);
	set_coarse_volume_flag(true);
}

//! Update a channel
void MD_Channel::update(int seq_ticks)
{
	while(seq_ticks--)
	{
		play_tick();
		if(macro_track)
			macro_track->update();
	}

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

void MD_Channel::seek(int ticks)
{
	skip_ticks(ticks);
	if(get_update_flag(Event::TEMPO))
		update_tempo();
	if(get_update_flag(Event::PAN))
		v_set_pan();
	clear_update_flag(Event::PAN);
	update_state();

	note_pitch = ((int16_t) get_last_note() + (int16_t) get_var(Event::TRANSPOSE))<<8;
	note_pitch += get_var(Event::DETUNE);
}

//! Write a single FM operator
uint8_t MD_Channel::write_fm_operator(int idx, int bank, int id, const std::vector<uint8_t>& idata)
{
	driver->ym2612_w(bank, 0x30, id, idx, idata[idx]);
	driver->ym2612_w(bank, 0x50, id, idx, idata[4+idx]);
	driver->ym2612_w(bank, 0x60, id, idx, idata[8+idx]);
	driver->ym2612_w(bank, 0x70, id, idx, idata[12+idx]);
	driver->ym2612_w(bank, 0x80, id, idx, idata[16+idx]);
	driver->ym2612_w(bank, 0x90, id, idx, idata[20+idx]);
	return idata[24+idx];
}

//! Write a 4op FM instrument
void MD_Channel::write_fm_4op(int bank, int id)
{
	uint16_t ins_id = get_var(Event::INS);
	if(driver->data.ins_type[ins_id] != MDSDRV_Data::INS_FM)
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
	static const uint16_t freqtab[13] = {1710, 1614, 1524, 1438, 1357, 1281, 1209, 1141, 1077, 1017, 960, 906, 855};
	uint8_t note = (pitch >> 8);
	uint8_t octave = (note / 12);
	uint8_t detune = pitch & 0xff;
	const uint16_t* base = &freqtab[note % 12];
	uint16_t set_pitch = base[0] + (((base[1] - base[0]) * detune) >> 8);
	set_pitch >>= octave;
	return set_pitch;
}

// Convert fine to coarse volume for PSG and PCM
uint8_t MD_Channel::get_psg_volume(uint16_t volume) const
{
	if(volume < 2)
		return 0;
	else if(volume >= 64)
		return 15;
	else if(volume >= 42)
		return 14;
	else
		return (volume-2)*3/8;
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
		platform_state[EVENT_LFO] = (std::strtol(tag[1].c_str(), 0, 0) << 4) | (std::strtol(tag[2].c_str(), 0, 0));
		return (1 << EVENT_LFO);
	}
	else if(iequal(tag[0], "lfodelay"))
	{
		if(tag.size() < 2)
			error("not enough parameters for 'lfodelay' command");
		platform_state[EVENT_LFO_DELAY] = std::strtol(tag[1].c_str(), 0, 0);
		return (1 << EVENT_LFO_DELAY);
	}
	else if(iequal(tag[0], "lforate"))
	{
		if(tag.size() < 2)
			error("not enough parameters for 'lforate' command");
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
		platform_state[EVENT_TL_MODIFY] = 0;
		return (1 << EVENT_WRITE_DATA);
	}
	else if(iequal(tag[0], "pcmrate"))
	{
		if(tag.size() < 2)
			error("not enough parameters for 'pcmrate' command");
		uint8_t data = std::strtol(tag[1].c_str(), 0, 0);
		if(data < 1 || data > 8)
			error("pcmrate argument must be between 1 and 8");
		if(driver->pcm_mode && pcm_channel_valid)
		{
			driver->pcm.set_pitch(pcm_channel_id, data);
			pcm_channel_enable = true;
		}
	}
	else if(iequal(tag[0], "pcmmode"))
	{
		if(tag.size() < 2)
			error("not enough parameters for 'pcmmode' command");
		uint8_t data = std::strtol(tag[1].c_str(), 0, 0);
		if(data < 2 || data > 3)
			error("pcmmode argument must be between 2 or 3");
		driver->pcm_rate = driver->pcm.set_mode(data);
		driver->pcm_counter = 0;
		driver->pcm_delta = driver->get_rate()/driver->pcm_rate;
		printf("set rate to %f", driver->pcm_delta);
	}
	else if(iequal(tag[0], "carry"))
	{
		macro_carry = true;
	}
	else if(MDSDRV_get_register(tag[0]))
	{
		if(tag.size() < 2)
			error("not enough parameters for 'write' command");

		uint8_t reg = MDSDRV_get_register(tag[0]);
		int16_t val = std::strtol(tag[1].c_str(), 0, 0);

		if(tag[1].size() > 0)
		{
			uint8_t sign = tag[1][0];
			if(sign == '+' || sign == '-')
				platform_state[EVENT_TL_MODIFY] = 1;
			else
				platform_state[EVENT_TL_MODIFY] = 0;
		}

		platform_state[EVENT_WRITE_ADDR] = reg;
		platform_state[EVENT_WRITE_DATA] = val;
		return (1 << EVENT_WRITE_DATA);
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
		case Event::NOTE:
			note_pitch = ((int16_t) event.param + (int16_t) get_var(Event::TRANSPOSE))<<8;
			note_pitch += get_var(Event::DETUNE);
			key_on_flag = true;
			if(!slur_flag)
			{
				key_off();
				if(macro_track && !macro_carry)
					reset_macro_track();
			}
			//continue
		case Event::TIE:
			if(get_update_flag(Event::INS))
			{
				set_ins();
				key_on_flag = true; //ok to retrigger envelopes
			}
			else if(get_update_flag(Event::VOL_FINE))
			{
				set_vol();
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
			update_tempo();
			break;
		case Event::PLATFORM:
			update_state();
			break;
		case Event::PAN:
			v_set_pan();
			break;
		case Event::PAN_ENVELOPE:
			macro_carry = false;
			reset_macro_track();
		default:
			break;
	}
}

void MD_Channel::update_tempo()
{
	clear_update_flag(Event::TEMPO);
	if(bpm_flag())
	{
		driver->tempo_delta = driver->bpm_to_delta(get_var(Event::TEMPO));
		printf("set tempo to %02x (%d bpm)\n", driver->tempo_delta, get_var(Event::TEMPO));
	}
	else
	{
		driver->tempo_delta = get_var(Event::TEMPO);
		printf("set tempo to %02x (direct)\n", driver->tempo_delta);
	}
}

void MD_Channel::update_state()
{
	if(get_platform_flag(EVENT_FM3))
	{
		last_pitch = 0xffff;
		v_key_off();
		clear_platform_flag(EVENT_FM3);
	}
	if(get_platform_flag(EVENT_WRITE_DATA))
	{
		int16_t data = get_platform_var(EVENT_WRITE_DATA);
		uint8_t addr = get_platform_var(EVENT_WRITE_ADDR);
		//printf("handle write data  =%02x,%02x\n", data,addr);
		if(addr >= 0xfc)
		{
			// handle TL
			addr -= 0xfc;
			if(get_platform_var(EVENT_TL_MODIFY))
			{
				data += tl[addr];
				if(data < 0)
					data = 0;
				else if(data > 0x7f)
					data = 0x7f;
			}
			tl[addr] = data;
			set_vol();
		}
		else
		{
			// everything else
			driver->ym2612_w(channel_id / 3, addr, channel_id % 3, 0, data);
		}
		clear_platform_flag(EVENT_WRITE_DATA);
	}
	if(get_platform_flag(EVENT_LFO))
	{
		v_set_pan();
	}
	if(get_platform_flag(EVENT_LFO_CONFIG))
	{
		uint8_t data = get_platform_var(EVENT_LFO_CONFIG);
		if(data)
			data += 7;
		driver->ym2612_w(0, 0x22, 0, 0, data);
	}
	v_set_type();
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
	}
	else
	{
		porta_value = note_pitch;
	}
	pitch = porta_value + (ins_transpose<<8);
	if(get_var(Event::PITCH_ENVELOPE))
	{
		if(key_on_flag || !pitch_env_data || get_update_flag(Event::PITCH_ENVELOPE))
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
				clear_update_flag(Event::PITCH_ENVELOPE);
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
		//Allow changing instruments at keyoff for CH3 special mode
		if(get_update_flag(Event::INS))
		{
			v_set_ins();
			set_vol_fm3();
			clear_update_flag(Event::INS);
			clear_update_flag(Event::VOL_FINE);
		}

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
	{
		set_pitch_fm3();
	}
	else
	{
		v_set_pitch();
	}
}

void MD_Channel::set_vol()
{
	if(get_platform_var(EVENT_FM3))
	{
		set_vol_fm3();
	}
	else
	{
		if(pcm_channel_enable)
		{
			uint8_t vol = get_var(Event::VOL_FINE);
			if(coarse_volume_flag())
				vol = 15-vol;
			else
				vol = get_psg_volume(vol);
			driver->pcm.set_vol(pcm_channel_id, vol);
		}
		v_set_vol();
	}
	clear_update_flag(Event::VOL_FINE);
}

void MD_Channel::set_ins()
{
	int16_t ins_id = get_var(Event::INS);
	if(driver->data.ins_type[ins_id] == MDSDRV_Data::INS_PCM && driver->pcm_mode && pcm_channel_valid)
	{
		uint8_t rate = driver->pcm.set_ins(pcm_channel_id, ins_id);
		driver->pcm.set_pitch(pcm_channel_id, rate);
		pcm_channel_enable = true;
	}
	else
	{
		if(pcm_channel_enable)
			driver->pcm.key_off(pcm_channel_id);
		v_set_ins();
		pcm_channel_enable = false;
	}
	set_vol();
	clear_update_flag(Event::INS);
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
	if(driver->data.ins_type[ins_id] == MDSDRV_Data::INS_PCM)
	{
		if(driver->pcm_mode && pcm_channel_valid)
		{
			driver->pcm.key_on(pcm_channel_id);
		}
		else if(!driver->pcm_mode)
		{
			int wave_header_id = driver->data.wave_map[ins_id];
			Wave_Bank::Sample sample = driver->data.wave_rom.get_sample_headers().at(wave_header_id);
			driver->last_pcm_channel = channel_id;
			driver->ym2612_w(0, 0x2b, 0, 0, 0x80); // DAC enable
			if(driver->vgm)
			{
				driver->vgm->dac_start(0x00, sample.position + sample.start, sample.size, sample.rate);
			}
		}
	}
}

void MD_Channel::key_off_pcm()
{
	if(driver->pcm_mode && pcm_channel_valid)
	{
		driver->pcm.key_off(pcm_channel_id);
	}
	else if(driver->last_pcm_channel == channel_id)
	{
		driver->ym2612_w(0, 0x2b, 0, 0, 0x00); // DAC disable
		if(driver->vgm)
		{
			driver->vgm->dac_stop(0x00);
		}
		driver->last_pcm_channel = -1;
	}
}

void MD_Channel::reset_macro_track()
{
	if(get_var(Event::PAN_ENVELOPE))
	{
		try
		{
			macro_track = std::make_unique<MD_MacroTrack>(*this,
					*driver->song,
					driver->song->get_track(get_var(Event::PAN_ENVELOPE)));
		}
		catch(std::exception& ex)
		{
			error(stringf("Macro track *%d is not defined", get_var(Event::PAN_ENVELOPE)).c_str());
		}
	}
	else
	{
		macro_track = nullptr;
	}
}

//! Initialize a macro track.
MD_MacroTrack::MD_MacroTrack(MD_Channel& channel, Song& song, Track& track)
	: Basic_Player(song, track)
	, channel(channel)
{
}

void MD_MacroTrack::event_hook()
{
	switch(event.type)
	{
		case Event::NOTE:
			break;
		case Event::PLATFORM:
			try
			{
				const Tag& tag = get_song()->get_platform_command(event.param);
				channel.platform_update(tag);
				channel.update_state();
			}
			catch (std::out_of_range &)
			{
				error(stringf("Platform command %d is not defined", event.param).c_str());
			}
			break;
		case Event::TRANSPOSE_REL:
			channel.set_var(Event::TRANSPOSE, channel.get_var(Event::TRANSPOSE) + event.param);
			channel.set_update_flag(Event::TRANSPOSE);
			break;
		case Event::VOL:
			channel.set_var(Event::VOL_FINE, event.param);
			channel.set_update_flag(Event::VOL_FINE);
			channel.set_coarse_volume_flag(true);
			break;
		// Previous behavior was to clear the VOL_BIT after VOL_FINE_REL
		// and set it after VOL_REL. It has now been changed so VOL_REL
		// and VOL_FINE_REL neither sets it or clears it.
		case Event::VOL_REL:
		case Event::VOL_FINE_REL:
			channel.set_var(Event::VOL_FINE, channel.get_var(Event::VOL_FINE) + event.param);
			channel.set_update_flag(Event::VOL_FINE);
			break;
		case Event::TEMPO_BPM: // not supported in MDSDRV
		case Event::TEMPO: // not supported in MDSDRV
		case Event::INS: // not supported in MDSDRV
			break;
		default:
			if(event.type >= Event::CHANNEL_CMD && event.type < Event::CMD_COUNT)
			{
				channel.set_var(event.type, event.param);
				channel.set_update_flag(event.type);
				if(event.type == Event::VOL_FINE)
					channel.set_coarse_volume_flag(false);
				if(event.type == Event::PAN)
					channel.v_set_pan();
			}
			break;
	}
	if(channel.get_update_flag(Event::VOL_FINE))
	{
		channel.set_vol();
	}
}

bool MD_MacroTrack::loop_hook()
{
	loop_count++;
	return 1;
}

void MD_MacroTrack::end_hook()
{
}

//! Update a macro track
void MD_MacroTrack::update()
{
	// If a note event is found, wait until we get a key on sync flag
	if(on_time)
	{
		if(key_on_flag)
		{
			on_time = 0;
			key_on_flag = false;
		}
	}
	// If a rest is found, wait the specified # of ticks
	else if(off_time)
	{
		off_time--;
		play_time++;
	}

	loop_count = 0;
	while(is_enabled() && !on_time && !off_time)
	{
		step_event();
		if(loop_count > 1)
			error("Infinite loop detected");
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
	pan_lfo = get_platform_var(EVENT_LFO) & 0x3f; // mask out panning
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
	if(key_on_flag && !slur_flag && !get_platform_var(EVENT_FM3))
	{
		env_pos = 0;
		env_delay = 0x1f;
		env_keyoff = 0;
	}
	// faster decay if key off
	if(env_delay < 0x20 || env_keyoff)
	{
		// dummy
		if(env_pos == 0xff)
		{
			return;
		}
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
		else if(event.type == Event::REST && env_keyoff)
		{
			driver->sn76489_w(1, id, 15); // mute
			env_keyoff = false; // remove keyoff flag to optimize writes
			env_pos = 0xff;
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
	if(driver->data.ins_type[ins_id] != MDSDRV_Data::INS_PSG)
		return;
	std::vector<uint8_t>* idata = &driver->data.data_bank.at(driver->data.envelope_map[ins_id]);
	set_envelope(idata);
}

void MD_PSGMelody::v_set_vol()
{
	uint8_t vol = get_var(Event::VOL_FINE);
	if(coarse_volume_flag())
		vol = 15-vol;
	else
		vol = get_psg_volume(vol);
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
	if(driver->data.ins_type[ins_id] != MDSDRV_Data::INS_PSG)
		return;
	std::vector<uint8_t>* idata = &driver->data.data_bank.at(driver->data.envelope_map[ins_id]);
	set_envelope(idata);
}

void MD_PSGNoise::v_set_vol()
{
	uint8_t vol = get_var(Event::VOL_FINE);
	if(coarse_volume_flag())
		vol = 15-vol;
	else
		vol = get_psg_volume(vol);
	vol += env_delay & 0x0f;
	if(vol > 15)
		vol = 15;
	driver->sn76489_w(1, id, vol);
}

void MD_PSGNoise::v_set_pitch()
{
	if(type == MD_PSGNoise::PSG3_WHITE || type == MD_PSGNoise::PSG3_PERIODIC)
	{
		uint16_t val = get_psg_pitch(pitch);
		driver->sn76489_w(0, 2, val);
		//driver->sn76489_w(0, 3, 7);
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
		int mode = get_platform_var(EVENT_CHANNEL_MODE);
		clear_platform_flag(EVENT_CHANNEL_MODE);
		if(mode == 1)
		{
			type = MD_PSGNoise::PSG3_WHITE;
			driver->sn76489_w(0, 3, 7);
		}
		else if(mode == 2)
		{
			type = MD_PSGNoise::PSG3_PERIODIC;
			driver->sn76489_w(0, 3, 3);
		}
		else
		{
			type = MD_PSGNoise::NORMAL;
		}
		last_pitch = 0xffff;
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

bool MD_PCMDriver::tables_initialized;
int8_t MD_PCMDriver::vol_table[16][256];

const uint8_t MD_PCMDriver::pitch_table[2][8] = {
	{
		0b10000000, //2ch mix mode
		0b10001000,
		0b10100100,
		0b10101010,
		0b11101010,
		0b11101110,
		0b11111110,
		0b11111111
	},
	{
		0b00100000, //3ch mix mode
		0b00100100,
		0b00101010,
		0b00101101,
		0b00111101,
		0b00111111,
		0b01111111,
		0b11111111
	},
};


//! constructs MD_PCMDriver.
MD_PCMDriver::MD_PCMDriver(MD_Driver& driver)
	: driver(&driver)
	, mode(0)
{
	if(!tables_initialized)
	{
		tables_initialized = true;

		static const uint8_t volt[16] = {
			255, 203, 161, 128, 102, 81, 64, 51, 40, 32, 26, 20, 16, 13, 10, 8
		};
		for(int tab = 0; tab < 16; tab++)
		{
			uint8_t tvol = volt[tab];
			for(int i=0; i<256; i++)
			{
				int8_t ivol = i ^ 0x80;
				vol_table[tab][i] = (ivol * tvol) >> 8;
			}
		}
	}

	// init channels
	for(int i=0; i<3; i++)
		channels[i] = {false, 0, 0, 0, 0, 0, 0, 0};
}

//! Set PCM driver mixing mode
double MD_PCMDriver::set_mode(int data)
{
	if(data < 4)
		mode = data;
	else
		mode = 0;

	if(data == 2)
		return 17500.0;
	else if(data == 3)
		return 13000.0;
	else
		return 50;
}

//! Set instrument. Return rate
uint8_t MD_PCMDriver::set_ins(int channel, int data)
{
	if(channel > mode)
		return 8;

	int wave_header_id = driver->data.wave_map[data];
	Wave_Bank::Sample sample = driver->data.wave_rom.get_sample_headers().at(wave_header_id);
	channels[channel].start = sample.position + sample.start;
	channels[channel].length = sample.size;

	float pitch = sample.rate / (MDSDRV_PCM_RATE / 8.0);
	uint8_t cp = pitch + 0.5;
	if(cp < 1)
		cp = 1;
	else if(cp > 8)
		cp = 8;
	return cp;
}

void MD_PCMDriver::set_vol(int channel, int data)
{
	if(channel > mode)
		return;

	channels[channel].volume = data & 15;
}

void MD_PCMDriver::set_pitch(int channel, int data)
{
	if(channel > mode)
		return;

	// Skip counter
	if(mode == 3)
		channels[channel].count = 3;
	else
		channels[channel].count = 0;

	channels[channel].phase = pitch_table[0][(data - 1) & 7];
}

void MD_PCMDriver::key_on(int channel)
{
	if(channel > mode)
		return;

	if(!channels[0].enabled && !channels[1].enabled && !channels[2].enabled)
		driver->ym2612_w(0, 0x2b, 0, 0, 0x80);

	channels[channel].position = 0;
	channels[channel].enabled = true;
}

void MD_PCMDriver::key_off(int channel)
{
	if(channel > mode)
		return;

	channels[channel].enabled =  false;

	if(!channels[0].enabled && !channels[1].enabled && !channels[2].enabled)
		driver->ym2612_w(0, 0x2b, 0, 0, 0x00);
}

void MD_PCMDriver::update()
{
	if(!mode)
		return;

	int8_t accumulator = 0;
	for(int i=0; i<mode; i++)
		accumulator = mix_channel(accumulator, i);

	if(channels[0].enabled || channels[1].enabled || channels[2].enabled)
		driver->ym2612_w(0, 0x2a, 0, 0, accumulator ^ 0x80);
}

inline int8_t MD_PCMDriver::mix_channel(int16_t accumulator, int channel)
{
	MD_PCMChannel& ch = channels[channel];

	if(!ch.enabled)
		return accumulator;

	uint8_t sample = driver->data.wave_rom.get_rom_data()[ch.start + ch.position];

	ch.position += ch.update_phase();
	if(ch.count && !(--ch.count))
	{
		ch.position += ch.update_phase();
		ch.count = 3;
	}

	if(ch.position > ch.length)
		key_off(channel);

	accumulator += vol_table[ch.volume][sample];
	if(accumulator > 127)
		accumulator = 127;
	if(accumulator < -128)
		accumulator = -128;
	return accumulator;
}


//! constructs a MD_Driver.
/*!
 * \param rate Sample rate.
 * \param vgm_interface Optional VGM interface. Set to nullptr to disable VGM.
 * \param is_pal Use 50hz sequence update rate
 */
MD_Driver::MD_Driver(unsigned int rate, VGM_Interface* vgm_interface, int pcm_mode, bool is_pal)
	: Driver(rate, vgm_interface)
	, pcm(*this)
	, vgm(vgm_interface)
	, pcm_mode(pcm_mode)
	, tempo_delta(255)
	, tempo_counter(0)
	, ticks(0)
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
	seq_counter = 0;
	seq_delta = rate/seq_rate;
	pcm_rate = pcm.set_mode(pcm_mode);
	pcm_counter = 0;
	pcm_delta = rate/pcm_rate;
}

//! Initiate playback
void MD_Driver::play_song(Song& song)
{
	this->song = &song;
	channels.clear();
	data.read_song(song);
	// Need to expose data.message in a good way later for development...
	//std::cout << data.message;
	if(vgm && !pcm_mode)
	{
		const std::vector<uint8_t>& dbdata = data.wave_rom.get_rom_data();
		vgm->datablock(0x00,
			dbdata.size() - data.wave_rom.get_free_bytes(),
			dbdata.data(),
			dbdata.size());
		vgm->dac_setup(0x00, 0x02, 0x00, 0x2a, 0x00);
	}
	// setup tempo
	tempo_delta = 128;
	tempo_counter = 0;
	ticks = 0;
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

//! Skip a specified number of ticks
void MD_Driver::skip_ticks(unsigned int ticks)
{
	this->ticks = ticks;
	for(auto it = channels.begin(); it != channels.end(); it++)
	{
		MD_Channel* ch = it->get();
		if(ch->is_enabled() && ticks)
			ch->seek(ticks);
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
int MD_Driver::get_loop_count()
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
	if(seq_counter >= 0)
	{
		// update tracks
		seq_counter -= seq_delta;
		seq_update();
	}
	if(pcm_counter >= 0)
	{
		// update pcm
		pcm_counter -= pcm_delta;
		pcm.update();
	}
	if(loop_trigger && get_loop_count() == 0)
	{
		set_loop();
		reset_loop_count();
		loop_trigger = 0;
	}
	// get the time to the next event
	double next_delta = std::max(seq_delta, pcm_delta);
	if((seq_counter + next_delta) > 0)
		next_delta -= seq_counter + next_delta;
	if((pcm_counter + next_delta) > 0)
		next_delta -= pcm_counter + next_delta;
	if(std::abs(next_delta) < 1/10000.0)
		next_delta += 1/10000.0;
	seq_counter += next_delta;
	pcm_counter += next_delta;
	return next_delta;
}


//! Converts BPM to fractional tempo
uint8_t MD_Driver::bpm_to_delta(uint16_t bpm)
{
	double base_tempo = 120. / (song->get_ppqn() * (1./seq_rate));
	double fract = (bpm / base_tempo)*256.;
	uint16_t new_tempo = (fract + 0.5) - 1;
	return std::min<uint16_t>(0xff, new_tempo);
}

void MD_Driver::seq_update()
{
	uint16_t next_counter = tempo_counter + tempo_delta + 1;
	uint8_t tempo_step = next_counter >> 7;
	tempo_counter = next_counter & 0x7f;
	ticks += tempo_step;
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

uint32_t MD_Driver::get_player_ticks()
{
	return ticks;
}
