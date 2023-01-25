#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/byteorder.h>

#include "led.h"

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)
#define BT_UUID_HTS_INTERMEDIATE BT_UUID_DECLARE_16(0x2a1e)

static uint8_t data[5];
static struct bt_gatt_indicate_params ind_params;
BT_GATT_SERVICE_DEFINE(
    hts_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_HTS),
    // Notification for Intermediate temperature
    BT_GATT_CHARACTERISTIC(BT_UUID_HTS_INTERMEDIATE, BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ, NULL, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    // Indicate for temperature measurement
    BT_GATT_CHARACTERISTIC(BT_UUID_HTS_MEASUREMENT, BT_GATT_CHRC_INDICATE,
                           BT_GATT_PERM_READ, NULL, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

/**
 * Data to advertise in the advertisement packets
 */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL),
    BT_DATA_BYTES(
        BT_DATA_UUID16_ALL,
        BT_UUID_16_ENCODE(
            BT_UUID_HTS_VAL)), // Advertising Health Termometer Service
};

/* Data to scan in response packets */
static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME,
            DEVICE_NAME_LEN), // Scanning name of device
};

/**
 * Initializes the bluetoothe device and starts advertisement
 */
static void bt_ready(int err) {
  char addr_s[BT_ADDR_LE_STR_LEN];
  bt_addr_le_t addr = {0};
  size_t count = 1;

  if (err) {
    printk("Bluetooth init failed (err %d)\n", err);
    return;
  }

  printk("Bluetooth initialized\n");

  /* Start advertising */
  err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
  if (err) {
    printk("Advertising failed to start (err %d)\n", err);
    return;
  }

  bt_id_get(&addr, &count);
  bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));

  printk("Beacon started, advertising as %s\n", addr_s);
}

/**
 * Callback called when a device is connected
 * Changes the light of the led to green
 *
 * @param conn Connection that was estabilished
 * @param err 0 if there was no error other value otherwise
 */
static void connected(struct bt_conn *conn, uint8_t err) {
  if (err) {
    printk("Error connecting (err %d)\n", err);
  }

  printk("Connected\n");
  green();
}

/**
 * Callback called when a device is disconnected
 * Changes the light of the led to red
 *
 * @param conn Connection that was removed
 * @param reason Reson for disconnecting
 */
static void disconnected(struct bt_conn *conn, uint8_t reason) {
  printk("Disconnected with reason %d\n", reason);
  red();
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

/**
 * Updates the data array based on the available temperature from the sensor
*/
void update_data() {
  uint32_t mantissa;
  uint8_t exponent;
  extern double temperature;

  mantissa = (uint32_t)(temperature * 100);
  exponent = (uint8_t)-2;

  data[0] = 0; /* temperature in celsius */
  sys_put_le24(mantissa, (uint8_t *)&data[1]);
  data[4] = exponent;
}

/**
 * Send a notification with the current temperature
 */
void notify_temperature() {
  update_data();
  bt_gatt_notify(NULL, &hts_svc.attrs[1], data, sizeof(data));
  printk("Notified temperature.\n");
}

/**
 * Indicates the current temperature
 */
void indicate_temperature() {
  update_data();
  ind_params.attr = &hts_svc.attrs[4];
  ind_params.data = &data;
  ind_params.len = sizeof(data);
  bt_gatt_indicate(NULL, &ind_params);
  printk("Indicated temperature.\n");
}

int init_bluetooth() {
  // Initialize the bluetooth device
  int err = bt_enable(bt_ready);
  if (err) {
    printk("Bluetooth init failed (err %d)\n", err);
    return -1;
  }

  // Register the callback functions for bluetooth
  bt_conn_cb_register(&conn_callbacks);
  return 0;
}