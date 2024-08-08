#
# Copyright (c) 2023 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

menu "Trigger"

config APP_TRIGGER_TIMEOUT_SECONDS
	int "Trigger timer timeout"
	default 3600

config APP_TRIGGER_FOTA_POLL_INTERVAL_SEC
	int "FOTA poll interval seconds"
	default 3600
	help
	  Duration in between polling for FOTA updates.

config FREQUENT_POLL_DURATION_INTERVAL_SEC
	int "Poll mode duration"
	default 600

module = APP_TRIGGER
module-str = Trigger
source "subsys/logging/Kconfig.template.log_config"

endmenu # Trigger
