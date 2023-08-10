#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>

int nordic_pwm_logger_init(const struct device *dev);

int nordic_pwm_logger_set_cycles(const struct device *dev, uint32_t channel,
				 uint32_t period_cycles, uint32_t pulse_cycles,
				 pwm_flags_t flags);

int nordic_pwm_logger_get_cycles_per_sec(const struct device *dev, uint32_t channel,
					 uint64_t *cycles);
