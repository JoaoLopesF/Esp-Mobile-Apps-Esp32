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
#include "freertos/queue.h"
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

static bool mLogActive = false;			// Log active ?

static Esp_Util& Util = Esp_Util::getInstance(); // @suppress("Unused variable declaration in file scope")

// Callbacks

BleServerCallbacks* mBleServerCallbacks;

#if BLE_TASK_RECEB_CPU == 1 // Only if it is for CPU 1

// Data for receiving queue

typedef struct {
	char data[BLE_MAX_SIZE_RECV + 1];
	uint8_t size;
} BleQueueReceb_t;

#endif

////// FreeRTOS

#if BLE_TASK_RECEB_CPU == 1 // Only if it is for CPU 1

// Task for receiving data

TaskHandle_t xTaskReceiveHandle = NULL;

// BLE Server receiving queue

static QueueHandle_t xBleQueueReceive;

#endif

///// Protoypes - private

// Task FreeRTOS

extern "C" {

#if BLE_TASK_RECEB_CPU == 1 // Only if it is for CPU 1

// Receiving data task

static void receive_Task(void *pvParameters);

#endif

// Callbacks for esp_uart_server

static void bleCallbackConnection();
static void bleCallbackMTU();
static void bleCallbackReceiveData(char* data, uint8_t size);
}

//////// Methods

void BleServer::initialize(const char* deviceName, BleServerCallbacks* pBleServerCallbacks) {

	// Initialize the connection

	logI("Initializing ble - device name=%s", deviceName);

	///////// BLE

	// Initialize the BLE

	// Set the callbacks

	mBleServerCallbacks = pBleServerCallbacks;

	// BLE routines in C based on pcbreflux example

	ble_uart_server_Initialize(deviceName);

	ble_uart_server_SetCallbackConnection(bleCallbackConnection,
			bleCallbackMTU);
	ble_uart_server_SetCallbackReceiveData(bleCallbackReceiveData);

#if BLE_TASK_RECEB_CPU == 1 // Only if it is for CPU 1

	// Create receive task in core 1

	xTaskCreatePinnedToCore(&receive_Task, "bleReceive_Task", 10240, NULL,
			BLE_TASK_RECEV_PRIOR, &xTaskReceiveHandle,
			BLE_TASK_RECEB_CPU);

	logI("Task for receipts started");

#endif

	logI("BLE initialized");

}

void BleServer::finalize() {

	// Ends

#if BLE_TASK_RECEB_CPU == 1 // Only if it is for CPU 1

	if (xTaskReceiveHandle != NULL) {

		vTaskDelete(xTaskReceiveHandle);

		// Delete a queue

		if (xBleQueueReceive != NULL) {
			vQueueDelete(xBleQueueReceive);
		}

		logI("Task ble receive encerrada");
	}

#endif

	// TODO: need also more ??? 
}

void BleServer::debug(bool on) {

	// Turns on/off the logging for this module

	mLogActive = on;

}

bool BleServer::connected() {

	// APP connected? (via BLE)

	return mConnected;

}

void BleServer::sendData(const char* data) {

	// Send data to APP (via BLE)

	if (!mConnected) {
		logE("not connected");
		return;
	}

	// Send data via UART BLE Server

	uint8_t size = strlen(data);

	logV("BLE [%d] -> %s", size, Util.strExpand(data).c_str());

	// Maximum of the sending (now it is via MTU)

	uint16_t maximum = ble_uart_server_MTU();

	if (maximum > BLE_MAX_SIZE_SEND) {
		maximum = BLE_MAX_SIZE_SEND;
	}

	// Send data, respecting the maximum size

	char send[maximum + 1];

	memset(send, 0, maximum + 1);

	uint8_t posSend = 0;

	for (uint8_t i = 0; i < size; i++) {

		send[posSend++] = data[i];

		if (posSend == maximum || i == (size - 1)) {// Has it reached the maximum or the end?

			// Send the data

			if (mLogActive && size > maximum) { // Only log if needing split
				logV("BLE sending part [%d] -> %s [max=%d]",
						posSend, Util.strExpand(send).c_str(), maximum);
			}

			// BLE routines in C based on pcbreflux example

			ble_uart_server_SendData(send, posSend);

			// Next shipment

			posSend = 0;

			memset(send, 0, maximum + 1);

		}
	}
}

///// Private - code in C

extern "C" {

#if BLE_TASK_RECEB_CPU == 1 // Only if it is for CPU 1

static void receive_Task(void *pvParameters) {

	// Task to receive data from BLE,
	// to use CPU 1 to process it

	logI("Initializing ble receive Task");

	// Create a queue to send queue data

	xBleQueueReceive = xQueueCreate(BLE_SIZE_QUEUE_RECV, sizeof(BleQueueReceb_t));

	if (xBleQueueReceive == NULL) {
		logE("Error occurred while creating queue");
	}

	/// Data to receive from the Ble Server queue

	BleQueueReceb_t dataQueue;

	////// Loop

	for (;;) {

		// Expect to receive something from the Ble Server queue
		// Adopted this logic to force processing on CPU 1

		BaseType_t ret = xQueueReceive(xBleQueueReceive, &dataQueue, portMAX_DELAY);

		if (ret == pdPASS) {

			// Check the queue

			if (dataQueue.size > 0) {

				// Data received

				if (mLogActive) {
					logV("queue -> extracted data (free %d), making the callback",
							uxQueueSpacesAvailable(xBleQueueReceive));
				}

				// Callback

				if (mBleServerCallbacks) {
					mBleServerCallbacks->onReceive(dataQueue.data,
							dataQueue.size);
				}

			} else {

				// Notice

				logE("queue_task -> queue -> tam 0!!!");
			}

		} else {

			// Notice

			logE("queue_task -> error!");
		}

	}

	////// End

	logI ("receive task closed");

	// Dete a queue

	if (xBleQueueReceive != NULL) {
		vQueueDelete(xBleQueueReceive);
	}

	// Delete this task

	vTaskDelete(NULL);
}

#endif

// BLE routines in C based on pcbreflux example

static void bleCallbackConnection() { // @suppress("Unused static function")

	// Callback for connection
	// TODO: this need a task too to run in CPU 1, if the connection process is heavy

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

static void bleCallbackMTU() { // @suppress("Unused static function")

	// Callback for MTU changed

	logI("BLE MTU changed to %d",ble_uart_server_MTU());

}

static void bleCallbackReceiveData(char *data, uint8_t size) { // @suppress("Unused static function")

	// Received data via BLE notification

	char aux[size + 1];
	sprintf(aux, "%.*s", size, data);

	logV("BLE received [%d] : %s", size, Util.strExpand(aux).c_str());
	
#if BLE_TASK_RECEB_CPU == 1 // Only if it is for CPU 1

	// Send data to the receiving queue

	// Store the data in the queue

	BleQueueReceb_t dataQueue;

	if (size > BLE_MAX_SIZE_RECV) {
		size = BLE_MAX_SIZE_RECV;
	}

	bzero(dataQueue.data, BLE_MAX_SIZE_RECV);
	strncpy (dataQueue.data, data, size);
	dataQueue.size = size;

	// Send the data to the queue (it waits if the queue is full)

	if (xQueueSend (xBleQueueReceive, &dataQueue, portMAX_DELAY) == pdFAIL) {

		logE("Error putting data into queue");

	} else {

		//logV("given in queue %d", size);
	}

#else // CPU 0 -> callback right here

	if (size> 0) {

		// Callback

		if (mBleServerCallbacks) {
			mBleServerCallbacks-> onReceive (aux, size);
		}

	}

#endif

}

} // Extern "C"


//////// End