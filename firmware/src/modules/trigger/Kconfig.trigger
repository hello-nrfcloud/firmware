#
# Copyright (c) 2023 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

menu "Trigger"

config APP_TRIGGER_THREAD_STACK_SIZE
	int "Thread stack size"
	default 2048

config APP_TRIGGER_TIMEOUT_SECONDS
	int "Trigger timer timeout"
	default 300

module = APP_TRIGGER
module-str = Trigger
source "subsys/logging/Kconfig.template.log_config"

endmenu # Trigger
