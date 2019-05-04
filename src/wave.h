//! \file wave.h
#ifndef WAVE_H
#define WAVE_H
#include "wave.h"
#include <string>
#include <vector>
#include <stdint.h>

//! Wave file
class Wave_File
{
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

		std::vector<std::vector<int16_t>> data;

		int load_file(const std::string& filename, uint8_t** buffer, uint32_t* filesize);
		uint32_t parse_chunk(const uint8_t* fdata);

	public:
		Wave_File(uint16_t channels, uint32_t rate, uint16_t bits);
		virtual ~Wave_File();
		void add_sample(const int16_t* sample, int count);
		int read(const std::string& filename);
		int save(const std::string& filename);
};


//! Base sound driver class
class Wave_Rom
{
	private:
		unsigned long max_size;
		unsigned long current_size;

	public:
		Wave_Rom(unsigned long max_size);
		virtual ~Wave_Rom();

};

#endif
