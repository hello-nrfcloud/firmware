/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/smf.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(trigger, CONFIG_APP_TRIGGER_LOG_LEVEL);

/* Trigger interval in the frequent poll state */
#define FREQUENT_POLL_TRIGGER_INTERVAL_SEC 10

/* Duration that the trigger module is in the frequent poll state */
#define FREQUENT_POLL_DURATION_INTERVAL_SEC 600

/* Forward declarations */
static void trigger_poll_work_fn(struct k_work *work);
static void trigger_data_sample_work_fn(struct k_work *work);
static const struct smf_state states[];

/* Delayable work used to schedule triggers for polling */
static K_WORK_DELAYABLE_DEFINE(trigger_poll_work, trigger_poll_work_fn);
/* Delayable work used to schedule triggers for data sampling */
static K_WORK_DELAYABLE_DEFINE(trigger_data_sample_work, trigger_data_sample_work_fn);

/* Timer used to exit the frequent poll state after 10 minutes */
static void frequent_poll_state_duration_timer_handler(struct k_timer * timer_id);
static K_TIMER_DEFINE(frequent_poll_duration_timer, frequent_poll_state_duration_timer_handler, NULL);

/* Zephyr SMF states */
enum state {
	STATE_INIT,
	STATE_CONNECTED,
	/* Sub-state to STATE_CONNECTED */
	STATE_FREQUENT_POLL,
	/* Sub-state to STATE_CONNECTED */
	STATE_NORMAL,
	STATE_DISCONNECTED
};

/* Private channel used to signal when the duration in the frequent poll state expires */
ZBUS_CHAN_DECLARE(PRIV_TRIGGER_CHAN);
ZBUS_CHAN_DEFINE(PRIV_TRIGGER_CHAN,
		 int, /* Unused */
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS(trigger),
		 ZBUS_MSG_INIT(0)
);

/* User defined state object.
 * Used to transfer data between state changes.
 */
struct s_object {
	/* This must be first */
	struct smf_ctx ctx;

	/* Last channel type that a message was received on */
	const struct zbus_channel *chan;

	/* Set default update interval */
	uint64_t update_interval_sec;

	/* Button number */
	uint8_t button_number;

	/* Cloud status */
	enum cloud_status status;

};

/* SMF state object variable */
static struct s_object state_object;

static void trigger_send(enum trigger_type type, k_timeout_t timeout)
{
	enum trigger_type trigger_type = type;

	int err = zbus_chan_pub(&TRIGGER_CHAN, &trigger_type, timeout);

	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
}

/* Handler called when the frequent poll duration timer expires */
static void frequent_poll_state_duration_timer_handler(struct k_timer * timer_id)
{
	ARG_UNUSED(timer_id);

	int unused = 0;
	int err;

	LOG_DBG("Frequent poll duration timer expired");

	err = zbus_chan_pub(&PRIV_TRIGGER_CHAN, &unused, K_NO_WAIT);
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
}

/* Delayed work used to signal poll triggers to the rest of the system */
static void trigger_poll_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);

	LOG_DBG("Sending trigger poll message");
	LOG_DBG("trigger poll work timeout: %d seconds", FREQUENT_POLL_TRIGGER_INTERVAL_SEC);

	trigger_send(TRIGGER_POLL, K_SECONDS(1));
	k_work_reschedule(&trigger_poll_work, K_SECONDS(FREQUENT_POLL_TRIGGER_INTERVAL_SEC));
}

/* Delayed work used to signal data sample triggers to the rest of the system */
static void trigger_data_sample_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);

	LOG_DBG("Sending trigger data sample message");
	LOG_DBG("trigger data sample work timeout: %lld seconds", state_object.update_interval_sec);

	trigger_send(TRIGGER_DATA_SAMPLE, K_SECONDS(1));
	k_work_reschedule(&trigger_data_sample_work, K_SECONDS(state_object.update_interval_sec));
}

/* Button handler called when a user pushes a button */
static void button_handler(uint32_t button_states, uint32_t has_changed)
{
	int err;
	uint8_t button_number = 1;

	if (has_changed & button_states & DK_BTN1_MSK) {
		LOG_DBG("Button 1 pressed!");

		err = zbus_chan_pub(&BUTTON_CHAN, &button_number, K_SECONDS(1));
		if (err) {
			LOG_ERR("zbus_chan_pub, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}
	}
}

static void frequent_poll_duration_timer_start(void)
{
	LOG_DBG("Starting frequent poll duration timer: %d seconds",
		FREQUENT_POLL_DURATION_INTERVAL_SEC);

	k_timer_start(&frequent_poll_duration_timer,
		      K_SECONDS(FREQUENT_POLL_DURATION_INTERVAL_SEC), K_NO_WAIT);
}

static void refresh_timers(int64_t timeout_sec)
{
	LOG_DBG("Scheduling poll work: %d seconds", FREQUENT_POLL_TRIGGER_INTERVAL_SEC);
	LOG_DBG("Scheduling data sample work: %lld seconds", timeout_sec);

	frequent_poll_duration_timer_start();
	k_work_reschedule(&trigger_poll_work, K_SECONDS(FREQUENT_POLL_TRIGGER_INTERVAL_SEC));
	k_work_reschedule(&trigger_data_sample_work, K_SECONDS(timeout_sec));
}

/* Zephyr State Machine framework handlers */

/* HSM states:
 *
 * STATE_INIT: Initializing module
 * STATE_CONNECTED: Connected to cloud.
 *	- STATE_FREQUENT_POLL: Sending poll triggers every 10 seconds for 10 minutes
 *			       Sending data sample triggers every configured update interval
 *	- STATE_NORMAL: Sending poll triggers every configured update interval
 *			Sending data sample triggers every configured update interval
 * STATE_DISCONNECTED: Sending of triggers is blocked
 */

/* STATE_INIT */

static void init_entry(void *o)
{
	ARG_UNUSED(o);

	LOG_DBG("init_entry");

	int err = dk_buttons_init(button_handler);

	if (err) {
		LOG_ERR("dk_buttons_init, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
}

static void init_run(void *o)
{
	struct s_object *user_object = o;

	LOG_DBG("init_run");

	if ((user_object->chan == &CLOUD_CHAN) && (user_object->status == CLOUD_CONNECTED)) {
		smf_set_state(SMF_CTX(&state_object), &states[STATE_CONNECTED]);
		return;
	}
}

/* STATE_CONNECTED */

static void connected_run(void *o)
{
	struct s_object *user_object = o;

	LOG_DBG("connected_run");

	if ((user_object->chan == &CLOUD_CHAN) && (user_object->status == CLOUD_DISCONNECTED)) {
		smf_set_state(SMF_CTX(&state_object), &states[STATE_DISCONNECTED]);
		return;
	}
}

/* STATE_FREQUENT_POLL */

static void frequent_poll_entry(void *o)
{
	struct s_object *user_object = o;

	LOG_DBG("frequent_poll_entry");

	LOG_DBG("trigger poll work timeout: %d seconds", FREQUENT_POLL_TRIGGER_INTERVAL_SEC);
	LOG_DBG("trigger data sample work timeout: %lld seconds", user_object->update_interval_sec);

	trigger_send(TRIGGER_DATA_SAMPLE, K_SECONDS(1));
	refresh_timers(user_object->update_interval_sec);
}

static void frequent_poll_run(void *o)
{
	struct s_object *user_object = o;

	LOG_DBG("frequent_poll_run");

	if (user_object->chan == &BUTTON_CHAN) {
		LOG_DBG("Button %d pressed in frequent poll state, restarting duration timer",
			user_object->button_number);

		frequent_poll_duration_timer_start();
		trigger_send(TRIGGER_DATA_SAMPLE, K_SECONDS(1));
	} else if (user_object->chan == &CONFIG_CHAN) {
		LOG_DBG("Configuration received, refreshing timers");

		trigger_send(TRIGGER_DATA_SAMPLE, K_SECONDS(1));
		refresh_timers(user_object->update_interval_sec);
	} else if (user_object->chan == &PRIV_TRIGGER_CHAN) {
		LOG_DBG("Frequent poll duration timer expired, going into normal state");
		smf_set_state(SMF_CTX(&state_object), &states[STATE_NORMAL]);
		return;
	}
}

static void frequent_poll_exit(void *o)
{
	ARG_UNUSED(o);

	LOG_DBG("frequent_poll_exit");
	LOG_DBG("Clearing delayed work and frequent poll duration timer");

	k_timer_stop(&frequent_poll_duration_timer);
	k_work_cancel_delayable(&trigger_poll_work);
	k_work_cancel_delayable(&trigger_data_sample_work);
}

/* STATE_NORMAL */

static void normal_entry(void *o)
{
	struct s_object *user_object = o;

	LOG_DBG("normal_entry");

	LOG_DBG("trigger poll work timeout: %lld seconds", user_object->update_interval_sec);
	LOG_DBG("trigger data sample work timeout: %lld seconds", user_object->update_interval_sec);

	k_work_reschedule(&trigger_poll_work, K_SECONDS(user_object->update_interval_sec));
	k_work_reschedule(&trigger_data_sample_work, K_SECONDS(user_object->update_interval_sec));
}

static void normal_run(void *o)
{
	struct s_object *user_object = o;

	LOG_DBG("normal_run");

	if (user_object->chan == &BUTTON_CHAN) {
		LOG_DBG("Button %d pressed in normal state, going into frequent poll state",
			user_object->button_number);

		smf_set_state(SMF_CTX(&state_object), &states[STATE_FREQUENT_POLL]);
	} else if (user_object->chan == &CONFIG_CHAN) {
		LOG_DBG("Configuration received in normal state, going into frequent poll state");

		smf_set_state(SMF_CTX(&state_object), &states[STATE_FREQUENT_POLL]);
	}
}

static void normal_exit(void *o)
{
	ARG_UNUSED(o);

	LOG_DBG("normal_exit");
	LOG_DBG("Clearning delayed work");

	k_work_cancel_delayable(&trigger_poll_work);
	k_work_cancel_delayable(&trigger_data_sample_work);
}

/* STATE_DISCONNECTED */

static void disconnected_run(void *o)
{
	ARG_UNUSED(o);

	LOG_DBG("disconnected_run");

	struct s_object *user_object = o;

	if (user_object->chan == &CLOUD_CHAN && (user_object->status == CLOUD_CONNECTED)) {
		smf_set_state(SMF_CTX(&state_object), &states[STATE_CONNECTED]);
		return;
	}
}

/* Construct state table */
static const struct smf_state states[] = {
	[STATE_INIT] = SMF_CREATE_STATE(
		init_entry,
		init_run,
		NULL,
		NULL,
		NULL
	),
	[STATE_CONNECTED] = SMF_CREATE_STATE(
		NULL,
		connected_run,
		NULL,
		NULL,
		&states[STATE_FREQUENT_POLL]
	),
	[STATE_FREQUENT_POLL] = SMF_CREATE_STATE(
		frequent_poll_entry,
		frequent_poll_run,
		frequent_poll_exit,
		&states[STATE_CONNECTED],
		NULL
	),
	[STATE_NORMAL] = SMF_CREATE_STATE(
		normal_entry,
		normal_run,
		normal_exit,
		&states[STATE_CONNECTED],
		NULL
	),
	[STATE_DISCONNECTED] = SMF_CREATE_STATE(
		NULL,
		disconnected_run,
		NULL,
		NULL,
		NULL
	)
};

/* Function called when there is a message received on a channel that the module listens to */
void trigger_callback(const struct zbus_channel *chan)
{
	int err;

	if ((chan != &CONFIG_CHAN) &&
	    (chan != &CLOUD_CHAN) &&
	    (chan != &BUTTON_CHAN) &&
	    (chan != &PRIV_TRIGGER_CHAN)) {
		LOG_ERR("Unknown channel");
		return;
	}

	/* Update the state object with the channel that the message was received on */
	state_object.chan = chan;

	/* Copy corresponding data to the state object depending on the incoming channel */
	if (&CONFIG_CHAN == chan) {
		const struct configuration *config = zbus_chan_const_msg(chan);

		if (config->config_present == false) {
			LOG_DBG("Configuration not present");
			return;
		}

		state_object.update_interval_sec = config->update_interval;
	}

	if (&CLOUD_CHAN == chan) {
		const enum cloud_status *status = zbus_chan_const_msg(chan);

		state_object.status = *status;
	}

	if (&BUTTON_CHAN == chan) {
		const int *button_number = zbus_chan_const_msg(chan);

		state_object.button_number = (uint8_t)*button_number;
	}

	/* State object updated, run SMF */
	err = smf_run_state(SMF_CTX(&state_object));
	if (err) {
		LOG_ERR("smf_run_state, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
}

static int trigger_init(void)
{
	/* Set default update interval */
	state_object.update_interval_sec = CONFIG_APP_TRIGGER_TIMEOUT_SECONDS;

	smf_set_initial(SMF_CTX(&state_object), &states[STATE_INIT]);

	return 0;
}

/* Define a ZBUS listener for this module */
ZBUS_LISTENER_DEFINE(trigger, trigger_callback);

/* Initialize module at SYS_INIT() */
SYS_INIT(trigger_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
