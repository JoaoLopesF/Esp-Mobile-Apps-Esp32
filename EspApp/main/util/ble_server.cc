/* ***********
 * Project   : util - Utilities to esp-idf
 * Programmer: Joao Lopes
 * Module    : ble_server - C++ class wrapper to esp_ble_uart_server
 * Comments  : wrapper to C routines, based in pcbreflux example
 * Versions  :
 * ------- 	-------- 	-------------------------
 * 0.1.0 	01/08/18 	First version
 * 0.1.1	20/08/18	BLE task for event can be disabled now
 * 0.3.0	23/08/18	Adjustments to allow sizes of BLE > 255
 * 						BLE has a Message now to receive data
 * 						Need when have more 1 message, to avoid empty string on event
 *
 **/

// Includes

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/Queue.h"
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

// Util

static Esp_Util& mUtil = Esp_Util::getInstance(); // @suppress("Unused variable declaration in file scope")

// Callbacks

static BleServerCallbacks* mBleServerCallbacks;

///// Protoypes - private

// FreeRTOS

#ifdef BLE_EVENTS_TASK_CPU // Task for events on CPU 1

	// Task FreeRTOS for events (for now only connection)

	static void event_Task(void *pvParameters);

	static TaskHandle_t xTaskEventHandle = NULL;

	// Task FreeRTOS for receive message - 23/08/18

	static void receive_Task(void *pvParameters);

	static TaskHandle_t xTaskReceiveHandle = NULL;

	// Event Message - 23/08/18

	static QueueHandle_t xQueueReceiveMessage;

	typedef struct
	{
		char message [BLE_LINE_MAX_SIZE + 1];
		uint16_t size;
	} BleReceiveMessage_t;

#endif

// Callbacks for esp_uart_server

extern "C" {

static void bleCallbackConnection();
static void bleCallbackMTU();
static void bleCallbackReceiveData(char* data, uint16_t size);

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

	// Create task for event in core 1

	xTaskCreatePinnedToCore(&event_Task, "bleEvent_Task", 5120, NULL,
			BLE_EVENTS_TASK_PRIOR, &xTaskEventHandle,
			BLE_EVENTS_TASK_CPU);

	logI("Task for event started");

	// Create task for receive message in core 1

	xTaskCreatePinnedToCore(&receive_Task, "bleRecv_Task", 5120, NULL,
			BLE_EVENTS_TASK_PRIOR, &xTaskReceiveHandle,
			BLE_EVENTS_TASK_CPU);

	logI("Task for receive message started");

#endif

	// Reserves the bytes for strings avoid fragmetaion

	mLineBuffer.reserve(BLE_LINE_MAX_SIZE); 

	// Debug 

	logI("BLE initialized");

}

/**
* @brief Finalize the BLE server
*/
void BleServer::finalize() {

	logI("Finalizing Ble server ...");

#ifdef BLE_EVENTS_TASK_CPU // Task for events on CPU 1

	if (xTaskEventHandle != NULL) {

		vTaskDelete(xTaskEventHandle);

		logI("Task for event deleted");

		if (xQueueReceiveMessage != NULL) {
			vQueueDelete(xQueueReceiveMessage);
		}

		vTaskDelete(xTaskReceiveHandle);

		logI("Task for receive deleted");
	}

#endif

	// Finalize it
	
	ble_uart_server_Finalize();

	logI("Finalized Ble server");

}

/**
* @brief Is client (mobile App) connected? (via BLE) 
*/
bool BleServer::connected() {

	return mConnected;

}

/**
* @brief Send data to App mobile (by BLE UART Server)
* Note: char* wrapper
*/
void BleServer::send(const char* data) {

	string aux = data;
	send(aux);
}

/**
* @brief Send data to App mobile (by BLE UART Server)
*/
void BleServer::send(string& data) {

	// Send data by UART BLE Server

	if (!mConnected) {
		logE("not connected");
		return;
	}

	// Process data

	uint16_t size = data.size();

	// Add new line, if not have it (need when split large messages)

	if (data[size-1] != '\n') {
		data.append(1u, '\n');
		size++;
	}

	logV("BLE message [%d] -> %s", size, mUtil.strExpand(data).c_str());

	// Maximum of the sending (now it is by ble_uart_server current MTU)

	uint16_t maximum = ble_uart_server_MTU();

	if (maximum > BLE_MSG_MAX_SIZE) {
		maximum = BLE_MSG_MAX_SIZE;
	}

	// Send data, respecting the maximum size, spliting if necessary

	char send[maximum + 1];

	memset(send, 0, maximum + 1);

	uint16_t posSend = 0;
	
	for (uint16_t i = 0; i < size; i++) {

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

const uint8_t* BleServer::getMacAddress() {

	return ble_uart_server_MacAddress();

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
void processEventReceive(char* data) {

	// Warning for bug: empty data

	if (strlen(data) == 0) {
		logW("empty message");
		return;
	}

	// Callback to data received

	if (mBleServerCallbacks) {
		mBleServerCallbacks->onReceive(data);
	}
}

#ifdef BLE_EVENTS_TASK_CPU // Task for events on CPU 1

/**
* @brief Task for event to run callbacks in CPU 1
*/
static void event_Task(void *pvParameters) {

	// Initialize

	logI("Initializing ble event Task");

	// Notification variable 

	uint32_t notification; 

	// Task loop

	for (;;) {

		// Wait something notified (seen in the FreeRTOS example) 

		if (xTaskNotifyWait (0, 0xffffffff, &notification, portMAX_DELAY) == pdPASS) { 

			// Process event by task notification

			logD ("Event received -> %u", notification); 

			switch (notification) { // (Only connection for now)

				case BLE_EVENT_CONNECTION: 	// Connection/disconection

					// Process event

					processEventConnection();
					break;	
			}
		}
	}

	////// End

	// Delete this task

	vTaskDelete(NULL);
	xTaskEventHandle = NULL;
}

/**
* @brief Task for receive messages to run callbacks in CPU 1
*/
static void receive_Task(void *pvParameters) {

	// Initialize

	logI("Initializing ble receive Task");

    // Create Message to receive message 

	xQueueReceiveMessage = xQueueCreate(BLE_SIZE_QUEUE_RECV, sizeof(BleReceiveMessage_t));

	if (xQueueReceiveMessage == NULL) {
		logE("Error on create Message");
	}

	// Message date

	BleReceiveMessage_t queueMessage;

	// Task loop

	for (;;) {

		// Expect to receive something from the Message
		// Adopted this logic to force processing on CPU 1
		// BLE has a Message now to receive messages (line) - 23/08/17
		// Need when have more 1 message, to avoid empty string on event

		BaseType_t ret = xQueueReceive(xQueueReceiveMessage, &queueMessage, portMAX_DELAY);

		if(ret == pdPASS) {

			// Verify Message

			if (queueMessage.size > 0) {

				// Message received

				if (mLogActive) {
					logV("Message -> data extracted (free %d), do the callback", uxQueueSpacesAvailable(xQueueReceiveMessage));
				}

				// Callback

				if (mBleServerCallbacks) {
					mBleServerCallbacks->onReceive(queueMessage.message);
				}

			} else {

				// Warning

				logE("Message -> size 0!");
			}

		} else {

			// Warning

			logE("error on get Message!");
		}
	}

	////// End

	// Delete this task

	if (xQueueReceiveMessage != NULL) {
		vQueueDelete(xQueueReceiveMessage);
	}

	vTaskDelete(NULL);
	xTaskEventHandle = NULL;
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

	// Notify taks

	xTaskNotify (xTaskEventHandle, BLE_EVENT_CONNECTION, eSetValueWithOverwrite);

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
static void bleCallbackReceiveData(char *data, uint16_t size) { // @suppress("Unused static function")

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

				logD("BLE line message received: %s", mLineBuffer.c_str());

				// Process this event

#ifdef BLE_EVENTS_TASK_CPU // Task for events on CPU 1

				// Add this to Message (necessary to task events not lose data) - 23/08/18

				BleReceiveMessage_t queueMessage;

				queueMessage.size = mLineBuffer.size();

				if (queueMessage.size > BLE_LINE_MAX_SIZE) {
					queueMessage.size = BLE_LINE_MAX_SIZE;
					logW("size overflow");
				}

				bzero (queueMessage.message, BLE_LINE_MAX_SIZE); 
				strncpy(queueMessage.message, mLineBuffer.c_str(), queueMessage.size);

				// Send message to queu, wait if it is full

				if (xQueueSend (xQueueReceiveMessage,  &queueMessage, portMAX_DELAY) == pdFAIL) {

					logE("Error to send queue");

				} else {

					logV("Message put on queue");
				}

#else // CPU 0 -> callback right here

				processEventReceive(mLineBuffer.c_str());
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