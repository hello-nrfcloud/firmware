/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/sensor/npm13xx_charger.h>
#include <zephyr/sys/util.h>
#include <nrf_fuel_gauge.h>
#include <date_time.h>
#include <math.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <zephyr/smf.h>
#include <modem/lte_lc.h>

#include "lp803448_model.h"
#include "message_channel.h"
#include "modules_common.h"
#include "bat_object_encode.h"

#if defined(CONFIG_MEMFAULT_NRF_PLATFORM_BATTERY_NPM13XX)
#include "memfault/metrics/platform/battery.h"
#endif /* CONFIG_MEMFAULT_NRF_PLATFORM_BATTERY_NPM13XX */

/* Register log module */
LOG_MODULE_REGISTER(battery, CONFIG_APP_BATTERY_LOG_LEVEL);

/* Register subscriber */
ZBUS_MSG_SUBSCRIBER_DEFINE(battery);

/* Private channel message types for internal state management. */
enum priv_battery_msg_type {
	/* Periodic sampling timer expired. */
	BATTERY_PRIV_SAMPLE_TIMER_EXPIRED,
	/* Modem entered sleep. */
	BATTERY_PRIV_MODEM_SLEEP_ENTRY,
	/* Modem exited sleep. */
	BATTERY_PRIV_MODEM_SLEEP_EXIT,
};

struct priv_battery_msg {
	enum priv_battery_msg_type type;
};

/* Private channel for internal messaging not intended for external use. */
ZBUS_CHAN_DEFINE(PRIV_BATTERY_CHAN,
		 struct priv_battery_msg,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(0)
);

/* Observe channels */
ZBUS_CHAN_ADD_OBS(TRIGGER_CHAN, battery, 0);
ZBUS_CHAN_ADD_OBS(NETWORK_CHAN, battery, 0);
ZBUS_CHAN_ADD_OBS(TIME_CHAN, battery, 0);
ZBUS_CHAN_ADD_OBS(PRIV_BATTERY_CHAN, battery, 0);

#define MAX_MSG_SIZE \
	(MAX(MAX(sizeof(enum trigger_type), sizeof(struct priv_battery_msg)), \
		(MAX(sizeof(enum network_status), sizeof(enum time_status)))))

BUILD_ASSERT(CONFIG_APP_BATTERY_WATCHDOG_TIMEOUT_SECONDS >
			CONFIG_APP_BATTERY_EXEC_TIME_SECONDS_MAX,
			"Watchdog timeout must be greater than maximum execution time");

/* nPM1300 register bitmasks */

/* CHARGER.BCHGCHARGESTATUS.CHARGECOMPLETE */
#define NPM1300_CHG_STATUS_COMPLETE_MASK BIT(1)
/* CHARGER.BCHGCHARGESTATUS.TRICKLECHARGE */
#define NPM1300_CHG_STATUS_TC_MASK BIT(2)
/* CHARGER.BCHGCHARGESTATUS.CONSTANTCURRENT */
#define NPM1300_CHG_STATUS_CC_MASK BIT(3)
/* CHARGER.BCHGCHARGESTATUS.CONSTANTVOLTAGE */
#define NPM1300_CHG_STATUS_CV_MASK BIT(4)
/* Active charging states mask */
#define NPM1300_CHG_ACTIVE_MASK (NPM1300_CHG_STATUS_TC_MASK | \
				 NPM1300_CHG_STATUS_CC_MASK | \
				 NPM1300_CHG_STATUS_CV_MASK)

BUILD_ASSERT(CONFIG_APP_BATTERY_IDLE_CURRENT_NA > 0, "Idle current must be set");

static const struct device *charger = DEVICE_DT_GET(DT_NODELABEL(npm1300_charger));

/* Forward declarations */
static struct s_object s_obj;
static void sample_timer_work_fn(struct k_work *work);

/* Delayable work used to schedule periodic fuel gauge sampling. */
static K_WORK_DELAYABLE_DEFINE(sample_timer_work, sample_timer_work_fn);

/* State machine */

/* Defininig the module states.
 *
 * STATE_INIT: Initializing and waiting for time to be available.
 * STATE_SAMPLING: Ready to sample. Parent of the active/idle substates.
 * STATE_ACTIVE: Modem is awake. Fuel gauge is sampled periodically.
 * STATE_IDLE: Modem is in sleep. Fuel gauge is put in idle mode.
 */
enum battery_module_state {
	STATE_INIT,
	STATE_SAMPLING,
	STATE_ACTIVE,
	STATE_IDLE,
};

/* User defined state object.
 * Used to transfer data between state changes.
 */
struct s_object {
	/* This must be first */
	struct smf_ctx ctx;

	/* Last channel type that a message was received on */
	const struct zbus_channel *chan;

	/* Buffer for last zbus message */
	uint8_t msg_buf[MAX_MSG_SIZE];

	/* Fuel gauge reference time */
	int64_t fuel_gauge_ref_time;

	/* Latest battery state-of-charge [%] */
	float percentage;

	/* Latest battery voltage [V] */
	float voltage;

	/* Latest battery current [A] */
	float current;

	/* Latest battery temperature [C] */
	float temperature;

	/* Whether the battery is currently charging */
	bool charging;
};

/* Forward declarations of state handlers */
static void state_init_entry(void *o);
static enum smf_state_result state_init_run(void *o);
static void state_sampling_entry(void *o);
static void state_active_entry(void *o);
static enum smf_state_result state_active_run(void *o);
static void state_active_exit(void *o);
static void state_idle_entry(void *o);
static enum smf_state_result state_idle_run(void *o);

static struct s_object s_obj;
static const struct smf_state states[] = {
	[STATE_INIT] =
		SMF_CREATE_STATE(state_init_entry, state_init_run, NULL,
				 NULL,	/* No parent state */
				 NULL), /* No initial transition */
	[STATE_SAMPLING] =
		SMF_CREATE_STATE(state_sampling_entry, NULL, NULL,
				 NULL,
				 &states[STATE_ACTIVE]),
	[STATE_ACTIVE] =
		SMF_CREATE_STATE(state_active_entry, state_active_run, state_active_exit,
				 &states[STATE_SAMPLING],
				 NULL),
	[STATE_IDLE] =
		SMF_CREATE_STATE(state_idle_entry, state_idle_run, NULL,
				 &states[STATE_SAMPLING],
				 NULL),
};

/* Helper functions */

static int charger_read_sensors(float *voltage, float *current, float *temp, int32_t *chg_status,
				bool *vbus_connected)
{
	struct sensor_value value;
	int err;

	err = sensor_sample_fetch(charger);
	if (err < 0) {
		return err;
	}

	sensor_channel_get(charger, SENSOR_CHAN_GAUGE_VOLTAGE, &value);
	*voltage = (float)value.val1 + ((float)value.val2 / 1000000);

	sensor_channel_get(charger, SENSOR_CHAN_GAUGE_TEMP, &value);
	*temp = (float)value.val1 + ((float)value.val2 / 1000000);

	sensor_channel_get(charger, SENSOR_CHAN_GAUGE_AVG_CURRENT, &value);
	*current = (float)value.val1 + ((float)value.val2 / 1000000);

	sensor_channel_get(charger, (enum sensor_channel)SENSOR_CHAN_NPM13XX_CHARGER_STATUS,
			   &value);
	*chg_status = value.val1;

	if (vbus_connected != NULL) {
		struct sensor_value vbus_present;

		err = sensor_attr_get(
			charger, (enum sensor_channel)SENSOR_CHAN_NPM13XX_CHARGER_VBUS_STATUS,
			(enum sensor_attribute)SENSOR_ATTR_NPM13XX_CHARGER_VBUS_PRESENT,
			&vbus_present);
		if (err == 0) {
			*vbus_connected = (vbus_present.val1 != 0);
		} else {
			LOG_DBG("Failed to read VBUS state: %d", err);
			*vbus_connected = false;
		}
	}

	return 0;
}

/* Inform the fuel gauge library of charger state changes so that predictions improve. */
static void update_charge_state_if_changed(int32_t chg_status, int32_t *prev_chg_status)
{
	int err;
	union nrf_fuel_gauge_ext_state_info_data ext_data;

	if (chg_status == *prev_chg_status) {
		return;
	}

	*prev_chg_status = chg_status;

	if (chg_status & NPM1300_CHG_STATUS_COMPLETE_MASK) {
		ext_data.charge_state = NRF_FUEL_GAUGE_CHARGE_STATE_COMPLETE;
	} else if (chg_status & NPM1300_CHG_STATUS_TC_MASK) {
		ext_data.charge_state = NRF_FUEL_GAUGE_CHARGE_STATE_TRICKLE;
	} else if (chg_status & NPM1300_CHG_STATUS_CC_MASK) {
		ext_data.charge_state = NRF_FUEL_GAUGE_CHARGE_STATE_CC;
	} else if (chg_status & NPM1300_CHG_STATUS_CV_MASK) {
		ext_data.charge_state = NRF_FUEL_GAUGE_CHARGE_STATE_CV;
	} else {
		ext_data.charge_state = NRF_FUEL_GAUGE_CHARGE_STATE_IDLE;
	}

	err = nrf_fuel_gauge_ext_state_update(NRF_FUEL_GAUGE_EXT_STATE_INFO_CHARGE_STATE_CHANGE,
					      &ext_data);
	if (err) {
		LOG_ERR("nrf_fuel_gauge_ext_state_update, error: %d", err);
	}
}

/* Read the charger, feed the fuel gauge and cache the latest battery values. */
static int sample_and_process(struct s_object *state_object)
{
	int err;
	static int32_t prev_chg_status = -1;
	int32_t chg_status;
	bool vbus_connected;
	float delta;
#if defined(CONFIG_MEMFAULT_NRF_PLATFORM_BATTERY_NPM13XX)
	sMfltPlatformBatterySoc soc;
#endif /* CONFIG_MEMFAULT_NRF_PLATFORM_BATTERY_NPM13XX */

	err = charger_read_sensors(&state_object->voltage, &state_object->current,
				   &state_object->temperature, &chg_status, &vbus_connected);
	if (err) {
		LOG_ERR("charger_read_sensors, error: %d", err);
		return err;
	}

	state_object->charging = (chg_status & NPM1300_CHG_ACTIVE_MASK) != 0;

	/* Inform fuel gauge of VBUS state. */
	err = nrf_fuel_gauge_ext_state_update(
		vbus_connected ? NRF_FUEL_GAUGE_EXT_STATE_INFO_VBUS_CONNECTED
			       : NRF_FUEL_GAUGE_EXT_STATE_INFO_VBUS_DISCONNECTED,
		NULL);
	if (err) {
		LOG_ERR("nrf_fuel_gauge_ext_state_update, error: %d", err);
	}

	/* Inform fuel gauge of charge state changes. */
	update_charge_state_if_changed(chg_status, &prev_chg_status);

	delta = (float)k_uptime_delta(&state_object->fuel_gauge_ref_time) / 1000.f;

	err = nrf_fuel_gauge_process(state_object->voltage, state_object->current,
				     state_object->temperature, delta,
				     &state_object->percentage, NULL);
	if (err) {
		LOG_ERR("nrf_fuel_gauge_process, error: %d", err);
		return err;
	}

#if defined(CONFIG_MEMFAULT_NRF_PLATFORM_BATTERY_NPM13XX)
	err = memfault_platform_get_stateofcharge(&soc);
	if (err) {
		LOG_ERR("memfault_platform_get_stateofcharge, error: %d", err);
		return err;
	}

	state_object->charging = !soc.discharging;
#endif /* CONFIG_MEMFAULT_NRF_PLATFORM_BATTERY_NPM13XX */

	return 0;
}

/* Encode the cached battery values and publish them on the payload channel. */
static void publish_battery_payload(struct s_object *state_object)
{
	int err;
	struct bat_object bat_object = { 0 };
	struct payload payload = { 0 };
	int64_t system_time;

	err = date_time_now(&system_time);
	if (err) {
		LOG_ERR("Failed to convert uptime to unix time, error: %d", err);
		return;
	}

	LOG_DBG("State of charge: %f", (double)roundf(state_object->percentage));
	LOG_DBG("The battery is %s", state_object->charging ? "charging" : "not charging");

	bat_object.state_of_charge_m.bt = (int32_t)(system_time / 1000);
	bat_object.state_of_charge_m.vi = (int32_t)(state_object->percentage + 0.5f);
	bat_object.voltage_m.vf = state_object->voltage;
	bat_object.temperature_m.vf = state_object->temperature;

	err = cbor_encode_bat_object(payload.buffer, sizeof(payload.buffer),
				     &bat_object, &payload.buffer_len);
	if (err) {
		LOG_ERR("Failed to encode bat object, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	err = zbus_chan_pub(&PAYLOAD_CHAN, &payload, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
}

static void timer_sample_start(uint32_t delay_ms)
{
	int err;

	err = k_work_reschedule(&sample_timer_work, K_MSEC(delay_ms));
	if (err < 0) {
		LOG_ERR("k_work_reschedule, error: %d", err);
		SEND_FATAL_ERROR();
	}
}

static void timer_sample_stop(void)
{
	int err;

	err = k_work_cancel_delayable(&sample_timer_work);
	if (err < 0) {
		LOG_ERR("k_work_cancel_delayable, error: %d", err);
	}
}

static void sample_timer_work_fn(struct k_work *work)
{
	int err;
	const struct priv_battery_msg msg = { .type = BATTERY_PRIV_SAMPLE_TIMER_EXPIRED };

	ARG_UNUSED(work);

	err = zbus_chan_pub(&PRIV_BATTERY_CHAN, &msg, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}
}

static void lte_lc_evt_handler(const struct lte_lc_evt *const evt)
{
	int err;
	struct priv_battery_msg msg;

	switch (evt->type) {
	case LTE_LC_EVT_MODEM_SLEEP_ENTER:
		msg.type = BATTERY_PRIV_MODEM_SLEEP_ENTRY;
		break;
	case LTE_LC_EVT_MODEM_SLEEP_EXIT:
		msg.type = BATTERY_PRIV_MODEM_SLEEP_EXIT;
		break;
	default:
		return;
	}

	err = zbus_chan_pub(&PRIV_BATTERY_CHAN, &msg, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}
}

/* State handlers */

static void state_init_entry(void *o)
{
	int err;
	struct sensor_value value;
	struct nrf_fuel_gauge_init_parameters parameters = {
		.model = &battery_model
	};
	int32_t chg_status;
	struct s_object *state_object = o;

	if (!device_is_ready(charger)) {
		LOG_ERR("Charger device not ready.");
		SEND_FATAL_ERROR();
		return;
	}

	err = charger_read_sensors(&parameters.v0, &parameters.i0, &parameters.t0, &chg_status,
				   NULL);
	if (err < 0) {
		LOG_ERR("charger_read_sensors, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	err = nrf_fuel_gauge_init(&parameters, NULL);
	if (err) {
		LOG_ERR("nrf_fuel_gauge_init, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	state_object->fuel_gauge_ref_time = k_uptime_get();

	/* Seed the cached battery values from the initial measurement. */
	state_object->voltage = parameters.v0;
	state_object->current = parameters.i0;
	state_object->temperature = parameters.t0;

	err = sensor_channel_get(charger, SENSOR_CHAN_GAUGE_DESIRED_CHARGING_CURRENT, &value);
	if (err) {
		LOG_ERR("sensor_channel_get(DESIRED_CHARGING_CURRENT), error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
}

static enum smf_state_result state_init_run(void *o)
{
	struct s_object *state_object = o;

	if (&TIME_CHAN == state_object->chan) {
		enum time_status time_status = MSG_TO_TIME_STATUS(state_object->msg_buf);

		if (time_status == TIME_AVAILABLE) {
			LOG_DBG("Time available, sampling can start");

			STATE_SET(STATE_SAMPLING);
			return SMF_EVENT_HANDLED;
		}
	}

	return SMF_EVENT_PROPAGATE;
}

static void state_sampling_entry(void *o)
{
	ARG_UNUSED(o);

	LOG_DBG("%s", __func__);

	/* Register for modem sleep notifications to drive the active/idle substates. */
	lte_lc_register_handler(lte_lc_evt_handler);
}

static void state_active_entry(void *o)
{
	int err;
	struct s_object *state_object = o;

	LOG_DBG("%s", __func__);


	err = sample_and_process(state_object);
	if (err) {
		LOG_ERR("sample_and_process, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
	/* Start periodic fuel gauge sampling. */
	timer_sample_start(CONFIG_APP_BATTERY_SAMPLE_INTERVAL_MS);
}

static enum smf_state_result state_active_run(void *o)
{
	int err;
	struct s_object *state_object = o;

	if (&PRIV_BATTERY_CHAN == state_object->chan) {
		const struct priv_battery_msg *msg =
			(const struct priv_battery_msg *)state_object->msg_buf;

		if (msg->type == BATTERY_PRIV_SAMPLE_TIMER_EXPIRED) {
			err = sample_and_process(state_object);
			if (err) {
				LOG_ERR("sample_and_process, error: %d", err);
				SEND_FATAL_ERROR();
				return SMF_EVENT_HANDLED;
			}
			timer_sample_start(CONFIG_APP_BATTERY_SAMPLE_INTERVAL_MS);

			return SMF_EVENT_HANDLED;
		}

		if (msg->type == BATTERY_PRIV_MODEM_SLEEP_ENTRY) {
			STATE_SET(STATE_IDLE);

			return SMF_EVENT_HANDLED;
		}

		if (msg->type == BATTERY_PRIV_MODEM_SLEEP_EXIT) {
			return SMF_EVENT_HANDLED;
		}
	}

	if (&TRIGGER_CHAN == state_object->chan) {
		enum trigger_type trigger_type = MSG_TO_TRIGGER_TYPE(state_object->msg_buf);

		if (trigger_type == TRIGGER_DATA_SAMPLE) {
			LOG_DBG("Data sample trigger received, getting battery data");

			err = sample_and_process(state_object);
			if (err) {
				LOG_ERR("sample_and_process, error: %d", err);
				SEND_FATAL_ERROR();
				return SMF_EVENT_HANDLED;
			}
			publish_battery_payload(state_object);

			return SMF_EVENT_HANDLED;
		}
	}

	return SMF_EVENT_PROPAGATE;
}

static void state_active_exit(void *o)
{
	ARG_UNUSED(o);

	LOG_DBG("%s", __func__);

	/* Stop periodic fuel gauge sampling. */
	timer_sample_stop();
}

static void state_idle_entry(void *o)
{
	int err;
	struct s_object *state_object = o;
	float idle_current = (float)CONFIG_APP_BATTERY_IDLE_CURRENT_NA / 1e9f;

	LOG_DBG("%s", __func__);

	err = sample_and_process(state_object);
	if (err) {
		LOG_ERR("sample_and_process, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
	/* Inform the fuel gauge of the expected idle current for accurate SoC during sleep. */
	err = nrf_fuel_gauge_idle_set(state_object->voltage, state_object->temperature,
				      idle_current);
	if (err) {
		LOG_ERR("nrf_fuel_gauge_idle_set, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
}

static enum smf_state_result state_idle_run(void *o)
{
	int err;
	struct s_object *state_object = o;

	if (&PRIV_BATTERY_CHAN == state_object->chan) {
		const struct priv_battery_msg *msg =
			(const struct priv_battery_msg *)state_object->msg_buf;

		if (msg->type == BATTERY_PRIV_MODEM_SLEEP_EXIT) {
			STATE_SET(STATE_ACTIVE);

			return SMF_EVENT_HANDLED;
		}

		if (msg->type == BATTERY_PRIV_MODEM_SLEEP_ENTRY) {
			return SMF_EVENT_HANDLED;
		}
	}

	if (&TRIGGER_CHAN == state_object->chan) {
		enum trigger_type trigger_type = MSG_TO_TRIGGER_TYPE(state_object->msg_buf);

		/* A data sample trigger indicates device activity (e.g. location search) that
		 * should be included in the battery estimate, so transition to the active state.
		 */
		if (trigger_type == TRIGGER_DATA_SAMPLE) {
			LOG_DBG("Data sample trigger received, getting battery data");

			err = sample_and_process(state_object);
			if (err) {
				LOG_ERR("sample_and_process, error: %d", err);
				SEND_FATAL_ERROR();
				return SMF_EVENT_HANDLED;
			}
			publish_battery_payload(state_object);
			STATE_SET(STATE_ACTIVE);

			return SMF_EVENT_HANDLED;
		}
	}

	return SMF_EVENT_PROPAGATE;
}

/* End of state handling */

static void task_wdt_callback(int channel_id, void *user_data)
{
	LOG_ERR("Watchdog expired, Channel: %d, Thread: %s",
		channel_id, k_thread_name_get((k_tid_t)user_data));

	SEND_FATAL_ERROR_WATCHDOG_TIMEOUT();
}

static void battery_task(void)
{
	int err;
	int task_wdt_id;
	const uint32_t wdt_timeout_ms = (CONFIG_APP_BATTERY_WATCHDOG_TIMEOUT_SECONDS * MSEC_PER_SEC);
	const uint32_t execution_time_ms = (CONFIG_APP_BATTERY_EXEC_TIME_SECONDS_MAX * MSEC_PER_SEC);
	const k_timeout_t zbus_wait_ms = K_MSEC(wdt_timeout_ms - execution_time_ms);

	LOG_DBG("Battery module task started");

	task_wdt_id = task_wdt_add(wdt_timeout_ms, task_wdt_callback, (void *)k_current_get());

	STATE_SET_INITIAL(STATE_INIT);

	while (true) {
		err = task_wdt_feed(task_wdt_id);
		if (err) {
			LOG_ERR("task_wdt_feed, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}

		err = zbus_sub_wait_msg(&battery, &s_obj.chan, s_obj.msg_buf, zbus_wait_ms);
		if (err == -ENOMSG) {
			continue;
		} else if (err) {
			LOG_ERR("zbus_sub_wait_msg, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}

		err = STATE_RUN();
		if (err) {
			LOG_ERR("handle_message, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}
	}
}

K_THREAD_DEFINE(battery_task_id,
		CONFIG_APP_BATTERY_THREAD_STACK_SIZE,
		battery_task, NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);
