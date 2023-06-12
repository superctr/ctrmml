#include <stdexcept>
#include <algorithm>
#include <cppunit/extensions/HelperMacros.h>
#include "../mml_input.h"
#include "../song.h"
#include "../platform/mdsdrv.h"
#include "../stringf.h"

class MDSDRV_Converter_Test : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(MDSDRV_Converter_Test);
	CPPUNIT_TEST(test_track_writer);
	CPPUNIT_TEST(test_track_writer_sequence_output);
	CPPUNIT_TEST(test_subroutine_handling);
	CPPUNIT_TEST(test_drum_mode_handling);
	CPPUNIT_TEST(test_loop_handling);
	CPPUNIT_TEST(test_loop_handling_sequence_output);
	CPPUNIT_TEST(test_sequence_optimization);
	CPPUNIT_TEST(test_data_output);
	CPPUNIT_TEST_SUITE_END();
private:
	Song *song;
	MML_Input *mml_input;
public:
	void setUp()
	{
		song = new Song();
		mml_input = new MML_Input(song);
	}
	void tearDown()
	{
		delete mml_input;
		delete song;
	}
	//! test basic functionality of the track writer.
	void test_track_writer()
	{
		mml_input->read_line("A l4o4v5cdef");
		mml_input->read_line("B l8o4V5 L ga");
		mml_input->read_line("C 'cmd 0xf6 0x2287'");
		auto converter = MDSDRV_Converter(*song);

		// check that we have two tracks
		CPPUNIT_ASSERT_EQUAL((int)3, (int)converter.track_list.size());

		// check first tracks. we cast to uint16 to make sure cppunit display asserts as integers
		CPPUNIT_ASSERT_EQUAL((int)6, (int)converter.track_list[0].size());
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::VOL, (uint16_t)converter.track_list[0][0].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0x85, converter.track_list[0][0].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa6, (uint16_t)converter.track_list[0][1].type); // cn4
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, converter.track_list[0][1].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa8, (uint16_t)converter.track_list[0][2].type); // dn4
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, converter.track_list[0][2].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xaa, (uint16_t)converter.track_list[0][3].type); // en4
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, converter.track_list[0][3].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xab, (uint16_t)converter.track_list[0][4].type); // fn4
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, converter.track_list[0][4].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::FINISH, (uint16_t)converter.track_list[0][5].type);

		// check second track.
		CPPUNIT_ASSERT_EQUAL((int)5, (int)converter.track_list[1].size());
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::VOL, (uint16_t)converter.track_list[1][0].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0x05, converter.track_list[1][0].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::SEGNO, (uint16_t)converter.track_list[1][1].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0x00, converter.track_list[1][1].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xad, (uint16_t)converter.track_list[1][2].type); // gn4
		CPPUNIT_ASSERT_EQUAL((uint16_t)12, converter.track_list[1][2].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xaf, (uint16_t)converter.track_list[1][3].type); // an4
		CPPUNIT_ASSERT_EQUAL((uint16_t)12, converter.track_list[1][3].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::JUMP, (uint16_t)converter.track_list[1][4].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0x00, converter.track_list[1][4].arg);

		// Check third track
		CPPUNIT_ASSERT_EQUAL((int)2, (int)converter.track_list[2].size());
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::FMREG, (uint16_t)converter.track_list[2][0].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0x2287, converter.track_list[2][0].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::FINISH, (uint16_t)converter.track_list[2][1].type);
	}
	//! test basic functionality of the track writer.
	void test_track_writer_sequence_output()
	{
		mml_input->read_line("A l4o4v5cdef");
		mml_input->read_line("B l8o4V5 L ga");
		mml_input->read_line("C 'cmd 0xf6 0x2287' 'cmd 0xe0'");
		auto converter = MDSDRV_Converter(*song);

		// check that we have two tracks
		CPPUNIT_ASSERT_EQUAL((int)3, (int)converter.track_list.size());

		auto trk1 = converter.convert_track(converter.track_list[0]);
		auto trk2 = converter.convert_track(converter.track_list[1]);
		auto trk3 = converter.convert_track(converter.track_list[2]);

		// check first tracks. we cast to uint16 to make sure cppunit display asserts as integers
		CPPUNIT_ASSERT_EQUAL((int)8, (int)trk1.size());
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::VOL, (uint16_t)trk1.at(0));
		CPPUNIT_ASSERT_EQUAL((uint16_t)0x85, (uint16_t)trk1.at(1));
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa6, (uint16_t)trk1.at(2)); // cn4
		CPPUNIT_ASSERT_EQUAL((uint16_t)23, (uint16_t)trk1.at(3));
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa8, (uint16_t)trk1.at(4)); // dn4
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xaa, (uint16_t)trk1.at(5)); // en4
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xab, (uint16_t)trk1.at(6)); // fn4
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::FINISH, (uint16_t)trk1.at(7));

		// check second track.
		CPPUNIT_ASSERT_EQUAL((int)8, (int)trk2.size());
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::VOL, (uint16_t)trk2.at(0));
		CPPUNIT_ASSERT_EQUAL((uint16_t)0x05, (uint16_t)trk2.at(1));
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xad, (uint16_t)trk2.at(2)); // gn4 (loop pos)
		CPPUNIT_ASSERT_EQUAL((uint16_t)11, (uint16_t)trk2.at(3));
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xaf, (uint16_t)trk2.at(4)); // an4
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::JUMP, (uint16_t)trk2.at(5));
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xff, (uint16_t)trk2.at(6));
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xfa, (uint16_t)trk2.at(7)); // 2-8 (loop_pos-next_inst)

		// check third track.
		CPPUNIT_ASSERT_EQUAL((int)5, (int)trk3.size());
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::FMREG, (uint16_t)trk3.at(0));
		CPPUNIT_ASSERT_EQUAL((uint16_t)0x22, (uint16_t)trk3.at(1));
		CPPUNIT_ASSERT_EQUAL((uint16_t)0x87, (uint16_t)trk3.at(2));
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::SLR, (uint16_t)trk3.at(3));
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::FINISH, (uint16_t)trk3.at(4));
	}
	void test_subroutine_handling()
	{
		mml_input->read_line("A *20 l8o4cde");
		mml_input->read_line("*20 v5 *22");
		mml_input->read_line("*21 k5 o4l4g"); //unused subroutine
		mml_input->read_line("*22 k1");
		auto converter = MDSDRV_Converter(*song);

		// check that we have one track
		CPPUNIT_ASSERT_EQUAL((int)1, (int)converter.track_list.size());
		CPPUNIT_ASSERT_EQUAL((int)5, (int)converter.track_list[0].size());
		// a bit lazy but we just check the type except the first
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::PAT, (uint16_t)converter.track_list[0][0].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0, (uint16_t)converter.track_list[0][0].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa6, (uint16_t)converter.track_list[0][1].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa8, (uint16_t)converter.track_list[0][2].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xaa, (uint16_t)converter.track_list[0][3].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::FINISH, (uint16_t)converter.track_list[0][4].type);

		// now check that there are two subroutines even though we declared 3
		CPPUNIT_ASSERT_EQUAL((int)2, (int)converter.subroutine_list.size());

		// check that the subroutine map looks like what we expected
		// map index is (id<<2 | drummode flag)
		CPPUNIT_ASSERT_NO_THROW(converter.subroutine_map.at(80));
		CPPUNIT_ASSERT_EQUAL((int)0, (int)converter.subroutine_map.at(80));
		CPPUNIT_ASSERT_THROW(converter.subroutine_map.at(84), std::out_of_range);
		CPPUNIT_ASSERT_NO_THROW(converter.subroutine_map.at(88));
		CPPUNIT_ASSERT_EQUAL((int)1, (int)converter.subroutine_map.at(88));

		// check the contents of the subroutine
		CPPUNIT_ASSERT_EQUAL((int)2, (int)converter.subroutine_list.size());
		CPPUNIT_ASSERT_EQUAL((int)3, (int)converter.subroutine_list[0].size());
		CPPUNIT_ASSERT_EQUAL((int)2, (int)converter.subroutine_list[1].size());

		// check contents of subroutines
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::VOL, (uint16_t)converter.subroutine_list[0][0].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::PAT, (uint16_t)converter.subroutine_list[0][1].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)1, (uint16_t)converter.subroutine_list[0][1].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::FINISH, (uint16_t)converter.subroutine_list[0][2].type);

		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::TRS, (uint16_t)converter.subroutine_list[1][0].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::FINISH, (uint16_t)converter.subroutine_list[1][1].type);
	}
	void test_drum_mode_handling()
	{
		mml_input->read_line("A D20 l8ab D0 l8o4e");
		mml_input->read_line("*20 o4c");
		mml_input->read_line("*21 o5c");
		auto converter = MDSDRV_Converter(*song);

		// check that we have one track
		CPPUNIT_ASSERT_EQUAL((int)1, (int)converter.track_list.size());
		CPPUNIT_ASSERT_EQUAL((int)6, (int)converter.track_list[0].size());
		// a bit lazy but we just check the type except the first
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::FLG, (uint16_t)converter.track_list[0][0].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)8, (uint16_t)converter.track_list[0][0].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0x82, (uint16_t)converter.track_list[0][1].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0x83, (uint16_t)converter.track_list[0][2].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::FLG, (uint16_t)converter.track_list[0][3].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0, (uint16_t)converter.track_list[0][3].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xaa, (uint16_t)converter.track_list[0][4].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::FINISH, (uint16_t)converter.track_list[0][5].type);

		// now check that there are two subroutines
		CPPUNIT_ASSERT_EQUAL((int)2, (int)converter.subroutine_list.size());

		// check that the subroutine map looks like what we expected
		// map index is (id<<2 | drummode flag)
		CPPUNIT_ASSERT_NO_THROW(converter.subroutine_map.at(82));
		CPPUNIT_ASSERT_EQUAL((int)0, (int)converter.subroutine_map.at(82));
		CPPUNIT_ASSERT_NO_THROW(converter.subroutine_map.at(86));
		CPPUNIT_ASSERT_EQUAL((int)1, (int)converter.subroutine_map.at(86));

		// check the contents of the subroutine
		CPPUNIT_ASSERT_EQUAL((int)2, (int)converter.subroutine_list.size());
		CPPUNIT_ASSERT_EQUAL((int)1, (int)converter.subroutine_list[0].size());
		CPPUNIT_ASSERT_EQUAL((int)1, (int)converter.subroutine_list[1].size());

		// check contents of subroutines
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::DMFINISH, (uint16_t)converter.subroutine_list[0][0].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0x24, (uint16_t)converter.subroutine_list[0][0].arg);

		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::DMFINISH, (uint16_t)converter.subroutine_list[1][0].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0x30, (uint16_t)converter.subroutine_list[1][0].arg);
	}
	//! test that loops are handled correctly in the output. (don't expand the loop in the converted data)
	void test_loop_handling()
	{
		mml_input->read_line("A [l4o4cde/f]4");
		auto converter = MDSDRV_Converter(*song);

		// check that we have a track
		CPPUNIT_ASSERT_EQUAL((int)1, (int)converter.track_list.size());

		// check first tracks. we cast to uint16 to make sure cppunit display asserts as integers
		CPPUNIT_ASSERT_EQUAL((int)8, (int)converter.track_list[0].size());
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::LP, (uint16_t)converter.track_list[0][0].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa6, (uint16_t)converter.track_list[0][1].type); // cn4
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, converter.track_list[0][1].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa8, (uint16_t)converter.track_list[0][2].type); // dn4
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, converter.track_list[0][2].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xaa, (uint16_t)converter.track_list[0][3].type); // en4
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, converter.track_list[0][3].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::LPB, (uint16_t)converter.track_list[0][4].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xab, (uint16_t)converter.track_list[0][5].type); // fn4
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, converter.track_list[0][5].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::LPF, (uint16_t)converter.track_list[0][6].type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)4, converter.track_list[0][6].arg);
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::FINISH, (uint16_t)converter.track_list[0][7].type);
	}

	//! test that loops are handled correctly in the output. (don't expand the loop in the converted data)
	void test_loop_handling_sequence_output()
	{
		mml_input->read_line("A l4o4c[cde/f]4");
		auto converter = MDSDRV_Converter(*song);

		// check that we have a track
		CPPUNIT_ASSERT_EQUAL((int)1, (int)converter.track_list.size());

		auto trk = converter.convert_track(converter.track_list[0]);

		// check first tracks. we cast to uint16 to make sure cppunit display asserts as integers
		CPPUNIT_ASSERT_EQUAL((int)13, (int)trk.size());
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa6, (uint16_t)trk.at(0)); // cn4
		CPPUNIT_ASSERT_EQUAL((uint16_t)23, (uint16_t)trk.at(1));
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::LP, (uint16_t)trk.at(2));
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa6, (uint16_t)trk.at(3)); // cn4
		CPPUNIT_ASSERT_EQUAL((uint16_t)23, (uint16_t)trk.at(4));
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa8, (uint16_t)trk.at(5)); // dn4
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xaa, (uint16_t)trk.at(6)); // en4
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::LPB, (uint16_t)trk.at(7));
		CPPUNIT_ASSERT_EQUAL((uint16_t)3, (uint16_t)trk.at(8));
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xab, (uint16_t)trk.at(9)); // fn4
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::LPF, (uint16_t)trk.at(10));
		CPPUNIT_ASSERT_EQUAL((uint16_t)4, (uint16_t)trk.at(11)); // fn4
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::FINISH, (uint16_t)trk.at(12));
	}
	//! Test note/rest length optimization
	void test_sequence_optimization()
	{
		mml_input->read_line("A l4 o4c r8 c c r:228 c r8 c r8 c r r:5 c:228 r8");
		auto converter = MDSDRV_Converter(*song);

		// check that we have a track
		CPPUNIT_ASSERT_EQUAL((int)1, (int)converter.track_list.size());

		auto trk = converter.convert_track(converter.track_list[0]);

		CPPUNIT_ASSERT_EQUAL((int)22, (int)trk.size());
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa6, (uint16_t)trk.at(0)); // c4
		CPPUNIT_ASSERT_EQUAL((uint16_t)23, (uint16_t)trk.at(1)); //
		CPPUNIT_ASSERT_EQUAL((uint16_t)11, (uint16_t)trk.at(2)); //   r8
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa6, (uint16_t)trk.at(3)); // c
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa6, (uint16_t)trk.at(4)); // c
		CPPUNIT_ASSERT_EQUAL((uint16_t)23, (uint16_t)trk.at(5)); //
		CPPUNIT_ASSERT_EQUAL((uint16_t)127, (uint16_t)trk.at(6)); //  r:228
		CPPUNIT_ASSERT_EQUAL((uint16_t)99, (uint16_t)trk.at(7)); //
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa6, (uint16_t)trk.at(8)); // c
		CPPUNIT_ASSERT_EQUAL((uint16_t)23, (uint16_t)trk.at(9)); //
		CPPUNIT_ASSERT_EQUAL((uint16_t)11, (uint16_t)trk.at(10)); //  r8
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa6, (uint16_t)trk.at(11)); //c
		CPPUNIT_ASSERT_EQUAL((uint16_t)0x80, (uint16_t)trk.at(12)); //r8
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa6, (uint16_t)trk.at(13)); //c
		CPPUNIT_ASSERT_EQUAL((uint16_t)23, (uint16_t)trk.at(14)); //
		CPPUNIT_ASSERT_EQUAL((uint16_t)(23+5), (uint16_t)trk.at(15)); // r +  r:5
		CPPUNIT_ASSERT_EQUAL((uint16_t)0xa6, (uint16_t)trk.at(16)); //c:228
		CPPUNIT_ASSERT_EQUAL((uint16_t)127, (uint16_t)trk.at(17)); //
		CPPUNIT_ASSERT_EQUAL((uint16_t)0x81, (uint16_t)trk.at(18)); //
		CPPUNIT_ASSERT_EQUAL((uint16_t)99, (uint16_t)trk.at(19)); //
		CPPUNIT_ASSERT_EQUAL((uint16_t)11, (uint16_t)trk.at(20)); //   r8
		CPPUNIT_ASSERT_EQUAL((uint16_t)MDSDRV_Event::FINISH, (uint16_t)trk.at(21)); //
	}
	//! Test that sequence data looks sound.
	void test_data_output()
	{
/*
// read FM envelope 1 = 1 [30]{00, 04, 00, 01, 1f, 1f, 1f, 1f, 00, 0f, 06, 1b, 13, 00, 00, 00, 05, 45, 34, 1b, 00, 00, 00, 00, 17, 26, 13, 00, 04, 18}
// read PSG envelope 2 = 2 [17]{10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 1a, 1b, 1c, 1d, 1e, 1f, 00}
		std::vector<uint8_t> fm_ins = {
			0x00, 0x04, 0x00, 0x01,
			0x1f, 0x1f, 0x1f, 0x1f,
			0x00, 0x0f, 0x06, 0x1b,
			0x13, 0x00, 0x00, 0x00,
			0x05, 0x45, 0x34, 0x1b,
			0x00, 0x00, 0x00, 0x00,
			0x17, 0x26, 0x13, 0x00,
			0x04, 0x30};
		std::vector<uint8_t> psg_ins = {
			0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
			0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x00};
*/

		mml_input->read_line("@1 fm 4 0");
		mml_input->read_line("  31 0 19 5 0 23 0 0 0 0");
		mml_input->read_line("  31 6 0 4 3 19 0 0 0 0");
		mml_input->read_line("  31 15 0 5 4 38 0 4 0 0");
		mml_input->read_line("  31 27 0 11 1 0 0 1 0 0");
		mml_input->read_line("@2 psg 15>0");
		mml_input->read_line("*20 o4l4 cdefgab>c");
		mml_input->read_line("A @1v15 o4l4 p2 *20 p1 *20");
		mml_input->read_line("G @2v15 o4l1 [r]8 *20 r:129 c:129");

		auto converter = MDSDRV_Converter(*song);

		// check that we have two tracks (+1 sub)
		CPPUNIT_ASSERT_EQUAL((int)2, (int)converter.track_list.size());
		CPPUNIT_ASSERT_EQUAL((int)1, (int)converter.subroutine_list.size());
		CPPUNIT_ASSERT_EQUAL((int)2, (int)converter.used_data_map.size());

		// sequence data
		std::vector<uint8_t> expected_data = {
			0x00,0x0c, // 00  &sdtop
			0x00,0x02, // 02  tcount
			0x00,0x00,0x00,0x12-0x0c, // 04 t0 - sdtop
			0x06,0x00,0x00,0x1f-0x0c, // 08 t1 - sdtop
			0x00,0x30-0x0c,  // 0c sub 0
			0x00,0x00,  // 0e env 1 (placeholder, overwritten by linker)
			0x00,0x00,  // 10 env 2 (placeholder, overwritten by linker)
			MDSDRV_Event::INS,0x01,MDSDRV_Event::VOL,0x8f,MDSDRV_Event::PAN,0x80, // 12 trk1
			MDSDRV_Event::PAT,0x00,MDSDRV_Event::PAN,0x40,MDSDRV_Event::PAT,0x00, // 18
			MDSDRV_Event::FINISH, // 1e
			MDSDRV_Event::INS,0x02,MDSDRV_Event::VOL,0x8f,MDSDRV_Event::LP,95, // 1f trk1
			MDSDRV_Event::LPF,8,MDSDRV_Event::PAT,0x00,0x7f,0x00,0xa6,0x7f,0x81,0x00,MDSDRV_Event::FINISH,  // 25
			0xa6,23,0xa8,0xaa,0xab,0xad,0xaf,0xb1,0xb2,MDSDRV_Event::FINISH // 30
		};

		CPPUNIT_ASSERT_EQUAL(expected_data.size(), converter.sequence_data.size());
		for(uint32_t i=0; i<std::min(expected_data.size(), converter.sequence_data.size()); i++)
		{
			uint16_t a, b;
			a = expected_data[i];
			b = converter.sequence_data[i];
			CPPUNIT_ASSERT_MESSAGE(stringf("mismatch at %02X, expected %02X != %02X found", i, a, b), a == b);
		}

		CPPUNIT_ASSERT_EQUAL(0, converter.used_data_map.at(1)); // FM instrument @1 (envelope_id=0)
		CPPUNIT_ASSERT_EQUAL(1, converter.used_data_map.at(2)); // PSG instrument @2 (envelope_id=1)
	}
};

class MDSDRV_Platform_Test : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(MDSDRV_Platform_Test);
	CPPUNIT_TEST(test_export_list);
	CPPUNIT_TEST_SUITE_END();
private:
	MDSDRV_Platform *platform;
public:
	void setUp()
	{
		platform = new MDSDRV_Platform(0);
	}
	void tearDown()
	{
		delete platform;
	}
	void test_export_list()
	{
		auto export_list = platform->get_export_formats();
		CPPUNIT_ASSERT_EQUAL(std::string("vgm"), export_list[0].first);
		CPPUNIT_ASSERT_EQUAL(std::string("mds"), export_list[1].first);
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(MDSDRV_Converter_Test);
CPPUNIT_TEST_SUITE_REGISTRATION(MDSDRV_Platform_Test);

