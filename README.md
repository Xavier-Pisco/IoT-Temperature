# IoT - Group 3

- Philipp Auer ()
- Rafael Cristino ()
- Xavier Pisco (e12206635)

## Code structure

- **esp32** - Folder with the code to execute on the ESP32
- **thingy** - Folder with the code to execute on the Thingys
- **mosquitto.conf** - File with the configuration for the Mosquitto broker.
- **README.md** - This file

## Dependencies

- [Mosquitto](https://mosquitto.org)
- [Arduino IDE](https://www.arduino.cc/en/software/) with the  following libraries:
    - ArduinoHTTPClient
    - ArduinoJSON
    - PubSubClient
    - ThingsBoard
- [ThingsBoard](https://thingsboard.io/)

## Setup

On the ThingsBoard you should:
- Create a device which is a gateway

Before uploading the code to the ESP32 you should do these changes in the  `esp32/esp32.ino` file:
- Change the `TOKEN` variable to the access token of the device
- Change the `SERVER` variable to the IP of the computer running Mosquitto
- Change the `WIFI_AP_NAME` and `WIFI_PASSWORD` to the WIFI credentials to which the ESP32 should connect

On the mosquitto.conf you should:
- Change the `address` to the ThingsBoard address
- Change the `remote_username` to the device access token

## Running

1. Build and upload the thingy code into the 3 Thingys
1. Build and upload the esp32 code into the ESP32
1. The 3 different temperatures should appear on the `Latest Telemetry` tab in the ThingsBoard device

## Custom dasboard

We created a dashboard which shows a graph with the 3 temperatures over time.

## Request temperature

You can request the temperatures via the command line by executing the following command:
```
mosquitto_pub -h <mosquitto_ip> -t "v1/devices/me/rpc/request/test" -m "{\"method\":\"test\", \"params\":{}}"
```

## Observed problems

During the development we had several issues, including:
- Complications configuring mosquitto to work as expected
- ThingsBoard button to request temperature doesn't send a message to mosquitto

