//! \file platform/mdsdrv.h
#ifndef PLATFORM_MDSDRV_COMMON_H
#define PLATFORM_MDSDRV_COMMON_H
#include "../core.h"
#include "../wave.h"
#include "../track.h"
#include "../player.h"
#include "../riff.h"
#include <map>
#include <vector>
#include <utility>

class MDSDRV_Data;
struct MDSDRV_Event;
class MDSDRV_Track_Writer;
class MDSDRV_Converter;
class MDSDRV_Linker;

// Current sequence version
#define MDSDRV_SEQ_VERSION_MAJOR 0
#define MDSDRV_SEQ_VERSION_MINOR 2

// Minimum compatible sequence version
#define MDSDRV_MIN_SEQ_VERSION_MAJOR 0
#define MDSDRV_MIN_SEQ_VERSION_MINOR 2

//! MDSDRV data bank
class MDSDRV_Data
{
	friend MDSDRV_Track_Writer;
	friend MDSDRV_Converter;
	friend MDSDRV_Linker;
	friend class MD_Driver;
	friend class MD_Channel;
	friend class MD_PSG;
	friend class MD_PSGMelody;
	friend class MD_PSGNoise;

	public:
		enum InstrumentType
		{
			INS_UNDEFINED = 0,
			INS_PSG = 1,
			INS_FM = 2,
			INS_PCM = 3
		};

		MDSDRV_Data();

		void read_song(Song& song);
		void add_instrument(uint16_t id, const Tag& tag);
		void add_pitch_envelope(uint16_t id, const Tag& tag);

	private:
		static const int data_count_max = 256;

		void add_ins_fm_4op(uint16_t id, const Tag& tag);
		void add_ins_fm_2op(uint16_t id, const Tag& tag);
		void add_ins_psg(uint16_t id, const Tag& tag);
		void add_ins_pcm(uint16_t id, const Tag& tag);

		void add_pitch_node(const char* s, std::vector<uint8_t>* env_data);
		void add_pitch_vibrato(const char* s, std::vector<uint8_t>* env_data);

		int add_unique_data(const std::vector<uint8_t>& data);
		std::string dump_data(uint16_t id, uint16_t mapped_id); // debug function

		//! Data bank, holds all instrument and envelope data
		std::vector<std::vector<uint8_t>> data_bank;
		//! Waverom bank, holds PCM samples.
		Wave_Bank wave_rom;
		//! Maps the current song instruments to data_bank entries.
		std::map<uint16_t, int> envelope_map;
		//! Maps the PCM instruments to a wave_rom header.
		std::map<uint16_t, int> wave_map;
		//! Maps the current song instrument to transpose settings (for FM 2op only)
		std::map<uint16_t, int> ins_transpose;
		//! Maps the current song pitch envelopes to data_bank entries.
		std::map<uint16_t, int> pitch_map;
		//! Specify the instrument types of the defined song instruments.
		std::map<uint16_t, InstrumentType> ins_type;
		//! Diagnostic message
		std::string message;
};

//! MDSDRV sequence event
struct MDSDRV_Event
{
	enum Type {
		SEGNO = 0x7f,	// virtual "segno" event

		REST = 0x80,
		TIE,
		NOTE,
		SLR = 0xe0,	// slur (legato)
		INS,		// instrument
		VOL,		// volume
		VOLM,		// volume modulate
		TRS,		// transpose
		TRSM,		// transpose modulate
		DTN,		// detune
		PTA,		// portamento
		PEG,		// pitch envelope
		PAN,		// panning enable
		LFO,		// lfo ams/pms
		LFOD,		// lfo delay
		FLG,		// channel flags
		FMCREG,		// channel register write
		FMTL,		// tl write
		FMTLM,		// tl modulate
		PCM,		// PCM instrument
		JUMP = 0xf5, // jump
		FMREG,		// global FM register write
		DMFINISH,	// drum mode subroutine: play note and exit
		COMM,		// communication variable byte
		TEMPO,		// tempo
		LP,			// loop start
		LPF,		// loop finish
		LPB,		// loop break
		LPBL,		// loop break (long jump)
		PAT,		// pattern/subroutine
		FINISH		// normal subroutine exit (0xff)
	};

	// explicit constructor needed since type is not enum Type
	inline MDSDRV_Event(uint8_t type, uint16_t arg)
		: type(type)
		, arg(arg)
	{}
	
	uint8_t type;
	uint16_t arg;
};

//! Track writer.
class MDSDRV_Track_Writer : public Basic_Player
{
	public:
		MDSDRV_Track_Writer(MDSDRV_Converter& mdsdrv,
				int id,
				bool in_drum_mode,
				std::vector<MDSDRV_Event>& converted_events);

	private:
		void event_hook() override;
		bool loop_hook() override;
		void end_hook() override;

		void parse_platform_event(const Tag& tag);
		uint8_t bpm_to_delta(uint16_t bpm);

		MDSDRV_Converter& mdsdrv;
		std::vector<MDSDRV_Event>& converted_events;
		bool in_drum_mode; //! set while executing a drum mode routine
		uint16_t drum_mode_offset; //! set to >0 to make note events call drum mode routines
		bool in_loop;
		uint16_t rest_time;
};

//! MDSDRV sequence converter
class MDSDRV_Converter
{
	friend MDSDRV_Track_Writer;
	friend class MDSDRV_Converter_Test;
	public:
		MDSDRV_Converter(Song& song);

		RIFF get_mds();

	private:
		void parse_track(int track_id);
		std::vector<uint8_t> convert_track(const std::vector<MDSDRV_Event>& event_list);
		int get_subroutine(int track_id, bool in_drum_mode);
		int get_envelope(int mapped_id);

		inline uint32_t get_data_id(int envelope_id) { return subroutine_list.size() + envelope_id; }

		Song* song;
		MDSDRV_Data data;
		//! Map of used data from the data bank.
		std::map<int, int> used_data_map;  // Maps event parameter to envelope_id
		std::map<int, int> subroutine_map; // Maps event parameter to track_id
		std::vector<std::vector<MDSDRV_Event>> subroutine_list;
		std::map<int, std::vector<MDSDRV_Event>> track_list;
		std::vector<uint8_t> sequence_data;
		uint16_t sequence_base;

};

//! MDSDRV data linker
class MDSDRV_Linker
{
	public:
		MDSDRV_Linker();

		void add_song(RIFF& mds);
		std::string get_seq_data_asm();
		std::vector<uint8_t> get_seq_data();
		std::vector<uint8_t> get_pcm_data();
		std::string get_statistics();

	private:
		int add_unique_data(const std::vector<uint8_t>& data);
		void check_version(uint8_t major, uint8_t minor);

		struct Seq_Data {
			std::vector<uint8_t> data;
			std::vector<std::pair<uint16_t,uint16_t>> patch_table;
		};
		std::vector<std::vector<uint8_t>> data_bank;
		std::vector<int> data_offset;
		std::vector<Seq_Data> seq_bank;
		Wave_Bank wave_rom;
};

#endif
