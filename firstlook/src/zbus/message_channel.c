/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "message_channel.h"

ZBUS_CHAN_DEFINE(TRIGGER_CHAN,			/* Name */
		 int,				/* Message type */
		 NULL,				/* Validator */
		 NULL,				/* User data */
		 ZBUS_OBSERVERS(sampler, gas_sensor, location, movement),	/* Observers */
		 ZBUS_MSG_INIT(0)		/* Initial value {0} */
);

ZBUS_CHAN_DEFINE(SAMPLER_CHAN,
		 struct deviceToCloud_message_union_, /* single message */
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(sampler),
		 ZBUS_MSG_INIT(0)
);

ZBUS_CHAN_DEFINE(MSG_OUT_CHAN,
		 struct deviceToCloud_message, /* message bundle ready to encode */
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(transport),
		 ZBUS_MSG_INIT(0)
);

ZBUS_CHAN_DEFINE(NETWORK_CHAN,
		 enum network_status, /* connected, disconnected events */
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(transport, location),
		 ZBUS_MSG_INIT(0)
);

ZBUS_CHAN_DEFINE(LED_CHAN,
		 struct led_message, /* RGB setting */
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(led),
		 ZBUS_MSG_INIT(0)
);

ZBUS_CHAN_DEFINE(FATAL_ERROR_CHAN,
		 int,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(error),
		 ZBUS_MSG_INIT(0)
);

ZBUS_CHAN_DEFINE(CONFIG_CHAN,
		 struct config_message,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(trigger, location),
		 ZBUS_MSG_INIT(0)
);
