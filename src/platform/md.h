// \filInstrumentType
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

//! Megadrive driver data bank
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
		std::string dump_data(uint16_t id, uint16_t mapped_id); // debug function
		void read_fm_4op(uint16_t id, const Tag& tag);
		void read_fm_2op(uint16_t id, const Tag& tag);
		void read_psg(uint16_t id, const Tag& tag);
		void read_pitch(uint16_t id, const Tag& tag);
		void add_pitch_node(const char* s, std::vector<uint8_t>* env_data);
		void add_pitch_vibrato(const char* s, std::vector<uint8_t>* env_data);
		void read_envelope(uint16_t id, const Tag& tag);

	public:
		std::vector<std::vector<uint8_t>> data_bank;
		//! Maps the current song instruments to data_bank entries.
		std::map<uint16_t, int> envelope_map;
		//! Maps the current song instrument to transpose settings (for FM 2op only)
		std::map<uint16_t, int> ins_transpose;
		//! Maps the current song pitch envelopes to data_bank entries.
		std::map<uint16_t, int> pitch_map;
		//! Specify the instrument types of the defined song instruments.
		std::map<uint16_t, InstrumentType> ins_type;

		MD_Data();
		void read_song(Song& song);
};

//! Megadrive abstract channel
class MD_Channel : public Player
{
	protected:
		void write_event();

		virtual void set_ins() = 0;
		virtual void set_vol() = 0;
		virtual void set_pan() = 0;
		virtual void key_on() = 0;
		virtual void key_off() = 0;
		virtual void set_pitch() = 0;
		virtual void set_type() = 0;
		virtual void update_envelope() = 0;
		void update_pitch();
		uint8_t write_fm_operator(int idx, int bank, int id, const std::vector<uint8_t>& idata);
		void write_fm_4op(int bank, int id);
		uint16_t get_fm_pitch(uint16_t pitch) const;
		uint16_t get_psg_pitch(uint16_t pitch) const;

		MD_Driver* driver;
		bool slur_flag; //!< Flag to disable key on for the next note
		bool key_on_flag;
		// Portamento
		uint16_t note_pitch; //< Target pitch for portamento
		uint16_t porta_value; //!< Current pitch (256 'cents' per semitone)
		uint16_t last_pitch; //!< Last pitch, used to optimize register writes
		// Pitch envelopes
		const std::vector<uint8_t>* pitch_env_data;
		uint16_t pitch_env_value; //!< Pitch envelope value
		uint8_t pitch_env_delay;
		uint8_t pitch_env_pos;
		// Target pitch
		uint16_t pitch; //!< pitch with portamento and envelope calculated
		int8_t ins_transpose; //!< Instrument transpose (for FM 2op). compiled files should have this already 'cooked'
		uint8_t con; //!< FM connection
		uint8_t tl[4]; // also used for Ch3 mode
	public:
		MD_Channel(MD_Driver& driver, int id);
		void update(int seq_ticks);
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
		uint8_t bank : 1; //!< YM2612 port id.
		uint8_t id : 2; //!< YM2612 channel id.
		uint8_t pan_lfo; //!< FM panning & lfo parameters
		void set_ins();
		void set_vol();
		void set_pan();
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
		//! Channel index
		int id;
		const std::vector<uint8_t>* env_data; //!< Pointer to envelope data
		bool env_keyoff; //!< Envelope key off flag
		uint8_t env_pos; //!< Envelope position
		uint8_t env_delay; //!< Envelope delay and current volume
		void set_envelope(std::vector<uint8_t>* idata);
		void set_pan();
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

//! Megadrive sound driver
/*!
 * In the future, this driver will produce files that are compatible
 * with a Megadrive sound driver written by me.
 */
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
		double seq_rate;
		double seq_delta;
		double pcm_delta;
		double seq_counter;
		double pcm_counter;

		uint8_t tempo_convert(uint16_t bpm);
		uint8_t tempo_delta;
		uint8_t tempo_counter;

		bool loop_trigger;

		void seq_update();

	public:
		MD_Driver(unsigned int rate, VGM_Writer* vgm, bool is_pal = false);
		void play_song(Song& song);
		void reset();
		bool is_playing();
		int loop_count();
		double play_step();
};

#endif

