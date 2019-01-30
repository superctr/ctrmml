/* mega drive driver for ctrmml */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <ctype.h>
#include "md.h"

#define DEBUG_FM(fmt,...) { }
#define DEBUG_PSG(fmt,...) { }
//#define DEBUG_FM(fmt,...) { printf(fmt, __VA_ARGS__); }
//#define DEBUG_PSG(fmt,...) { printf(fmt, __VA_ARGS__); }

struct md_driver *md_driver_init()
{
	struct md_driver *driver = calloc(1, sizeof(*driver));
	int i;
	for(i=0; i<10; i++)
	{
		driver->ch[i].player = track_player_init(0, 0);
	}
	driver->data = calloc(1, sizeof(uint8_t) * MD_DATA_SIZE_MAX);
	driver->data_size = MD_DATA_SIZE_MAX;
	driver->pcm = calloc(1, sizeof(int8_t) * MD_PCM_SIZE_MAX);
	driver->pcm_size = MD_PCM_SIZE_MAX;
	return driver;
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
	if(driver->data_count > MD_DATA_COUNT_MAX)
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

static void md_read_psg(struct md_driver *driver, struct tag *tag, uint8_t id)
{
	static uint8_t env_data[256];
	uint8_t *curr_pos = env_data, *loop_pos = NULL, *last_pos = curr_pos;
	int last=-1;
	if(!tag)
	{
		fprintf(stderr, "warning: empty psg instrument @%d\n", id);
		return;
	}
	while(tag)
	{
		char* s = tag->key;
		if(*s == '|')
			loop_pos = curr_pos;
		else if(*s == '/')
		{
			// insert sustain
			if(last == -1)
			{
				last = 15;
				*curr_pos++ = 0x1f - last;
			}
			*curr_pos++ = 0x01;
			last = -1;
		}
		else if(isdigit(*s))
		{
			uint8_t initial = strtol(tag->key, &s, 0);
			uint8_t final = initial;
			uint8_t length = 0;
			double delta = 0, counter = 0;
			if(*s == '>' && *++s)
				final = strtol(s, &s, 0);
			final = (final > 15) ? 15 : (final < 0) ? 0 : final; // bounds check
			initial = (initial > 15) ? 15 : (initial < 0) ? 0 : initial;
			if(final != initial)
				length = abs(final-initial)+1; // set default length
			if(*s == ':' && *++s)
				length = strtol(s, &s, 0);
			if(length == 0)
				length++;
			else if(length > 1) // calculate slide
				delta = (double)(final-initial)/(length-1);
			counter = initial + 0.5;
			while(length--)
			{
				// last value is always the final
				uint8_t val = (length) ? (int)counter : final;
				DEBUG_PSG("%2.2f = %d,  delta=%2.2f > %d\n", counter, val, delta, final);
				// add to duration of previous value if it's the same
				if((int)counter == last && *last_pos < 0xf0)
					*last_pos += 0x10;
				else
				{
					last_pos = curr_pos;
					*curr_pos++ = 0x1f - val;
				}
				last = val;
				counter += delta;
			}
		}
		else
		{
			fprintf(stderr, "warning: unrecognized envelope value '%s'", s);
			return;
		}
		tag = tag_get_next(tag);
	}
	// add loop command at the end
	if(!loop_pos)
	{
		*curr_pos++ = 0x00;
	}
	else
	{
		*curr_pos++ = 0x02;
		*curr_pos = loop_pos - env_data;
		curr_pos++;
	}

	for(last_pos = env_data; last_pos < curr_pos; last_pos ++)
			DEBUG_PSG("%02x ", *last_pos);
	DEBUG_PSG("\n");
	driver->ins_data_index[id] = md_add_unique_data(driver, env_data, last_pos-env_data);
}

void md_read_envelopes(struct md_driver *driver, struct song *song)
{
	struct tag* tag = song->tag;
	// we need to add a dummy psg envelope to prevent possible errors
	static uint8_t dummy_psg[4] = {0x10, 0x01, 0x1f, 0x00}; // 15 / 0
	driver->ins_data_index[0] = md_add_unique_data(driver, dummy_psg, 4);

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
				md_read_psg(driver, tag_get_next(child), env);
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
	// much of this could be implemented as a framework in playback.c
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
