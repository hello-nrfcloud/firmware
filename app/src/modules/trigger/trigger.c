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

void trigger_callback(const struct zbus_channel *chan);

/* Define a ZBUS listener for this module */
ZBUS_LISTENER_DEFINE(trigger, trigger_callback);

/* Observe channels */
ZBUS_CHAN_ADD_OBS(CONFIG_CHAN, trigger, 0);
ZBUS_CHAN_ADD_OBS(CLOUD_CHAN, trigger, 0);
ZBUS_CHAN_ADD_OBS(BUTTON_CHAN, trigger, 0);
ZBUS_CHAN_ADD_OBS(LOCATION_CHAN, trigger, 0);

/* Data sample trigger interval in the frequent poll state */
#define FREQUENT_POLL_TRIGGER_INTERVAL_SEC 60
/* Shadow poll trigger interval in the frequent poll state */
#define FREQUENT_POLL_SHADOW_POLL_TRIGGER_INTERVAL_SEC 20

/* Forward declarations */
static void trigger_work_fn(struct k_work *work);
static void trigger_fota_poll_work_fn(struct k_work *work);
static void trigger_shadow_poll_work_fn(struct k_work *work);
static const struct smf_state states[];

/* Delayable work used to schedule triggers for polling and data sampling */
static K_WORK_DELAYABLE_DEFINE(trigger_work, trigger_work_fn);
static K_WORK_DELAYABLE_DEFINE(trigger_fota_poll_work, trigger_fota_poll_work_fn);
static K_WORK_DELAYABLE_DEFINE(trigger_shadow_poll_work, trigger_shadow_poll_work_fn);

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
	/* Sub-state to STATE_CONNECTED */
	STATE_BLOCKED,
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

	/* Update interval, will always reflect what is received in incoming CONFIG channel
	 * messages
	 */
	uint64_t update_interval_configured_sec;

	/* Update interval used when scheduling data sample triggers, will differ depending on the
	 * state that the module is in
	 */
	uint64_t update_interval_used_sec;

	/* Update interval used when scheduling shadow poll triggers, will differ depending on the
	 * state that the module is in
	 */
	uint64_t shadow_poll_interval_used_sec;

	/* Location search status, used to block trigger events */
	bool location_search;

	/* Button number */
	uint8_t button_number;

	/* Cloud status */
	enum cloud_status status;

	/* Trigger mode */
	enum trigger_mode trigger_mode;
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

/* Delayed work used to send triggers to the rest of the system */
static void trigger_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);

	LOG_DBG("Sending data sample trigger");

	trigger_send(TRIGGER_DATA_SAMPLE, K_SECONDS(1));

	k_work_reschedule(&trigger_work, K_SECONDS(state_object.update_interval_used_sec));
}

static void trigger_shadow_poll_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);

	LOG_DBG("Sending shadow poll trigger");

	trigger_send(TRIGGER_POLL, K_SECONDS(1));

	k_work_reschedule(&trigger_shadow_poll_work,
			  K_SECONDS(state_object.shadow_poll_interval_used_sec));
}

static void trigger_fota_poll_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);

	LOG_DBG("Sending FOTA poll trigger");

	trigger_send(TRIGGER_FOTA_POLL, K_SECONDS(1));

	k_work_reschedule(&trigger_fota_poll_work,
			  K_SECONDS(CONFIG_APP_TRIGGER_FOTA_POLL_INTERVAL_SEC));
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

static void frequent_poll_duration_timer_start(bool force_restart)
{
	if ((k_timer_remaining_get(&frequent_poll_duration_timer) == 0) || force_restart) {
		LOG_DBG("Starting frequent poll duration timer: %d seconds",
			CONFIG_FREQUENT_POLL_DURATION_INTERVAL_SEC);

		k_timer_start(&frequent_poll_duration_timer,
			      K_SECONDS(CONFIG_FREQUENT_POLL_DURATION_INTERVAL_SEC), K_NO_WAIT);
		return;
	}
}

static void frequent_poll_duration_timer_stop(void)
{
	k_timer_stop(&frequent_poll_duration_timer);
}

/* Zephyr State Machine framework handlers */

/* HSM states:
 *
 * STATE_INIT: Initializing module
 * STATE_CONNECTED: Connected to cloud.
 *	- STATE_FREQUENT_POLL: Sending poll and data sample triggers every 20 seconds for 10 minutes
 *	- STATE_NORMAL: Sending poll triggers every configured update interval
 *				Sending data sample triggers every configured update interval
 *	- STATE_BLOCKED: Sending of triggers is blocked due to an active location search
 * STATE_DISCONNECTED: Sending of triggers is blocked due to being disconnected
 *
 *
 * Note:
 *
 * Every state is responsible for cancelling any delayed work or timer
 * that was scheduled in that state.
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

	if ((user_object->chan == &CLOUD_CHAN) && (user_object->status == CLOUD_CONNECTED_READY_TO_SEND)) {
		LOG_DBG("Cloud connected, going into connected state");
		smf_set_state(SMF_CTX(&state_object), &states[STATE_CONNECTED]);
		return;
	}
}

/* STATE_CONNECTED */

static void connected_run(void *o)
{
	struct s_object *user_object = o;

	LOG_DBG("connected_run");

	if ((user_object->chan == &CLOUD_CHAN) &&
	    ((user_object->status == CLOUD_CONNECTED_PAUSED) ||
	     (user_object->status == CLOUD_DISCONNECTED))) {
		LOG_DBG("Cloud disconnected/paused, going into disconnected state");
		smf_set_state(SMF_CTX(&state_object), &states[STATE_DISCONNECTED]);
		return;
	}
}

/* STATE_BLOCKED */

static void blocked_run(void *o)
{
	struct s_object *user_object = o;

	LOG_DBG("blocked_run");

	if (user_object->chan == &LOCATION_CHAN && !user_object->location_search) {
		LOG_DBG("Location search done");

		if (user_object->trigger_mode == TRIGGER_MODE_NORMAL) {
			LOG_DBG("Going into normal state");
			smf_set_state(SMF_CTX(&state_object), &states[STATE_NORMAL]);
		} else {
			LOG_DBG("Going into frequent poll state");
			smf_set_state(SMF_CTX(&state_object), &states[STATE_FREQUENT_POLL]);
		}
		return;
	} else if (user_object->chan == &PRIV_TRIGGER_CHAN) {
		/* Frequent poll duration timer expired. Since the current state is BLOCKED,
		 * continue to remain in this state but only change the trigger mode so that
		 * when the location search is done, the state machine transitions into Normal mode.
		 */
		LOG_DBG("Changing the trigger mode in state object ");
		user_object->trigger_mode = TRIGGER_MODE_NORMAL;
		return;
	} else if (user_object->chan == &BUTTON_CHAN) {
		LOG_DBG("Button %d pressed in blocked state, restarting duration timer",
			user_object->button_number);

		frequent_poll_duration_timer_start(true);

	} else if (user_object->chan == &CONFIG_CHAN) {
		LOG_DBG("Configuration received, refreshing poll duration timer");

		frequent_poll_duration_timer_start(true);
	} else {
		LOG_DBG("Message received on channel %s. Ignoring.", zbus_chan_name(user_object->chan));
		/* Do nothing. Parent state may have handling for this. */
	}
}

/* STATE_FREQUENT_POLL */

static void frequent_poll_entry(void *o)
{
	struct s_object *user_object = o;

	LOG_DBG("frequent_poll_entry");

	user_object->update_interval_used_sec = FREQUENT_POLL_TRIGGER_INTERVAL_SEC;
	user_object->shadow_poll_interval_used_sec = FREQUENT_POLL_SHADOW_POLL_TRIGGER_INTERVAL_SEC;
	user_object->trigger_mode = TRIGGER_MODE_POLL;

	if (user_object->chan == &LOCATION_CHAN && !user_object->location_search) {
		LOG_DBG("Location search done");

		k_work_reschedule(&trigger_work, K_SECONDS(user_object->update_interval_used_sec));
		k_work_reschedule(&trigger_shadow_poll_work,
				  K_SECONDS(user_object->shadow_poll_interval_used_sec));
		k_work_reschedule(&trigger_fota_poll_work,
				  K_SECONDS(CONFIG_APP_TRIGGER_FOTA_POLL_INTERVAL_SEC));
		return;
	}
	int err = zbus_chan_pub(&TRIGGER_MODE_CHAN, &user_object->trigger_mode, K_SECONDS(1));

	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	LOG_DBG("Sending data sample triggers every %d seconds for %d minutes",
		FREQUENT_POLL_TRIGGER_INTERVAL_SEC,
		CONFIG_FREQUENT_POLL_DURATION_INTERVAL_SEC / 60);

	LOG_DBG("Sending shadow poll triggers every %d seconds for %d minutes",
		FREQUENT_POLL_SHADOW_POLL_TRIGGER_INTERVAL_SEC,
		CONFIG_FREQUENT_POLL_DURATION_INTERVAL_SEC / 60);

	LOG_DBG("Sending FOTA poll triggers every %d seconds",
		CONFIG_APP_TRIGGER_FOTA_POLL_INTERVAL_SEC);

	frequent_poll_duration_timer_start(false);
	k_work_reschedule(&trigger_work, K_NO_WAIT);
	k_work_reschedule(&trigger_fota_poll_work, K_NO_WAIT);
	k_work_reschedule(&trigger_shadow_poll_work, K_NO_WAIT);
}

static void frequent_poll_run(void *o)
{
	struct s_object *user_object = o;

	LOG_DBG("frequent_poll_run");

	if (user_object->chan == &LOCATION_CHAN && user_object->location_search) {
		LOG_DBG("Location search started, going into blocked state");

		smf_set_state(SMF_CTX(&state_object), &states[STATE_BLOCKED]);
		return;
	} else if (user_object->chan == &PRIV_TRIGGER_CHAN) {
		LOG_DBG("Going into normal state");
		smf_set_state(SMF_CTX(&state_object), &states[STATE_NORMAL]);
		return;
	} else if (user_object->chan == &BUTTON_CHAN) {
		LOG_DBG("Button %d pressed in frequent poll state, restarting duration timer",
			user_object->button_number);

		frequent_poll_duration_timer_start(true);
		k_work_reschedule(&trigger_work, K_NO_WAIT);
		k_work_reschedule(&trigger_fota_poll_work, K_NO_WAIT);
		k_work_reschedule(&trigger_shadow_poll_work, K_NO_WAIT);

	} else if (user_object->chan == &CONFIG_CHAN) {
		LOG_DBG("Configuration received, refreshing poll duration timer");

		frequent_poll_duration_timer_start(true);
	} else {
		/* Parent state may have handling of this event. */
		LOG_DBG("Message received on channel %s. Ignoring.", zbus_chan_name(user_object->chan));
	}
}

static void frequent_poll_exit(void *o)
{
	ARG_UNUSED(o);

	LOG_DBG("frequent_poll_exit");

	k_work_cancel_delayable(&trigger_work);
	k_work_cancel_delayable(&trigger_fota_poll_work);
	k_work_cancel_delayable(&trigger_shadow_poll_work);
}

/* STATE_NORMAL */

static void normal_entry(void *o)
{
	struct s_object *user_object = o;

	LOG_DBG("normal_entry");

	user_object->update_interval_used_sec = user_object->update_interval_configured_sec;
	user_object->shadow_poll_interval_used_sec = user_object->update_interval_configured_sec;
	user_object->trigger_mode = TRIGGER_MODE_NORMAL;

	/* Send message on trigger mode channel */
	int err = zbus_chan_pub(&TRIGGER_MODE_CHAN, &user_object->trigger_mode, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	LOG_DBG("Sending data sample triggers every "
		"configured update interval: %lld seconds",
		user_object->update_interval_configured_sec);

	LOG_DBG("Sending shadow poll triggers every %lld seconds",
		user_object->shadow_poll_interval_used_sec);

	LOG_DBG("Sending FOTA poll triggers every %d seconds",
		CONFIG_APP_TRIGGER_FOTA_POLL_INTERVAL_SEC);

	k_work_reschedule(&trigger_work, K_SECONDS(user_object->update_interval_used_sec));
	k_work_reschedule(&trigger_shadow_poll_work,
			  K_SECONDS(user_object->shadow_poll_interval_used_sec));
	k_work_reschedule(&trigger_fota_poll_work,
			  K_SECONDS(CONFIG_APP_TRIGGER_FOTA_POLL_INTERVAL_SEC));
}

static void normal_run(void *o)
{
	struct s_object *user_object = o;

	LOG_DBG("normal_run");

	if (user_object->chan == &LOCATION_CHAN && user_object->location_search) {
		LOG_DBG("Location search started, going into blocked state");

		smf_set_state(SMF_CTX(&state_object), &states[STATE_BLOCKED]);
		return;
	} else if (user_object->chan == &BUTTON_CHAN) {
		LOG_DBG("Button %d pressed in normal state, going into frequent poll state",
			user_object->button_number);

		smf_set_state(SMF_CTX(&state_object), &states[STATE_FREQUENT_POLL]);
		return;
	} else if (user_object->chan == &CONFIG_CHAN) {
		LOG_DBG("Configuration received in normal state, going into frequent poll state");

		smf_set_state(SMF_CTX(&state_object), &states[STATE_FREQUENT_POLL]);
		return;
	}
}

static void normal_exit(void *o)
{
	struct s_object *user_object = o;

	LOG_DBG("normal_exit");

	user_object->update_interval_used_sec = FREQUENT_POLL_TRIGGER_INTERVAL_SEC;
	user_object->shadow_poll_interval_used_sec = FREQUENT_POLL_SHADOW_POLL_TRIGGER_INTERVAL_SEC;

	k_work_cancel_delayable(&trigger_work);
	k_work_cancel_delayable(&trigger_fota_poll_work);
	k_work_cancel_delayable(&trigger_shadow_poll_work);
}

/* STATE_DISCONNECTED */

static void disconnected_entry(void *o)
{
	ARG_UNUSED(o);

	LOG_DBG("disconnected_entry");
	/* Cancel frequent poll duration timer if running. This can happen when state machine was
	 * in BLOCKED state and then a disconnection happened.
	 */
	frequent_poll_duration_timer_stop();
}

static void disconnected_run(void *o)
{
	struct s_object *user_object = o;

	LOG_DBG("disconnected_run");

	if (user_object->chan == &CLOUD_CHAN && (user_object->status == CLOUD_CONNECTED_READY_TO_SEND)) {
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
	[STATE_BLOCKED] = SMF_CREATE_STATE(
		NULL,
		blocked_run,
		NULL,
		&states[STATE_CONNECTED],
		NULL
	),
	[STATE_DISCONNECTED] = SMF_CREATE_STATE(
		disconnected_entry,
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
	    (chan != &LOCATION_CHAN) &&
	    (chan != &BUTTON_CHAN) &&
	    (chan != &PRIV_TRIGGER_CHAN)) {
		LOG_ERR("Unknown channel");
		return;
	}

	LOG_DBG("Received message on channel %s", zbus_chan_name(chan));

	/* Update the state object with the channel that the message was received on */
	state_object.chan = chan;

	/* Copy corresponding data to the state object depending on the incoming channel */
	if (&CONFIG_CHAN == chan) {
		const struct configuration *config = zbus_chan_const_msg(chan);

		if (config->update_interval_present) {
			state_object.update_interval_configured_sec = config->update_interval;
		}
	} else if (&CLOUD_CHAN == chan) {
		const enum cloud_status *status = zbus_chan_const_msg(chan);

		state_object.status = *status;
	} else if (&BUTTON_CHAN == chan) {
		const int *button_number = zbus_chan_const_msg(chan);

		state_object.button_number = (uint8_t)*button_number;
	} else if (&LOCATION_CHAN == chan) {
		const enum location_status *location_status = zbus_chan_const_msg(chan);

		state_object.location_search = (*location_status == LOCATION_SEARCH_STARTED);
	} else {
		/* PRIV_TRIGGER_CHAN event. Frequent Poll Duration timer expired*/
		LOG_DBG("Message received on PRIV_TRIGGER_CHAN channel.");
		/* Do nothing to the state object */
	}

	LOG_DBG("Running SMF");

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
	state_object.update_interval_configured_sec = CONFIG_APP_TRIGGER_TIMEOUT_SECONDS;
	state_object.update_interval_used_sec = CONFIG_APP_TRIGGER_TIMEOUT_SECONDS;
	state_object.shadow_poll_interval_used_sec = FREQUENT_POLL_TRIGGER_INTERVAL_SEC;
	state_object.trigger_mode = TRIGGER_MODE_POLL;

	smf_set_initial(SMF_CTX(&state_object), &states[STATE_INIT]);

	return 0;
}

/* Initialize module at SYS_INIT() */
SYS_INIT(trigger_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
