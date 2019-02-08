#include <cppunit/extensions/HelperMacros.h>
#include "../mml_input.h"

class MML_Input_Test : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(MML_Input_Test);
	CPPUNIT_TEST(test_basic_mml);
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
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(MML_Input_Test);

