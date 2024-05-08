# Notes from inside the machine room

There is no real spec on what each part of the application does,
but we shall note down the intentions we had when building them
so we can create tests to check if we fullfil those intentions.

## App Module

This module manages the settings of the application.
It initializes the `simple_config` library, waits until it gets a cloud connection and then runs `simple_config_update` to tell the cloud the current config and fetch updates. After this initial part, it waits for trigger events to run `simple_config_update` again. It shall only call this function if there is an active cloud connection.

This is implemented with it's own thread since there is initialization to do.
To react to events, a simple subscriber is used. There might be value in changing this to a listener for less memory usage.

## Error Module

This tiny module has been copied verbatim from the MQTT sample. It uses a listener to react to error events. Depending on a config, the application is restarted on fatal errors.

## FOTA Module

This module shall react to trigger events to check for available FOTA updates.
Only check for updates if we are connected to cloud.
After successful initialization, mark the current image as confirmed. (Idea: This means we should be able to do another FOTA, which is the main thing to confirm.)
A listener is used here to stop other activity while processing the trigger.

This publishes a bool on the FOTA_ONGOING_CHAN to tell the rest of the app not to disturb the fota that's in progress. TODO: what about mfw and bootloader fota? Does that work?

## LED Module

This module listens to the LED_CHAN and applies the led state to LED1 on the board. Currently, the format is a simple int, controlling the red led.

## Location Module

This module shall react to trigger events to process location requests.
It has its own thread because location requests take a while to process.
Note: if we intend to use GNSS tracking, we need data transmission to shut up for a while.
A listener is used to trigger internal semaphores. TODO: this could also be done with a subscriber.

## Network module

This module tries to keep the device online and informs the rest of the application about the current network status on ZBUS. It offloads the heavy lifting to connection manager and forwards net_mgmt events.

## Sampler module

This is not fully specified nor implemented yet. Currently, this just sends a raw-bytes message on trigger.

This module is intended to collect data and send it to cloud.
The data collected would probably be environmental data or motion data.

## Trigger module

This module is intended to trigger active tasks like location and polling sensors. It springs into action periodically and on button press.

## Environmental module

This module interfaces with the IAQ library to collect and forward environmental data.
