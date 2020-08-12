/*! \file driver.h
 *  \brief Sound driver base.
 *
 *  A sound driver handles playback of Song files, either real-time
 *  using sound chip interfaces or emulator (such as libvgm), or
 *  just conversion to VGM file.
 */
#ifndef DRIVER_H
#define DRIVER_H
#include "core.h"

//! Sound driver base class.
/*!
 *  \todo this will provide an abstraction between derived
 *        classes and the VGM file or sound chip interfaces provided by
 *        the calling functions.
 *
 *  By using the interfaces `<chip>_w` to write to sound chip registers,
 *  a derived class can support both real-time playback and VGM logging.
 */
class Driver
{
	public:
		Driver(unsigned int rate, VGM_Interface* vgm);

		// TODO: split into load_song() and play_song()?
		//! Play a new song
		virtual void play_song(Song& song) = 0;
		//! Reset the sound driver, silencing sound chips and
		//! allowing for playback to be restarted or a new
		//! song to be played.
		virtual void reset() = 0;
		//! Play a tick and return the delta until the next.
		virtual double play_step() = 0;
		//! Return false if song has finished playback, true otherwise.
		virtual bool is_playing() = 0;

	protected:
		// VGM low-level
		void write(uint8_t command, uint16_t port, uint16_t reg, uint16_t data);
		void set_loop();

		// VGM write helpers
		void ym2612_w(uint8_t port, uint8_t reg, uint8_t ch, uint8_t op, uint16_t data);
		void sn76489_w(uint8_t reg, uint8_t ch, uint16_t data);

	private:
		VGM_Interface* vgm;
		double delta;
		unsigned int rate;
};

#endif

