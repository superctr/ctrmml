/* mega drive driver for ctrmml */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <ctype.h>
#include <assert.h>
#include "md.h"

#define DEBUG_FM(fmt,...) { }
#define DEBUG_PSG(fmt,...) { }
//#define DEBUG_FM(fmt,...) { printf(fmt, __VA_ARGS__); }
//#define DEBUG_PSG(fmt,...) { printf(fmt, __VA_ARGS__); }

static uint8_t opn_con_op[8] = {3,3,3,3,2,1,1,0};
static uint16_t opn_freq[13] = {644, 681, 722, 765, 810, 858, 910, 964, 1021, 1081, 1146, 1214, 1288};
static uint16_t psg_freq[13] = {851, 803, 758, 715, 675, 637, 601, 568, 536, 506, 477, 450, 425};

void md_driver_command(void *ptr, struct atom* atom);

static void opn_w(struct md_driver *driver, uint8_t port, uint8_t reg, uint8_t ch, uint8_t op, uint16_t data)
{
	if(reg == 0x28)
	{
		data <<= 4;
		data += ch | (port << 2);
		DEBUG_FM("opn-keyon port %d reg %02x data %02x (ch %d op %d)\n", port, reg, data, ch, op);
		vgm_write(0x52, 0, reg, data);
	}
	else if(reg >= 0x30 && reg < 0xa0)
	{
		reg += op*4;
		reg += ch;
		DEBUG_FM("opn-param port %d reg %02x data %02x (ch %d op %d)\n", port, reg, data, ch, op);
		vgm_write(0x52, port, reg, data);
	}
	else if(reg >= 0xa0 && reg < 0xb0) // ch3 operator freq
	{
		reg += ch;
		// would use a lookup table in 68k code
		if(reg >= 0xa8 && op == 0)
			reg += 1;
		else if(reg >= 0xa8 && op == 1)
			reg += 0;
		else if(reg >= 0xa8 && op == 2)
			reg += 2;
		else if(reg >= 0xa8 && op == 3)
			reg = 0xa2;
		DEBUG_FM("opn-fnum  port %d reg %02x data %04x (ch %d op %d)\n", port, reg, data, ch, op);
		vgm_write(0x52, port, reg+4, data>>8);
		vgm_write(0x52, port, reg, data&0xff);
	}
	else if(reg >= 0xb0)
	{
		reg += ch;
		DEBUG_FM("opn-param port %d reg %02x data %02x (ch %d op %d)\n", port, reg, data, ch, op);
		vgm_write(0x52, port, reg, data);
	}
	else
	{
		DEBUG_FM("opn-param port %d reg %02x data %02x\n", port, reg, data);
		vgm_write(0x52, 0, reg, data); // port0 only
	}
	//driver->borrowed_sample++;
	//vgm_delay(100);
}

static void psg_w(struct md_driver *driver, uint8_t reg, uint8_t ch, uint16_t data)
{
	uint8_t cmd1, cmd2;
	if(reg == 0) // frequency
	{
		data &= 0x3ff;
		cmd1 = (data & 0x0f) | (ch << 5) | 0x80;
		cmd2 = data >> 4;
		DEBUG_PSG("psg %02x,%02x (ch %d freq %04x)\n", cmd1, cmd2, ch, data);
		// don't send second byte if noise
		vgm_write(0x50, 0, 0, cmd1);
		if(ch < 3)
			vgm_write(0x50, 0, 0, cmd2);
	}
	else if(reg == 1) // volume
	{
		cmd1 = (data & 0x0f) | (ch << 5) | 0x90;
		DEBUG_PSG("psg %02x (ch %d vol %04x)\n", cmd1, ch,  data);
		vgm_write(0x50, 0, 0, cmd1);
	}
}

void md_driver_reset(struct md_driver *driver, struct song* song)
{
	int i;
	for(i=0; i<10; i++)
	{
		struct track_player *player = driver->ch[i].player;
		memset(&driver->ch[i], 0, sizeof(driver->ch[i]));
		driver->ch[i].player = player;
		driver->ch[i].driver = driver;
		driver->tempo_delta = 255;
		driver->tempo_counter=  0;
		if(player && !track_player_reset(player, song, i))
			driver->ch[i].enabled = 1;
		driver->ch[i].bank = 0;
		player->atom_post_callback = md_driver_command;
		player->cb_state = &driver->ch[i];
		// assign channels
		if(i < 6)
		{
			driver->ch[i].type = MDCH_FM;
			driver->ch[i].bank = i/3;
			driver->ch[i].id = i%3;
			opn_w(driver, driver->ch[i].bank, 0x40, driver->ch[i].id, 0, 0x7f); // tl=max
			opn_w(driver, driver->ch[i].bank, 0x40, driver->ch[i].id, 1, 0x7f);
			opn_w(driver, driver->ch[i].bank, 0x40, driver->ch[i].id, 2, 0x7f);
			opn_w(driver, driver->ch[i].bank, 0x40, driver->ch[i].id, 3, 0x7f);
			opn_w(driver, driver->ch[i].bank, 0x28, driver->ch[i].id, 0, 0); // key off
			driver->ch[i].pan_lfo = 0xc0; // l/r enabled
			opn_w(driver, driver->ch[i].bank, 0xb4, driver->ch[i].id, 0, 0xc0); // enable l/r
		}
		else if(i < 10)
		{
			driver->ch[i].type = (i==9) ? MDCH_PSGN_A :  MDCH_PSG;
			driver->ch[i].id = i-6;
			psg_w(driver, 1, driver->ch[i].id, 15); // disable output
			driver->ch[i].specific.psg_env.start = driver->data_start[0];
			assert(driver->data_start[0] != 0);
			driver->ch[i].specific.psg_env.pos = 3;
			driver->ch[i].specific.psg_env.delay = 0;
		}
	}
}

// static void opn_w(struct md_driver *driver, uint8_t port, uint8_t reg, uint8_t ch, uint8_t op, uint16_t data)
static void ch_set_freq(struct md_channel *ch)
{
	uint8_t note_num = (ch->pitch >> 8) + ch->ins_transpose;
	uint8_t note = note_num % 12;
	uint8_t octave = note_num / 12;
	uint8_t detune = ch->pitch & 0xff;
	uint16_t* base;
	uint16_t val;
	switch(ch->type)
	{
		case MDCH_FM:
		case MDCH_FM3_A:
		case MDCH_FM3_B:
			base = &opn_freq[note];
			val = base[0] + (((base[1] - base[0]) * detune) >> 8);
			val += (octave & 7) << 11;
			break;
		case MDCH_PSG:
		case MDCH_PSGN_B:
			octave--;
			base = &psg_freq[note];
			val = base[0] + (((base[1] - base[0]) * detune) >> 8);
			val >>= octave;
			break;
		default:
			val = note;
			break;
	}

	switch(ch->type)
	{
		case MDCH_FM3_A:
			opn_w(ch->driver, ch->bank, 0xa8, 0, 0, val);
			opn_w(ch->driver, ch->bank, 0xa8, 0, 2, val);
			break;
		case MDCH_FM3_B:
			opn_w(ch->driver, ch->bank, 0xa8, 0, 0, val);
			// continue
		case MDCH_FM:
			opn_w(ch->driver, ch->bank, 0xa0, ch->id, 0, val);
			break;
		case MDCH_PSG:
			psg_w(ch->driver, 0, ch->id, val);
			break;
		case MDCH_PSGN_A:
			psg_w(ch->driver, 0, 3, val);
			break;
		case MDCH_PSGN_B:
			psg_w(ch->driver, 0, 2, val);
			psg_w(ch->driver, 0, 3, 7);
			break;
		default:
			break;
	}
}

static void ch_set_vol(struct md_channel *ch, uint8_t vol, int coarse)
{
	int operator;
	if(coarse)
	{
		if(vol > 15)
			vol = 15;
		if(ch->type & MDCH_PSG)
			vol = 15-vol;
		else // 2dB per step
			vol = 2 + (15-vol)*3 - (15-vol)/3;
	}
	switch(ch->type)
	{
		case MDCH_FM:
			for(operator=3; operator>=0; operator--)
			{
				uint8_t max_tl = ch->specific.tl[operator];
				if(operator >= opn_con_op[ch->con])
					max_tl += vol;
				if(max_tl > 127)
					max_tl = 127;
				opn_w(ch->driver, ch->bank, 0x40, ch->id, operator, max_tl);
			}
			break;
		case MDCH_PSG:
		case MDCH_PSGN_A:
		case MDCH_PSGN_B:
			vol += ch->specific.psg_env.delay & 0x0f;
			if(vol > 15)
				vol = 15;
			psg_w(ch->driver, 1, ch->id, vol);
			break;
		default:
			printf("set vol not supported for type %d yet\n", ch->type);
			break;
	}
}

// 15 for key on all operators
static void ch_key_on(struct md_channel *ch, uint8_t val)
{
	uint8_t mask = 15;
	switch(ch->type)
	{
		case MDCH_FM:
			opn_w(ch->driver, ch->bank, 0x28, ch->id, 0, val & mask);
			break;
		case MDCH_PSG:
		case MDCH_PSGN_A:
		case MDCH_PSGN_B:
			if(val)
			{
				ch->specific.psg_env.pos = 0;
				ch->specific.psg_env.delay = 0x1f;
			}
			break;
		default:
			printf("keyon not supported for type %d yet\n", ch->type);
			break;
	}
}

// Todo: this is quite inconvenient so i should reorder the FM instrument parameter
// order.

// Write FM parameters for an operator
// Return the value of the TL reg
static uint8_t ch_fm_op_ins(struct md_channel *ch, uint8_t *idata, int op)
{
	uint8_t retval;
	opn_w(ch->driver, ch->bank, 0x30, ch->id, op, *idata);
	idata += 4;
	retval = *idata;
	idata += 4;
	opn_w(ch->driver, ch->bank, 0x50, ch->id, op, *idata);
	idata += 4;
	opn_w(ch->driver, ch->bank, 0x60, ch->id, op, *idata);
	idata += 4;
	opn_w(ch->driver, ch->bank, 0x70, ch->id, op, *idata);
	idata += 4;
	opn_w(ch->driver, ch->bank, 0x80, ch->id, op, *idata);
	idata += 4;
	opn_w(ch->driver, ch->bank, 0x90, ch->id, op, *idata);
	return retval;
}

static void ch_set_ins(struct md_channel *ch, uint8_t ins, uint8_t vol)
{
	int addr;
	uint8_t *idata = ch->driver->data_start[ch->driver->ins_data_index[ins]];
	switch(ch->type)
	{
		case MDCH_FM:
			DEBUG_FM("ins %3d = %02x\n", ins, ch->driver->ins_data_index[ins]);
			opn_w(ch->driver, ch->bank, 0x40, ch->id, 0, 0x7f); // tl=max
			opn_w(ch->driver, ch->bank, 0x40, ch->id, 1, 0x7f);
			opn_w(ch->driver, ch->bank, 0x40, ch->id, 2, 0x7f);
			opn_w(ch->driver, ch->bank, 0x40, ch->id, 3, 0x7f);
			opn_w(ch->driver, ch->bank, 0x28, ch->id, 0, 0); // key off
			for(addr=0; addr<4; addr++)
			{
				// load instrument parameters
				ch->specific.tl[addr] = ch_fm_op_ins(ch, idata + addr, addr);
			}
			idata += 28;
			opn_w(ch->driver, ch->bank, 0xb0, ch->id, 0, *idata);
			ch->con = *idata & 7;
			break;
		case MDCH_PSG:
		case MDCH_PSGN_A:
		case MDCH_PSGN_B:
			DEBUG_PSG("psg ins %3d = %02x\n", ins, ch->driver->ins_data_index[ins]);
			ch->specific.psg_env.start = idata;
			ch->specific.psg_env.pos = 0;
			ch->specific.psg_env.delay = 15;
			break;
		default:
			printf("set ins not supported for type %d yet\n", ch->type);
			break;
	}
	ch->ins_transpose = ch->driver->ins_transpose[ins];
}

#define CH_STATE(var) player->ch_state[var - ATOM_CMD_CHANNEL_MODE]
void md_driver_command(void *ptr, struct atom* atom)
{
	struct md_channel *ch = ptr;
	struct track_player *player = ch->player;
	switch(atom->type)
	{
		case ATOM_CMD_CHANNEL_MODE:
			if(atom->param < 2 && (ch->type == MDCH_PSGN_A || ch->type == MDCH_PSGN_B))
				ch->type = MDCH_PSGN_A + atom->param;
			break;
		case ATOM_CMD_INS:
			//printf("set ins %d to %d\n", ch->id, CH_STATE(ATOM_CMD_INS));
			ch_set_ins(ch, CH_STATE(ATOM_CMD_INS), CH_STATE(ATOM_CMD_VOL));
			// continue ...
		case ATOM_CMD_VOL:
		case ATOM_CMD_VOL_FINE:
		case ATOM_CMD_VOL_REL:
		case ATOM_CMD_VOL_FINE_REL:
			//printf("set vol to %d\n", CH_STATE(ATOM_CMD_VOL));
			ch_set_vol(ch, CH_STATE(ATOM_CMD_VOL_FINE), player->ch_flag & (1<<30));
			//printf("set vol %d to %d\n", ch->id, CH_STATE(ATOM_CMD_VOL_FINE));
			break;
		case ATOM_NOTE:
			ch->pitch = ((atom->param + CH_STATE(ATOM_CMD_TRANSPOSE))<<8) + CH_STATE(ATOM_CMD_DETUNE);
			if(!ch->slur_flag)
			{
				ch_key_on(ch, 0);
				ch_set_freq(ch);
				ch_key_on(ch, 15);
			}
			else
			{
				ch_set_freq(ch);
				ch->slur_flag = 0;
			}
			//printf("note %d (%d)\n", ch->id, atom->param);
			break;
		case ATOM_REST:
			ch_key_on(ch, 0);
			break;
		case ATOM_CMD_SLUR:
			ch->slur_flag = 1;
			break;
		case ATOM_TIE:
			//printf("tie %d\n", atom->param);
			break;
		case ATOM_CMD_TEMPO:
		case ATOM_CMD_TEMPO_BPM:
			if(player->ch_flag & (1<<31))
				ch->driver->tempo_delta = (CH_STATE(ATOM_CMD_TEMPO)*256 / 150) - 1;
			else
				ch->driver->tempo_delta = CH_STATE(ATOM_CMD_TEMPO);
			printf("set tempo to %02x\n", ch->driver->tempo_delta);
		default:
			break;
	}
}

static void md_psg_update(struct md_driver *driver, struct md_channel *ch)
{
	if(ch->type != MDCH_PSG && ch->type != MDCH_PSGN_A && ch->type != MDCH_PSGN_B)
		return;

	// faster decay if key off
	if(ch->specific.psg_env.delay < 0x20 || ch->player->state == CHANNEL_KEYOFF)
	{
		uint8_t *env_pos = ch->specific.psg_env.start + ch->specific.psg_env.pos;
		assert(env_pos);
		// sustain command
		if(*env_pos == 0x01 && ch->player->state != CHANNEL_KEYON)
		{
			env_pos++;
			ch->player->state = CHANNEL_INACTIVE; // remove keyoff flag to set normal delay
		}
		// jump command
		else if(*env_pos == 0x02 && ch->player->state == CHANNEL_KEYON)
		{
			env_pos = ch->specific.psg_env.start + env_pos[1];
		}
		// volume + length
		if(*env_pos > 0x0f)
		{
			ch->specific.psg_env.delay = *env_pos;
			ch_set_vol(ch, ch->CH_STATE(ATOM_CMD_VOL_FINE), ch->player->ch_flag & (1<<30));
			env_pos++;
		}
		// unknown command or stop command
		else if(ch->player->state == CHANNEL_KEYOFF)
		{
			psg_w(ch->driver, 1, ch->id, 15);
			ch->player->state = CHANNEL_INACTIVE; // remove keyoff flag to optimize writes
		}
		ch->specific.psg_env.pos = env_pos - ch->specific.psg_env.start;
	}
	else
	{
		ch->specific.psg_env.delay -= 0x10;
	}
}

int md_driver_update(void *ptr)
{
	struct md_driver *driver = ptr;
	struct md_channel *ch = driver->ch;
	int completed_ch = 10;
	int step = 0;
	int i;
	int tempo_delta = driver->tempo_counter + driver->tempo_delta + 1;
	int tempo_step = tempo_delta >> 8;
	driver->tempo_counter = tempo_delta & 0xff;
	for(i=0; i<10; i++, ch++)
	{
		step = 0;
		if(!ch->enabled)
			continue;
		if(ch->player->loop_count < 2)
		{
			completed_ch--;
		}
		if(tempo_step && ch->player->delay)
		{
			ch->player->delay--;
		}
		else if(tempo_step)
		{
			while(step == 0)
			{
				 step = track_player_step(ch->player);
			}
			if(step < 0)
			{
				// for PSG we need to silence the output
				if(ch->type & MDCH_PSG)
					psg_w(driver, 1, ch->id, 15);
				else
					ch_key_on(ch, 0);
				ch->enabled = 0;
				continue;
			}
			ch->player->delay = step-1; // we already do one step
		}

		// update envelopes...
		md_psg_update(driver, ch);

	}
	// handle loops.....
	return (completed_ch == 10);
}
