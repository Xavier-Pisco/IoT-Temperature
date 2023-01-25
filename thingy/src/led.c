#include <zephyr/drivers/gpio.h>

#define LED0_NODE DT_NODELABEL(led0)
#define LED1_NODE DT_NODELABEL(led1)

static const struct gpio_dt_spec redLed = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec greenLed = GPIO_DT_SPEC_GET(LED1_NODE, gpios);

void green() {
  gpio_pin_set_dt(&greenLed, 1);
  gpio_pin_set_dt(&redLed, 0);
}

void red() {
  gpio_pin_set_dt(&greenLed, 0);
  gpio_pin_set_dt(&redLed, 1);
}

int init_leds() {
  if (!device_is_ready(redLed.port) || !device_is_ready(greenLed.port)) {
    return -1;
  }
  if (gpio_pin_configure_dt(&redLed, GPIO_OUTPUT_INACTIVE) < 0 ||
      gpio_pin_configure_dt(&greenLed, GPIO_OUTPUT_INACTIVE) < 0) {
    return -1;
  }
  return 0;
}