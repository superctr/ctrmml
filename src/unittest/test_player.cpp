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
		CPPUNIT_ASSERT_EQUAL(Event::END, player.get_event().type);
	}
	void test_loop()
	{
		mml_input->read_line("A o3[cd/e]3");
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
		CPPUNIT_ASSERT_EQUAL(Event::END, player.get_event().type);
	}
	void test_loop_position()
	{
		mml_input->read_line("A L o3cd");
		auto player = Player(*song, song->get_track(0));
		player.step_event();
		CPPUNIT_ASSERT_EQUAL(Event::SEGNO, player.get_event().type);
		// three loops
		for(int i=0; i<3; i++)
		{
			player.step_event();
			CPPUNIT_ASSERT_EQUAL((unsigned int)i, player.get_loop_count());
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
		mml_input->read_line("A *10 o3e");
		mml_input->read_line("*10 o3cd");
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
};

CPPUNIT_TEST_SUITE_REGISTRATION(Player_Test);

