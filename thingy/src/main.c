// Common includes
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/types.h>

#include "bt.h"
#include "led.h"
#include "sensor.h"

void main(void) {
  if (init_leds()) {
    printk("Couldn't initialize leds. Continuing...\n");
  }

  if (init_sensor()) {
    printk("Couldn't initialize temperature sensor. Exiting.\n");
    return;
  }

  if (init_bluetooth()) {
    printk("Couldn't initialize bluetooth. Exiting.\n");
    return;
  }
  red();

  while (1) {
    notify_temperature();

    k_sleep(K_SECONDS(1));
  }
}