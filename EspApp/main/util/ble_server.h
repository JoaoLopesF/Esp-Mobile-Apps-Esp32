/*
 * ble_server.h
 */

#ifndef UTIL_BLE_SERVER_H_
#define UTIL_BLE_SERVER_H_

///// Includes

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sdkconfig.h"

// C++

#include <string>
using std::string;

////// Defines

//// BLE

// Maximum size of message -> from ble_uart_server

#define BLE_MSG_MAX_SIZE GATTS_CHAR_VAL_LEN_MAX

// Maximum size of line (larger, due message joined in any parts)
// TODO: see it - change if you need

#define BLE_LINE_MAX_SIZE 350

// Timeout of receive line, need to control the join of splited messages (in millis)

#define BLE_TIMEOUT_RECV_LINE 1500

// BLE Task for events
// Optimized do CPU 1, if possible
// Due ESP-IDF BLE stuff and his callbacks runs in CPU 0
// You can force it to CPU 0, if needed

#if !CONFIG_FREERTOS_UNICORE
	
	// Task - comment it if not need callbacks on CPU1

	#define BLE_EVENTS_TASK_CPU 1
	#define BLE_EVENTS_TASK_PRIOR 5

	#ifdef BLE_EVENTS_TASK_CPU

		// Event (notifications) (for now only connection)

		#define BLE_EVENT_CONNECTION 1

		// Queue to store data of receive messages

		#define BLE_SIZE_QUEUE_RECV 3

	#endif
#endif

////// Classes

class BleServer;
class BleServerCallbacks;

// Ble Server - C++ class wrapper to ble_uart_server (C)

class BleServer
{
	public:

		void initialize(const char*, BleServerCallbacks*);
		void finalize();
		bool connected();
		void send(const char*);
		void send(string&);
		const uint8_t* getMacAddress();

	private:

		void processEventConnection();
		void processEventReceive();
};

// Callbacks - based in Kolban BLE callback example code

class BleServerCallbacks {

public:
	virtual ~BleServerCallbacks() {}
	virtual void onConnect() = 0;
	virtual void onDisconnect() = 0;
	virtual void onReceive(const char* message) = 0;

};

#endif /* UTIL_BLE_SERVER_H_ */

//////// End