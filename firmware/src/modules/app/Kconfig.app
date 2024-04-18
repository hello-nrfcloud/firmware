module = APP
module-str = APP
source "subsys/logging/Kconfig.template.log_config"

menu "App"

config APP_MODULE_THREAD_STACK_SIZE
	int "Thread stack size"
	default 4096

config APP_MODULE_MESSAGE_QUEUE_SIZE
	int "Message queue size"
	default 5
	help
	  ZBus subscriber message queue size.

endmenu
