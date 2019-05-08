//! \file wave.h
#ifndef WAVE_H
#define WAVE_H
#include "core.h"
#include <string>
#include <vector>
#include <stdint.h>

//! Wave file
class Wave_File
{
	friend class Wave_Rom;
	private:
		uint16_t channels;
		uint16_t stype;
		uint16_t sbits;
		uint32_t srate;
		uint32_t slength;
		uint16_t step;

		bool use_smpl_chunk;
		int32_t transpose;
		uint32_t lstart;
		uint32_t lend;

		// data[channel][n]
		std::vector<std::vector<int16_t>> data;

		int load_file(const std::string& filename, uint8_t** buffer, uint32_t* filesize);
		uint32_t parse_chunk(const uint8_t* fdata);

	public:
		Wave_File(uint16_t channels=0, uint32_t rate=0, uint16_t bits=0);
		Wave_File(const std::string& filename);

		virtual ~Wave_File();
		void add_sample(const int16_t* sample, int count);
		int read(const std::string& filename);
		int save(const std::string& filename);
};

//! Base wave rom
class Wave_Rom
{
	public:
		struct Sample
		{
			unsigned long start_pos;
			unsigned long size;
			unsigned long loop_start;
			unsigned long loop_end;
			unsigned long rate;
			int transpose;
		};
	protected:
		struct Gap
		{
			unsigned long start_pos;
			unsigned long size;
		};

		unsigned long max_size;
		unsigned long current_size;

		Tag include_paths;
		std::vector<uint8_t> rom_data;
		std::vector<Gap> gaps;
		std::vector<Sample> samples;

		virtual std::vector<uint8_t> encode_sample(const std::string& encoding_type, const std::vector<int16_t>& input);
		virtual uint32_t fit_sample(unsigned long loop_start, unsigned long sample_size);

	public:
		Wave_Rom(unsigned long max_size);
		virtual ~Wave_Rom();

		void set_include_paths(const Tag& tag);
		unsigned int add_sample(const Tag& tag);
		unsigned int get_free_bytes();
		const std::vector<Sample>& get_sample_headers();
		const std::vector<uint8_t>& get_rom_data();
};

#endif
