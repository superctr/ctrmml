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

class MD_Channel;
class MD_Driver;

class MD_Data
{
	enum InstrumentType
	{
		INS_UNDEFINED = 0,
		INS_PSG = 1,
		INS_FM = 2
	};
	private:
		static const int data_count_max = 256;
		int add_unique_data(const std::vector<uint8_t>& data);
		std::string dump_data(uint16_t id); // debug function
		void read_fm_4op(uint16_t id, const Tag& tag);
		void read_fm_2op(uint16_t id, const Tag& tag);
		void read_psg(uint16_t id, const Tag& tag);
		void read_envelope(uint16_t id, const Tag& tag);

	public:
		std::vector<std::vector<uint8_t>> data_bank;
		std::map<uint16_t, int> envelope_map;
		std::map<uint16_t, int> ins_transpose;
		std::map<uint16_t, InstrumentType> ins_type;
		void read_song(Song& song);
};

//! Megadrive abstract channel
class MD_Channel : public Player
{
	protected:
		virtual void set_ins() = 0;
		virtual void set_vol() = 0;
		virtual void key_on() = 0;
		virtual void key_off() = 0;
		virtual void set_pitch() = 0;
		virtual void set_type() = 0;
		virtual void update_envelope() = 0;
		uint8_t write_fm_operator(int idx, int bank, int id, const std::vector<uint8_t>& idata);
		void write_fm_4op(int bank, int id);
		uint16_t get_fm_pitch() const;
		uint16_t get_psg_pitch() const;

		MD_Driver* driver;
		bool slur_flag;
		uint16_t pitch;
		uint8_t ins_transpose; // compiled files should have this already 'cooked'
		uint8_t pan_lfo;
		uint8_t con;
		uint8_t tl[4]; // also used for Ch3 mode
		void write_event();
	public:
		MD_Channel(MD_Driver& driver, int id);
		void update(bool sequence_update);
};

//! Megadrive FM channel
class MD_FM : public MD_Channel
{
	enum
	{
		NORMAL = 0,
		FM3_2OP = 1
	};
	private:
		int type;
		uint8_t bank : 1;
		uint8_t id : 2;
		void set_ins();
		void set_vol();
		void key_on();
		void key_off();
		void set_pitch();
		void set_type();
		void update_envelope();
	public:
		MD_FM(MD_Driver& driver, int track_id, int channel_id);
};

//! Megadrive abstract PSG channel
class MD_PSG : public MD_Channel
{
	protected:
		int id;
		std::vector<uint8_t>* env_data;
		bool env_keyoff;
		uint8_t env_pos;
		uint8_t env_delay;
		void set_envelope(std::vector<uint8_t>* idata);
		void update_envelope();
	public:
		MD_PSG(MD_Driver& driver, int track_id, int channel_id);
};

//! Megadrive PSG melodic channel
class MD_PSGMelody : public MD_PSG
{
	enum
	{
		NORMAL = 0,
		FM3 = 1
	};
	private:
		int type;
		void set_ins();
		void set_vol();
		void key_on();
		void key_off();
		void set_pitch();
		void set_type();
	public:
		MD_PSGMelody(MD_Driver& driver, int track_id, int channel_id);
};

//! Megadrive PSG noise channel
class MD_PSGNoise : public MD_PSG
{
	enum
	{
		NORMAL = 0,
		MELODIC = 1
	};
	private:
		int type;
		void set_ins();
		void set_vol();
		void key_on();
		void key_off();
		void set_pitch();
		void set_type();
	public:
		MD_PSGNoise(MD_Driver& driver, int track_id, int channel_id);
};

class MD_Driver : public Driver
{
	friend MD_Channel;
	friend MD_FM;
	friend MD_PSG;
	friend MD_PSGMelody;
	friend MD_PSGNoise;
	private:
		MD_Data data;
		Song* song;
		std::vector<std::unique_ptr<MD_Channel>> channels;
		double seq_delta;
		double pcm_delta;
		double seq_counter;
		double pcm_counter;

		uint8_t tempo_delta;
		uint8_t tempo_counter;

		void seq_update();

	public:
		MD_Driver(unsigned int rate, VGM_Writer* vgm, bool is_pal = false);
		void prepare_song(Song& song);
		void play_song(Song& song);
		void reset();
		bool is_playing();
		int loop_count();
		double play_step();
};

#endif
