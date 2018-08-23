/************
 * Project   : Esp-Idf-App-Mobile - Esp-Idf - Firmware on the Esp32 board - Ble
 * Programmer: Joao Lopes
 * Module    : ble - BLE C++ class to interface with BLE server
 * Comments  : 
 * Versions  :
 * ------- 	-------- 	-------------------------
 * 0.1.0 	01/08/18 	First version
 */

///// Includes

#include "esp_system.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// C++

#include <string>
using namespace std;

// Utilities

#include "util/log.h"
#include "util/esp_util.h"

// BLE UART Server

#include "util/ble_server.h"

// From the project

#include "main.h"

#include "ble.h"

///// Variables

// Log

static const char* TAG = "ble-server";

// Server class

static BleServer mBleServer;

// Utility class - uncomment if you need

//static Esp_Util& mUtil = Esp_Util::getInstance(); // @suppress("Unused variable declaration in file scope")

/**
 * @brief Class MyBleServerCalllbacks - based on code of Kolban
 */
class MyBleServerCallbacks: public BleServerCallbacks {

	/**
	 * @brief OnConnect - for connections
	 */
	void onConnect() {

		// Ble connected

		logD("Ble connected!");

		// Flag app not connected - it only setted where message initial is received

		mAppConnected = false;

	}

	/**
	 * @brief OnConnect - for disconnections
	 */
	void onDisconnect() {

		// Ble disconnected

		logD("Ble disconnected!");

		// Flag app not connected 

		mAppConnected = false;

		// Initializes app (main.cc)

		appInitialize(true);

	}

	/**
	 * @brief OnConnect - for receive data 
	 */
	void onReceive(const char *message) {

		// Received data via BLE server - by callback

		//logV("BLE recv: [%u] %s", strlen(message), message);

		// Process the message (main.cc)

		processBleMessage(message);
	}
};

//////// Methods

/**
 * @brief Initialize the BLE Server
 */
void bleInitialize() {

	mBleServer.initialize(BLE_DEVICE_NAME, new MyBleServerCallbacks());

	// Debug

	logI("BLE Server initialized!");
}

/**
 * @brief Finish BLE
 */
void bleFinalize() {
	
	mBleServer.finalize();

	logI("BLE Server finalized");
}

/**
 * @brief Is an BLE client Connected ?
 */
bool bleConnected() {

	return mBleServer.connected();

}

/**
 * @brief Send data to mobile app (via BLE)
 * Note: char* wrapper
 */
void bleSendData(const char* data) {
	string aux = data;
	bleSendData(aux);
}
/**
 * @brief Send data to mobile app (via BLE)
 */
void bleSendData(string& data) {

	if (!mBleServer.connected()) {
		logE("BLE not connected");
		return;
	}

	// Considers the message sent as feedback as well

	mLastTimeFeedback = mTimeSeconds;

	// Debug

	//logD("data [%u] -> %s", data.size(), Util.strExpand(data).c_str());

	// Send by Ble Server

	mBleServer.send(data);

}

/**
 * @brief Return the mac address
 */
const uint8_t* bleMacAddress() {

	return mBleServer.getMacAddress();
}

//////// End
