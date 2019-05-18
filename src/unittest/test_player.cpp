#include <cppunit/extensions/HelperMacros.h>
#include "../mml_input.h"
#include "../song.h"
#include "../track.h"
#include "../player.h"

class Player_Test : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(Player_Test);
	CPPUNIT_TEST(test_get_event);
	CPPUNIT_TEST(test_loop);
	CPPUNIT_TEST(test_loop_position);
	CPPUNIT_TEST(test_jump);
	CPPUNIT_TEST(test_play_tick);
	CPPUNIT_TEST(test_quantize_play_tick);
	CPPUNIT_TEST(test_skip_ticks);
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
	void test_get_event()
	{
		mml_input->read_line("A o3cv5");
		auto player = Player(*song, song->get_track(0));
		player.step_event();
		CPPUNIT_ASSERT_EQUAL(Event::NOTE, player.get_event().type);
		player.step_event();
		CPPUNIT_ASSERT_EQUAL(Event::VOL, player.get_event().type);
		player.step_event();
		CPPUNIT_ASSERT_EQUAL(false, player.is_enabled());
	}
	void test_loop()
	{
		mml_input->read_line("A o4[cd/e]3");
		auto player = Player(*song, song->get_track(0));
		player.step_event();
		CPPUNIT_ASSERT_EQUAL(Event::LOOP_START, player.get_event().type);
		for(int i=0; i<3; i++)
		{
			player.step_event();
			CPPUNIT_ASSERT_EQUAL(Event::NOTE, player.get_event().type);
			CPPUNIT_ASSERT_EQUAL((int16_t)36, player.get_event().param);
			player.step_event();
			CPPUNIT_ASSERT_EQUAL(Event::NOTE, player.get_event().type);
			CPPUNIT_ASSERT_EQUAL((int16_t)38, player.get_event().param);
			player.step_event();
			CPPUNIT_ASSERT_EQUAL(Event::LOOP_BREAK, player.get_event().type);
			if(i != 2) // does not run on last loop
			{
				player.step_event();
				CPPUNIT_ASSERT_EQUAL(Event::NOTE, player.get_event().type);
				player.step_event();
				CPPUNIT_ASSERT_EQUAL(Event::LOOP_END, player.get_event().type);
			}
		}
		player.step_event();
		CPPUNIT_ASSERT_EQUAL(false, player.is_enabled());
	}
	void test_loop_position()
	{
		mml_input->read_line("A L o4cd");
		auto player = Player(*song, song->get_track(0));
		CPPUNIT_ASSERT_EQUAL(-1, player.get_loop_count());
		player.step_event();
		CPPUNIT_ASSERT_EQUAL(Event::SEGNO, player.get_event().type);
		// three loops
		for(int i=0; i<3; i++)
		{
			player.step_event();
			CPPUNIT_ASSERT_EQUAL((int)i, player.get_loop_count());
			CPPUNIT_ASSERT_EQUAL(Event::NOTE, player.get_event().type);
			CPPUNIT_ASSERT_EQUAL((int16_t)36, player.get_event().param);
			player.step_event();
			CPPUNIT_ASSERT_EQUAL(Event::NOTE, player.get_event().type);
			CPPUNIT_ASSERT_EQUAL((int16_t)38, player.get_event().param);
			player.step_event();
			CPPUNIT_ASSERT_EQUAL(Event::END, player.get_event().type);
		}
	}
	void test_jump()
	{
		mml_input->read_line("A *10 o4e");
		mml_input->read_line("*10 o4cd");
		auto player = Player(*song, song->get_track(0));
		player.step_event();
		CPPUNIT_ASSERT_EQUAL(Event::JUMP, player.get_event().type);
		player.step_event();
		CPPUNIT_ASSERT_EQUAL(Event::NOTE, player.get_event().type);
		CPPUNIT_ASSERT_EQUAL((int16_t)36, player.get_event().param);
		player.step_event();
		CPPUNIT_ASSERT_EQUAL(Event::NOTE, player.get_event().type);
		CPPUNIT_ASSERT_EQUAL((int16_t)38, player.get_event().param);
		player.step_event();
		CPPUNIT_ASSERT_EQUAL(Event::END, player.get_event().type);
		player.step_event();
		CPPUNIT_ASSERT_EQUAL(Event::NOTE, player.get_event().type);
		CPPUNIT_ASSERT_EQUAL((int16_t)40, player.get_event().param);
	}
	// result should be the same
	void test_play_tick()
	{
		mml_input->read_line("A l16cdefg"); // length should be 30
		auto player = Player(*song, song->get_track(0));
		for(int i=0; i<30; i++)
			player.play_tick();
		CPPUNIT_ASSERT_EQUAL((unsigned int)30, player.get_play_time());
		CPPUNIT_ASSERT_EQUAL(5, player.note_count);
		CPPUNIT_ASSERT_EQUAL(0, player.rest_count);
		for(int i=0; i<30; i++)
			player.play_tick();
		CPPUNIT_ASSERT_EQUAL((unsigned int)60, player.get_play_time());
		CPPUNIT_ASSERT_EQUAL(5, player.note_count);
		CPPUNIT_ASSERT_EQUAL(1, player.rest_count);
	}
	// result should be the same
	void test_quantize_play_tick()
	{
		mml_input->read_line("A q4l16cdefg"); // length should be 30
		auto player = Player(*song, song->get_track(0));
		for(int i=0; i<30; i++)
			player.play_tick();
		CPPUNIT_ASSERT_EQUAL((unsigned int)30, player.get_play_time());
		CPPUNIT_ASSERT_EQUAL(5, player.note_count);
		CPPUNIT_ASSERT_EQUAL(5, player.rest_count);
		for(int i=0; i<30; i++)
			player.play_tick();
		CPPUNIT_ASSERT_EQUAL((unsigned int)60, player.get_play_time());
		CPPUNIT_ASSERT_EQUAL(5, player.note_count);
		CPPUNIT_ASSERT_EQUAL(6, player.rest_count);
	}
	// result should be the same
	void test_skip_ticks()
	{
		mml_input->read_line("A l16cdefg"); // length should be 30
		auto player = Player(*song, song->get_track(0));
		player.skip_ticks(23);
		for(int i=0; i<13; i++)
			player.play_tick();
		CPPUNIT_ASSERT_EQUAL((unsigned int)36, player.get_play_time());
		CPPUNIT_ASSERT_EQUAL(1, player.note_count);
		CPPUNIT_ASSERT_EQUAL(1, player.rest_count);
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(Player_Test);

