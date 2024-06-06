/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/zbus/zbus.h>
#include <net/nrf_cloud_coap.h>
#include <net/nrf_cloud_fota_poll.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/dfu/mcuboot.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(fota, CONFIG_APP_FOTA_LOG_LEVEL);

/* FOTA support context */
static void fota_reboot(enum nrf_cloud_fota_reboot_status status);
static struct nrf_cloud_fota_poll_ctx ctx = {
	.reboot_fn = fota_reboot
};
static bool fota_initialized;

void fota_reboot(enum nrf_cloud_fota_reboot_status status)
{
	LOG_INF("Rebooting with FOTA status %d", status);

	/* TODO: disconnect from network? */

	/* Flush the logging buffers */
	LOG_PANIC();

	/* Reboot the device */
	sys_reboot(SYS_REBOOT_COLD);
}


void fota_callback(const struct zbus_channel *chan)
{
	int err = 0;

	if (&CLOUD_CHAN == chan)
	{
		const enum cloud_status *status = zbus_chan_const_msg(chan);

		if (*status == CLOUD_CONNECTED)
		{
			LOG_DBG("Cloud connected, initializing FOTA");
			err = nrf_cloud_fota_poll_init(&ctx);
			if (err) {
				LOG_ERR("nrf_cloud_fota_poll_init failed: %d", err);
				return;
				/* TODO: can we recover from this? */
			}

			/* Process pending FOTA job, the FOTA type is returned */
			err = nrf_cloud_fota_poll_process_pending(&ctx);
			if (err < 0) {
				LOG_ERR("nrf_cloud_fota_poll_process_pending failed: %d", err);
			} else if (err != NRF_CLOUD_FOTA_TYPE__INVALID) {
				LOG_INF("Processed pending FOTA job type: %d", err);
			}
			fota_initialized = true;
			boot_write_img_confirmed();
		}
	}
	if (&TRIGGER_CHAN == chan && fota_initialized)
	{

		const enum trigger_type *trigger_type = zbus_chan_const_msg(chan);

		if (*trigger_type != TRIGGER_POLL) {
			return;
		}

		/* tell the rest of the app not to disturb */
		bool fota_ongoing = true;
		err = zbus_chan_pub(&FOTA_ONGOING_CHAN, &fota_ongoing, K_NO_WAIT);
		if (err) {
			LOG_ERR("zbus_chan_pub, error: %d", err);
			SEND_FATAL_ERROR();
		}

		LOG_DBG("Polling for FOTA updates");
		err = nrf_cloud_fota_poll_process(&ctx);
		if (err && err != -EAGAIN) {
			LOG_ERR("nrf_cloud_fota_poll_process failed: %d", err);
		}

		/* release the warning */
		fota_ongoing = false;
		err = zbus_chan_pub(&FOTA_ONGOING_CHAN, &fota_ongoing, K_NO_WAIT);
		if (err) {
			LOG_ERR("zbus_chan_pub, error: %d", err);
			SEND_FATAL_ERROR();
		}
	}
}


/* Register listener - led_callback will be called everytime a channel that the module listens on
 * receives a new message.
 */
ZBUS_LISTENER_DEFINE(fota, fota_callback);
