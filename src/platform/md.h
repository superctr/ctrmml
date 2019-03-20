// \file platform/md.h
#ifndef PLATFORM_MD_H
#define PLATFORM_MD_H
#include "../core.h"
#include "../player.h"
#include "../driver.h"
#include "../vgm.h"
#include <map>
#include <memory>
#include <vector>

class MD_Data
{
	private:
		static const int data_count_max = 256;
		int add_unique_data(const std::vector<uint8_t>& data);
		void read_fm_4op(uint16_t id, const Tag& tag);
		void read_fm_2op(uint16_t id, const Tag& tag);
		void read_psg(uint16_t id, const Tag& tag);
		void read_envelope(uint16_t id, const Tag& tag);

	public:
		std::vector<std::vector<uint8_t>> data_bank;
		std::map<uint16_t, int> envelope_map;
		void read_song(Song& song);
};

class MD_Channel : public Player
{
	enum Type
	{
		FM = 0,
		FM3_A = 1,
		FM3_B = 2,
		PSG = 4,
		PSGN_A = 5,
		PSGN_B = 6,
	};
	private:
		class MD_Driver* driver;
		Type type : 3;
		bool slur_flag : 1;
		int bank : 1;
		int id : 2;
		uint16_t pitch;
		uint8_t ins_transpose; // compiled files should have this already 'cooked'
		uint8_t pan_lfo;
		uint8_t con;
		uint8_t *pit_start; // start position of pitch envelope
		uint8_t *pit_pos; // end position of pitch envelope
		uint8_t pit_phase; // envelope rate stored in pit_start[0]
		uint8_t pit_depth;
		union
		{
			uint8_t tl[4];
			struct
			{
				uint8_t* start;
				uint8_t pos;
				uint8_t delay;
			} psg_env;
		} chip_data;
		void write_event();
	public:
		MD_Channel(Song& song, int id);
};

class MD_Driver : public Driver
{
	private:
		MD_Data data;
		Song* song;
		std::unique_ptr<MD_Channel> ch[10];
		double seq_rate;
		double pcm_rate;
		double seq_delta;
		double pcm_delta;

		uint8_t tempo_delta;
		uint8_t tempo_counter;

	public:
		MD_Driver(unsigned int rate, VGM_Writer* vgm, bool is_pal = false);
		void prepare_song(Song& song);
		void play_song(Song& song);
		void reset();
		double play_step();
};

#endif
