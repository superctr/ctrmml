#include "vgm.h"
#include <new>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>

#if defined(__unix__)
#include <cuchar>
#endif
// windows?
#include <wchar.h>

//#define DEBUG_FM(fmt,...) { }
//#define DEBUG_PSG(fmt,...) { }
#define DEBUG_FM(fmt,...) { printf(fmt, __VA_ARGS__); }
#define DEBUG_PSG(fmt,...) { printf(fmt, __VA_ARGS__); }

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

//! Constructs a VGM writer.
/*!
 * \param filename If blank, no file is written to the disk.
 * \param version Minor part of the VGM file version. Major version 0x1 is always written.
 * \param header_size the size of the VGM file header.
 */
VGM_Writer::VGM_Writer(const char* filename, int version, int header_size)
	: filename(filename), curr_delay(0), sample_count(0), loop_sample(0)
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

//! Destroys a vgm writer.
VGM_Writer::~VGM_Writer()
{
	poke32(0x04, get_position() - 4);
	if(filename.size())
	{
		std::cout << "Writing " << filename << "...\n";
		auto output = std::ofstream(filename, std::ios::binary);
		output.write((char*)buffer, get_position());
		std::free(buffer);
	}
}

//! Gets the current buffer position
uint32_t VGM_Writer::get_position()
{
	return buffer_pos - buffer;
}

//! Adds a delay
void VGM_Writer::delay(double count)
{
	curr_delay += count;
}

//! Sets the loop point
void VGM_Writer::set_loop()
{
	add_delay();
	loop_sample = sample_count;
	poke32(0x1c, get_position()-0x1c);
}

//! Add a VGM stop command (0x66)
void VGM_Writer::stop()
{
	add_delay();
	*buffer_pos++ = 0x66;
	poke32(0x18, sample_count);
	if(loop_sample)
		poke32(0x20, sample_count - loop_sample);
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

void VGM_Writer::add_gd3(const char* s)
{
	long l;
#if defined(_WIN32) && defined(__MINGW32__) && !defined(__NO_ISOCEXT)
	l = _snwprintf((wchar_t*)buffer_pos,256,L"%S", s);
	buffer_pos += (l+1)*2;
#elif defined(__unix__)
	long max=256;
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
#else
	l = swprintf((wchar_t*)buffer_pos,256,L"%s", s);
	buffer_pos += (l+1)*2;
#endif // defined
}

//! Write GD3 tags. Always call this after vgm_stop().
void VGM_Writer::write_tag()
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

	add_gd3(""); // Track name
	add_gd3(""); // Track name (native)
	add_gd3(""); // Game name
	add_gd3(""); // Game name (native)
	add_gd3(""); // System name
	add_gd3(""); // System name (native)
	add_gd3(""); // Author name
	add_gd3(""); // Author name (native)
	add_gd3(ts); // Time
	add_gd3(""); // Pack author
	add_gd3(tracknotes.c_str()); // Notes

	poke32(len_s, get_position()-len_s-4); // length
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

//! Return a long from the vgm buffer
uint32_t VGM_Writer::peek32(uint32_t offset)
{
	return *(uint32_t*)(buffer+offset);
}

//! Return a short from the vgm buffer
uint16_t VGM_Writer::peek16(uint32_t offset)
{
	return *(uint16_t*)(buffer+offset);
}

//! Return a char from the vgm buffer
uint8_t VGM_Writer::peek8(uint32_t offset)
{
	return *(uint8_t*)(buffer+offset);
}

//! Adds a datablock.
void VGM_Writer::datablock(uint8_t dbtype, uint32_t dbsize, uint8_t* datablock, uint32_t maxsize, uint32_t mask, uint32_t flags)
{
	reserve(dbsize + 100);
	add_delay();
}

VGM_Driver::VGM_Driver(unsigned int rate, VGM_Writer* vgm)
	: vgm_writer(vgm), delta(0), rate(rate), finished(0)
{
}

void VGM_Driver::write(uint8_t command, uint16_t port, uint16_t reg, uint16_t data)
{
	if(vgm_writer)
		vgm_writer->write(command, port, reg, data);
}

void VGM_Driver::ym2612_w(uint8_t port, uint8_t reg, uint8_t ch, uint8_t op, uint16_t data)
{
	if(reg == 0x28)
	{
		data <<= 4;
		data += ch | (port << 2);
		DEBUG_FM("opn-keyon port %d reg %02x data %02x (ch %d op %d)\n", port, reg, data, ch, op);
		write(0x52, 0, reg, data);
	}
	else if(reg >= 0x30 && reg < 0xa0)
	{
		reg += op*4;
		reg += ch;
		DEBUG_FM("opn-param port %d reg %02x data %02x (ch %d op %d)\n", port, reg, data, ch, op);
		write(0x52, port, reg, data);
	}
	else if(reg >= 0xa0 && reg < 0xb0) // ch3 operator freq
	{
		reg += ch;
		// would use a lookup table in 68k code
		if(reg >= 0xa8 && op == 0)
			reg += 1;
		else if(reg >= 0xa8 && op == 1)
			reg += 0;
		else if(reg >= 0xa8 && op == 2)
			reg += 2;
		else if(reg >= 0xa8 && op == 3)
			reg = 0xa2;
		DEBUG_FM("opn-fnum  port %d reg %02x data %04x (ch %d op %d)\n", port, reg, data, ch, op);
		write(0x52, port, reg+4, data>>8);
		write(0x52, port, reg, data&0xff);
	}
	else if(reg >= 0xb0)
	{
		reg += ch;
		DEBUG_FM("opn-param port %d reg %02x data %02x (ch %d op %d)\n", port, reg, data, ch, op);
		write(0x52, port, reg, data);
	}
	else
	{
		DEBUG_FM("opn-param port %d reg %02x data %02x\n", port, reg, data);
		write(0x52, 0, reg, data); // port0 only
	}
}

void VGM_Driver::sn76489_w(uint8_t port, uint8_t reg, uint8_t ch, uint8_t data)
{
	uint8_t cmd1, cmd2;
	if(reg == 0) // frequency
	{
		data &= 0x3ff;
		cmd1 = (data & 0x0f) | (ch << 5) | 0x80;
		cmd2 = data >> 4;
		DEBUG_PSG("psg %02x,%02x (ch %d freq %04x)\n", cmd1, cmd2, ch, data);
		// don't send second byte if noise
		write(0x50, 0, 0, cmd1);
		if(ch < 3)
			write(0x50, 0, 0, cmd2);
	}
	else if(reg == 1) // volume
	{
		cmd1 = (data & 0x0f) | (ch << 5) | 0x90;
		DEBUG_PSG("psg %02x (ch %d vol %04x)\n", cmd1, ch,  data);
		write(0x50, 0, 0, cmd1);
	}
}

bool VGM_Driver::is_finished() const
{
	return finished;
}

