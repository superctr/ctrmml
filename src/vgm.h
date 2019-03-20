//! \file vgm.h
#ifndef VGM_H
#define VGM_H
#include "core.h"
#include <string>

//! Writes VGM files.
class VGM_Writer
{
	private:
		std::string filename;
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
		void delay(double count);
		void set_loop();
		void stop();
		void write(uint8_t command, uint16_t port, uint16_t reg, uint16_t data);
		void write_tag();
		void poke32(uint32_t offset, uint32_t data);
		void poke16(uint32_t offset, uint16_t data);
		void poke8(uint32_t offset, uint8_t data);
		uint32_t peek32(uint32_t offset);
		uint16_t peek16(uint32_t offset);
		uint8_t peek8(uint32_t offset);
		void datablock(uint8_t dbtype, uint32_t dbsize, uint8_t* datablock, uint32_t maxsize, uint32_t mask, uint32_t flags);
};

//! Base sound driver class
class VGM_Driver
{
	private:
		VGM_Writer* vgm_writer;
		double delta;
		unsigned int rate;

	protected:
		bool finished;
		void write(uint8_t command, uint16_t port, uint16_t reg, uint16_t data);
		void ym2612_w(uint8_t port, uint8_t reg, uint8_t ch, uint8_t op, uint16_t data);
		void sn76489_w(uint8_t port, uint8_t reg, uint8_t ch, uint8_t data);

	public:
		VGM_Driver(unsigned int rate, VGM_Writer* vgm);

		//! Play a song
		virtual void play_song(Song& song) = 0;
		//! Reset the sound driver
		virtual void reset() = 0;
		//! Play a tick and return the delta until the next.
		virtual double play_step() = 0;

		bool is_finished() const;
};
#endif
