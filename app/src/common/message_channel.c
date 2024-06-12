/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "message_channel.h"

ZBUS_CHAN_DEFINE(TRIGGER_CHAN,
		 enum trigger_type,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(fota, app, battery, location, environmental),
		 ZBUS_MSG_INIT(0)
);

ZBUS_CHAN_DEFINE(FOTA_ONGOING_CHAN,
		 bool,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(false)
);

ZBUS_CHAN_DEFINE(PAYLOAD_CHAN,
		 struct payload,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(transport),
		 ZBUS_MSG_INIT(0)
);

ZBUS_CHAN_DEFINE(NETWORK_CHAN,
		 enum network_status,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(transport, battery),
		 NETWORK_DISCONNECTED
);

ZBUS_CHAN_DEFINE(FATAL_ERROR_CHAN,
		 int,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(led),
		 ZBUS_MSG_INIT(0)
);

ZBUS_CHAN_DEFINE(CONFIG_CHAN,
		 struct configuration,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(led, trigger),
		 ZBUS_MSG_INIT(0)
);

ZBUS_CHAN_DEFINE(CLOUD_CHAN,
		 enum cloud_status,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(fota, app, trigger),
		 CLOUD_DISCONNECTED
);

ZBUS_CHAN_DEFINE(BUTTON_CHAN,
		 uint8_t,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(trigger),
		 ZBUS_MSG_INIT(0)
);
