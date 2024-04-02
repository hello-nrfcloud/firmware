# nRF Cloud CoAP Simple Config Library

This library is intended to make it easy to handle device runtime configuration using device shadow.
It's primarily geared towards nRF Cloud CoAP.

## What does it do?

With nRF Cloud CoAP you'll need to poll the cloud manually to check if there are config updates
or scheduled FOTA jobs for the device. Since it makes sense to bundle network operations, this library
needs to be called with `simple_config_update()`. Do this along with the check for FOTA jobs.

There needs to be a way to read and write config items. These are represented as string keys and values that can be strings, integers, booleans or floats.
To make this happen without being too opinionated about config handling, we provide two functions:
### `simple_config_set(key, value)`
Write a config entry: This is to inform the cloud about the latest config state the app knows.
These are collected until `simple_config_update()` is called the next time.

Whenever a config change comes from the device side, this needs to be set.
This could be when stored settings are reset due to flashing or updating.
Likewise settings could change due to local user input if your device supports that.

Recommendation: on boot, go through all your settings to report the current config.
This will make sure you are synchronized with the cloud.

### `simple_config_set_callback(callback)`
Register a callback to be notified about settings changes.
This will only be called for config items that are different in the "desired" and "reported" objects in the device shadow.
Settings may be rejected in the callback using the return value.

## Implementation ideas

* config objects are currently handled as cJSON objects, making heap usage necessary
* this library tries to keep as little internal state as possible
* a cJSON object is used to collect all pending config updates
