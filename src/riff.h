//! \file riff.h
#ifndef RIFF_H
#define RIFF_H
#include "core.h"
#include <vector>

//! RIFF (Resource Interchange File Format) I/O class
class RIFF
{
	private:
		uint32_t size;
		uint32_t position;
		uint32_t type;
		std::vector<uint8_t> data;

	public:
		const static uint32_t TYPE_RIFF = 0x52494646;
		const static uint32_t TYPE_LIST = 0x4C495354;
		const static uint32_t ID_NONE = 0x20202020;

		RIFF(uint32_t chunk_type);
		RIFF(uint32_t chunk_type, const std::vector<uint8_t>& initial_data);
		RIFF(uint32_t chunk_type, uint32_t id);
		RIFF(uint32_t chunk_type, uint32_t id, const std::vector<uint8_t>& initial_data);
		RIFF(const std::vector<uint8_t>& initial_data);

		void rewind();
		bool at_end() const;

		//void set_type(uint32_t type);
		uint32_t get_type() const;
		void set_id(uint32_t id);
		uint32_t get_id() const;
		//uint32_t get_size() const;

		void add_chunk(const class RIFF& new_chunk);
		void add_data(std::vector<uint8_t>& new_data);

		std::vector<uint8_t>& get_data();
		std::vector<uint8_t> get_chunk();

		std::vector<uint8_t> to_bytes() const;
};

#endif
