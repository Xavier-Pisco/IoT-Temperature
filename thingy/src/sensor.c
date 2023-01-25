#include "bt.h"
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>

static const struct device *temp_dev = DEVICE_DT_GET_ONE(st_hts221);
double temperature;

/**
 * Read the temperature from the sensor into a global variable
*/
static void get_temperature(const struct device *dev,
                            const struct sensor_trigger *trig) {
  struct sensor_value temp;
  if (sensor_sample_fetch(dev) < 0) {
    printk("Sensor sample update error\n");
    return;
  }

  if (sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp) < 0) {
    printk("Cannot read HTS221 temperature channel\n");
    return;
  }

  temperature = sensor_value_to_double(&temp);

  printk("Temperature is approximately %dC\n", (int)temperature);

  indicate_temperature();
}

/**
 * Initializes the hts221 sensor
*/
int init_sensor() {
  if (!device_is_ready(temp_dev)) {
    return -1;
  }

  // Activates sensor trigger when there is new data to read
  if (IS_ENABLED(CONFIG_HTS221_TRIGGER)) {
    struct sensor_trigger trig = {
        .type = SENSOR_TRIG_DATA_READY,
        .chan = SENSOR_CHAN_ALL,
    };
    // Add the callback function to the trigger
    if (sensor_trigger_set(temp_dev, &trig, get_temperature) < 0) {
      return -1;
    }
  }

  return 0;
}