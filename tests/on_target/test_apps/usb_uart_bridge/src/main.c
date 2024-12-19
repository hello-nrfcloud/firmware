#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/watchdog.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* defines for watchdog */
#define WDT_MAX_WINDOW  10000U
#define WDT_FEED_WORKER_DELAY_MS 2500U

#define MSG_SIZE 5120
#define LINE_LEN 64
#define LINE_COUNT 64

#define RECEIVE_TIMEOUT 1000

/* queue to store up to 8 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 8, 4);

static const struct device *uart0 = DEVICE_DT_GET(DT_NODELABEL(uart0));
static const struct device *uart1 = DEVICE_DT_GET(DT_NODELABEL(uart1));

struct wdt_data_storage {
	const struct device *wdt_drv;
	int wdt_channel_id;
	struct k_work_delayable system_workqueue_work;
};

static struct wdt_data_storage wdt_data = {
	.wdt_drv = DEVICE_DT_GET(DT_NODELABEL(wdt))
};



/* Define a semaphore for UART1 transmission synchronization */
K_SEM_DEFINE(uart1_tx_sem, 1, 1);  // Initial count = 1, max count = 1

/* RX buffer for UART0 DMA */
static uint8_t rx_dma_buf[MSG_SIZE];

/* Buffer used in callback to process incoming lines */
static char rx_line_buf[MSG_SIZE];
static size_t rx_line_buf_pos = 0;

/* UART Print function using async API for UART0 */
static void print_uart0_async(const struct device *uart, const char *str)
{
    size_t len = strlen(str);
    if (len == 0) {
        return;
    }
    uart_tx(uart, (const uint8_t *)str, len, SYS_FOREVER_MS);
}

/* UART Print function using async API for UART1 with semaphore */
static void print_uart1_async(const struct device *uart, const char *str)
{
    size_t len = strlen(str);
    if (len == 0) {
        return;
    }

    /* Acquire the semaphore before initiating transmission */
    if (k_sem_take(&uart1_tx_sem, K_FOREVER) == 0) {
        int ret = uart_tx(uart, (const uint8_t *)str, len, SYS_FOREVER_MS);
        if (ret < 0) {
            /* Transmission failed, release the semaphore */
            k_sem_give(&uart1_tx_sem);
        }
        /* If uart_tx is successful, the semaphore will be released in the callback */
    }
}

/* UART0 callback to handle TX completion */
static void uart0_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
	switch (evt->type) {
	case UART_RX_RDY:
        /* Data received. Copy it into rx_line_buf and look for line endings */
		const uint8_t *data = &evt->data.rx.buf[evt->data.rx.offset];
		size_t len = evt->data.rx.len;
		for (size_t i = 0; i < len; i++) {
			char c = (char)data[i];
			if ((c == '\n' || c == '\r') && rx_line_buf_pos > 0) {
				/* Terminate string */
				rx_line_buf[rx_line_buf_pos] = '\0';
				/* Put line into message queue (if full, dropped) */
				k_msgq_put(&uart_msgq, rx_line_buf, K_NO_WAIT);
				/* Reset buffer */
				rx_line_buf_pos = 0;
			} else if (rx_line_buf_pos < (sizeof(rx_line_buf) - 1)) {
				rx_line_buf[rx_line_buf_pos++] = c;
			}
			/* else: drop characters beyond buffer size */
		}
	    break;
	case UART_RX_DISABLED:
        uart_rx_enable(uart0, rx_dma_buf, sizeof(rx_dma_buf), RECEIVE_TIMEOUT);
		break;
	case UART_RX_STOPPED:
		/* RX stopped due to an error. Restart RX. */
        uart_rx_enable(dev, rx_dma_buf, sizeof(rx_dma_buf), RECEIVE_TIMEOUT);
		break;
	default:
		break;
	}
}

/* UART1 callback to handle TX completion */
static void uart1_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
    switch (evt->type) {
    case UART_TX_DONE:
    case UART_TX_ABORTED:
        /* Release the semaphore to allow the next transmission */
        k_sem_give(&uart1_tx_sem);
        break;
    default:
        break;
    }
}


/*
 * Generate a large predefined string to be checked by test
 */
static void generate_str(char *str, int line_len, int line_count)
{
	int count = 0;
	char c;
	int t;

	for (int i = 0; i < line_count; ++i) {
		for (int j = i; j < i + line_len-1; ++j) {
			t = j % line_len;
			c = '!' + t;
			if (c == '\"' || c == '\'' || c == '\\') {
				c = 'a';
			}
			*(str+count) = c;
			count++;
		}
		if (i < line_count-1) {
			*(str+count) = 'A';
			count++;
		}
	}
	*(str+count) = '\0';
}

static void init_watchdog(void)
{
	struct wdt_timeout_cfg wdt_config = {
		/* Reset SoC when watchdog timer expires. */
		.flags = WDT_FLAG_RESET_SOC,

		/* Expire watchdog after max window */
		.window.min = 0U,
		.window.max = WDT_MAX_WINDOW,
	};
	wdt_data.wdt_channel_id = wdt_install_timeout(wdt_data.wdt_drv, &wdt_config);
	wdt_setup(wdt_data.wdt_drv, WDT_OPT_PAUSE_HALTED_BY_DBG);
}

static void init_uart(void)
{
	/* Set the UART0 callback */
	uart_callback_set(uart0, uart0_cb, NULL);
	/* Set the UART1 callback */
	uart_callback_set(uart1, uart1_cb, NULL);

	/* Enable asynchronous RX (required by async API) */
    uart_rx_enable(uart0, rx_dma_buf, sizeof(rx_dma_buf), RECEIVE_TIMEOUT);

	/* Enable UART1 asynchronous RX (required by async API), though we don't use RX here.
	 * Just provide a dummy buffer. */
    static uint8_t dummy_rx_buf1[1];
    uart_rx_enable(uart1, dummy_rx_buf1, sizeof(dummy_rx_buf1), SYS_FOREVER_MS);

	/* Configure UART1 to 1000000 baud */
	struct uart_config cfg1;
	uart_config_get(uart1, &cfg1);
	cfg1.baudrate = 1000000;
	uart_configure(uart1, &cfg1);
}

/* Buffer to read from message queue */
static char buffer_to_read_msgq[MSG_SIZE];

/* Control String */
static char control_str[LINE_LEN*LINE_COUNT];

int main(void)
{
    int err;
    char hex_buf[5];

	/* Ensure UART0 is ready */
	if (!device_is_ready(uart0)) {
		return 0;
	}

	/* Ensure UART1 is ready */
	if (!device_is_ready(uart1)) {
		return 0;
	}

    init_uart();
	init_watchdog();



    /* Generate a control string */
    generate_str(control_str, LINE_LEN, LINE_COUNT);

	while (1) {
        err = k_msgq_get(&uart_msgq, buffer_to_read_msgq, K_MSEC(WDT_FEED_WORKER_DELAY_MS));
        if (!err) {
            wdt_feed(wdt_data.wdt_drv, wdt_data.wdt_channel_id);
            if (strstr(buffer_to_read_msgq, control_str)) {
				print_uart0_async(uart0, "Control string received from usb via bridge to nrf9160!\r\n");
			}
            /* CHECK_UART0_SMOKE */
            else if (strstr(buffer_to_read_msgq, "CHECK_UART0_SMOKE")) {
				print_uart0_async(uart0, "This message should be seen on UART0!\r\n");
            }
            /* CHECK_UART1_SMOKE */
            else if (strstr(buffer_to_read_msgq, "CHECK_UART1_SMOKE")) {
				print_uart1_async(uart1, "This message should be seen on UART1!\r\n");
            }
            else if (strstr(buffer_to_read_msgq, "CHECK_UART1_4k_TRACES")) {
				print_uart0_async(uart0, "4k of data sent over UART1\r\n");
				print_uart1_async(uart1, control_str);
			}
            else if (strstr(buffer_to_read_msgq, "CHECK_UART1_100k_TRACES")) {
				print_uart0_async(uart0, "100k of data sent over UART1\r\n");
				for (int i = 0; i < 25; ++i) {
					print_uart1_async(uart1, control_str);
				}
			}
            else if (strstr(buffer_to_read_msgq, "CHECK_UART1_400k_TRACES")) {
				print_uart0_async(uart0, "400k of data sent over UART1\r\n");
				for (int i = 0; i < 100; ++i) {
					print_uart1_async(uart1, control_str);
				}
			}
            else if (strstr(buffer_to_read_msgq, "CHECK_UART1_600k_TRACES")) {
				print_uart0_async(uart0, "600k of data sent over UART1\r\n");
				for (int i = 0; i < 150; ++i) {
					print_uart1_async(uart1, control_str);
				}
			}
            else {
				print_uart0_async(uart0, "Unexpected message received:\r\n");
				for (int i = 0; i < (int)strlen(buffer_to_read_msgq); ++i) {
					snprintk(hex_buf, sizeof(hex_buf), "%02X, ", buffer_to_read_msgq[i]);
					print_uart1_async(uart0, hex_buf);
				}
				print_uart0_async(uart0, "\r\n");
			}
        }
        else if (err == -EAGAIN) {
            wdt_feed(wdt_data.wdt_drv, wdt_data.wdt_channel_id);
			print_uart0_async(uart0, "UART0 running at baudrate 115200\r\n");
            print_uart1_async(uart1, "UART1 running at baudrate 1000000\r\n");
        }
        else {
			print_uart0_async(uart0, "Error receiving message from queue\r\n");
			return 0;
		}
		k_sleep(K_SECONDS(1));
	}
    return 0;
}
