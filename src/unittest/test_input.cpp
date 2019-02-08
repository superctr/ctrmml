#include <cppunit/extensions/HelperMacros.h>
#include "../input.h"

class Line_Input_Test : public CppUnit::TestFixture, private Line_Input
{
	CPPUNIT_TEST_SUITE(Line_Input_Test);
	CPPUNIT_TEST(test_get);
	CPPUNIT_TEST(test_get_zero_at_eol);
	CPPUNIT_TEST(test_get_token);
	CPPUNIT_TEST(test_unget);
	CPPUNIT_TEST_SUITE_END();
public:
	Line_Input_Test() : Line_Input(0) {}
	void setUp()
	{
		buffer = "";
		line = 0;
		column = 0;
	}
	void tearDown()
	{
	}
	void test_get()
	{
		buffer = "hello";
		CPPUNIT_ASSERT('h' == get());
		CPPUNIT_ASSERT('e' == get());
		CPPUNIT_ASSERT('l' == get());
		CPPUNIT_ASSERT('l' == get());
		CPPUNIT_ASSERT('o' == get());
	}
	void test_get_zero_at_eol()
	{
		buffer = "hi";
		CPPUNIT_ASSERT('h' == get());
		CPPUNIT_ASSERT('i' == get());
		CPPUNIT_ASSERT(0 == get());
		CPPUNIT_ASSERT(0 == get());
		CPPUNIT_ASSERT(0 == get());
	}
	void test_get_token()
	{
		buffer = "\t   hello";
		CPPUNIT_ASSERT('h' == get_token());
		CPPUNIT_ASSERT('e' == get_token());
		CPPUNIT_ASSERT('l' == get_token());
		CPPUNIT_ASSERT('l' == get_token());
		CPPUNIT_ASSERT('o' == get_token());
	}
	void test_unget()
	{
		buffer = "hello";
		int c = get();
		unget(c);
		test_get();
	}
	bool parse_line()
	{
		// Dummy function
		return 1;
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(Line_Input_Test);

