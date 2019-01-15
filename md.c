/* mega drive driver for ctrmml */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "ctrmml.h"
#include "vgm.h"

enum md_channel_type
{
	MDCH_FM = 0,
	MDCH_FM3_A = 1,
	MDCH_FM3_B = 2,
	MDCH_PSG = 4,
	MDCH_PSGN_A = 5, // internal frequency
	MDCH_PSGN_B = 6 // ch3 frequency
};

struct md_channel
{
	struct md_driver *driver;
	struct track_player *player;
	unsigned int enabled : 1;
	enum md_channel_type type : 3;
	unsigned int bank : 1;
	unsigned int id : 2;
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
	int data_count; // amount of entries in the data table
	char* data_start[DATA_COUNT_MAX]; // start offset of the envelope or patch data
	int data_length[DATA_COUNT_MAX]; // size of the envelope or patch data
	int data_size; // size of data chunk
	uint8_t* data; // size of env/patch data chunk

	int pcm_count;
	char* pcm_start[DATA_COUNT_MAX];
	int pcm_length[DATA_COUNT_MAX];
	int pcm_size;
	int8_t* pcm; // size of PCM data chunk

	int patch_data_index[DATA_COUNT_MAX]; // entry in data table
};

static void opn_w(struct md_driver *driver, uint8_t port, uint8_t reg, uint8_t ch, uint8_t op, uint16_t data)
{
	if(reg == 0x28)
	{
		data <<= 4;
		data += ch;
		printf("opn-keyon port %d reg %02x data %02x (ch %d op %d)\n", port, reg, data, ch, op);
	}
	else if(reg >= 0x30 && reg < 0xa0)
	{
		reg += op*4;
		reg += ch;
		printf("opn-param port %d reg %02x data %02x (ch %d op %d)\n", port, reg, data, ch, op);
	}
	else if(reg >= 0xa0 && reg < 0xb0) // ch3 operator freq
	{
		// would use a lookup table in 68k code
		if(reg >= 0xa4 && op == 0)
			reg += 1;
		else if(reg >= 0xa4 && op == 1)
			reg += 0;
		else if(reg >= 0xa4 && op == 2)
			reg += 2;
		else if(reg >= 0xa4 && op == 3)
			reg = 0xa2;
		printf("opn-fnum  port %d reg %02x data %04x (ch %d op %d)\n", port, reg, data, ch, op);
	}
	else if(reg >= 0xb0)
	{
		reg += ch;
		printf("opn-param port %d reg %02x data %02x (ch %d op %d)\n", port, reg, data, ch, op);
	}
	else
	{
		printf("opn-param port %d reg %02x data %02x\n", port, reg, data);
	}
}

static void psg_w(struct md_driver *driver, uint8_t reg, uint8_t ch, uint16_t data)
{
	uint8_t cmd1, cmd2;
	if(reg == 0) // frequency
	{
		data &= 0x3ff;
		cmd1 = (data & 0x0f) | (ch << 5) | 0x80;
		cmd2 = data >> 4;
		printf("psg %02x,%02x (ch %d freq %04x)\n", cmd1, cmd2, ch, data);
		// don't send second byte if noise
	}
	else if(reg == 1) // volume
	{
		cmd1 = (data & 0x0f) | (ch << 5) | 0x90;
		printf("psg %02x (ch %d vol %04x)\n", cmd1, ch,  data);
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
		if(player && track_player_reset(player, song, i))
			driver->ch[i].enabled = 1;
		driver->ch[i].bank = 0;
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
	md_driver_reset(driver, 0);
	return driver;
}

void md_create_vgm(struct song* song, char* filename)
{
	struct md_driver *driver = md_driver_init();
	printf("vgm file = %s\n", filename);
}
