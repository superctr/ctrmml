/* mega drive driver for ctrmml */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include "ctrmml.h"
#include "vgm.h"
#include "playback.h"

//#define DEBUG_FM(fmt,...) { printf(fmt, __VA_ARGS__); }
#define DEBUG_FM(fmt,...) { }
#define DEBUG_PSG(fmt,...) { printf(fmt, __VA_ARGS__); }

enum md_channel_type
{
	MDCH_FM = 0,
	MDCH_FM3_A = 1, // op1,3
	MDCH_FM3_B = 2, // op2,4
	MDCH_PSG = 4,
	MDCH_PSGN_A = 5, // internal frequency
	MDCH_PSGN_B = 6 // ch3 frequency
};

struct md_channel
{
	struct md_driver *driver;
	struct track_player *player;
	unsigned int enabled : 1;
	unsigned int slur_flag : 1;
	enum md_channel_type type : 3;
	unsigned int bank : 1;
	unsigned int id : 2;
	uint16_t pitch;
	uint8_t pan_lfo;
	uint8_t con;
	uint8_t *pit_start; // start position of pitch envelope
	uint8_t *pit_pos; // end position of pitch envelope
	uint8_t pit_phase; // envelope rate stored in pit_start[0]
	uint8_t pit_depth;
	union {
		uint8_t tl[4];
		struct {
			uint16_t env_start;
			uint8_t env_pos;
			uint8_t env_delay;
		} psg_env;
	} specific;
};

#define DATA_COUNT_MAX 256
#define DATA_SIZE_MAX 8192
#define PCM_SIZE_MAX 0x100000
struct md_driver
{
	struct song *song;
	struct md_channel ch[10];

	uint8_t tempo_delta; // tempo delta - 1 (0=1/256 frames per tick, 255=1 frame per tick)
	uint8_t tempo_counter;

	int data_count; // amount of entries in the data table
	uint8_t* data_start[DATA_COUNT_MAX]; // start offset of the envelope or patch data
	int data_length[DATA_COUNT_MAX]; // size of the envelope or patch data
	int data_size; // size of data chunk
	uint8_t* data; // size of env/patch data chunk

	int pcm_count;
	char* pcm_start[DATA_COUNT_MAX];
	int pcm_length[DATA_COUNT_MAX];
	int pcm_size;
	int8_t* pcm; // size of PCM data chunk

	int ins_data_index[DATA_COUNT_MAX]; // entry in data table
	int pitch_data_index[DATA_COUNT_MAX];

	int borrowed_sample; // used for vgm writing. may use write queue instead
};

struct md_driver *md_driver_init();
void md_driver_reset(struct md_driver *driver, struct song* song);
int md_driver_update(void *ptr);

void md_read_envelopes(struct md_driver *driver, struct song *song);

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
		else if(i < 9)
		{
			driver->ch[i].type = MDCH_PSG;
			driver->ch[i].id = i-6;
			psg_w(driver, 1, driver->ch[i].id, 15); // disable output
		}
		else
		{
			driver->ch[i].type = MDCH_PSGN_A;
			driver->ch[i].id = 3;
			psg_w(driver, 1, 3, 15); // disable output
		}
	}
}

struct md_driver *md_driver_init()
{
	struct md_driver *driver = calloc(1, sizeof(*driver));
	int i;
	for(i=0; i<10; i++)
	{
		driver->ch[i].player = track_player_init(0, 0);
	}
	driver->data = calloc(1, sizeof(uint8_t) * DATA_SIZE_MAX);
	driver->data_size = DATA_SIZE_MAX;
	driver->pcm = calloc(1, sizeof(int8_t) * PCM_SIZE_MAX);
	driver->pcm_size = PCM_SIZE_MAX;
	return driver;
}

// static void opn_w(struct md_driver *driver, uint8_t port, uint8_t reg, uint8_t ch, uint8_t op, uint16_t data)
static void ch_set_freq(struct md_channel *ch)
{
	uint8_t note_num = ch->pitch >> 8;
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
		default:
			base = &psg_freq[note];
			val = base[0] + (((base[1] - base[0]) * detune) >> 8);
			val >>= octave;
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
			val >>= octave;
			// continue
		default:
			psg_w(ch->driver, 0, ch->id, val);
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
		if(ch->type == MDCH_PSG)
			vol = 15-vol;
		else
			vol = 2+(15-vol)*3;
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
		default:
			printf("set ins not supported for type %d yet\n", ch->type);
			break;
	}
}

#define CH_STATE(var) player->ch_state[var - ATOM_CMD_CHANNEL_MODE]
void md_driver_command(void *ptr, struct atom* atom)
{
	struct md_channel *ch = ptr;
	struct track_player *player = ch->player;
	switch(atom->type)
	{
		case ATOM_CMD_INS:
			printf("set ins %d to %d\n", ch->id, CH_STATE(ATOM_CMD_INS));
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
				ch->enabled = 0;
				continue;
			}
			ch->player->delay = step-1; // we already do one step
		}

		// update envelopes...

	}
	// handle loops.....
	return (completed_ch == 10);
}

int md_add_unique_data(struct md_driver *driver, uint8_t* data, int length)
{
	int i;
	uint8_t *start_offset = driver->data;
	// look for previous matching data in the table
	for(i=0; i<driver->data_count; i++)
	{
		if(length != driver->data_length[i])
			continue;
		if(memcmp(driver->data_start[i], data, length))
			continue;
		return i;
	}
	if(driver->data_count > DATA_COUNT_MAX)
	{
		fprintf(stderr, "error: maximum amount of data table entries reached\n");
		exit(-1);
	}
	else if(driver->data_count > 0)
	{
		start_offset = driver->data_start[driver->data_count-1] + driver->data_length[driver->data_count-1];
	}
	driver->data_start[driver->data_count] = start_offset;
	driver->data_length[driver->data_count] = length;
	memcpy(start_offset, data, length);
	return driver->data_count++;
}

// Read FM instrument
static void md_read_fm(struct md_driver *driver, struct tag *tag, uint8_t id)
{
	int i;
	static uint8_t fm_data[29];
	static int8_t ins_data[42];

	for(int i=0; i<42; i++)
	{
		if(!tag)
		{
			fprintf(stderr, "warning: not enough parameters for fm instrument @%d\n", id);
			return;
		}
		ins_data[i] = strtol(tag->key, NULL, 0);
		tag = tag_get_next(tag);
	}

	for(i=0; i<4; i++)
	{
		int op = 2;
		// physical operator order is 1,3,2,4
		if(i & 2)
			op += 10;
		if(i & 1)
			op += 20;
		// DT,MUL
		fm_data[0 + i] = (ins_data[op + 8] << 4) | (ins_data[op + 7] & 15);
		DEBUG_FM("op%d dt/ml  %02x\n", i, fm_data[0+i]);
		// TL
		fm_data[4 + i] = ins_data[op + 5];
		DEBUG_FM("op%d tl     %02x\n", i, fm_data[4+i]);
		// KS/AR
		fm_data[8 + i] = (ins_data[op + 6] << 6) | (ins_data[op + 0] & 31);
		DEBUG_FM("op%d ks     %02x\n", i, fm_data[8+i]);
		// AM/DR (AM sensitivity not supported yet)
		fm_data[12 + i] = (ins_data[op + 1] & 31);
		DEBUG_FM("op%d dr     %02x\n", i, fm_data[12+i]);
		// SR
		fm_data[16 + i] = (ins_data[op + 2] & 31);
		DEBUG_FM("op%d sr     %02x\n", i, fm_data[16+i]);
		// SL/RR
		fm_data[20 + i] = (ins_data[op + 4] << 4) | (ins_data[op + 3] & 15);
		DEBUG_FM("op%d sl/rr  %02x\n", i, fm_data[20+i]);
		// SSG-EG
		fm_data[24 + i] = ins_data[op + 9] & 15;
		DEBUG_FM("op%d ssg-eg %02x\n", i, fm_data[24+i]);
	}
	// FB/ALG
	fm_data[28] = (ins_data[0] & 7) | (ins_data[1] << 3);
	DEBUG_FM("fb/alg %02x\n", fm_data[28]);
	driver->ins_data_index[id] = md_add_unique_data(driver, fm_data, sizeof(fm_data));
	DEBUG_FM("fm ins @%d added at %02x\n", id, driver->ins_data_index[id]);
}

void md_read_envelopes(struct md_driver *driver, struct song *song)
{
	struct tag* tag = song->tag;
	while(tag)
	{
		unsigned char env;
		if(sscanf(tag->key, "@%hhu", &env) == 1)
		{
			struct tag* child = tag_get_property(tag);
			if(!child)
			{
				fprintf(stderr,"warning: missing parameter for instrument @%d\n", env);
			}
			else if(!strcasecmp(child->key, "psg"))
			{
				printf("read psg instrument %d (%s)\n", env, tag->key);
			}
			else if(!strcasecmp(child->key, "fm"))
			{
				printf("read fm instrument %d (%s)\n", env, tag->key);
				md_read_fm(driver, tag_get_next(child), env);
			}
			else
			{
				fprintf(stderr,"unknown type '%s' for instrument @%d\n", child->key, env);
				fprintf(stderr,"\tvalid: 'psg' or 'fm'\n");
			}
		}
		else if(sscanf(tag->key, "@M%hhu", &env) == 1)
		{
			printf("read pitch %d (%s)\n", env, tag->key);
		}
		tag = tag_get_next(tag);
	}
}

void md_create_vgm(struct song* song, char* filename)
{
	int res;
	struct md_driver *driver;
	struct pb_handler pb_handler;
	vgm_open(filename, 0x51, 0x80);
	vgm_poke32(0x2C, 7670454); // ym2612
	vgm_poke32(0x0C, 3579575); // sega psg
	vgm_poke16(0x28, 0x0009);
	vgm_poke8(0x2A, 0x10);
	vgm_poke8(0x2B, 0x03);
	driver = md_driver_init();
	md_read_envelopes(driver, song);
	md_driver_reset(driver, song);
	driver->borrowed_sample = 0;
	pb_handler.rate = 60.0 / 44100.0;
	pb_handler.delta_time = 0;
	pb_handler.pb_state = driver;
	pb_handler.callback = md_driver_update;
	while(!res)
	{
		res = playback(&pb_handler, 1, 1);
		if(driver->borrowed_sample == 0)
			vgm_delay(100);
		else
			driver->borrowed_sample--;
	}
	vgm_stop();
	vgm_write_tag();
	vgm_close();
	printf("vgm file = %s\n", filename);
}
