module = APP
module-str = APP
source "subsys/logging/Kconfig.template.log_config"

menu "App"

config APP_MODULE_THREAD_STACK_SIZE
	int "Thread stack size"
	default 3072

config APP_MODULE_WATCHDOG_TIMEOUT_SECONDS
	int "Watchdog timeout seconds"
	default 1800

config APP_MODULE_EXEC_TIME_SECONDS_MAX
	int "Maximum execution time seconds"
	default 60
	help
	  Maximum time allowed for a single execution of the module thread loop.

config APP_MODULE_RECV_BUFFER_SIZE
	int "Receive buffer size"
	default 512

endmenu
