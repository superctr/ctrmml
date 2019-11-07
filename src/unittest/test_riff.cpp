#include <stdexcept>
#include <cppunit/extensions/HelperMacros.h>
#include "../riff.h"

class RIFF_Test : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(RIFF_Test);
	CPPUNIT_TEST(test_riff_initialize);
	CPPUNIT_TEST(test_riff_add_data);
	CPPUNIT_TEST(test_riff_add_chunk);
	CPPUNIT_TEST(test_riff_add_chunk_alignment);
	CPPUNIT_TEST(test_riff_encode_decode);
	CPPUNIT_TEST_SUITE_END();
public:
	void setUp()
	{
	}
	void tearDown()
	{
	}
	void test_riff_initialize()
	{
		// empty RIFF chunk
		{
			std::vector<uint8_t> data_block = {
				'R','I','F','F',4,0,0,0,' ',' ',' ',' '
			};
			auto riff = RIFF(RIFF::TYPE_RIFF);
			CPPUNIT_ASSERT_MESSAGE("RIFF(chunk_type)", riff.to_bytes() == data_block);
		}
		// empty RIFF chunk with ID
		{
			std::vector<uint8_t> data_block = {
				'R','I','F','F',4,0,0,0,'R','I','F','F'
			};
			auto riff = RIFF(RIFF::TYPE_RIFF, RIFF::TYPE_RIFF);
			CPPUNIT_ASSERT_MESSAGE("RIFF(chunk_type,id)", riff.to_bytes() == data_block);
		}
		// empty RIFF chunk with predefined data
		{
			std::vector<uint8_t> initial_data = {
				65,66,67,68,4,0,0,0,69,70,71,72
			};
			std::vector<uint8_t> data_block = {
				'R','I','F','F',16,0,0,0,' ',' ',' ',' ',65,66,67,68,4,0,0,0,69,70,71,72
			};
			auto riff = RIFF(RIFF::TYPE_RIFF, initial_data);
			CPPUNIT_ASSERT_MESSAGE("RIFF(chunk_type,initial_data)", riff.to_bytes() == data_block);
		}
		// empty RIFF chunk with ID and predefined data
		{
			std::vector<uint8_t> initial_data = {
				65,66,67,68,4,0,0,0,69,70,71,72
			};
			std::vector<uint8_t> data_block = {
				'L','I','S','T',16,0,0,0,'R','I','F','F',65,66,67,68,4,0,0,0,69,70,71,72
			};
			auto riff = RIFF(RIFF::TYPE_LIST, RIFF::TYPE_RIFF, initial_data);
			CPPUNIT_ASSERT_MESSAGE("RIFF(chunk_type,id,initial_data)", riff.to_bytes() == data_block);
		}
		// empty non-RIFF chunk
		{
			std::vector<uint8_t> data_block = {
				65,66,67,68,0,0,0,0
			};
			auto riff = RIFF(0x41424344);
			CPPUNIT_ASSERT_MESSAGE("RIFF(0x41424344)", riff.to_bytes() == data_block);
		}
		// empty non-RIFF chunk with predefined data
		{
			std::vector<uint8_t> initial_data = {
				65,66,67,68,4,0,0,0,69,70,71
			};
			std::vector<uint8_t> data_block = {
				65,66,67,68,11,0,0,0,65,66,67,68,4,0,0,0,69,70,71,0
			};
			auto riff = RIFF(0x41424344, initial_data);
			CPPUNIT_ASSERT_MESSAGE("RIFF(chunk_type,initial_data)", riff.to_bytes() == data_block);
		}
	}
	void test_riff_add_data()
	{
		std::vector<uint8_t> added_data = {
			65,66,67,68,4,0,0,0,69,70,71,72
		};
		std::vector<uint8_t> data_block = {
			65,66,67,68,12,0,0,0,65,66,67,68,4,0,0,0,69,70,71,72
		};
		auto riff = RIFF(0x41424344);
		riff.add_data(added_data);
		CPPUNIT_ASSERT_MESSAGE("RIFF(chunk_type,initial_data)", riff.to_bytes() == data_block);
		auto riff2 = RIFF(RIFF::TYPE_RIFF);
		CPPUNIT_ASSERT_THROW(riff2.add_data(added_data), std::invalid_argument);
	}
	void test_riff_add_chunk()
	{
		std::vector<uint8_t> data_block = {
			'R','I','F','F',16,0,0,0,' ',' ',' ',' ',65,66,67,68,4,0,0,0,69,70,71,72
		};
		auto riff = RIFF(RIFF::TYPE_RIFF);
		auto new_chunk = RIFF(0x41424344,0x45464748);
		riff.add_chunk(new_chunk);
		CPPUNIT_ASSERT_MESSAGE("RIFF(chunk_type,initial_data)", riff.to_bytes() == data_block);
		auto riff2 = RIFF(0x41424344);
		CPPUNIT_ASSERT_THROW(riff2.add_chunk(new_chunk), std::invalid_argument);
	}
	void test_riff_add_chunk_alignment()
	{
		std::vector<uint8_t> added_data = {
			1,2,3
		};
		// it's not clear if the size of the RIFF chunk should include the padding byte
		// but that is what I assume.
		std::vector<uint8_t> data_block = {
			'R','I','F','F',27,0,0,0,' ',' ',' ',' ',65,66,67,68,3,0,0,0,1,2,3,0,
			65,66,67,69,3,0,0,0,1,2,3,0
		};
		auto riff = RIFF(RIFF::TYPE_RIFF);
		riff.add_chunk(RIFF(0x41424344,added_data));
		riff.add_chunk(RIFF(0x41424345,added_data));
		CPPUNIT_ASSERT_MESSAGE("RIFF(chunk_type,initial_data)", riff.to_bytes() == data_block);
	}
	// dirty way to convert a string to vector container
	std::vector<uint8_t> str_to_vector(const char* str)
	{
		std::vector<uint8_t> vec = {};
		while(*str)
			vec.push_back(*str++);
		return vec;
	}
	void test_riff_encode_decode()
	{
		auto added_data = str_to_vector("some bytes!");
		auto added_data2 = str_to_vector("even more bytes...");
		auto added_data3 = str_to_vector("first element of a list");
		auto added_data4 = str_to_vector("second element of a list");
		auto added_data5 = str_to_vector("one final element");
		auto riff = RIFF(RIFF::TYPE_RIFF);
		riff.add_chunk(RIFF(0x41424344,added_data));
		riff.add_chunk(RIFF(0x41424345,added_data2));
		auto list = RIFF(RIFF::TYPE_LIST);
		list.set_id(0x41424346);
		list.add_chunk(RIFF(0x41424347,added_data3));
		list.add_chunk(RIFF(0x41424348,added_data4));
		riff.add_chunk(list);
		riff.add_chunk(RIFF(0x41424349,added_data5));
		// check first chunk
		CPPUNIT_ASSERT_EQUAL(false, riff.at_end());
		auto chunk = RIFF(riff.get_chunk());
		CPPUNIT_ASSERT_EQUAL((uint32_t)0x41424344, chunk.get_type());
		CPPUNIT_ASSERT_MESSAGE("mismatch", added_data == chunk.get_data());
		// check second chunk
		CPPUNIT_ASSERT_EQUAL(false, riff.at_end());
		chunk = RIFF(riff.get_chunk());
		CPPUNIT_ASSERT_EQUAL((uint32_t)0x41424345, chunk.get_type());
		CPPUNIT_ASSERT_MESSAGE("mismatch", added_data2 == chunk.get_data());
		// check third chunk
		CPPUNIT_ASSERT_EQUAL(false, riff.at_end());
		chunk = RIFF(riff.get_chunk());
		CPPUNIT_ASSERT_EQUAL((uint32_t)RIFF::TYPE_LIST, chunk.get_type());
		CPPUNIT_ASSERT_EQUAL((uint32_t)0x41424346, chunk.get_id());
		// check first sub chunk
		CPPUNIT_ASSERT_EQUAL(false, chunk.at_end());
		auto subchunk = RIFF(chunk.get_chunk());
		CPPUNIT_ASSERT_EQUAL((uint32_t)0x41424347, subchunk.get_type());
		CPPUNIT_ASSERT_MESSAGE("mismatch", added_data3 == subchunk.get_data());
		// check second sub chunk
		CPPUNIT_ASSERT_EQUAL(false, chunk.at_end());
		subchunk = RIFF(chunk.get_chunk());
		CPPUNIT_ASSERT_EQUAL((uint32_t)0x41424348, subchunk.get_type());
		CPPUNIT_ASSERT_MESSAGE("mismatch", added_data4 == subchunk.get_data());
		// at the end of subchunk
		CPPUNIT_ASSERT_EQUAL(true, chunk.at_end());
		// check fifth chunk
		CPPUNIT_ASSERT_EQUAL(false, riff.at_end());
		chunk = RIFF(riff.get_chunk());
		CPPUNIT_ASSERT_EQUAL((uint32_t)0x41424349, chunk.get_type());
		CPPUNIT_ASSERT_MESSAGE("mismatch", added_data5 == chunk.get_data());
		CPPUNIT_ASSERT_EQUAL(true, riff.at_end());
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(RIFF_Test);

