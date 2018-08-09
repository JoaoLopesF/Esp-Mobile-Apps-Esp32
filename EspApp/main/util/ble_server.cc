/* ***********
 * Project   : util - Utilities to esp-idf
 * Programmer: Joao Lopes
 * Module    : ble_server - C++ class wrapper to esp_ble_uart_server
 * Comments  : wrapper to C routines, based in pcbreflux example
 * Versions  :
 * ------- 	-------- 	-------------------------
 * 0.1.0 	01/08/18 	First version
 **/

// Includes

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "sdkconfig.h"

// C++

#include <string>
using std::string;

// Utilities

#include "log.h"
#include "esp_util.h"

// BLE UART SERVER in C (based in pbcreflux example)

extern "C" {
#include "ble_uart_server.h"
}

// This project

#include "ble_server.h"

////// Variables

static const char* TAG = "ble_server";	// Log tag

static bool mConnected = false;			// Connected ?

static string mLineBuffer = ""; 		// Line buffer of data received via communication 
static string mMessageRecv = ""; 		// Menssage received 

// Util

static Esp_Util& mUtil = Esp_Util::getInstance(); // @suppress("Unused variable declaration in file scope")

// Callbacks

static BleServerCallbacks* mBleServerCallbacks;

////// FreeRTOS

#ifdef BLE_EVENTS_TASK_CPU // Task for events on CPU 1

// Task for events

static TaskHandle_t xTaskEventsHandle = NULL;

#endif

///// Protoypes - private

#ifdef BLE_EVENTS_TASK_CPU // Task for events on CPU 1

// Task FreeRTOS for events

static void events_Task(void *pvParameters);

#endif

// Callbacks for esp_uart_server

extern "C" {

static void bleCallbackConnection();
static void bleCallbackMTU();
static void bleCallbackReceiveData(char* data, uint8_t size);

} // extern "C"

//////// Methods

/**
* @brief Initialize the BLE server
*/
void BleServer::initialize(const char* deviceName, BleServerCallbacks* pBleServerCallbacks) {

	logI("Initializing BLE Server - device name=%s", deviceName);

	// Set the callbacks

	mBleServerCallbacks = pBleServerCallbacks;

	// BLE routines in C based on pcbreflux example

	ble_uart_server_Initialize(deviceName);

	ble_uart_server_SetCallbackConnection(bleCallbackConnection,
			bleCallbackMTU);
	ble_uart_server_SetCallbackReceiveData(bleCallbackReceiveData);

#ifdef BLE_EVENTS_TASK_CPU // Task for events on CPU 1

	// Create task for events in core 1

	xTaskCreatePinnedToCore(&events_Task, "bleEvents_Task", 5120, NULL,
			BLE_EVENTS_TASK_PRIOR, &xTaskEventsHandle,
			BLE_EVENTS_TASK_CPU);

	logI("Task for events started");

#endif

	// Reserves the bytes for strings avoid fragmetaion

	mLineBuffer.reserve(BLE_MSG_MAX_SIZE); 
	mMessageRecv.reserve(BLE_MSG_MAX_SIZE); 

	// Debug 

	logI("BLE initialized");

}

/**
* @brief Finalize the BLE server
*/
void BleServer::finalize() {

#ifdef BLE_EVENTS_TASK_CPU // Task for events on CPU 1

	if (xTaskEventsHandle != NULL) {

		vTaskDelete(xTaskEventsHandle);

		logI("Task for ble events deleted");
	}

#endif

	// Finalize it

	ble_uart_server_Finalize();

}

/**
* @brief Is client (mobile App) connected? (via BLE) 
*/
bool BleServer::connected() {

	return mConnected;

}

/**
* @brief Send data to APP (by BLE)
*/
void BleServer::send(const char* data) {

	if (!mConnected) {
		logE("not connected");
		return;
	}

	// Send data via UART BLE Server

	uint8_t size = strlen(data);

	logV("BLE message [%d] -> %s", size, mUtil.strExpand(data).c_str());

	// Maximum of the sending (now it is by ble_uart_server current MTU)

	uint16_t maximum = ble_uart_server_MTU();

	if (maximum > BLE_MSG_MAX_SIZE) {
		maximum = BLE_MSG_MAX_SIZE;
	}

	// Send data, respecting the maximum size, spliting if necessary

	char send[maximum + 1];

	memset(send, 0, maximum + 1);

	uint8_t posSend = 0;

	for (uint8_t i = 0; i < size; i++) {

		send[posSend++] = data[i];

		if (posSend == maximum || i == (size - 1)) {// Has it reached the maximum or the end?

			// Send the data

			if (size > maximum) { // Only log if needing split
				logV("BLE sending part [%d] -> %s [max=%d]",
						posSend, mUtil.strExpand(send).c_str(), maximum);
			}

			// BLE routines in C based on pcbreflux example

			ble_uart_server_SendData(send, posSend);

			// Next shipment

			posSend = 0;

			memset(send, 0, maximum + 1);

		}
	}
}

///// Privates 

/**
* @brief Process event for connection/disconnection
* This code is called or by event task (CPU 1) or direct (no event task) 
*/
void processEventConnection() {

	if (ble_uart_server_ClientConnected()) { // Connected

		logI ("BLE client connected");

		mConnected = true;

		// Callback

		if (mBleServerCallbacks) {
			mBleServerCallbacks->onConnect();
		}

	} else { // Disconnected

		logI ("BLE client disconnected");
		
		mConnected = false;

		// Callback

		if (mBleServerCallbacks) {
			mBleServerCallbacks->onDisconnect();
		}
	}
}

/**
* @brief Process event for receive messages (lines)
* This code is called or by event task (CPU 1) or direct (no event task) 
*/
void processEventReceive() {

	// Callback to data received

	if (mBleServerCallbacks) {
		mBleServerCallbacks->onReceive(mMessageRecv.c_str());
	}

	// Clear It

	mMessageRecv = "";

}

#ifdef BLE_EVENTS_TASK_CPU // Task for events on CPU 1

/**
* @brief Task for events to run callbacks in CPU 1
*/
static void events_Task(void *pvParameters) {

	logI("Initializing ble events Task");

	uint32_t notification; // Notification variable 

	for (;;) {

		// Wait something notified (seen in the FreeRTOS example) 

		if (xTaskNotifyWait (0, 0xffffffff, &notification, portMAX_DELAY) == pdPASS) { 

			// Process event by task notification

			logD ("Event received -> %u", notification); 

			switch (notification) {

				case BLE_EVENT_CONNECTION: 	// Connection/disconection
					processEventConnection();
					break;
				case BLE_EVENT_RECEIVE: 	// Receive message (line)
					processEventReceive();
					break;

			}
		}
	}

	////// End

	// Delete this task

	vTaskDelete(NULL);
	xTaskEventsHandle = NULL;
}

#endif

////// BLE callbacks from ble_uart_server

extern "C" {

/**
* @brief Callback for connection
*/
static void bleCallbackConnection() { // @suppress("Unused static function")

	// Process this event

#ifdef BLE_EVENTS_TASK_CPU // Task for events on CPU 1

	xTaskNotify (xTaskEventsHandle, BLE_EVENT_CONNECTION, eSetValueWithOverwrite);

#else // CPU 0 -> callback right here

	processEventConnection();

#endif

}

/**
* @brief Callback for MTU changed by BLE client (mobile APP)
*/
static void bleCallbackMTU() { // @suppress("Unused static function")

	logI("BLE MTU changed to %d",ble_uart_server_MTU());

}

/**
* @brief Received data via BLE notification
*/
static void bleCallbackReceiveData(char *data, uint8_t size) { // @suppress("Unused static function")

	static uint32_t lastTime=0;// To control timeout of receving messages (lines)

	// Verify time of last receipt, if line buffer is no empty

	if (mLineBuffer.size() > 0 && 
		(millis() - lastTime) >= BLE_TIMEOUT_RECV_LINE) {

		// Timeout -> clear the buffer

		logI("timeout - clear buffer");

		mLineBuffer = "";

	}

	// Mark this time 

	lastTime = millis();

	// Received data via BLE server - by callback

	char aux[size + 1];
	sprintf(aux, "%.*s", size, data);

	logV("BLE received [%d] : %s", size, mUtil.strExpand(aux).c_str());
	
	// Process the received data

	for (uint8_t i = 0; i < size; i++) {

		char character = data[i];

		// Process the message upon receiving a new line

		if (character == '\n') { // New line

			if (mLineBuffer.length() > 0) { // Not empty ?

				// Copy buffer to message variable (necessary to task events not lose data)

				mMessageRecv = mLineBuffer;

				logD("BLE line message received: %s", mMessageRecv.c_str());

				// Process this event

#ifdef BLE_EVENTS_TASK_CPU // Task for events on CPU 1

				xTaskNotify (xTaskEventsHandle, BLE_EVENT_RECEIVE, eSetValueWithOverwrite);

#else // CPU 0 -> callback right here

				processEventReceive();
#endif
				// Clear buffer

				mLineBuffer = "";

			}

		} else if (character != '\r') { // Not for CR, for safe

			// Concat character to buffer

			mLineBuffer.append(1u, character);

		}
	}
}

} // Extern "C"

//////// End