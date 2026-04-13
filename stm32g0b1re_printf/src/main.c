#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#define CONSOLE_UART_NODE DT_CHOSEN(zephyr_console)
#define SPARE_UART_NODE DT_ALIAS(spare_uart)
#define SPARE_UART_GPIO_PORT DT_NODELABEL(gpioc)
#define SPARE_UART_TX_PIN 4
#define SPARE_UART_RX_PIN 5
#define ALTERNATE_COMMAND "Alternate"

#if !DT_NODE_HAS_STATUS(CONSOLE_UART_NODE, okay)
#error "zephyr,console chosen UART is not enabled"
#endif

#if !DT_NODE_HAS_STATUS(SPARE_UART_NODE, okay)
#error "spare-uart alias is not enabled in the board overlay"
#endif

PINCTRL_DT_DEV_CONFIG_DECLARE(SPARE_UART_NODE);

static const struct device *const console_uart = DEVICE_DT_GET(CONSOLE_UART_NODE);
static const struct device *const spare_uart = DEVICE_DT_GET(SPARE_UART_NODE);
static const struct device *const spare_uart_gpio = DEVICE_DT_GET(SPARE_UART_GPIO_PORT);
static const struct pinctrl_dev_config *const spare_uart_pcfg =
	PINCTRL_DT_DEV_CONFIG_GET(SPARE_UART_NODE);

static void spare_uart_write(const char *str)
{
	while (*str != '\0') {
		uart_poll_out(spare_uart, *str++);
	}
}

static void spare_uart_drain_rx(void)
{
	unsigned char rx;

	while (uart_poll_in(spare_uart, &rx) == 0) {
	}
}

static int spare_uart_pins_to_gpio_input(void)
{
	int ret;

	ret = gpio_pin_configure(spare_uart_gpio, SPARE_UART_TX_PIN,
				 GPIO_INPUT | GPIO_PULL_UP);
	if (ret != 0) {
		return ret;
	}

	return gpio_pin_configure(spare_uart_gpio, SPARE_UART_RX_PIN,
				  GPIO_INPUT | GPIO_PULL_UP);
}

static int spare_uart_pins_to_uart(void)
{
	return pinctrl_apply_state(spare_uart_pcfg, PINCTRL_STATE_DEFAULT);
}

static void spare_uart_gpio_input_example(unsigned int hold_seconds, const char *reason)
{
	int ret;

	printf("%s: USART1 TX/RX pins become GPIO inputs for %u seconds\r\n",
	       reason, hold_seconds);
	spare_uart_write("\r\nUSART1 pins become GPIO inputs temporarily.\r\n");
	k_sleep(K_MSEC(10));
	spare_uart_drain_rx();

	ret = spare_uart_pins_to_gpio_input();
	if (ret != 0) {
		printf("Failed to switch USART1 pins to GPIO input: %d\r\n", ret);
		return;
	}

	printf("USART1 TX/RX pins are GPIO inputs for %u seconds\r\n", hold_seconds);
	printf("PC4 level=%d, PC5 level=%d\r\n",
	       gpio_pin_get(spare_uart_gpio, SPARE_UART_TX_PIN),
	       gpio_pin_get(spare_uart_gpio, SPARE_UART_RX_PIN));

	k_sleep(K_SECONDS(hold_seconds));

	ret = spare_uart_pins_to_uart();
	if (ret != 0) {
		printf("Failed to restore USART1 pinctrl: %d\r\n", ret);
		return;
	}

	spare_uart_drain_rx();
	printf("USART1 TX/RX pins restored to UART\r\n");
	spare_uart_write("USART1 pins restored to UART mode.\r\n");
}

static void alternate_pin_mode_example(void)
{
	spare_uart_gpio_input_example(1, "Alternate detected on spare UART");
}

static bool alternate_command_received(unsigned char rx)
{
	static size_t match_pos;
	const char command[] = ALTERNATE_COMMAND;

	if (rx == command[match_pos]) {
		match_pos++;
		if (match_pos == strlen(command)) {
			match_pos = 0;
			return true;
		}
	} else {
		match_pos = (rx == command[0]) ? 1U : 0U;
	}

	return false;
}

static void handle_console_input(void)
{
	unsigned char rx;
	bool enter_pressed = false;

	while (uart_poll_in(console_uart, &rx) == 0) {
		if ((rx == '\r') || (rx == '\n')) {
			enter_pressed = true;
		}
	}

	if (enter_pressed) {
		spare_uart_gpio_input_example(10, "Enter detected on console");
	}
}

int main(void)
{
	int counter = 0;

	printf("STM32G0B1RE Zephyr printf example\r\n");
	printf("Board: %s\r\n", CONFIG_BOARD_TARGET);

	if (!device_is_ready(console_uart)) {
		printf("Console UART is not ready\r\n");
		return 0;
	}

	if (!device_is_ready(spare_uart)) {
		printf("Spare UART is not ready\r\n");
		return 0;
	}

	if (!device_is_ready(spare_uart_gpio)) {
		printf("GPIO port for spare UART pins is not ready\r\n");
		return 0;
	}

	printf("Spare UART: %s, 115200 8N1\r\n", spare_uart->name);
	printf("Pins: USART1 TX=PC4(D1), RX=PC5(D0)\r\n");
	spare_uart_drain_rx();
	spare_uart_write("Spare UART ready. Send 'Alternate' to test GPIO/UART switching.\r\n");

	while (1) {
		unsigned char rx;
		char tx_msg[64];

		printf("printf loop count: %d\r\n", counter++);
		handle_console_input();

		snprintf(tx_msg, sizeof(tx_msg), "spare uart loop count: %d\r\n", counter);
		spare_uart_write(tx_msg);

		while (uart_poll_in(spare_uart, &rx) == 0) {
			printf("Spare UART RX: 0x%02x '%c'\r\n",
			       (unsigned int)rx, (rx >= 32U && rx <= 126U) ? rx : '.');
			uart_poll_out(spare_uart, rx);

			if (alternate_command_received(rx)) {
				alternate_pin_mode_example();
			}
		}

		k_sleep(K_SECONDS(1));
	}

	return 0;
}
