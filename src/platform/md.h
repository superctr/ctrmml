//! \file platform/md.h
#ifndef PLATFORM_MD_H
#define PLATFORM_MD_H
#include "../core.h"
#include "../player.h"
#include "../driver.h"
#include "../vgm.h"
#include "mdsdrv.h"
#include <memory>
#include <vector>

class MD_Channel;
class MD_MacroTrack;
class MD_Driver;

//! Megadrive abstract channel
class MD_Channel : public Player
{
	friend MD_MacroTrack;

	public:
		MD_Channel(MD_Driver& driver, int id);
		void update(int seq_ticks);
		void seek(int ticks);

	protected:
		enum
		{
			EVENT_CHANNEL_MODE = 0,
			EVENT_LFO = 1,
			EVENT_LFO_DELAY = 2,
			EVENT_LFO_CONFIG = 3,
			EVENT_FM3 = 4,
			EVENT_WRITE_ADDR = 5,
			EVENT_WRITE_DATA = 6,
			EVENT_TL_MODIFY = 7,
		};

		uint8_t write_fm_operator(int idx, int bank, int id, const std::vector<uint8_t>& idata);
		void write_fm_4op(int bank, int id);
		uint16_t get_fm_pitch(uint16_t pitch) const;
		uint16_t get_psg_pitch(uint16_t pitch) const;
		uint8_t get_psg_volume(uint16_t volume) const;

		virtual void v_set_ins() = 0;
		virtual void v_set_vol() = 0;
		virtual void v_set_pan() = 0;
		virtual void v_key_on() = 0;
		virtual void v_key_off() = 0;
		virtual void v_set_pitch() = 0;
		virtual void v_set_type() = 0;
		virtual void v_update_envelope() = 0;

		MD_Driver* driver;
		int channel_id;
		bool pcm_channel_enable;
		bool pcm_channel_valid;
		int pcm_channel_id;

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
		// Macro track
		std::unique_ptr<MD_MacroTrack> macro_track;
		bool macro_carry;

	private:
		uint32_t parse_platform_event(const Tag& tag, int16_t* platform_state) override;
		void write_event() override;

		void update_tempo();
		void update_state();
		void update_pitch();

		void key_on();
		void key_off();
		void set_pitch();
		void set_vol();
		void set_ins();

		void set_pitch_fm3();
		void set_vol_fm3();

		void key_on_pcm();
		void key_off_pcm();

		void reset_macro_track();
};

//! Megadrive macro track
class MD_MacroTrack : public Basic_Player
{
	public:
		MD_MacroTrack(MD_Channel& channel, Song& song, Track& track);
		void update();

	private:
		void event_hook() override;
		bool loop_hook() override;
		void end_hook() override;

		MD_Channel& channel;
		int loop_count;
};

//! Megadrive FM channel
class MD_FM : public MD_Channel
{
	public:
		MD_FM(MD_Driver& driver, int track_id, int channel_id);

	private:
		void v_set_ins() override;
		void v_set_vol() override;
		void v_set_pan() override;
		void v_key_on() override;
		void v_key_off() override;
		void v_set_pitch() override;
		void v_set_type() override;
		void v_update_envelope() override;

		enum
		{
			NORMAL = 0,
			FM3_2OP = 1
		} type;
		uint8_t bank : 1; //!< YM2612 port id.
		uint8_t id : 2; //!< YM2612 channel id.
		uint8_t pan_lfo; //!< FM panning & lfo parameters
};

//! Megadrive abstract PSG channel
class MD_PSG : public MD_Channel
{
	public:
		MD_PSG(MD_Driver& driver, int track_id, int channel_id);

	protected:
		void set_envelope(std::vector<uint8_t>* idata);
		void v_key_on() override;
		void v_key_off() override;
		void v_set_pan() override;
		void v_update_envelope() override;

		//! Channel index
		int id;
		const std::vector<uint8_t>* env_data; //!< Pointer to envelope data
		bool env_keyoff; //!< Envelope key off flag
		uint8_t env_pos; //!< Envelope position
		uint8_t env_delay; //!< Envelope delay and current volume
};

//! Megadrive PSG melodic channel
class MD_PSGMelody : public MD_PSG
{
	public:
		MD_PSGMelody(MD_Driver& driver, int track_id, int channel_id);
	private:
		enum
		{
			NORMAL = 0,
			FM3 = 1
		} type;

		void v_set_ins() override;
		void v_set_vol() override;
		void v_set_pitch() override;
		void v_set_type() override;
};

//! Megadrive PSG noise channel
class MD_PSGNoise : public MD_PSG
{
	public:
		MD_PSGNoise(MD_Driver& driver, int track_id, int channel_id);

	private:
		enum
		{
			NORMAL = 0,
			PSG3_WHITE = 1,
			PSG3_PERIODIC = 2
		} type;

		void v_set_ins() override;
		void v_set_vol() override;
		void v_set_pitch() override;
		void v_set_type() override;
};

//! Megadrive dummy channel
class MD_Dummy : public MD_Channel
{
	public:
		MD_Dummy(MD_Driver& driver, int track_id, int channel_id);

	private:
		int id;

		void v_set_ins() override;
		void v_set_vol() override;
		void v_set_pan() override;
		void v_key_on() override;
		void v_key_off() override;
		void v_set_pitch() override;
		void v_set_type() override;
		void v_update_envelope() override;
};

struct MD_PCMChannel
{
	bool enabled;
	int8_t pitch;
	uint32_t start;
	uint32_t length;
	uint32_t position;
	uint8_t volume;
	uint8_t phase;
	uint8_t count;

	inline int update_phase()
	{
		int out = phase >> 7;
		phase = (phase << 1) | out;
		return out;
	}
};

//! MDSDRV PCM emulator
class MD_PCMDriver
{
	public:
		MD_PCMDriver(MD_Driver& driver);

		double set_mode(int data); // returns sample rate
		uint8_t set_ins(int channel, int data);
		void set_vol(int channel, int data);
		void set_pitch(int channel, int data);
		void key_on(int channel);
		void key_off(int channel);

		void update();

	protected:
		MD_Driver* driver;
		MD_PCMChannel channels[3];

		int mode;

		int8_t mix_channel(int16_t accumulator, int channel);

		static bool tables_initialized;
		static int8_t vol_table[16][256];
		static const uint8_t pitch_table[2][8];
};

//! Megadrive sound driver
class MD_Driver : public Driver
{
	friend MD_Channel;
	friend MD_FM;
	friend MD_PSG;
	friend MD_PSGMelody;
	friend MD_PSGNoise;
	friend MD_PCMDriver;
	public:
		MD_Driver(unsigned int rate, VGM_Interface* vgm_interface, int pcm_mode = 0, bool is_pal = false);

		void play_song(Song& song);
		void reset();
		void skip_ticks(unsigned int ticks);
		bool is_playing();
		int get_loop_count();
		double play_step();
		uint32_t get_player_ticks();

	private:
		uint8_t bpm_to_delta(uint16_t bpm);
		void seq_update();
		void reset_loop_count();

		MDSDRV_Data data;
		MD_PCMDriver pcm;
		Song* song;
		VGM_Interface* vgm;

		std::vector<std::unique_ptr<MD_Channel>> channels;
		int pcm_mode;
		double seq_rate;
		double pcm_rate;
		double seq_delta;
		double pcm_delta;
		double seq_counter;
		double pcm_counter;

		uint8_t tempo_delta;
		uint8_t tempo_counter;
		uint32_t ticks;

		uint8_t fm3_mask;
		uint8_t fm3_con;
		uint8_t fm3_tl[4];

		int last_pcm_channel;

		bool loop_trigger;
};

#endif

