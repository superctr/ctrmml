#include <stdexcept>
#include <cppunit/extensions/HelperMacros.h>
#include "../song.h"
#include "../track.h"

class Song_Test : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(Song_Test);
	CPPUNIT_TEST(test_set_tag);
	CPPUNIT_TEST(test_add_tag);
	CPPUNIT_TEST(test_add_tag_trim_trailing_spaces);
	CPPUNIT_TEST(test_add_tag_list);
	CPPUNIT_TEST(test_add_tag_list_enclosed);
	CPPUNIT_TEST(test_add_tag_list_enclosed_escape);
	CPPUNIT_TEST(test_add_tag_list_comma_separation);
	CPPUNIT_TEST(test_add_tag_list_successive_calls);
	CPPUNIT_TEST(test_add_tag_list_semicolon);
	CPPUNIT_TEST(test_add_tag_list_space_semicolon);
	CPPUNIT_TEST(test_add_tag_list_enclosed_semicolon);
	CPPUNIT_TEST(test_get_track);
	CPPUNIT_TEST_EXCEPTION(test_get_invalid_track, std::out_of_range);
	CPPUNIT_TEST(test_get_track_map);
	CPPUNIT_TEST(test_set_platform_command);
	CPPUNIT_TEST(test_get_platform_command);
	CPPUNIT_TEST(test_get_tag_order_list);
	CPPUNIT_TEST_SUITE_END();
private:
	Song *song;
	Tag *tag;
public:
	void setUp()
	{
		song = new Song();
		tag = nullptr;
	}
	void tearDown()
	{
		delete song;
	}
	// create tag and verify that it exists
	void test_set_tag()
	{
		song->set_tag("Tag1", "first value in tag1");
		tag = &song->get_tag("Tag1");
		CPPUNIT_ASSERT_EQUAL(std::string("first value in tag1"), tag->at(0));
		song->set_tag("Tag1", "overwritten value in tag1");
		CPPUNIT_ASSERT_EQUAL(std::string("overwritten value in tag1"), tag->at(0));
	}
	// create tag and verify that it exists
	void test_add_tag()
	{
		song->add_tag("Tag1", "first value in tag1");
		song->add_tag("Tag1", "second value in tag1");
		song->add_tag("Tag2", "Value2");

		// Test two different methods of retrieving the value.
		tag = &song->get_tag("Tag1");
		CPPUNIT_ASSERT_EQUAL(std::string("first value in tag1"), tag->at(0));
		CPPUNIT_ASSERT_EQUAL(std::string("second value in tag1"), tag->at(1));
		CPPUNIT_ASSERT_EQUAL(std::string("Value2"), song->get_tag_front("Tag2"));
	}
	// create tag with spaces and verify spaces are removed
	void test_add_tag_trim_trailing_spaces()
	{
		song->add_tag("Tag1", "trailing spaces   ");
		CPPUNIT_ASSERT_EQUAL(std::string("trailing spaces"), song->get_tag_front("Tag1"));
	}
	// Space separated values
	void test_add_tag_list()
	{
		song->add_tag_list("Tag1", "my tag      list");
		tag = &song->get_tag("Tag1");
		CPPUNIT_ASSERT_EQUAL(std::string("my"), tag->at(0));
		CPPUNIT_ASSERT_EQUAL(std::string("tag"), tag->at(1));
		CPPUNIT_ASSERT_EQUAL(std::string("list"), tag->at(2));
		CPPUNIT_ASSERT(tag->size() == 3);
	}
	// Test double-quote-enclosed tags
	void test_add_tag_list_enclosed()
	{
		song->add_tag_list("Tag1", "\" One enclosed tag with leading and trailing spaces \" \"Another enclosed tag\", \"and a final one\"");
		tag = &song->get_tag("Tag1");
		CPPUNIT_ASSERT_EQUAL(std::string(" One enclosed tag with leading and trailing spaces "), tag->at(0));
		CPPUNIT_ASSERT_EQUAL(std::string("Another enclosed tag"), tag->at(1));
		CPPUNIT_ASSERT_EQUAL(std::string("and a final one"), tag->at(2));
		CPPUNIT_ASSERT(tag->size() == 3);
	}
	// Escaped \" sequence in double-quote-enclosed tags
	void test_add_tag_list_enclosed_escape()
	{
		song->add_tag_list("Tag1", "\"Enclosed tag with an \\\"escaped\\\" quote mark\"");
		CPPUNIT_ASSERT_EQUAL(std::string("Enclosed tag with an \"escaped\" quote mark"), song->get_tag_front("Tag1"));
		CPPUNIT_ASSERT(song->get_tag("Tag1").size() == 1);
	}
	// Separating values with commas
	void test_add_tag_list_comma_separation()
	{
		song->add_tag_list("Tag1", "first, second,, fourth, , sixth");
		tag = &song->get_tag("Tag1");
		CPPUNIT_ASSERT_EQUAL(std::string("first"), tag->at(0));
		CPPUNIT_ASSERT_EQUAL(std::string("second"), tag->at(1));
		CPPUNIT_ASSERT_EQUAL(std::string(""), tag->at(2));
		CPPUNIT_ASSERT_EQUAL(std::string("fourth"), tag->at(3));
		CPPUNIT_ASSERT_EQUAL(std::string(""), tag->at(4));
		CPPUNIT_ASSERT_EQUAL(std::string("sixth"), tag->at(5));
		CPPUNIT_ASSERT(tag->size() == 6);
	}
	// Successive calls should add more values to the tag
	void test_add_tag_list_successive_calls()
	{
		song->add_tag_list("Tag1", "first, second");
		song->add_tag_list("Tag1", "third, fourth");
		tag = &song->get_tag("Tag1");
		CPPUNIT_ASSERT_EQUAL(std::string("first"), tag->at(0));
		CPPUNIT_ASSERT_EQUAL(std::string("second"), tag->at(1));
		CPPUNIT_ASSERT_EQUAL(std::string("third"), tag->at(2));
		CPPUNIT_ASSERT_EQUAL(std::string("fourth"), tag->at(3));
		CPPUNIT_ASSERT(tag->size() == 4);
	}
	// No more tags should be added after a semicolon is read
	void test_add_tag_list_semicolon()
	{
		song->add_tag_list("Tag1", "first, second; This is a comment");
		tag = &song->get_tag("Tag1");
		CPPUNIT_ASSERT_EQUAL(std::string("first"), tag->at(0));
		CPPUNIT_ASSERT_EQUAL(std::string("second"), tag->at(1));
		CPPUNIT_ASSERT(tag->size() == 2);
	}
	// Test that no extra tags are added even if spaces or commas are
	// added after the last tag.
	void test_add_tag_list_space_semicolon()
	{
		song->add_tag_list("Tag1", "first, second ; This is a comment");
		song->add_tag_list("Tag2", "first, second, ; This is a comment");
		tag = &song->get_tag("Tag1");
		CPPUNIT_ASSERT_EQUAL(std::string("first"), tag->at(0));
		CPPUNIT_ASSERT_EQUAL(std::string("second"), tag->at(1));
		CPPUNIT_ASSERT(tag->size() == 2);
		tag = &song->get_tag("Tag2");
		CPPUNIT_ASSERT_EQUAL(std::string("first"), tag->at(0));
		CPPUNIT_ASSERT_EQUAL(std::string("second"), tag->at(1));
		CPPUNIT_ASSERT(tag->size() == 2);
	}
	// Semicolons are allowed inside enclosed tags.
	void test_add_tag_list_enclosed_semicolon()
	{
		song->add_tag_list("Tag1", "\";_;\"");
		tag = &song->get_tag("Tag1");
		CPPUNIT_ASSERT_EQUAL(std::string(";_;"), tag->at(0));
		CPPUNIT_ASSERT(tag->size() == 1);
	}
	// Track addressing
	void test_get_track_map()
	{
		Track &t1 = song->get_track_map()[0];
		Track &t2 = song->get_track_map()[12];
		t1.add_note(12);
		t2.add_note(45);
		t2.add_note(78);
		CPPUNIT_ASSERT_EQUAL((unsigned long) 1, t1.get_event_count());
		CPPUNIT_ASSERT_EQUAL((unsigned long) 2, t2.get_event_count());
	}
	void test_get_track()
	{
		Track &t1 = song->get_track_map()[0];
		Track &t2 = song->get_track(0);
		t1.add_note(12);
		t1.add_note(45);
		t1.add_note(78);
		CPPUNIT_ASSERT_EQUAL((unsigned long) 3, t1.get_event_count());
		CPPUNIT_ASSERT_EQUAL((unsigned long) 3, t2.get_event_count());
	}
	void test_get_invalid_track()
	{
		// Doesn't exist, we should get std::out_of_range
		Track &t = song->get_track(0);
		t.add_note(12); // should not get this far
	}
	// Platform commands
	void test_set_platform_command()
	{
		CPPUNIT_ASSERT_EQUAL((int16_t)-32768, song->register_platform_command(-1, "first"));
		CPPUNIT_ASSERT_EQUAL((int16_t)123, song->register_platform_command(123, "second"));
		CPPUNIT_ASSERT_EQUAL((int16_t)-32767, song->register_platform_command(-1, "third"));

	}
	void test_get_platform_command()
	{
		int16_t param1 = song->register_platform_command(-1, "first");
		int16_t param2 = song->register_platform_command(123, "second");
		int16_t param3 = song->register_platform_command(-1, "third");
		CPPUNIT_ASSERT_EQUAL(std::string("first"), song->get_platform_command(param1).at(0));
		CPPUNIT_ASSERT_EQUAL(std::string("second"), song->get_platform_command(param2).at(0));
		CPPUNIT_ASSERT_EQUAL(std::string("third"), song->get_platform_command(param3).at(0));
	}
	void test_get_tag_order_list()
	{
		song->add_tag("Tag1", "first");
		song->add_tag("Tag3", "second");
		song->add_tag("Tag2", "third");

		tag = &song->get_tag_order_list();
		CPPUNIT_ASSERT_EQUAL(std::string("Tag1"), tag->at(0));
		CPPUNIT_ASSERT_EQUAL(std::string("Tag3"), tag->at(1));
		CPPUNIT_ASSERT_EQUAL(std::string("Tag2"), tag->at(2));
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(Song_Test);

