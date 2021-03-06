#include <stdexcept>
#include <cppunit/extensions/HelperMacros.h>
#include "../input.h"

class Line_Input_Test : public CppUnit::TestFixture, private Line_Input
{
	CPPUNIT_TEST_SUITE(Line_Input_Test);
	CPPUNIT_TEST(test_get);
	CPPUNIT_TEST(test_get_zero_at_eol);
	CPPUNIT_TEST(test_get_token);
	CPPUNIT_TEST(test_unget);
	CPPUNIT_TEST(test_unget_at_eol);
	CPPUNIT_TEST_EXCEPTION(test_unget_too_many, std::out_of_range);
	CPPUNIT_TEST(test_get_num);
	CPPUNIT_TEST(test_get_num_increment);
	CPPUNIT_TEST(test_get_num_multiple);
	CPPUNIT_TEST(test_get_num_sign);
	CPPUNIT_TEST(test_get_num_hex);
	CPPUNIT_TEST(test_get_line);
	CPPUNIT_TEST_EXCEPTION(test_get_num_eol, std::invalid_argument);
	CPPUNIT_TEST_EXCEPTION(test_get_num_nan, std::invalid_argument);
	CPPUNIT_TEST(test_get_num_nan_increment);
	CPPUNIT_TEST(test_inputref);
	CPPUNIT_TEST_SUITE_END();
public:
	Line_Input_Test() : Line_Input(0) {}
	void setUp()
	{
		set_buffer("");
		line = 0;
		column = 0;
	}
	void tearDown()
	{
	}
	void test_get()
	{
		set_buffer("hello");
		CPPUNIT_ASSERT_EQUAL((int)'h', get());
		CPPUNIT_ASSERT_EQUAL((int)'e', get());
		CPPUNIT_ASSERT_EQUAL((int)'l', get());
		CPPUNIT_ASSERT_EQUAL((int)'l', get());
		CPPUNIT_ASSERT_EQUAL((int)'o', get());
	}
	void test_get_zero_at_eol()
	{
		set_buffer("hi");
		CPPUNIT_ASSERT_EQUAL((int)'h', get());
		CPPUNIT_ASSERT_EQUAL((int)'i', get());
		CPPUNIT_ASSERT_EQUAL((int)0, get());
		CPPUNIT_ASSERT_EQUAL((int)0, get());
		CPPUNIT_ASSERT_EQUAL((int)0, get());
	}
	void test_get_token()
	{
		set_buffer("\t   hello");
		CPPUNIT_ASSERT_EQUAL((int)'h', get_token());
		CPPUNIT_ASSERT_EQUAL((int)'e', get_token());
		CPPUNIT_ASSERT_EQUAL((int)'l', get_token());
		CPPUNIT_ASSERT_EQUAL((int)'l', get_token());
		CPPUNIT_ASSERT_EQUAL((int)'o', get_token());
	}
	void test_unget()
	{
		set_buffer("hello");
		int c = get();
		unget(c);
		test_get();
	}
	void test_unget_at_eol()
	{
		set_buffer("hi");
		get();
		get();
		CPPUNIT_ASSERT_EQUAL((int)0, get()); // = 0
		unget(); // should not cause extra write
		CPPUNIT_ASSERT_EQUAL(std::string("hi"), *buffer);
		CPPUNIT_ASSERT_EQUAL((unsigned int)2, column);
	}
	void test_unget_too_many()
	{
		set_buffer("hi");
		get();
		unget();
		unget(); // should throw std::out_of_range
	}
	void test_get_num()
	{
		set_buffer("123");
		CPPUNIT_ASSERT_EQUAL((int)123, get_num());
	}
	void test_get_num_increment()
	{
		set_buffer("123 456");
		get_num();
		CPPUNIT_ASSERT_EQUAL((unsigned int)3, column);
	}
	void test_get_num_multiple()
	{
		set_buffer("123 456 789 1012345 678");
		CPPUNIT_ASSERT_EQUAL((int)123, get_num());
		CPPUNIT_ASSERT_EQUAL((int)456, get_num());
		CPPUNIT_ASSERT_EQUAL((int)789, get_num());
		CPPUNIT_ASSERT_EQUAL((int)1012345, get_num());
		CPPUNIT_ASSERT_EQUAL((int)678, get_num());
	}
	void test_get_num_sign()
	{
		set_buffer("-123 +123 -123 +123");
		CPPUNIT_ASSERT_EQUAL((int)-123, get_num());
		CPPUNIT_ASSERT_EQUAL((int)123, get_num());
		CPPUNIT_ASSERT_EQUAL((int)-123, get_num());
		CPPUNIT_ASSERT_EQUAL((int)123, get_num());
	}
	void test_get_num_hex()
	{
		set_buffer("$12 x2345");
		CPPUNIT_ASSERT_EQUAL((int)0x12, get_num());
		CPPUNIT_ASSERT_EQUAL((int)0x2345, get_num());
	}
	void test_get_num_eol()
	{
		set_buffer("123");
		get_num();
		get_num(); // should throw std::invalid_argument
	}
	void test_get_num_nan()
	{
		set_buffer("Skitrumpa");
		get_num(); // should throw std::invalid_argument
	}
	// check that column is unchanged if exception is thrown
	void test_get_num_nan_increment()
	{
		set_buffer("Skitrumpa");
		try {get_num();} // should throw std::invalid_argument
		catch(std::invalid_argument&)
		{
		}
		CPPUNIT_ASSERT_EQUAL((unsigned int)0, column);
	}
	void test_get_line()
	{
		set_buffer("0123456789");
		CPPUNIT_ASSERT_EQUAL(std::string("0123456789"), get_line());
		seek(3);
		CPPUNIT_ASSERT_EQUAL(std::string("3456789"), get_line());
	}
	void parse_line()
	{
		// Dummy function
	}
	void test_inputref()
	{
		set_buffer("Skitrumpa");
		std::shared_ptr<InputRef> ptr = get_reference();
		get();
		get(); // increment column a few times
		CPPUNIT_ASSERT_EQUAL((unsigned int)0,ptr->get_column());
		ptr = get_reference();
		CPPUNIT_ASSERT_EQUAL((unsigned int)2,ptr->get_column());
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(Line_Input_Test);

