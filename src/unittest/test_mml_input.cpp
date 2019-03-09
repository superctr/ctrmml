#include <cppunit/extensions/HelperMacros.h>
#include "../mml_input.h"
#include "../song.h"
#include "../track.h"

class MML_Input_Test : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(MML_Input_Test);
	CPPUNIT_TEST(test_basic_mml);
	CPPUNIT_TEST(test_mml_note_octave);
	CPPUNIT_TEST(test_mml_note_duration);
	CPPUNIT_TEST(test_mml_loop);
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
	void test_basic_mml()
	{
		mml_input->read_line("A cdef");
		CPPUNIT_ASSERT(song->get_track(0).get_event_count() == 4);
	}
	void test_mml_note_octave()
	{
		mml_input->read_line("A o3cd>e<f");
		CPPUNIT_ASSERT_EQUAL(Event::NOTE, song->get_track(0).get_event(0).type);
		CPPUNIT_ASSERT_EQUAL((int16_t)36, song->get_track(0).get_event(0).param);
		CPPUNIT_ASSERT_EQUAL(Event::NOTE, song->get_track(0).get_event(1).type);
		CPPUNIT_ASSERT_EQUAL((int16_t)38, song->get_track(0).get_event(1).param);
		CPPUNIT_ASSERT_EQUAL(Event::NOTE, song->get_track(0).get_event(2).type);
		CPPUNIT_ASSERT_EQUAL((int16_t)52, song->get_track(0).get_event(2).param);
		CPPUNIT_ASSERT_EQUAL(Event::NOTE, song->get_track(0).get_event(3).type);
		CPPUNIT_ASSERT_EQUAL((int16_t)41, song->get_track(0).get_event(3).param);
	}
	void test_mml_note_duration()
	{
		mml_input->read_line("A l16cd8e.f8.");
		CPPUNIT_ASSERT_EQUAL((uint16_t)6, song->get_track(0).get_event(0).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)12, song->get_track(0).get_event(1).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)9, song->get_track(0).get_event(2).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)18, song->get_track(0).get_event(3).on_time);
	}
	void test_mml_loop()
	{
		mml_input->read_line("A [c] [c]5");
		CPPUNIT_ASSERT_EQUAL(Event::LOOP_START, song->get_track(0).get_event(0).type);
		CPPUNIT_ASSERT_EQUAL(Event::LOOP_END, song->get_track(0).get_event(2).type);
		CPPUNIT_ASSERT_EQUAL((int16_t)2, song->get_track(0).get_event(2).param); // default parameter
		CPPUNIT_ASSERT_EQUAL(Event::LOOP_START, song->get_track(0).get_event(3).type);
		CPPUNIT_ASSERT_EQUAL(Event::LOOP_END, song->get_track(0).get_event(5).type);
		CPPUNIT_ASSERT_EQUAL((int16_t)5, song->get_track(0).get_event(5).param); // default parameter
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(MML_Input_Test);

