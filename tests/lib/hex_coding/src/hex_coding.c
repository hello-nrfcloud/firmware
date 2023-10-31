#include <unity.h>

#include "hex_coding/hex_coding.h"

#include <zephyr/kernel.h>
#include <zephyr/fff.h>

DEFINE_FFF_GLOBALS;

#define SENTINAL_VALUE	0xAE

void setUp(void)
{

}

void test_zero_buf(void)
{
	uint8_t in = SENTINAL_VALUE;
	uint8_t out = SENTINAL_VALUE;

	hex_encode(&in, &out, 0);
	TEST_ASSERT_EQUAL(SENTINAL_VALUE, in);
	TEST_ASSERT_EQUAL(SENTINAL_VALUE, out);

	hex_decode(&in, &out, 0);
	TEST_ASSERT_EQUAL(SENTINAL_VALUE, in);
	TEST_ASSERT_EQUAL(SENTINAL_VALUE, out);
}

void test_encode(void)
{
	const char *expected_out = "892705ABE3D4E51ACD3D";
	struct {
		uint8_t in[10];
		uint8_t sentinal_top;
		char out[20];
		uint8_t sentinal_bottom;
	} test_data = {
		.in = {0x89, 0x27, 0x5, 0xab, 0xe3, 0xd4, 0xe5, 0x1a, 0xcd, 0x3d},
		.out = {0},
		.sentinal_top = SENTINAL_VALUE,
		.sentinal_bottom = SENTINAL_VALUE,
	};

	hex_encode(test_data.in, test_data.out, ARRAY_SIZE(test_data.in));

	TEST_ASSERT_EQUAL_CHAR_ARRAY(expected_out, test_data.out, ARRAY_SIZE(test_data.out));
	TEST_ASSERT_EQUAL(SENTINAL_VALUE, test_data.sentinal_top);
	TEST_ASSERT_EQUAL(SENTINAL_VALUE, test_data.sentinal_bottom);
}

void test_decode(void)
{
	const uint8_t expected_out[] = {
		0x69, 0x51, 0x27, 0x55,
		0x73, 0x34, 0xcb, 0x86,
		0x62, 0x4a, 0xb5, 0x16,
		0xb2, 0x92, 0x26
	};
	struct {
		char in[30];
		uint8_t sentinal_top;
		uint8_t out[15];
		uint8_t sentinal_bottom;
	} test_data = {
		.in = "695127557334CB86624AB516B29226",
		.out = {0},
		.sentinal_top = SENTINAL_VALUE,
		.sentinal_bottom = SENTINAL_VALUE,
	};

	hex_decode(test_data.in, test_data.out, ARRAY_SIZE(test_data.in));

	TEST_ASSERT_EQUAL_CHAR_ARRAY(expected_out, test_data.out, ARRAY_SIZE(test_data.out));
	TEST_ASSERT_EQUAL(SENTINAL_VALUE, test_data.sentinal_top);
	TEST_ASSERT_EQUAL(SENTINAL_VALUE, test_data.sentinal_bottom);
}

/* It is required to be added to each test. That is because unity's
 * main may return nonzero, while zephyr's main currently must
 * return 0 in all cases (other values are reserved).
 */
extern int unity_main(void);

int main(void)
{
	/* use the runner from test_runner_generate() */
	(void)unity_main();

	return 0;
}
