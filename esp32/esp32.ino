#include <WiFi.h>
#include <ThingsBoard.h>
#include <string.h>
#include "BLEDevice.h"

/*******
BASED ON 
- https://thingsboard.io/docs/samples/esp32/gpio-control-pico-kit-dht22-sensor/
- https://www.hackster.io/magicbit0/connect-your-magicbit-esp32-to-thingsboard-a48181
*******/

#define DEVICE_AMOUNT 3

// WiFi access point
#define WIFI_AP_NAME "iot"// "WIFI_AP"
// WiFi password
#define WIFI_PASSWORD "123456789"// "WIFI_PASSWORD"

// ThingsBoard token
#define TOKEN "iarvR1fs1kKiT9pEmzmE" // "TOKEN"
// ThingsBoard server instance.
#define SERVER "192.168.12.1"

// Baud rate for debug serial
#define SERIAL_DEBUG_BAUD    115200

// The remote service we wish to connect to.
static BLEUUID serviceUUID("00001809-0000-1000-8000-00805f9b34fb");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("00002a1e-0000-1000-8000-00805f9b34fb");

// Initialize ThingsBoard client
WiFiClient esp_client;
// Initialize ThingsBoard instance
ThingsBoard tb(esp_client);
// the Wifi radio's status
int status = WL_IDLE_STATUS;

// Whether to connect to BLE devices
static boolean do_connect = false;
// Count of found devices
static int count_devices = 0;

// MAC addresses of our thingys
static const char * mac_addresses[] = {
  "f7:ac:1d:4a:7b:b7",
  "ca:07:3f:2c:1c:10",
  "f0:ae:26:c8:65:3b"  
};

// The device representation of the thingys
static BLEAdvertisedDevice * ble_devices[] = {
  NULL, NULL, NULL
};

// Last temperatures values
static float temperatures[] = { 0., 0., 0. };


void init_wifi()
{
  Serial.println("Connecting to AP ...");
  WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to AP");
}


void reconnect_wifi() {
  status = WiFi.status();
  if (status != WL_CONNECTED) {
    WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("Connected to AP");
  }
}


/**
* Decode tempereature coming from the thingys
*/
float decode_temperature_celsius(uint8_t * data) {
  uint32_t result = 0;
  for (int i = 3; i > 0; i--) {
    result = (result << 8) + data[i];
  }
  return result / 100.;
}


/*
 * Callback for characteristic notify
 */
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify,
  int id) {
    temperatures[id] = decode_temperature_celsius(pData);
    char device_name[40];
    sprintf(device_name, "group3_Dev%d", id);
    tb.sendTelemetryFloat(device_name, temperatures[id]);
}

static void notify_callback0(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  notifyCallback(pBLERemoteCharacteristic, pData, length, isNotify, 0);
}

static void notify_callback1(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  notifyCallback(pBLERemoteCharacteristic, pData, length, isNotify, 1);
}

static void notify_callback2(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  notifyCallback(pBLERemoteCharacteristic, pData, length, isNotify, 2);
}

typedef void (*notify_callback_type)(BLERemoteCharacteristic*, uint8_t*, size_t, bool);

static notify_callback_type notifyCallbacks[] = {
  notify_callback0, notify_callback1, notify_callback2
};


/**
 * Callback for BLE client
 */
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    Serial.printf("Disconnected BLE client %s\n", pclient->toString().c_str());
  }
};


/**
* Forms connection to BLE servers from thingys
*/
bool connect_ble_server(BLEAdvertisedDevice * myDevice, int clientId) {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());

    BLEClient * pClient = BLEDevice::createClient();

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to BLE server");
    pClient->setMTU(517); //set client to request maximum MTU from server (default is 23 otherwise)
  
    // Obtain a reference to the service we are after.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }

    // Obtain a reference to the characteristic we are after.
    BLERemoteCharacteristic * pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }

    if(pRemoteCharacteristic->canNotify())
      pRemoteCharacteristic->registerForNotify(notifyCallbacks[clientId]);

    return true;
}


/**
 * Scan for BLE servers and find out thingys.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {

 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    Serial.printf("Address: %s\n", advertisedDevice.getAddress().toString().c_str());

    for (int i = 0; i < DEVICE_AMOUNT; i++) {
      if (strcmp(mac_addresses[i], advertisedDevice.getAddress().toString().c_str()) == 0
          && advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
        ble_devices[i] = new BLEAdvertisedDevice(advertisedDevice);
        count_devices++;
        break;
      }
    }

    if (count_devices == DEVICE_AMOUNT) {
      Serial.println("Got all devices; stopping scan and starting connect.");      
      BLEDevice::getScan()->stop();
      do_connect = true;
    }
  }
};


/**
 * Callback for RPC
 */
RPC_Response requestAVGTemperature(const RPC_Data &data) {
    float response = (temperatures[0] + temperatures[1] + temperatures[2]) / 3.;
    printf("[RPC] Senging AVG temperature: %f\n", response);
    tb.sendTelemetryFloat("group3_AVGtemp", response);
    return RPC_Response("test", 1);
}

// ESP32 recognizes message from test but not from "readings" or "requestReadings" :/
const std::vector<RPC_Callback> RPCCallbacks = {
  RPC_Callback("test", requestAVGTemperature),
};


void setup() {
  // Initialize serial for debugging
  Serial.begin(SERIAL_DEBUG_BAUD);
  WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
  init_wifi();
  
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device. Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
}


void loop() {
  // Reconnect to WiFi, if needed
  if (WiFi.status() != WL_CONNECTED) {
    reconnect_wifi();
    return;
  }

  // Reconnect to ThingsBoard, if needed
  if (!tb.connected()) {
    Serial.printf("Connecting to thingsboard (%s) with token %s\n", SERVER, TOKEN);
    if (!tb.connect(SERVER, TOKEN)) {
      Serial.println("Failed to connect");
      return;
    }
    if (!tb.RPC_Subscribe(RPCCallbacks.cbegin(), RPCCallbacks.cend()))  {
      Serial.println("Failed to subscribe RPC.");
    }
  }

  // Process messages
  tb.loop();

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect. Now we connect to it. Once we are 
  // connected we set the connected flag to be false.
  if (do_connect == true) {
    for (int i = 0; i < DEVICE_AMOUNT; i++) {
      if (ble_devices[i] != NULL && connect_ble_server(ble_devices[i], i)) {
        Serial.printf("We are now connected to BLE Server %d.\n", i);
      } else {
        Serial.println("We have failed to connect to the server; there is nothin more we will do.");
      }
    }
    do_connect = false;
  }
  
  delay(1000); // Delay a second between loops.
}
