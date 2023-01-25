#ifndef bt_h
#define bt_h

/**
 * Sends a notification via bluetooth with the temperature
 */
void notify_temperature();
/**
 * Indicates the temperature via bluetooth
 */
void indicate_temperature();
/**
 * Initialize bluetooth device and wait for connections
 */
int init_bluetooth();

#endif