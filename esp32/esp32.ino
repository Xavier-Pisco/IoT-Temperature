#include <WiFi.h>           // WiFi control for ESP32
#include <ThingsBoard.h>    // ThingsBoard SDK
#include <string.h>
#include "BLEDevice.h"
//#include "BLEScan.h"

// The remote service we wish to connect to.
static BLEUUID serviceUUID("00001809-0000-1000-8000-00805f9b34fb");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("00002a1e-0000-1000-8000-00805f9b34fb");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static int countDevices = 0;

/*******
BASED ON 
- https://thingsboard.io/docs/samples/esp32/gpio-control-pico-kit-dht22-sensor/
- https://www.hackster.io/magicbit0/connect-your-magicbit-esp32-to-thingsboard-a48181
*******/

// Helper macro to calculate array size
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

// WiFi access point
#define WIFI_AP_NAME       "iot"// "WIFI_AP"
// WiFi password
#define WIFI_PASSWORD      "123456789"// "WIFI_PASSWORD"

// See https://thingsboard.io/docs/getting-started-guides/helloworld/
// to understand how to obtain an access token
#define TOKEN              "iarvR1fs1kKiT9pEmzmE" // "TOKEN"
// ThingsBoard server instance.
#define THINGSBOARD_SERVER  "128.130.123.100"

// Baud rate for debug serial
#define SERIAL_DEBUG_BAUD    115200

// Initialize ThingsBoard client
WiFiClient espClient;
// Initialize ThingsBoard instance
ThingsBoard tb(espClient);
// the Wifi radio's status
int status = WL_IDLE_STATUS;


// Period of sending a temperature/humidity data.
int send_delay = 2000;
unsigned long millis_counter;
void InitWiFi()
{
  Serial.println("Connecting to AP ...");
  // attempt to connect to WiFi network

  WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to AP");
}

void reconnect() {
  // Loop until we're reconnected
  status = WiFi.status();
  if ( status != WL_CONNECTED) {
    WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("Connected to AP");
  }
}

float decode_temperature_celsius(uint8_t * data) {
  uint32_t result = 0;
  for (int i = 3; i > 0; i--) {
    result = (result << 8) + data[i];
  }
  return result / 100.;
}

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify,
  int id) {
    Serial.printf("Notify callback for device %d characteristic ", id);
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");
    Serial.printf("%f\n", decode_temperature_celsius(pData));
}

static void notifyCallback0(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  notifyCallback(pBLERemoteCharacteristic, pData, length, isNotify, 0);
}

static void notifyCallback1(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  notifyCallback(pBLERemoteCharacteristic, pData, length, isNotify, 1);
}

static void notifyCallback2(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  notifyCallback(pBLERemoteCharacteristic, pData, length, isNotify, 2);
}

typedef void (*notifyCallbackType)(BLERemoteCharacteristic*, uint8_t*, size_t, bool);

static notifyCallbackType notifyCallbacks[] = {
  notifyCallback0, notifyCallback1, notifyCallback2
};

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("onDisconnect");
  }
};

bool connectToServer(BLEAdvertisedDevice * myDevice, int clientId) {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());

    BLEClient * pClient = BLEDevice::createClient();

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");
    pClient->setMTU(517); //set client to request maximum MTU from server (default is 23 otherwise)
  
    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");


    // Obtain a reference to the characteristic in the service of the remote BLE server.
    //Serial.println();
    
    // auto characteristics = pRemoteService->getCharacteristics();
    // for (auto it = characteristics->begin(); it != characteristics->end(); it++) {
    //   Serial.println(std::string(it->first));
    // }

    BLERemoteCharacteristic * pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our characteristic");

    // Read the value of the characteristic.
    if(pRemoteCharacteristic->canRead()) {
      std::string value = pRemoteCharacteristic->readValue();
      Serial.print("The characteristic value was: ");
      Serial.println(value.c_str());
    }

    if(pRemoteCharacteristic->canNotify())
      pRemoteCharacteristic->registerForNotify(notifyCallbacks[clientId]);

    connected = true;
    return true;
}

static const char * macAddresses[] = {
  "f7:ac:1d:4a:7b:b7",
  "ca:07:3f:2c:1c:10",
  "f0:ae:26:c8:65:3b"  
};

static BLEAdvertisedDevice * bleDevices[] = {
  NULL, NULL, NULL
};

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {

 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    Serial.printf("Address: %s\n", advertisedDevice.getAddress().toString().c_str());

    for (int i = 0; i < 3; i++) {
      if (strcmp(macAddresses[i], advertisedDevice.getAddress().toString().c_str()) == 0
          && advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
        bleDevices[i] = new BLEAdvertisedDevice(advertisedDevice);
        countDevices++;
        break;
      }
    }

    if (countDevices == 2) {
      Serial.println("Got all devices; stopping scan and starting connect.");      
      BLEDevice::getScan()->stop();
      doConnect = true;
      doScan = false;
    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks


// Setup an application
void setup() {
  // Initialize serial for debugging
  Serial.begin(SERIAL_DEBUG_BAUD);
  WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
  InitWiFi();
  
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
}

// Main application loop
void loop() {


  // Reconnect to WiFi, if needed
  if (WiFi.status() != WL_CONNECTED) {
    reconnect();
    return;
  }


  // Reconnect to ThingsBoard, if needed
  if (!tb.connected()) {
    // Connect to the ThingsBoard
    Serial.print("Connecting to: ");
    Serial.print(THINGSBOARD_SERVER);
    Serial.print(" with token ");
    Serial.println(TOKEN);
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN)) {
      Serial.println("Failed to connect");
      return;
    }
  }

 

  if(millis()-millis_counter > send_delay) {
    Serial.println("Sending data...");

    // Uploads new telemetry to ThingsBoard using MQTT.
    // See https://thingsboard.io/docs/reference/mqtt-api/#telemetry-upload-api
    // for more details
    float h = 10.;
    // Read temperature as Celsius (the default)
    float t = 199.;
    
    if (isnan(h) || isnan(t)) {
      Serial.println("Failed to read from DHT sensor!");
    } else {
      Serial.print("Temperature:");
      Serial.print(t);
      Serial.print(" Humidity ");
      Serial.println(h);
      tb.sendTelemetryFloat("temperature", t);
      tb.sendTelemetryFloat("humidity", h);
    }

    millis_counter = millis(); //reset millis counter
  }

  // Process messages
  tb.loop();

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    for (int i = 0; i < 3; i++) {
      if (bleDevices[i] != NULL && connectToServer(bleDevices[i], i)) {
        Serial.println("We are now connected to the BLE Server.");
      } else {
        Serial.println("We have failed to connect to the server; there is nothin more we will do.");
      }
    }
    doConnect = false;
  }

  // If we are connected to a peer BLE Server, update the characteristic each time we are reached
  // with the current time since boot.
  if (connected) {
    String newValue = "Time since boot: " + String(millis()/1000);
  }
  // if(doScan){
  //   BLEDevice::getScan()->start(0);  // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
  // }
  
  delay(1000); // Delay a second between loops.
}