//! \file vgm.h
#ifndef VGM_H
#define VGM_H
#include "core.h"
#include <string>

//! Structure for song tags
struct VGM_Tag
{
	std::string title;
	std::string author;
	std::string creator;
	std::string date;
	std::string system;
	std::string notes;
	std::string game;
	std::string title_j;
	std::string author_j;
	std::string system_j;
	std::string game_j;
};

//! Writes VGM files.
class VGM_Writer
{
	private:
		std::string filename;
		bool completed;

		uint8_t* buffer;
		uint8_t* buffer_pos;

		static const uint32_t initial_buffer_alloc = 100000;
		uint32_t buffer_alloc;

		double curr_delay;
		uint32_t sample_count;
		uint32_t loop_sample;

		void my_memcpy(void* src, int size);
		void add_datablockcmd(uint8_t dtype, uint32_t size, uint32_t romsize, uint32_t offset);
		void add_delay();
		void add_gd3(const char* s);
		void reserve(uint32_t bytes);

	public:
		VGM_Writer(const char* filename, int version = 0x61, int header_size = 0x80);
		virtual ~VGM_Writer();

		uint32_t get_position();
		uint32_t get_sample_count();
		uint32_t get_loop_sample();
		void delay(double count);
		void set_loop();
		void stop();
		void write(uint8_t command, uint16_t port, uint16_t reg, uint16_t data);
		void dac_setup(uint8_t sid, uint8_t chip_id, uint32_t port, uint32_t reg, uint8_t db_id);
		void dac_start(uint8_t sid, uint32_t start, uint32_t length, uint32_t freq);
		void dac_stop(uint8_t sid);

		void write_tag(const VGM_Tag& tag = {});
		void poke32(uint32_t offset, uint32_t data);
		void poke16(uint32_t offset, uint16_t data);
		void poke8(uint32_t offset, uint8_t data);
		uint32_t peek32(uint32_t offset);
		uint16_t peek16(uint32_t offset);
		uint8_t peek8(uint32_t offset);
		void datablock(uint8_t dbtype, uint32_t dbsize, const uint8_t* db, uint32_t maxsize, uint32_t mask = 0xffffffff, uint32_t flags = 0, uint32_t offset = 0);
};

#endif
