#include <cppunit/extensions/HelperMacros.h>
#include "../track.h"

class Song_Test : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(Song_Test);
	CPPUNIT_TEST(test_add_tag);
	CPPUNIT_TEST(test_add_tag_trim_trailing_spaces);
	CPPUNIT_TEST(test_add_tag_list);
	CPPUNIT_TEST(test_add_tag_list_enclosed);
	CPPUNIT_TEST(test_add_tag_list_commas);
	CPPUNIT_TEST_SUITE_END();
private:
	Song *song;
public:
	void setUp()
	{
		song = new Song();
	}
	void tearDown()
	{
		delete song;
	}
	// create tag and verify that it exists
	void test_add_tag()
	{
		song->add_tag("Tag1", "first value in tag1");
		song->add_tag("Tag1", "second value in tag1");
		song->add_tag("Tag2", "Value2");

		// Test two different methods of retrieving the value.
		Tag *tag;
		tag = song->get_tag("Tag1");
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
	void test_add_tag_list()
	{
		song->add_tag_list("Tag1", "my tag      list");
		Tag *tag;
		tag = song->get_tag("Tag1");
		CPPUNIT_ASSERT_EQUAL(std::string("my"), tag->at(0));
		CPPUNIT_ASSERT_EQUAL(std::string("tag"), tag->at(1));
		CPPUNIT_ASSERT_EQUAL(std::string("list"), tag->at(2));
		CPPUNIT_ASSERT(tag->size() == 3);
	}
	void test_add_tag_list_enclosed()
	{
		song->add_tag_list("Tag1", "\" One enclosed tag with leading and trailing spaces \" \"Another enclosed tag\", \"and a final one\"");
		Tag *tag;
		tag = song->get_tag("Tag1");
		CPPUNIT_ASSERT_EQUAL(std::string(" One enclosed tag with leading and trailing spaces "), tag->at(0));
		CPPUNIT_ASSERT_EQUAL(std::string("Another enclosed tag"), tag->at(1));
		CPPUNIT_ASSERT_EQUAL(std::string("and a final one"), tag->at(2));
		CPPUNIT_ASSERT(tag->size() == 3);
	}
	void test_add_tag_list_commas()
	{
		song->add_tag_list("Tag1", "first, second,, fourth, , sixth");
		Tag *tag;
		tag = song->get_tag("Tag1");
		CPPUNIT_ASSERT_EQUAL(std::string("first"), tag->at(0));
		CPPUNIT_ASSERT_EQUAL(std::string("second"), tag->at(1));
		CPPUNIT_ASSERT_EQUAL(std::string(""), tag->at(2));
		CPPUNIT_ASSERT_EQUAL(std::string("fourth"), tag->at(3));
		CPPUNIT_ASSERT_EQUAL(std::string(""), tag->at(4));
		CPPUNIT_ASSERT_EQUAL(std::string("sixth"), tag->at(5));
		CPPUNIT_ASSERT(tag->size() == 6);
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(Song_Test);
