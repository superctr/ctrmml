#ifndef MD_H
#define MD_H

#include "ctrmml.h"
#include "vgm.h"
#include "playback.h"

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
	int8_t ins_transpose; // compiled files should have this already 'cooked'
	uint8_t pan_lfo;
	uint8_t con;
	uint8_t *pit_start; // start position of pitch envelope
	uint8_t *pit_pos; // end position of pitch envelope
	uint8_t pit_phase; // envelope rate stored in pit_start[0]
	uint8_t pit_depth;
	union {
		uint8_t tl[4];
		struct {
			uint8_t* start;
			uint8_t pos;
			uint8_t delay;
		} psg_env;
	} specific;
};

#define MD_DATA_COUNT_MAX 256
#define MD_DATA_SIZE_MAX 8192
#define MD_PCM_SIZE_MAX 0x100000
struct md_driver
{
	struct song *song;
	struct md_channel ch[10];

	uint8_t tempo_delta; // tempo delta - 1 (0=1/256 frames per tick, 255=1 frame per tick)
	uint8_t tempo_counter;

	int data_count; // amount of entries in the data table
	uint8_t* data_start[MD_DATA_COUNT_MAX]; // start offset of the envelope or patch data
	int data_length[MD_DATA_COUNT_MAX]; // size of the envelope or patch data
	int data_size; // size of data chunk
	uint8_t* data; // size of env/patch data chunk

	int pcm_count;
	char* pcm_start[MD_DATA_COUNT_MAX];
	int pcm_length[MD_DATA_COUNT_MAX];
	int pcm_size;
	int8_t* pcm; // size of PCM data chunk

	int ins_data_index[MD_DATA_COUNT_MAX]; // entry in data table
	int pitch_data_index[MD_DATA_COUNT_MAX];
	int8_t ins_transpose[MD_DATA_COUNT_MAX];

	int borrowed_sample; // used for vgm writing. may use write queue instead
};

struct md_driver *md_driver_init();

// General functions
void md_read_envelopes(struct md_driver *driver, struct song *song);

// Playback functions
void md_driver_reset(struct md_driver *driver, struct song* song);
int md_driver_update(void *ptr);

#endif
