/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>

#include <zephyr/fff.h>
#include <zephyr/drivers/pwm.h>
#include "message_channel.h"


DEFINE_FFF_GLOBALS;


FAKE_VALUE_FUNC(int, nordic_pwm_logger_set_cycles,const struct device *, uint32_t,
				 uint32_t, uint32_t,
				 pwm_flags_t);

void setUp(void)
{
	RESET_FAKE(nordic_pwm_logger_set_cycles);
}				 

void test_zero_msg(void)
{
	struct led_message m = {0};

	int err = zbus_chan_pub(&LED_CHAN, &m, K_NO_WAIT);

	TEST_ASSERT_EQUAL(0, err);

	/* expect channels 0,1,2 to be set */
	TEST_ASSERT_EQUAL(0, nordic_pwm_logger_set_cycles_fake.arg1_history[0]);
	TEST_ASSERT_EQUAL(1, nordic_pwm_logger_set_cycles_fake.arg1_history[1]);
	TEST_ASSERT_EQUAL(2, nordic_pwm_logger_set_cycles_fake.arg1_history[2]);

	/* expect pulse to be set to 0,0,0 */
	TEST_ASSERT_EQUAL(0, nordic_pwm_logger_set_cycles_fake.arg3_history[0]);
	TEST_ASSERT_EQUAL(0, nordic_pwm_logger_set_cycles_fake.arg3_history[1]);
	TEST_ASSERT_EQUAL(0, nordic_pwm_logger_set_cycles_fake.arg3_history[2]);
}

void test_white_color(void)
{
	struct led_message m =
	{
		.led_blue = 100,
		.led_green = 100,
		.led_red = 100,
		.timestamp = 1691751633,
	};

	int err = zbus_chan_pub(&LED_CHAN, &m, K_NO_WAIT);

	TEST_ASSERT_EQUAL(0, err);

	/* expect channels 0,1,2 to be set */
	TEST_ASSERT_EQUAL(0, nordic_pwm_logger_set_cycles_fake.arg1_history[0]);
	TEST_ASSERT_EQUAL(1, nordic_pwm_logger_set_cycles_fake.arg1_history[1]);
	TEST_ASSERT_EQUAL(2, nordic_pwm_logger_set_cycles_fake.arg1_history[2]);

	/* expect pulse to be set according to color */
	TEST_ASSERT_EQUAL(nordic_pwm_logger_set_cycles_fake.arg2_history[0] * m.led_red / 100,
		nordic_pwm_logger_set_cycles_fake.arg3_history[0]);
	TEST_ASSERT_EQUAL(nordic_pwm_logger_set_cycles_fake.arg2_history[1] * m.led_green / 100,
		nordic_pwm_logger_set_cycles_fake.arg3_history[1]);
	TEST_ASSERT_EQUAL(nordic_pwm_logger_set_cycles_fake.arg2_history[2] * m.led_blue / 100,
		nordic_pwm_logger_set_cycles_fake.arg3_history[2]);
}

void test_purple_color(void)
{
	struct led_message m =
	{
		.led_blue = 50,
		.led_green = 0,
		.led_red = 50,
		.timestamp = 1691751633,
	};

	int err = zbus_chan_pub(&LED_CHAN, &m, K_NO_WAIT);

	TEST_ASSERT_EQUAL(0, err);

	/* expect channels 0,1,2 to be set */
	TEST_ASSERT_EQUAL(0, nordic_pwm_logger_set_cycles_fake.arg1_history[0]);
	TEST_ASSERT_EQUAL(1, nordic_pwm_logger_set_cycles_fake.arg1_history[1]);
	TEST_ASSERT_EQUAL(2, nordic_pwm_logger_set_cycles_fake.arg1_history[2]);

	/* expect pulse to be set according to color */
	TEST_ASSERT_EQUAL(nordic_pwm_logger_set_cycles_fake.arg2_history[0] * m.led_red / 100,
		nordic_pwm_logger_set_cycles_fake.arg3_history[0]);
	TEST_ASSERT_EQUAL(nordic_pwm_logger_set_cycles_fake.arg2_history[1] * m.led_green / 100,
		nordic_pwm_logger_set_cycles_fake.arg3_history[1]);
	TEST_ASSERT_EQUAL(nordic_pwm_logger_set_cycles_fake.arg2_history[2] * m.led_blue / 100,
		nordic_pwm_logger_set_cycles_fake.arg3_history[2]);
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
