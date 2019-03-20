#include <stdexcept>
#include <cppunit/extensions/HelperMacros.h>
#include "../vgm.h"

class VGM_Writer_Test : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(VGM_Writer_Test);
	CPPUNIT_TEST(test_vgm_output);
	CPPUNIT_TEST_SUITE_END();
public:
	void setUp()
	{
	}
	void tearDown()
	{
	}
	// VGM output will need to be verified manually as few asserts have been made on the output
	// right now we verify that allocations won't fail
	void test_vgm_output()
	{
		auto vgm = VGM_Writer("", 0x51, 0x80);
		vgm.poke32(0x2C, 7670454); // YM2612
		vgm.poke32(0x0C, 3579575); // SegaPSG
		vgm.poke16(0x28, 0x0009);
		vgm.poke8(0x2A, 0x10);
		vgm.poke8(0x2B, 0x03);
		vgm.write(0x50, 0, 0, 0x9f); // Mute PSG
		vgm.write(0x50, 0, 0, 0xbf);
		vgm.write(0x50, 0, 0, 0xdf);
		vgm.write(0x50, 0, 0, 0xff);
		vgm.write(0x52, 0, 0x2b, 0x80); // FM dac enable
		for(int i=0; i<1000000; i++)
		{
			// write a sawtooth waveform
			vgm.write(0x52, 0, 0x2a, (i & 0xff)); // FM dac data
			vgm.delay(1.5);
		}
		vgm.stop();
		vgm.write_tag();
		// verify sample count (1.5*1000000)
		CPPUNIT_ASSERT_EQUAL((uint32_t) 1500000, vgm.peek32(0x18));
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(VGM_Writer_Test);

