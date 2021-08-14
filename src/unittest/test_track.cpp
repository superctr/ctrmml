#include <stdexcept>
#include <cppunit/extensions/HelperMacros.h>
#include "../track.h"

class Track_Test : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(Track_Test);
	CPPUNIT_TEST(test_add_notes);
	CPPUNIT_TEST(test_illegal_quantize);
	CPPUNIT_TEST(test_add_note_quantize);
	CPPUNIT_TEST(test_add_note_early_release);
	CPPUNIT_TEST(test_add_note_default_duration);
	CPPUNIT_TEST(test_add_note_octave_change);
	CPPUNIT_TEST(test_tie_extend_duration);
	CPPUNIT_TEST(test_tie_extend_add_tie);
	CPPUNIT_TEST(test_tie_extend_add_rest);
	CPPUNIT_TEST(test_tie_only_add_tie);
	CPPUNIT_TEST(test_tie_only_add_tie2);
	CPPUNIT_TEST(test_slur);
	CPPUNIT_TEST(test_slur_impossible);
	CPPUNIT_TEST(test_reverse_rest_shorten_on_time);
	CPPUNIT_TEST(test_reverse_rest_shorten_off_time);
	CPPUNIT_TEST_EXCEPTION(test_reverse_rest_overflow, std::length_error);
	CPPUNIT_TEST_EXCEPTION(test_reverse_rest_impossible, std::domain_error);
	CPPUNIT_TEST(test_get_event_count);
	CPPUNIT_TEST(test_key_signature);
	CPPUNIT_TEST(test_shuffle);
	CPPUNIT_TEST_SUITE_END();
private:
	Track *track;
public:
	void setUp()
	{
		track = new Track();
	}
	void tearDown()
	{
		delete track;
	}
	// Add notes and verify that they are present in the event list.
	void test_add_notes()
	{
		int i;
		std::vector<Event>::iterator it;
		for(i=0; i<10; i++)
			track->add_note(i, 24);
		for(i=0, it = track->get_events().begin();
				it != track->get_events().end(); i++, it++)
		{
			CPPUNIT_ASSERT_EQUAL((int16_t) (i + 12*5),it->param);
			CPPUNIT_ASSERT_EQUAL((uint16_t) 24,it->on_time);
		}
		// whatever
	}
	// test illegal quantize values
	void test_illegal_quantize()
	{
		CPPUNIT_ASSERT(track->set_quantize(12,8) == -1);
		CPPUNIT_ASSERT(track->set_quantize(20) == -1);
		CPPUNIT_ASSERT(track->set_quantize(0,0) == -1);
		CPPUNIT_ASSERT(track->set_quantize(8) == 0);
		CPPUNIT_ASSERT(track->set_quantize(10,20) == 0);
		CPPUNIT_ASSERT(track->set_quantize(0) == 0); // special case
	}
	// Set quantize and add notes and verify that durations are correct.
	void test_add_note_quantize()
	{
		track->set_quantize(6);
		track->add_note(1, 24);
		track->set_quantize(2);
		track->add_note(1, 24);
		track->set_quantize(8);
		track->add_note(1, 24);
		track->set_quantize(4);
		track->add_note(1, 24);
		track->set_early_release(0); // should cancel
		track->add_note(1, 24);
		CPPUNIT_ASSERT_EQUAL((uint16_t)18, track->get_event(0).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)6, track->get_event(0).off_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)6, track->get_event(1).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)18, track->get_event(1).off_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, track->get_event(2).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0, track->get_event(2).off_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)12, track->get_event(3).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)12, track->get_event(3).off_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, track->get_event(4).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0, track->get_event(4).off_time);
	}
	// Set early release and add notes and verify that durations are correct.
	void test_add_note_early_release()
	{
		track->set_early_release(6);
		track->add_note(1, 24);
		track->set_early_release(2);
		track->add_note(1, 24);
		track->set_early_release(8);
		track->add_note(1, 24);
		track->set_early_release(4);
		track->add_note(1, 24);
		track->set_early_release(35);
		track->add_note(1, 24);
		track->set_quantize(8); // should cancel
		track->add_note(1, 24);
		CPPUNIT_ASSERT_EQUAL((uint16_t)18, track->get_event(0).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)6, track->get_event(0).off_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)22, track->get_event(1).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)2, track->get_event(1).off_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)16, track->get_event(2).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)8, track->get_event(2).off_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)20, track->get_event(3).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)4, track->get_event(3).off_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)1, track->get_event(4).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)23, track->get_event(4).off_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, track->get_event(5).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0, track->get_event(5).off_time);
	}
	// Set default duration, add notes and verify that durations are correct
	void test_add_note_default_duration()
	{
		track->set_duration(16);
		track->add_note(1);
		track->set_duration(50);
		track->add_note(1);
		CPPUNIT_ASSERT_EQUAL((uint16_t)16, track->get_event(0).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)50, track->get_event(1).on_time);
	}
	// Add notes with various octave changes and verify that notes are correct.
	void test_add_note_octave_change()
	{
		track->set_octave(0);
		track->add_note(0);
		CPPUNIT_ASSERT_EQUAL((int16_t)0, track->get_event(0).param);
		track->set_octave(1);
		track->add_note(0);
		CPPUNIT_ASSERT_EQUAL((int16_t)12, track->get_event(1).param);
		track->change_octave(1);
		track->add_note(0);
		CPPUNIT_ASSERT_EQUAL((int16_t)24, track->get_event(2).param);
		track->change_octave(-2);
		track->add_note(0);
		CPPUNIT_ASSERT_EQUAL((int16_t)0, track->get_event(3).param);
	}
	// Add a tie and verify that the duration of the previous note is changed.
	void test_tie_extend_duration()
	{
		track->set_quantize(8);
		track->add_note(0,24);
		track->add_tie(24);
		CPPUNIT_ASSERT_EQUAL((uint16_t)48, track->get_event(0).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0, track->get_event(0).off_time);
		track->set_quantize(4);
		track->add_note(0,24);
		track->add_tie(24);
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, track->get_event(1).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, track->get_event(1).off_time);
	}
	// Add note, then an unrelated event, then a tie and verify that a new tie event is added.
	void test_tie_extend_add_tie()
	{
		track->set_quantize(8);
		track->add_note(0,24);
		track->add_event(Event::VOL, 10); // type doesn't matter
		track->add_tie(24);
		CPPUNIT_ASSERT_EQUAL((Event::Type)Event::TIE, track->get_event(2).type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, track->get_event(0).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, track->get_event(2).on_time);
	}
	// Add note, then an unrelated event, then a tie and verify that a new rest is added.
	void test_tie_extend_add_rest()
	{
		track->set_quantize(2);
		track->add_note(0,24);
		track->add_event(Event::VOL, 10); // type doesn't matter
		track->add_tie(24);
		CPPUNIT_ASSERT_EQUAL((Event::Type)Event::REST, track->get_event(2).type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)12, track->get_event(0).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)12, track->get_event(0).off_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, track->get_event(2).off_time);
	}
	// Add an unrelated event, then a tie and verify that a new tie is added.
	void test_tie_only_add_tie2()
	{
		track->set_quantize(8);
		track->add_event(Event::VOL, 10); // type doesn't matter
		track->add_tie(24);
		CPPUNIT_ASSERT_EQUAL((Event::Type)Event::TIE, track->get_event(1).type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, track->get_event(1).on_time);
	}
	// Add just a tie and verify that a new tie is added.
	void test_tie_only_add_tie()
	{
		track->set_quantize(8);
		track->add_tie(24);
		CPPUNIT_ASSERT_EQUAL((Event::Type)Event::TIE, track->get_event(0).type);
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, track->get_event(0).on_time);
	}
	// Add slur and verify that the articulation of the previous note is legato.
	void test_slur()
	{
		track->set_quantize(4);
		track->add_note(0,24);
		track->add_slur();
		track->add_note(1,24);
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, track->get_event(0).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0, track->get_event(0).off_time);
	}
	// Add slur and verify that the function returns error if it is impossible to set the previous note
	// to legato.
	void test_slur_impossible()
	{
		track->set_quantize(4);
		track->add_note(0,24);
		track->add_event(Event::REST);
		CPPUNIT_ASSERT(track->add_slur() == -1);
	}
	// Add a reverse rest and verify that the previous note was shortened.
	void test_reverse_rest_shorten_on_time()
	{
		track->set_quantize(8);
		track->add_note(0,24);
		track->reverse_rest(10);
		CPPUNIT_ASSERT_EQUAL((uint16_t)14, track->get_event(0).on_time);
		track->set_quantize(4);
		track->add_note(0,24);
		track->reverse_rest(20);
		CPPUNIT_ASSERT_EQUAL((uint16_t)4, track->get_event(1).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)0, track->get_event(1).off_time);
	}
	// Add a reverse rest and verify that the previous note was shortened.
	void test_reverse_rest_shorten_off_time()
	{
		track->set_quantize(2);
		track->add_note(0,24);
		track->reverse_rest(10);
		CPPUNIT_ASSERT_EQUAL((uint16_t)8, track->get_event(0).off_time);
	}
	// Attempt to add a reverse rest where it is impossible
	void test_reverse_rest_impossible()
	{
		track->add_event(Event::VOL,12); // just some dummy events
		track->add_event(Event::INS,34);
		track->add_event(Event::PAN,56);
		track->reverse_rest(48); // throw std::domain_error
	}
	void test_reverse_rest_overflow()
	{
		track->add_note(0,24);
		track->reverse_rest(48); // throw std::length_error
	}
	void test_get_event_count()
	{
		track->add_event(Event::VOL,12); // just some dummy events
		track->add_event(Event::INS,34);
		track->add_event(Event::PAN,56);
		unsigned long expected = 3;
		CPPUNIT_ASSERT_EQUAL(expected, track->get_event_count());
	}
	void test_key_signature()
	{
		// set key to A major
		track->set_key_signature("A");
		CPPUNIT_ASSERT_EQUAL((int8_t)1, track->get_key_signature('c'));
		CPPUNIT_ASSERT_EQUAL((int8_t)0, track->get_key_signature('d'));
		CPPUNIT_ASSERT_EQUAL((int8_t)0, track->get_key_signature('e'));
		CPPUNIT_ASSERT_EQUAL((int8_t)1, track->get_key_signature('f'));
		CPPUNIT_ASSERT_EQUAL((int8_t)1, track->get_key_signature('g'));
		CPPUNIT_ASSERT_EQUAL((int8_t)0, track->get_key_signature('a'));
		CPPUNIT_ASSERT_EQUAL((int8_t)0, track->get_key_signature('b'));
		//remove the c and f sharp
		track->set_key_signature("=cf");
		CPPUNIT_ASSERT_EQUAL((int8_t)0, track->get_key_signature('c'));
		CPPUNIT_ASSERT_EQUAL((int8_t)0, track->get_key_signature('d'));
		CPPUNIT_ASSERT_EQUAL((int8_t)0, track->get_key_signature('e'));
		CPPUNIT_ASSERT_EQUAL((int8_t)0, track->get_key_signature('f'));
		CPPUNIT_ASSERT_EQUAL((int8_t)1, track->get_key_signature('g'));
		CPPUNIT_ASSERT_EQUAL((int8_t)0, track->get_key_signature('a'));
		CPPUNIT_ASSERT_EQUAL((int8_t)0, track->get_key_signature('b'));
		//add them back
		track->set_key_signature("+cf");
		CPPUNIT_ASSERT_EQUAL((int8_t)1, track->get_key_signature('c'));
		CPPUNIT_ASSERT_EQUAL((int8_t)0, track->get_key_signature('d'));
		CPPUNIT_ASSERT_EQUAL((int8_t)0, track->get_key_signature('e'));
		CPPUNIT_ASSERT_EQUAL((int8_t)1, track->get_key_signature('f'));
		CPPUNIT_ASSERT_EQUAL((int8_t)1, track->get_key_signature('g'));
		CPPUNIT_ASSERT_EQUAL((int8_t)0, track->get_key_signature('a'));
		CPPUNIT_ASSERT_EQUAL((int8_t)0, track->get_key_signature('b'));
		// set key to Bb minor
		track->set_key_signature("b-");
		CPPUNIT_ASSERT_EQUAL((int8_t)0, track->get_key_signature('c'));
		CPPUNIT_ASSERT_EQUAL((int8_t)-1, track->get_key_signature('d'));
		CPPUNIT_ASSERT_EQUAL((int8_t)-1, track->get_key_signature('e'));
		CPPUNIT_ASSERT_EQUAL((int8_t)0, track->get_key_signature('f'));
		CPPUNIT_ASSERT_EQUAL((int8_t)-1, track->get_key_signature('g'));
		CPPUNIT_ASSERT_EQUAL((int8_t)-1, track->get_key_signature('a'));
		CPPUNIT_ASSERT_EQUAL((int8_t)-1, track->get_key_signature('b'));
		// add a flat to C and F
		track->set_key_signature("-cf");
		CPPUNIT_ASSERT_EQUAL((int8_t)-1, track->get_key_signature('c'));
		CPPUNIT_ASSERT_EQUAL((int8_t)-1, track->get_key_signature('d'));
		CPPUNIT_ASSERT_EQUAL((int8_t)-1, track->get_key_signature('e'));
		CPPUNIT_ASSERT_EQUAL((int8_t)-1, track->get_key_signature('f'));
		CPPUNIT_ASSERT_EQUAL((int8_t)-1, track->get_key_signature('g'));
		CPPUNIT_ASSERT_EQUAL((int8_t)-1, track->get_key_signature('a'));
		CPPUNIT_ASSERT_EQUAL((int8_t)-1, track->get_key_signature('b'));
		// try resetting and setting at the same time
		track->set_key_signature("=d+f");
		CPPUNIT_ASSERT_EQUAL((int8_t)-1, track->get_key_signature('c'));
		CPPUNIT_ASSERT_EQUAL((int8_t)0, track->get_key_signature('d'));
		CPPUNIT_ASSERT_EQUAL((int8_t)-1, track->get_key_signature('e'));
		CPPUNIT_ASSERT_EQUAL((int8_t)1, track->get_key_signature('f'));
		CPPUNIT_ASSERT_EQUAL((int8_t)-1, track->get_key_signature('g'));
		CPPUNIT_ASSERT_EQUAL((int8_t)-1, track->get_key_signature('a'));
		CPPUNIT_ASSERT_EQUAL((int8_t)-1, track->get_key_signature('b'));
	}
	// Test a shuffle rhythm
	void test_shuffle()
	{
		track->set_shuffle(4);
		track->add_note(0,24);
		track->add_note(0,24);
		CPPUNIT_ASSERT_EQUAL((uint16_t)28, track->get_event(0).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)20, track->get_event(1).on_time);
		track->add_note(0,24);
		track->add_rest(24);
		CPPUNIT_ASSERT_EQUAL((uint16_t)28, track->get_event(2).on_time);
		CPPUNIT_ASSERT_EQUAL((uint16_t)20, track->get_event(3).off_time);
		track->add_note(0,24);
		track->add_tie(24);
		CPPUNIT_ASSERT_EQUAL((uint16_t)48, track->get_event(4).on_time);
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(Track_Test);

