//! \file util.h
#ifndef UTIL_H
#define UTIL_H

//! Write 32-bit little endian integer to a vector.
static inline void write_le32(std::vector<uint8_t>& data, uint32_t pos, uint32_t new_data)
{
	if(data.size() < pos+4)
		data.resize(pos+4);
	data[pos] = new_data;
	data[pos+1] = new_data>>8;
	data[pos+2] = new_data>>16;
	data[pos+3] = new_data>>24;
}

//! Write 32-bit big endian integer to a vector.
static inline void write_be32(std::vector<uint8_t>& data, uint32_t pos, uint32_t new_data)
{
	if(data.size() < pos+4)
		data.resize(pos+4);
	data[pos+3] = new_data;
	data[pos+2] = new_data>>8;
	data[pos+1] = new_data>>16;
	data[pos] = new_data>>24;
}

//! Write 16-bit big endian integer to a vector.
static inline void write_be16(std::vector<uint8_t>& data, uint32_t pos, uint32_t new_data)
{
	if(data.size() < pos+2)
		data.resize(pos+2);
	data[pos+1] = new_data;
	data[pos+0] = new_data>>8;
}

//! Read 32-bit little endian integer to a vector.
static inline uint32_t read_le32(const std::vector<uint8_t>& data, uint32_t pos)
{
	return data.at(pos) | (data.at(pos+1)<<8) | (data.at(pos+2)<<16) | (data.at(pos+3)<<24);
}

//! Read 32-bit big endian integer to a vector.
static inline uint32_t read_be32(const std::vector<uint8_t>& data, uint32_t pos)
{
	return data.at(pos+3) | (data.at(pos+2)<<8) | (data.at(pos+1)<<16) | (data.at(pos+0)<<24);
}

#endif // UTIL_H
