#include <stdexcept>
#include <cppunit/extensions/HelperMacros.h>
#include <istream>
#include "../stringf.h"

class Misc_Test : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(Misc_Test);
	CPPUNIT_TEST(test_open_test_file_latin1);
	CPPUNIT_TEST(test_open_test_file_unicode);
	CPPUNIT_TEST_SUITE_END();
public:
	void setUp()
	{
	}
	void tearDown()
	{
	}
	void test_open_test_file_latin1()
	{
		std::ifstream inputfile = std::ifstream(get_native_filename(u8"src/unittest/åäö.txt").data());
		if(!inputfile)
			CPPUNIT_FAIL("file couldn't be opened");
		std::string str;
		std::getline(inputfile, str);
		CPPUNIT_ASSERT_EQUAL(std::string("my filename contains Latin1 characters"), str);
	}
	void test_open_test_file_unicode()
	{
		std::ifstream inputfile = std::ifstream(get_native_filename(u8"src/unittest/テスト.txt").data());
		if(!inputfile)
			CPPUNIT_FAIL("file couldn't be opened");
		std::string str;
		std::getline(inputfile, str);
		CPPUNIT_ASSERT_EQUAL(std::string("my filename contains Unicode characters"), str);
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(Misc_Test);
