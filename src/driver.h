//! \file driver.h
#ifndef DRIVER_H
#define DRIVER_H
#include "core.h"

//! Base sound driver class
class Driver
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
		Driver(unsigned int rate, VGM_Writer* vgm);

		//! Play a song
		virtual void play_song(Song& song) = 0;
		//! Reset the sound driver
		virtual void reset() = 0;
		//! Play a tick and return the delta until the next.
		virtual double play_step() = 0;

		bool is_finished() const;
};

#endif

