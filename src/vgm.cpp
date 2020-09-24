#include <new>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>

#if defined(_WIN32)
#include <windows.h>
#else
#include <cuchar>
#endif

#include <clocale>
#include <cwchar>

#include "vgm.h"

void VGM_Interface::set_loop()
{
}

void VGM_Interface::stop()
{
}

//=====================================================================

//! Constructs a VGM_Writer.
/*!
 * \param filename If blank, no file is written to the disk.
 * \param version Minor part of the VGM file version. Major version 0x1 is always written.
 * \param header_size the size of the VGM file header.
 */
VGM_Writer::VGM_Writer(const char* filename, int version, int header_size)
	: filename(filename),
	completed(0),
	curr_delay(0),
	sample_count(0),
	loop_sample(0)
{
	// create initial buffer
	buffer = (uint8_t*) std::calloc(initial_buffer_alloc, sizeof(uint8_t));
	if(!buffer)
		throw std::bad_alloc();
	buffer_pos = buffer;
	buffer_alloc = initial_buffer_alloc;

	// vgm magic
	my_memcpy((uint32_t*)"Vgm ", 4);
	buffer_pos += 4;
	*buffer_pos++ = version;
	*buffer_pos++ = 0x01;

	// header offset
	poke32(0x34, header_size - 0x34);
	buffer_pos = buffer + header_size;
}

//! VGM_Writer destructor
VGM_Writer::~VGM_Writer()
{
	poke32(0x04, get_position() - 4);
	if(filename.size() && completed)
	{
		std::cout << "Writing " << filename << "...\n";
		auto output = std::ofstream(filename, std::ios::binary);
		output.write((char*)buffer, get_position());
		std::free(buffer);
	}
}

//! Write a command.
void VGM_Writer::write(uint8_t command, uint16_t port, uint16_t reg, uint16_t data)
{
	reserve(100);
	add_delay();

	if(command == 0xe1) // C352
	{
		*buffer_pos++ = command;
		*buffer_pos++ = reg>>8;
		*buffer_pos++ = reg&0xff;
		*buffer_pos++ = data>>8;
		*buffer_pos++ = data&0xff;
	}
	else if(command > 0x50 && command < 0x60) // yamaha chips
	{
		*buffer_pos++ = command + port;
		*buffer_pos++ = reg;
		*buffer_pos++ = data;
	}
	else if(command == 0x50 || command == 0x30) // PSG
	{
		*buffer_pos++ = command;
		*buffer_pos++ = data;
	}
	else // following is for D0-D6 commands...
	{
		*buffer_pos++ = command;
		*buffer_pos++ = port;
		*buffer_pos++ = (reg&0xff);
		*buffer_pos++ = (data&0xff);
	}
}

//! DAC stream setup
void VGM_Writer::dac_setup(uint8_t sid, uint8_t chip_id, uint32_t port, uint32_t reg, uint8_t db_id)
{
	reserve(100);
	add_delay();
	*buffer_pos++ = 0x90;
	*buffer_pos++ = sid;
	*buffer_pos++ = chip_id;
	*buffer_pos++ = port;
	*buffer_pos++ = reg;
	*buffer_pos++ = 0x91;
	*buffer_pos++ = sid;
	*buffer_pos++ = db_id;
	*buffer_pos++ = 1;
	*buffer_pos++ = 0;
}

//! DAC stream playback
void VGM_Writer::dac_start(uint8_t sid, uint32_t start, uint32_t length, uint32_t freq)
{
	reserve(100);
	add_delay();
	*buffer_pos++ = 0x92;
	*buffer_pos++ = sid;
	*buffer_pos++ = freq & 0xff;
	*buffer_pos++ = freq >> 8;
	*buffer_pos++ = freq >> 16;
	*buffer_pos++ = freq >> 24;
	*buffer_pos++ = 0x93;
	*buffer_pos++ = sid;
	*buffer_pos++ = start & 0xff;
	*buffer_pos++ = start >> 8;
	*buffer_pos++ = start >> 16;
	*buffer_pos++ = start >> 24;
	*buffer_pos++ = 0x01;
	*buffer_pos++ = length & 0xff;
	*buffer_pos++ = length >> 8;
	*buffer_pos++ = length >> 16;
	*buffer_pos++ = length >> 24;
}

//! DAC stream playback
void VGM_Writer::dac_stop(uint8_t sid)
{
	reserve(100);
	add_delay();
	*buffer_pos++ = 0x94;
	*buffer_pos++ = sid;
}

//! Sets the loop point
void VGM_Writer::set_loop()
{
	add_delay();
	loop_sample = sample_count;
	poke32(0x1c, get_position()-0x1c);
}

//! Adds a datablock.
/*!
 * NOTE: mask argument is currently ignored. So leave it -1.
 */
void VGM_Writer::datablock(uint8_t dbtype, uint32_t dbsize, const uint8_t* db, uint32_t maxsize, uint32_t mask, uint32_t flags, uint32_t offset)
{
	reserve(dbsize + 100);
	add_delay();
	add_datablockcmd(dbtype, dbsize | flags, maxsize, offset);
	for(uint32_t i = 0; i < dbsize; i++)
	{
		*buffer_pos++ = *db++;
	}
}

//! Adds a delay
void VGM_Writer::delay(double count)
{
	curr_delay += count;
}

//! Add a VGM stop command (0x66)
void VGM_Writer::stop()
{
	add_delay();
	*buffer_pos++ = 0x66;
	poke32(0x18, sample_count);
	if(loop_sample)
		poke32(0x20, sample_count - loop_sample);
	completed = 1;
}

//! Write a long to the vgm buffer
void VGM_Writer::poke32(uint32_t offset, uint32_t data)
{
	*(uint32_t*)(buffer+offset) = data;
}

//! Write a short to the vgm buffer
void VGM_Writer::poke16(uint32_t offset, uint16_t data)
{
	*(uint16_t*)(buffer+offset) = data;
}

//! Write a char to the vgm buffer
void VGM_Writer::poke8(uint32_t offset, uint8_t data)
{
	*(uint8_t*)(buffer+offset) = data;
}

//! Write GD3 tags. Only call this after calling VGM_Writer::stop().
void VGM_Writer::write_tag(const VGM_Tag& tag)
{
	reserve(2000);
	std::time_t t;
	std::time(&t);
	std::tm* tm = std::localtime(&t);
	char ts[32];
	std::strftime(ts,32,"%Y-%m-%d %H:%M:%S",tm);
	std::string tracknotes = "ctrmml (built " __DATE__ " " __TIME__ ")";
	poke32(0x14, get_position()-0x14);
	my_memcpy((uint8_t*)"Gd3 \x00\x01\x00\x00", 8);
	uint32_t len_s = get_position();
	buffer_pos += 4;

	add_gd3(tag.title.c_str()); // Track name
	add_gd3(tag.title_j.c_str()); // Track name (native)
	add_gd3(tag.game.c_str()); // Game name
	add_gd3(tag.game_j.c_str()); // Game name (native)
	add_gd3(tag.system.c_str()); // System name
	add_gd3(tag.system_j.c_str()); // System name (native)
	add_gd3(tag.author.c_str()); // Author name
	add_gd3(tag.author_j.c_str()); // Author name (native)
	if(tag.date.size())
		add_gd3(tag.date.c_str()); // Date
	else
		add_gd3(ts);
	add_gd3(tag.creator.c_str()); // Pack author
	if(tag.notes.size())
		add_gd3(tag.notes.c_str()); // Notes
	else
		add_gd3(tracknotes.c_str());

	poke32(len_s, get_position()-len_s-4); // length
}

//! Gets the current buffer position
uint32_t VGM_Writer::get_position() const
{
	return buffer_pos - buffer;
}

//! Gets the current sample position
uint32_t VGM_Writer::get_sample_count() const
{
	return sample_count;
}

//! Gets the sample count at the loop position
uint32_t VGM_Writer::get_loop_sample() const
{
	return loop_sample;
}

//! Return a long from the vgm buffer
uint32_t VGM_Writer::peek32(uint32_t offset) const
{
	return *(uint32_t*)(buffer+offset);
}

//! Return a short from the vgm buffer
uint16_t VGM_Writer::peek16(uint32_t offset) const
{
	return *(uint16_t*)(buffer+offset);
}

//! Return a char from the vgm buffer
uint8_t VGM_Writer::peek8(uint32_t offset) const
{
	return *(uint8_t*)(buffer+offset);
}

//! Get the VGM buffer.
std::vector<uint8_t> VGM_Writer::get_buffer()
{
	if(completed)
		poke32(0x04, get_position() - 4);

	return std::vector<uint8_t>(buffer, buffer + get_position());
}

void VGM_Writer::my_memcpy(void* src, int size)
{
	std::memcpy(buffer_pos,src,size);
	buffer_pos += size;
}

void VGM_Writer::add_datablockcmd(uint8_t dtype, uint32_t size, uint32_t romsize, uint32_t offset)
{
	*buffer_pos++ = 0x67;
	*buffer_pos++ = 0x66;
	*buffer_pos++ = dtype;
	size += 8;
	my_memcpy((uint32_t*)&size,4);
	my_memcpy((uint32_t*)&romsize,4);
	my_memcpy((uint32_t*)&offset,4);
}

void VGM_Writer::add_delay()
{
	if(curr_delay >= 1)
	{
		int delay = std::floor(curr_delay);
		curr_delay -= delay;

		sample_count += delay;
		int commandcount = delay/65535;
		uint16_t finalcommand = delay%65535;

		while(commandcount)
		{
			*buffer_pos++ = 0x61;
			*buffer_pos++ = 0xff;
			*buffer_pos++ = 0xff;
			commandcount--;
		}

		if(finalcommand > 16)
		{
			*buffer_pos++ = 0x61;
			my_memcpy(&finalcommand,2);
		}
		else if(finalcommand > 0)
		{
			*buffer_pos++ = 0x70 + finalcommand-1;
		}
	}
}

void VGM_Writer::add_gd3(const char* s)
{
	long l;
	long max=256;
#if defined(_WIN32)
	l = MultiByteToWideChar(CP_UTF8,0,s,-1,(wchar_t*)buffer_pos,max);
	if(l == 0)
	{
		std::cerr << "Something very bad happened. MultiByteToWideChar failed\n";
		buffer_pos += 2;
	}
	else
	{
		buffer_pos += l * sizeof(wchar_t);
		if(l == max)
			buffer_pos += 2;
	}
#else
	std::setlocale(LC_ALL, "en_US.utf8");
	static std::mbstate_t mbstate;
	while((l = std::mbrtoc16((char16_t*)buffer_pos,s,max,&mbstate)))
	{
		if(l == -1 || l == -2)
			break;
		else if(l == -3)
			buffer_pos += 2;
		else
		{
			s += l;
			buffer_pos += 2;
			max --;
		}
	}
	buffer_pos += 2;
#endif
}

void VGM_Writer::reserve(uint32_t bytes)
{
	// resize buffer if needed
	while((buffer_alloc - get_position()) < bytes)
	{
		uint8_t* temp;
		temp = (uint8_t*)realloc(buffer,buffer_alloc*2);
		if(temp)
		{
			buffer_alloc *= 2;
			buffer_pos = temp+get_position();
			buffer = temp;
		}
		else
		{
			throw std::bad_alloc();
		}
	}
}
