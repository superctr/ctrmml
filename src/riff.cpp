#include "riff.h"
#include <stdexcept>

//! Write 32-bit little endian integer.
static void write_le32(std::vector<uint8_t>& data, uint32_t pos, uint32_t new_data)
{
	if(data.size() < pos+4)
		data.resize(pos+4);
	data[pos] = new_data;
	data[pos+1] = new_data>>8;
	data[pos+2] = new_data>>16;
	data[pos+3] = new_data>>24;
}

//! Write 32-bit big endian integer.
static void write_be32(std::vector<uint8_t>& data, uint32_t pos, uint32_t new_data)
{
	if(data.size() < pos+4)
		data.resize(pos+4);
	data[pos+3] = new_data;
	data[pos+2] = new_data>>8;
	data[pos+1] = new_data>>16;
	data[pos] = new_data>>24;
}

//! Read 32-bit little endian integer.
static uint32_t read_le32(const std::vector<uint8_t>& data, uint32_t pos)
{
	return data.at(pos) | (data.at(pos+1)<<8) | (data.at(pos+2)<<16) | (data.at(pos+3)<<24);
}

//! Read 32-bit big endian integer.
static uint32_t read_be32(const std::vector<uint8_t>& data, uint32_t pos)
{
	return data.at(pos+3) | (data.at(pos+2)<<8) | (data.at(pos+1)<<16) | (data.at(pos+0)<<24);
}

//! Create new RIFF.
RIFF::RIFF(uint32_t chunk_type)
	: type(chunk_type)
	, data()
{
	if(type == TYPE_RIFF || type == TYPE_LIST)
	{
		write_be32(data,0,ID_NONE);
	}
	rewind();
}

//! Create new RIFF with predefined data.
RIFF::RIFF(uint32_t chunk_type, const std::vector<uint8_t>& initial_data)
	: type(chunk_type)
	, data()
{
	if(type == TYPE_RIFF || type == TYPE_LIST)
	{
		write_be32(data,0,ID_NONE);
	}
	data.insert(data.end(),initial_data.begin(),initial_data.end());
	rewind();
}

//! Create new RIFF with predefined ID.
RIFF::RIFF(uint32_t chunk_type, uint32_t id)
	: type(chunk_type)
	, data()
{
	write_be32(data,0,id);
	rewind();
}

//! Create new RIFF with predefined ID and data.
RIFF::RIFF(uint32_t chunk_type, uint32_t id, const std::vector<uint8_t>& initial_data)
	: type(chunk_type)
	, data()
{
	write_be32(data,0,id);
	data.insert(data.end(),initial_data.begin(),initial_data.end());
	rewind();
}

//! Create new RIFF from a chunk
/*!
 * \exception std::out_of_range too small.
 */
RIFF::RIFF(const std::vector<uint8_t>& initial_data)
	: type()
	, data()
{
	if(initial_data.size() < 8)
		throw std::out_of_range("RIFF::RIFF");
	type = read_be32(initial_data,0);
	uint32_t size = read_le32(initial_data,4);
	uint32_t actual_size = initial_data.size() - 8;
	if(size > actual_size) // Handle incorrect data size
		size = actual_size;
	data.insert(data.end(),initial_data.begin()+8,initial_data.begin()+8+size);
	rewind();
}

//! Rewinds the chunk counter.
void RIFF::rewind()
{
	if(type == TYPE_RIFF || type == TYPE_LIST)
		position = 4;
	else
		position = 0;
}

//! Returns true if the chunk counter points at the end.
bool RIFF::at_end() const
{
	// at the end of an even or uneven sized chunk
	if(position >= data.size())
		return true;
	// at the end of an even sized list with an alignment byte
	// added but counted into the size (shouldn't normally happen...)
	else if((position & 1) && position >= data.size() - 1)
		return true;
	else
		return false;
}

//! Get the RIFF type.
uint32_t RIFF::get_type() const
{
	return type;
}

//! Set the ID (RIFF or LIST type only).
/*!
 * \exception std::invalid_argument if not applicable to this RIFF type.
 */
void RIFF::set_id(uint32_t id)
{
	if(type == TYPE_RIFF || type == TYPE_LIST)
		return write_be32(data,0,id);
	else
		throw std::invalid_argument("RIFF::set_id()");
}

//! Get the ID (RIFF or LIST type only).
/*!
 * \exception std::invalid_argument if not applicable to this RIFF type.
 */
uint32_t RIFF::get_id() const
{
	if(type == TYPE_RIFF || type == TYPE_LIST)
		return read_be32(data,0);
	else
		throw std::invalid_argument("RIFF::get_id()");
}

//! Add a chunk (RIFF or LIST type only).
/*!
 * \exception std::invalid_argument if not applicable to this RIFF type.
 */
void RIFF::add_chunk(const class RIFF& new_chunk)
{
	// If data size is uneven, insert an aligment byte.
	if(data.size() & 1)
		data.push_back(0);

	if(type == TYPE_RIFF || type == TYPE_LIST)
	{
		auto new_data = new_chunk.data;
		write_be32(data,data.size(),new_chunk.get_type());
		write_le32(data,data.size(),new_data.size());
		data.insert(data.end(),new_data.begin(),new_data.end());
	}
	else
	{
		throw std::invalid_argument("RIFF::add_chunk()");
	}
}

//! Add data. (non-RIFF or LIST types only)
/*!
 * \exception std::invalid_argument if not applicable to this RIFF type.
 */
void RIFF::add_data(std::vector<uint8_t>& new_data)
{
	if(type == TYPE_RIFF || type == TYPE_LIST)
	{
		throw std::invalid_argument("RIFF::add_data()");
	}
	else
	{
		data.insert(data.end(),new_data.begin(),new_data.end());
	}
}

//! Get chunk data.
/*! Returns a non-const reference to the internal data vector,
 * means that the data can be modified, but you should not make a
 * pointer of this data or keep the reference for a long time as
 * it can easily be corrupted by resizing the data vector (e.g. by
 * add_chunk() or add_data())
 */
std::vector<uint8_t>& RIFF::get_data()
{
	return data;
}

//! Return a vector containing the next chunk and increments the position.
std::vector<uint8_t> RIFF::get_chunk()
{
	// has to be a list type chunk.
	if(type != TYPE_RIFF && type != TYPE_LIST)
		throw std::invalid_argument("RIFF::get_chunk()");

	// If data size is uneven, insert an aligment byte.
	if(position & 1)
		position++;

	std::vector<uint8_t> chunk_data;
	write_le32(chunk_data,0,read_le32(data,position)); // could be optimized
	position += 4;
	uint32_t size = read_le32(data,position);
	write_be32(chunk_data,4,size);
	position += 4;
	if(position+size > data.size()) // Handle incorrect data size
		size = data.size() - position;
	chunk_data.insert(chunk_data.end(),data.begin()+position,data.begin()+position+size);
	position += size;

	return chunk_data;
}

//! Convert the RIFF object to a byte vector.
std::vector<uint8_t> RIFF::to_bytes() const
{
	std::vector<uint8_t> chunk_data;
	write_be32(chunk_data,0,type);
	write_le32(chunk_data,4,data.size());
	chunk_data.insert(chunk_data.end(),data.begin(),data.end());
	// Add alignment byte if needed.
	if(data.size() & 1)
		chunk_data.push_back(0);
	return chunk_data;
}
