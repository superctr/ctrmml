#include <stdexcept>
#include <cppunit/extensions/HelperMacros.h>
#include "../conf.h"

class Conf_Test : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(Conf_Test);
	CPPUNIT_TEST(test_conf_parse1);
	CPPUNIT_TEST(test_conf_parse2);
	CPPUNIT_TEST(test_conf_parse3);
	CPPUNIT_TEST(test_conf_parse4);
	CPPUNIT_TEST(test_conf_parse5);
	CPPUNIT_TEST_SUITE_END();
public:
	void setUp()
	{
	}
	void tearDown()
	{
	}
	void test_conf_parse1()
	{
		auto conf = Conf::from_string("test { some stuff }");
		CPPUNIT_ASSERT_EQUAL((int)1, (int)conf.subkeys.size());
		CPPUNIT_ASSERT_EQUAL(std::string("test"), conf.subkeys[0].key);
		CPPUNIT_ASSERT_EQUAL((int)2, (int)conf.subkeys[0].subkeys.size());
		CPPUNIT_ASSERT_EQUAL(std::string("some"), conf.subkeys[0].subkeys[0].key);
		CPPUNIT_ASSERT_EQUAL(std::string("stuff"), conf.subkeys[0].subkeys[1].key);
	}

	void test_conf_parse2()
	{
		auto conf = Conf::from_string("foo: bar baz");
		CPPUNIT_ASSERT_EQUAL((int)2, (int)conf.subkeys.size());
		CPPUNIT_ASSERT_EQUAL(std::string("foo"), conf.subkeys[0].key);
		CPPUNIT_ASSERT_EQUAL((int)1, (int)conf.subkeys[0].subkeys.size());
		CPPUNIT_ASSERT_EQUAL(std::string("bar"), conf.subkeys[0].subkeys[0].key);
		CPPUNIT_ASSERT_EQUAL(std::string("baz"), conf.subkeys[1].key);
	}

	void test_conf_parse3()
	{
		auto conf = Conf::from_string("\"escaped string\": \"another escaped string\" \"i love them\"");
		CPPUNIT_ASSERT_EQUAL((int)2, (int)conf.subkeys.size());
		CPPUNIT_ASSERT_EQUAL(std::string("escaped string"), conf.subkeys[0].key);
		CPPUNIT_ASSERT_EQUAL((int)1, (int)conf.subkeys[0].subkeys.size());
		CPPUNIT_ASSERT_EQUAL(std::string("another escaped string"), conf.subkeys[0].subkeys[0].key);
		CPPUNIT_ASSERT_EQUAL(std::string("i love them"), conf.subkeys[1].key);
	}

	void test_conf_parse4()
	{
		// the ':' creates an extra level and i think it's intentional
		auto conf = Conf::from_string("\"more testing\": {strange,love}");
		CPPUNIT_ASSERT_EQUAL((int)1, (int)conf.subkeys.size());
		CPPUNIT_ASSERT_EQUAL(std::string("more testing"), conf.subkeys[0].key);
		CPPUNIT_ASSERT_EQUAL((int)1, (int)conf.subkeys[0].subkeys.size());
		CPPUNIT_ASSERT_EQUAL((int)2, (int)conf.subkeys[0].subkeys[0].subkeys.size());
		CPPUNIT_ASSERT_EQUAL(std::string("strange"), conf.subkeys[0].subkeys[0].subkeys[0].key);
		CPPUNIT_ASSERT_EQUAL(std::string("love"), conf.subkeys[0].subkeys[0].subkeys[1].key);
	}

	void test_conf_parse5()
	{
		auto conf = Conf::from_string("foo bar; baz");
		CPPUNIT_ASSERT_EQUAL((int)2, (int)conf.subkeys.size());
		CPPUNIT_ASSERT_EQUAL(std::string("foo"), conf.subkeys[0].key);
		CPPUNIT_ASSERT_EQUAL(std::string("bar"), conf.subkeys[1].key);
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(Conf_Test);

