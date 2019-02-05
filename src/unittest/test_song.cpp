#include <cppunit/extensions/HelperMacros.h>
#include "../track.h"

class Song_Test : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(Song_Test);
	CPPUNIT_TEST(test_add_tag);
	CPPUNIT_TEST(test_add_tag_trim_trailing_spaces);
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
		song->add_tag("Tag1", "Value1");
		song->add_tag("Tag2", "Value2");

		// Test two different methods of retrieving the value.
		Tag *tag;
		tag = song->get_tag("Tag1");
		CPPUNIT_ASSERT_EQUAL(std::string("Value1"), tag->front());
		CPPUNIT_ASSERT_EQUAL(std::string("Value2"), song->get_tag_front("Tag2"));
	}
	// create tag with spaces and verify spaces are removed
	void test_add_tag_trim_trailing_spaces()
	{
		song->add_tag("Tag1", "trailing spaces   ");
		CPPUNIT_ASSERT_EQUAL(std::string("trailing spaces"), song->get_tag_front("Tag1"));
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(Song_Test);
