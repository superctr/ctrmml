#include "driver.h"
#include "vgm.h"

#define DEBUG_FM(fmt,...) { }
#define DEBUG_PSG(fmt,...) { }
//#define DEBUG_FM(fmt,...) { printf(fmt, __VA_ARGS__); }
//#define DEBUG_PSG(fmt,...) { printf(fmt, __VA_ARGS__); }

Driver::Driver(unsigned int rate, VGM_Writer* vgm)
	: vgm_writer(vgm), delta(0), rate(rate), finished(0)
{
}

void Driver::write(uint8_t command, uint16_t port, uint16_t reg, uint16_t data)
{
	if(vgm_writer)
		vgm_writer->write(command, port, reg, data);
}

void Driver::set_loop()
{
	if(vgm_writer)
	{
		vgm_writer->set_loop();
	}
}

void Driver::ym2612_w(uint8_t port, uint8_t reg, uint8_t ch, uint8_t op, uint16_t data)
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

void Driver::sn76489_w(uint8_t reg, uint8_t ch, uint16_t data)
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

bool Driver::is_finished() const
{
	return finished;
}

