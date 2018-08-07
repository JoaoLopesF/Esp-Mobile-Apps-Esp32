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

static BleServer Ble;

// Utility class

static Esp_Util& Util = Esp_Util::getInstance(); // @suppress("Unused variable declaration in file scope")

// BLE connected

static bool mBleConnected = false;

// Line

static string mBleLine = ""; 			// Line received via communication
static bool mBleReceivingLine = false; 	// Receiving the characters of the line

/**
 * @brief Class MyBleServerCalllbacks - based on code of Kolban
 */
class MyBleServerCalllbacks: public BleServerCallbacks {

	/**
	 * @brief OnConnect - for connections
	 */
	void onConnect() {

		// Ble connected

		mBleConnected = true;

		logD("Ble connected!");

		// Clear variables

		mBleLine = "";
		mBleReceivingLine = false;

	}

	/**
	 * @brief OnConnect - for disconnections
	 */
	void onDisconnect() {

		// Ble disconnected

		mBleConnected = false;

		logD("Ble disconnected!");

		// Clear variables

		mBleLine = "";
		mBleReceivingLine = false;

		// Flag app not connected

		mAppConnected = false;

		// Initializes app (main.cc)

		appInitialize(true);

	}

	/**
	 * @brief OnConnect - for receive data 
	 */
	void onReceive(char *data, uint8_t size) {

		// Received data via BLE server - by callback

		char aux[size + 1];
		sprintf(aux, "%.*s", size, data);

		// logV("BLE recv (%d):%s", tamanho, Util.strExpand(aux).c_str());

		// Mark the timeSeconds of the last receipt

		mLastTimeReceivedData = mTimeSeconds;

		// Process the received data

		for (uint8_t i = 0; i < size; i++) {

			char character = data[i];

			// Process the message upon receiving a new line

			if (character == '\n') {

				logD("BLE line receive: %s", mBleLine.c_str());

				mBleReceivingLine = false;

				// Process the received line

				if (mBleLine.length() > 0) {

					// Process the message (main.cc)
					processBleMessage(mBleLine);

				}

				mBleLine = "";

			} else if (character != '\r') {

				// Concat

				mBleLine.append(1u, character);
				mBleReceivingLine = true;

			}
		}
	}
};

//////// Methods

/**
 * @brief Initialize the BLE Server
 */
void bleInitialize() {

	Ble.initialize(BLE_DEVICE_NAME, new MyBleServerCalllbacks());

	Ble.debug(true); // Comment it to disable log debug

	// Reserves the bytes for strings

	mBleLine.reserve(185); // 185 is a default MTU, if is possible

	// Debug

	logI("BLE Server initialized!");
}

/**
 * @brief Finish BLE
 */
void bleFinalize() {
	
	Ble.finalize();

	logI("BLE Server finalized");
}

/**
 * @brief Is an BLE client Connected ?
 */
bool bleConnected() {

	return mBleConnected;

}

/**
 * @brief Send data to mobile app (via BLE)
 */
void bleSendData(string data) {

	if (!mBleConnected) {
		logE("BLE not connected");
		return;
	}

	// Considers the message sent as feedback as well

	mLastTimeFeedback = mTimeSeconds;

	// Add new line (need when split large messages)

	data.append("\n");

	// Debug

	//logD("data [%u] -> %s", data.size(), Util.strExpand(data).c_str());

	// Send by Ble Server

	Ble.sendData(data.c_str());

}

/**
 * @brief Check timeout on receipt of data (usefull if the message has splitted)
 */
void bleVerifyTimeouts() {

	if (mBleReceivingLine
			&& (mTimeSeconds - mLastTimeReceivedData) > BLE_TIMEOUT_RECV_LINE) {

		mBleReceivingLine = false;
		mBleLine = "";

		logD("receive timeout - line");

	}
}

//////// End
